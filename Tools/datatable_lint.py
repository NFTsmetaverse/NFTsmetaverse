#!/usr/bin/env python3
"""
Check every DataTable JSON against the USTRUCT it will be imported as.

UE's DataTable JSON importer is quiet about nearly everything that can go
wrong. An unknown key is ignored. A missing key silently takes the C++ default.
A misspelled enumerator imports as the enum's zero value -- so a row meant to
read ExecutionParty becomes Mob, and nothing anywhere says so. None of these
tables has ever been imported, so none of that has ever been caught.

This parses the row structs out of the headers and checks the JSON against
them: unknown keys, type mismatches, bad enumerator names, duplicate row keys,
and the Name/ID agreement this project's tables rely on.

Usage:  python3 Tools/datatable_lint.py [--verbose]
Exit code is the number of errors.
"""

import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SOURCE_ROOT = os.path.join(ROOT, "Source")
DATA_ROOT = os.path.join(ROOT, "Content", "Data")

SCALARS = {
    "int32": (int,), "int64": (int,), "uint8": (int,), "uint32": (int,),
    "float": (int, float), "double": (int, float),
    "bool": (bool,),
    "FName": (str,), "FString": (str,), "FText": (str,),
}


def strip_comments(text):
    out, i, n = [], 0, len(text)
    while i < n:
        two = text[i:i + 2]
        if two == "//":
            j = text.find("\n", i); j = n if j < 0 else j
            out.append(" " * (j - i)); i = j
        elif two == "/*":
            j = text.find("*/", i + 2); j = n if j < 0 else j + 2
            out.append("".join(c if c == "\n" else " " for c in text[i:j])); i = j
        else:
            out.append(text[i]); i += 1
    return "".join(out)


def balanced_body(code, start):
    open_at = code.find("{", start)
    if open_at < 0:
        return ""
    depth, i = 0, open_at
    while i < len(code):
        if code[i] == "{":
            depth += 1
        elif code[i] == "}":
            depth -= 1
            if depth == 0:
                return code[open_at + 1:i]
        i += 1
    return code[open_at + 1:]


def parse_headers():
    """Returns (structs, enums). structs: name -> {field: type}. enums: name -> [enumerators]."""
    structs, enums, row_structs = {}, {}, set()

    for dirpath, _, filenames in os.walk(SOURCE_ROOT):
        for name in sorted(filenames):
            if not name.endswith(".h"):
                continue
            code = strip_comments(open(os.path.join(dirpath, name), encoding="utf-8").read())

            for match in re.finditer(r"\benum\s+class\s+(\w+)\s*(?::\s*\w+)?\s*\{", code):
                body = balanced_body(code, match.end() - 1)
                entries = []
                # Greedy to the LAST ')' on the line: a UMETA DisplayName can
                # itself contain parentheses ("Act I ... (AD 30-44)"), and a
                # non-greedy match stops inside the string and drops the entry.
                for entry in re.finditer(r"^\s*(\w+)\s*(?:UMETA\s*\(.*\))?\s*(?:=[^,\n]*)?,?\s*$",
                                         body, re.M):
                    value = entry.group(1)
                    if value and not value.startswith("UMETA"):
                        entries.append(value)
                enums[match.group(1)] = entries

            for match in re.finditer(r"\bUSTRUCT\s*\([^)]*\)\s*struct\s+(?:[A-Z_]+_API\s+)?(\w+)"
                                     r"\s*(?::\s*public\s+(\w+))?", code):
                struct_name, base = match.group(1), match.group(2)
                body = balanced_body(code, match.end())
                fields = {}
                for prop in re.finditer(r"\bUPROPERTY\s*\(", body):
                    # The macro's arguments nest: meta = (ClampMin = "1"). Matching
                    # [^)]* stops at the inner ')' and drags it into the declaration,
                    # which quietly turns "int32" into ") int32" and excludes the
                    # field from every check below. Balance the parens instead.
                    depth, i = 0, prop.end() - 1
                    while i < len(body):
                        if body[i] == "(":
                            depth += 1
                        elif body[i] == ")":
                            depth -= 1
                            if depth == 0:
                                break
                        i += 1
                    end = body.find(";", i)
                    if end < 0:
                        continue
                    decl = " ".join(body[i + 1:end].split())
                    decl = re.sub(r"\s*=.*$", "", decl).strip()
                    field = re.match(r"^(.*?)\s+(\w+)$", decl)
                    if field:
                        fields[field.group(2)] = field.group(1).strip()
                structs[struct_name] = fields
                if base == "FTableRowBase":
                    row_structs.add(struct_name)

    return structs, enums, row_structs


def check_value(value, ctype, structs, enums, path, problems):
    ctype = ctype.replace("TObjectPtr<", "").rstrip(">") if ctype.startswith("TObjectPtr") else ctype

    array = re.match(r"^TArray\s*<\s*(.+?)\s*>$", ctype)
    if array:
        if not isinstance(value, list):
            problems.append(f"{path}: expected a list for TArray, got {type(value).__name__}")
            return
        for index, item in enumerate(value):
            check_value(item, array.group(1), structs, enums, f"{path}[{index}]", problems)
        return

    if ctype in SCALARS:
        # bool is a subclass of int in Python; check it before the numeric types.
        if ctype == "bool":
            if not isinstance(value, bool):
                problems.append(f"{path}: expected a bool, got {json.dumps(value)[:40]}")
        elif isinstance(value, bool):
            problems.append(f"{path}: expected {ctype}, got a bool")
        elif not isinstance(value, SCALARS[ctype]):
            problems.append(f"{path}: expected {ctype}, got {json.dumps(value)[:40]}")
        elif ctype in ("int32", "int64", "uint8", "uint32") and isinstance(value, float):
            problems.append(f"{path}: expected an integer, got {value}")
        return

    if ctype in enums:
        if not isinstance(value, str):
            problems.append(f"{path}: expected an enumerator name for {ctype}, got {json.dumps(value)[:40]}")
        elif not enums[ctype]:
            problems.append(f"{path}: enum {ctype} was parsed with no enumerators -- checker bug, "
                            f"not a content bug; report it rather than editing the JSON")
        elif value not in enums[ctype]:
            near = [e for e in enums[ctype] if e.lower() == str(value).lower()]
            hint = f" (did you mean '{near[0]}'?)" if near else f" -- valid: {', '.join(enums[ctype][:6])}"
            problems.append(f"{path}: '{value}' is not a {ctype} enumerator{hint}; "
                            f"UE would import this as '{enums[ctype][0]}'")
        return

    if ctype in structs:
        if not isinstance(value, dict):
            problems.append(f"{path}: expected an object for {ctype}, got {type(value).__name__}")
            return
        for key, item in value.items():
            if key not in structs[ctype]:
                problems.append(f"{path}.{key}: '{ctype}' has no such field -- UE will ignore it")
            else:
                check_value(item, structs[ctype][key], structs, enums, f"{path}.{key}", problems)
        return

    # Unknown type: not something this checker models, so say nothing.


def camel_tokens(name):
    return set(re.findall(r"[A-Z][a-z]+", name))


def find_id_field(struct_name, fields):
    """
    The row's own identifier, if it has one.

    Not every row has one: a travel route is keyed by its (From, To) pair and
    FromLocationID is an endpoint, not an identity. So require the field to
    actually name the struct's own subject -- FCombatEncounterRow.EncounterTypeID
    shares "Encounter", FTravelRouteRow.FromLocationID shares nothing with
    "TravelRoute" -- rather than taking the first FName ending in "ID".
    """
    subject = camel_tokens(re.sub(r"(Row|Definition)$", "", struct_name))
    for field, ctype in fields.items():
        if ctype != "FName" or not field.endswith("ID"):
            continue
        if camel_tokens(field[:-2]) & subject:
            return field
        return None      # the first ID-ish field is not an identity; assume none
    return None


def pick_row_struct(rows, structs, row_structs):
    """Score each FTableRowBase struct against the table's keys."""
    keys = set()
    for row in rows:
        keys |= set(row.keys())
    keys.discard("Name")

    best, best_score = None, -1
    for name in row_structs:
        fields = set(structs[name])
        if not fields:
            continue
        score = len(keys & fields) - len(keys - fields)
        if score > best_score:
            best, best_score = name, score
    return best, best_score


def main():
    verbose = "--verbose" in sys.argv
    structs, enums, row_structs = parse_headers()

    total_errors, total_rows = 0, 0
    print(f"{len(row_structs)} row structs, {len(enums)} enums parsed from headers\n")

    for filename in sorted(os.listdir(DATA_ROOT)):
        if not filename.endswith(".json"):
            continue
        path = os.path.join(DATA_ROOT, filename)
        problems = []

        try:
            rows = json.load(open(path, encoding="utf-8"))
        except json.JSONDecodeError as error:
            print(f"{filename}: NOT VALID JSON -- {error}")
            total_errors += 1
            continue

        if not isinstance(rows, list):
            print(f"{filename}: the importer expects a top-level array")
            total_errors += 1
            continue

        struct_name, score = pick_row_struct(rows, structs, row_structs)
        if struct_name is None:
            print(f"{filename}: no FTableRowBase struct resembles these rows")
            total_errors += 1
            continue

        fields = structs[struct_name]
        seen_names = {}

        id_field = find_id_field(struct_name, fields)

        for index, row in enumerate(rows):
            total_rows += 1
            label = row.get("Name", f"#{index}")

            if "Name" not in row:
                problems.append(f"row #{index}: no 'Name', which is the DataTable row key")
            elif label in seen_names:
                problems.append(f"row '{label}': duplicate row key (first seen at #{seen_names[label]})")
            else:
                seen_names[label] = index

            for key, value in row.items():
                if key == "Name":
                    continue
                if key not in fields:
                    problems.append(f"row '{label}': '{key}' is not a field of {struct_name} "
                                    f"-- UE will silently ignore it")
                    continue
                check_value(value, fields[key], structs, enums, f"row '{label}'.{key}", problems)

            # This project's convention: the row key and the struct's own ID field
            # carry the same string, because code looks rows up by the ID field and
            # the editor lists them by Name. A disagreement means one of the two
            # paths finds nothing.
            if id_field and id_field in row and "Name" in row and row[id_field] != row["Name"]:
                problems.append(f"row '{label}': Name is '{row['Name']}' but {id_field} is "
                                f"'{row[id_field]}'; lookups use {id_field}")

        missing = set(fields) - {k for row in rows for k in row}
        status = "ok" if not problems else f"{len(problems)} PROBLEM(S)"
        print(f"{filename:34} -> {struct_name:28} {len(rows):3} rows  {status}")

        if verbose and missing:
            print(f"    (never set, will take C++ defaults: {', '.join(sorted(missing))})")
        for problem in problems:
            print(f"    {problem}")
        total_errors += len(problems)

    print(f"\n{total_rows} rows checked, {total_errors} problems")
    return total_errors


if __name__ == "__main__":
    sys.exit(min(main(), 250))
