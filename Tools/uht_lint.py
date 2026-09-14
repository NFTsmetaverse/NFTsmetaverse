#!/usr/bin/env python3
"""
Static checks for the things that fail at the UnrealHeaderTool boundary.

This project has never been compiled -- there is no UE toolchain in the
environment it was written in -- so the first person to open it in the editor
will meet every one of these at once. None of this replaces a compiler. It
covers the specific, mechanical rules UHT enforces and the one class of C++
error that reflection code makes easy to commit: using a type from another
header without including it.

Usage:  python3 Tools/uht_lint.py [--quiet]
Exit code is the number of errors (warnings do not fail the run).
"""

import os
import re
import sys
from collections import defaultdict

SOURCE_ROOT = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "Source")

# Types UHT cannot reflect, as UPROPERTY members or in UFUNCTION signatures.
UNREFLECTABLE = [
    (r"\bstd::", "std:: types are not reflectable"),
    (r"\bTStrongObjectPtr\b", "TStrongObjectPtr is not reflectable (use TObjectPtr in a UPROPERTY)"),
    (r"\bTSharedPtr\b", "TSharedPtr is not reflectable"),
    (r"\bTSharedRef\b", "TSharedRef is not reflectable"),
    (r"\bTUniquePtr\b", "TUniquePtr is not reflectable"),
    (r"\bTFunction\b", "TFunction is not reflectable"),
    (r"\bTOptional\b", "TOptional is not reflectable"),
    (r"\blong\s+long\b", "long long is not a reflectable type (use int64)"),
    (r"\bunsigned\s+(int|long|short)\b", "unsigned int/long/short are not reflectable (use uint32/uint64)"),
    (r"\bconst\s+TCHAR\s*\*", "raw TCHAR* is not reflectable (use FString/FName/FText)"),
]

# uint8 is the only allowed underlying type for a BlueprintType enum.
ENUM_CLASS_RE = re.compile(r"UENUM\s*\(([^)]*)\)\s*enum\s+class\s+(\w+)\s*:\s*(\w+)")
ENUM_NO_BASE_RE = re.compile(r"UENUM\s*\(([^)]*)\)\s*enum\s+class\s+(\w+)\s*\{")

class Finding:
    def __init__(self, level, path, line, message):
        self.level, self.path, self.line, self.message = level, path, line, message

    def __str__(self):
        rel = os.path.relpath(self.path, os.path.dirname(SOURCE_ROOT))
        return f"{self.level:7} {rel}:{self.line}: {self.message}"


def strip_comments(text):
    """Blank out comments and string literals, preserving line structure."""
    out = []
    i, n = 0, len(text)
    while i < n:
        two = text[i:i + 2]
        if two == "//":
            j = text.find("\n", i)
            j = n if j < 0 else j
            out.append(" " * (j - i))
            i = j
        elif two == "/*":
            j = text.find("*/", i + 2)
            j = n if j < 0 else j + 2
            out.append("".join(c if c == "\n" else " " for c in text[i:j]))
            i = j
        elif text[i] == '"':
            j = i + 1
            while j < n and not (text[j] == '"' and text[j - 1] != "\\"):
                j += 1
            j = min(j + 1, n)
            out.append("".join(c if c == "\n" else " " for c in text[i:j]))
            i = j
        else:
            out.append(text[i])
            i += 1
    return "".join(out)


def line_of(text, index):
    return text.count("\n", 0, index) + 1


def source_files(root):
    for dirpath, _, filenames in os.walk(root):
        for name in sorted(filenames):
            if name.endswith((".h", ".cpp")):
                yield os.path.join(dirpath, name)


# --- Rule set ---------------------------------------------------------------

def check_generated_include(path, raw, code, findings):
    if not path.endswith(".h"):
        return
    needs = re.search(r"\b(UCLASS|USTRUCT|UENUM|UINTERFACE)\s*\(", code)
    includes = re.findall(r'#include\s+"([^"]+)"', raw)
    generated = [i for i in includes if i.endswith(".generated.h")]

    if needs and not generated:
        findings.append(Finding("ERROR", path, line_of(code, needs.start()),
            "declares a reflected type but never includes its .generated.h"))
        return
    if not generated:
        return

    expected = os.path.basename(path)[:-2] + ".generated.h"
    if os.path.basename(generated[-1]) != expected:
        findings.append(Finding("ERROR", path, 1,
            f"includes '{generated[-1]}' but this file must include '{expected}'"))
    if includes[-1] != generated[-1]:
        findings.append(Finding("ERROR", path, 1,
            f"'{generated[-1]}' must be the LAST include; '{includes[-1]}' follows it"))


def _balanced_body(code, start):
    """The text between the first { after `start` and its matching }."""
    open_at = code.find("{", start)
    if open_at < 0:
        return ""
    depth, i, n = 0, open_at, len(code)
    while i < n:
        if code[i] == "{":
            depth += 1
        elif code[i] == "}":
            depth -= 1
            if depth == 0:
                return code[open_at:i]
        i += 1
    return code[open_at:]


def check_generated_body(path, raw, code, findings):
    for match in re.finditer(r"\b(UCLASS|USTRUCT|UINTERFACE)\s*\(([^)]*)\)", code):
        # Scan the declaration's own body only. A fixed-size window runs on into
        # whatever is declared next and happily finds its GENERATED_BODY instead.
        body = _balanced_body(code, match.end())
        if not body:
            continue
        if "GENERATED_BODY()" not in body and "GENERATED_USTRUCT_BODY()" not in body:
            findings.append(Finding("ERROR", path, line_of(code, match.start()),
                f"{match.group(1)} has no GENERATED_BODY()"))


def check_enum_base(path, raw, code, findings):
    for match in ENUM_CLASS_RE.finditer(code):
        spec, name, base = match.groups()
        if "BlueprintType" in spec and base != "uint8":
            findings.append(Finding("ERROR", path, line_of(code, match.start()),
                f"UENUM(BlueprintType) '{name}' is based on '{base}'; BlueprintType requires uint8"))
    for match in ENUM_NO_BASE_RE.finditer(code):
        spec, name = match.groups()
        if "BlueprintType" in spec:
            findings.append(Finding("ERROR", path, line_of(code, match.start()),
                f"UENUM(BlueprintType) '{name}' has no explicit ': uint8' base"))


def _declaration_after(code, index, limit=600):
    """The declaration following a UFUNCTION/UPROPERTY macro, up to ; or {."""
    tail = code[index:index + limit]
    close = tail.find(")")
    if close < 0:
        return ""
    rest = tail[close + 1:]
    end = min([p for p in (rest.find(";"), rest.find("{")) if p >= 0] or [len(rest)])
    return rest[:end].strip()


def check_ufunction_signatures(path, raw, code, findings):
    for match in re.finditer(r"\bUFUNCTION\s*\(", code):
        decl = _declaration_after(code, match.start())
        if not decl:
            continue
        line = line_of(code, match.start())

        # UHT rejects reference and pointer returns outright.
        head = decl.split("(")[0]
        if re.search(r"&\s*\w+\s*$", head):
            kind = "a const reference" if re.match(r"^\s*const\b", head) else "a reference"
            findings.append(Finding("ERROR", path, line,
                f"UFUNCTION returns {kind}, which UHT rejects: '{head.strip()}'"))

        for pattern, why in UNREFLECTABLE:
            if re.search(pattern, decl):
                findings.append(Finding("ERROR", path, line, f"UFUNCTION signature: {why} -- '{decl[:90]}'"))

        # An out parameter must be a non-const reference; a const& array reads as
        # an input, which is legal, so only flag non-reference container returns.
        if re.match(r"^\s*(TArray|TMap|TSet)\s*<", head) and "&" not in head:
            findings.append(Finding("WARN", path, line,
                f"UFUNCTION returns a container by value: '{head.strip()}' (legal, but prefer an out parameter)"))


def check_uproperty_types(path, raw, code, findings):
    for match in re.finditer(r"\bUPROPERTY\s*\(", code):
        decl = _declaration_after(code, match.start(), limit=400)
        if not decl:
            continue
        line = line_of(code, match.start())

        for pattern, why in UNREFLECTABLE:
            if re.search(pattern, decl):
                findings.append(Finding("ERROR", path, line, f"UPROPERTY: {why} -- '{decl[:90]}'"))

        if re.search(r"\bconst\b", decl):
            findings.append(Finding("ERROR", path, line, f"UPROPERTY cannot be const: '{decl[:90]}'"))

        if re.search(r"\bstatic\b", decl):
            findings.append(Finding("ERROR", path, line, f"UPROPERTY cannot be static: '{decl[:90]}'"))

        # A bare U*/A* pointer member in a UCLASS that is not a UPROPERTY is a GC
        # hazard; one that IS a UPROPERTY should be TObjectPtr in UE5.
        if re.match(r"^\s*(class\s+)?[UA]\w+\s*\*\s*\w+", decl):
            findings.append(Finding("WARN", path, line,
                f"UPROPERTY uses a raw object pointer; UE5 prefers TObjectPtr: '{decl[:70]}'"))


def check_delegate_params(path, raw, code, findings):
    for match in re.finditer(r"DECLARE_DYNAMIC(_MULTICAST)?_DELEGATE(_\w+)?\s*\(([^;]*?)\)\s*;", code, re.S):
        args = match.group(3)
        line = line_of(code, match.start())
        if "&" in args:
            findings.append(Finding("ERROR", path, line,
                "dynamic delegate parameters cannot be references"))
        for pattern, why in UNREFLECTABLE:
            if re.search(pattern, args):
                findings.append(Finding("ERROR", path, line, f"dynamic delegate: {why}"))


def check_api_macro(path, raw, code, findings):
    """Exported reflected types need the module API macro to link from tests."""
    if not path.endswith(".h"):
        return
    for match in re.finditer(r"\bUSTRUCT\s*\(([^)]*)\)\s*struct\s+([A-Z_]+\s+)?(\w+)", code):
        spec, api, name = match.groups()
        if not api:
            findings.append(Finding("WARN", path, line_of(code, match.start()),
                f"USTRUCT '{name}' has no CHAINOFWITNESSES_API; it will not link outside this module"))


RULES = [check_generated_include, check_generated_body, check_enum_base,
         check_ufunction_signatures, check_uproperty_types, check_delegate_params,
         check_api_macro]


# --- Missing-include detection ---------------------------------------------

DEFINITION_RE = re.compile(
    r"^\s*(?:UCLASS|USTRUCT|UENUM|UINTERFACE)\s*\([^)]*\)\s*$\n"
    r"^\s*(?:class|struct|enum\s+class)\s+(?:[A-Z_]+_API\s+)?(\w+)", re.M)
PLAIN_DEF_RE = re.compile(r"^\s*(?:class|struct)\s+(?:[A-Z_]+_API\s+)?([UAFE][A-Z]\w+)\s*(?::|\{)", re.M)
ENUM_DEF_RE = re.compile(r"^\s*enum\s+class\s+(\w+)", re.M)


def build_symbol_index(files):
    """symbol -> the header that defines it."""
    index = {}
    for path in files:
        if not path.endswith(".h"):
            continue
        code = strip_comments(open(path, encoding="utf-8").read())
        for regex in (DEFINITION_RE, PLAIN_DEF_RE, ENUM_DEF_RE):
            for match in regex.finditer(code):
                index.setdefault(match.group(1), path)
    return index


def resolve_include(include, files):
    """Map an include path to a file in the module, if it is one of ours."""
    tail = include.replace("\\", "/")
    for path in files:
        norm = path.replace("\\", "/")
        if norm.endswith("/" + tail) or os.path.basename(norm) == os.path.basename(tail):
            if norm.endswith("/" + tail):
                return path
    for path in files:
        if os.path.basename(path) == os.path.basename(tail):
            return path
    return None


def check_missing_includes(files, findings):
    index = build_symbol_index(files)

    # Symbols reachable from a file: its own, plus everything its includes bring
    # in, transitively through our own headers.
    include_map = {}
    for path in files:
        raw = open(path, encoding="utf-8").read()
        include_map[path] = [resolve_include(i, files)
                             for i in re.findall(r'#include\s+"([^"]+)"', raw)]

    def reachable(path, seen=None):
        if seen is None:
            seen = set()
        if path in seen or path is None:
            return set()
        seen.add(path)
        code = strip_comments(open(path, encoding="utf-8").read())
        symbols = set()
        for regex in (DEFINITION_RE, PLAIN_DEF_RE, ENUM_DEF_RE):
            symbols |= {m.group(1) for m in regex.finditer(code)}
        # A forward declaration is enough for a pointer or reference.
        symbols |= set(re.findall(r"^\s*(?:class|struct)\s+([UAFE][A-Z]\w+)\s*;", code, re.M))
        for dep in include_map.get(path, []):
            symbols |= reachable(dep, seen)
        return symbols

    for path in files:
        code = strip_comments(open(path, encoding="utf-8").read())
        have = reachable(path)
        # Only flag symbols used by value: a declared local, a return type, a
        # member. Pointers and references are satisfied by a forward declaration.
        used = set()
        for match in re.finditer(r"\b([FE][A-Z]\w+)\b(?!\s*[*&])", code):
            used.add(match.group(1))
        for symbol in sorted(used - have):
            if symbol not in index:
                continue  # not ours; another module's problem
            if index[symbol] == path:
                continue
            first = re.search(r"\b" + symbol + r"\b", code)
            findings.append(Finding("ERROR", path, line_of(code, first.start()),
                f"uses '{symbol}' by value but does not include "
                f"'{os.path.relpath(index[symbol], os.path.join(SOURCE_ROOT, 'ChainOfWitnesses'))}'"))


def main():
    quiet = "--quiet" in sys.argv
    files = list(source_files(SOURCE_ROOT))
    findings = []

    for path in files:
        raw = open(path, encoding="utf-8").read()
        code = strip_comments(raw)
        for rule in RULES:
            rule(path, raw, code, findings)

    check_missing_includes(files, findings)

    errors = [f for f in findings if f.level == "ERROR"]
    warnings = [f for f in findings if f.level == "WARN"]

    for finding in errors + (warnings if not quiet else []):
        print(finding)

    print(f"\n{len(files)} files, {len(errors)} errors, {len(warnings)} warnings")
    return len(errors)


if __name__ == "__main__":
    sys.exit(min(main(), 250))
