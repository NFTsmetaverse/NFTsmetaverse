#!/usr/bin/env python3
"""
Check that every ID one table names actually exists in another.

datatable_lint.py checks each table against its C++ struct: right keys, right
types, real enumerators. It cannot see that a dialogue node grants
'Frag_PapiasOnMark' when the fragment is spelt 'Frag_PapiasMark', because both
are perfectly good FNames. That failure is silent at import AND at runtime --
the node simply grants nothing, forever.

This project uses a consistent ID prefix per kind of thing, so the check is
tractable: every prefixed string in every table must exist in whichever table
declares that prefix.

The manifest below is deliberately explicit rather than inferred. Two of these
are not obvious -- a location declares the hub city and the region it belongs
to -- and two kinds of ID are not declared in content at all.

Usage:  python3 Tools/crossref_lint.py [--list]
Exit code is the number of dangling references.
"""

import json
import os
import re
import sys
from collections import defaultdict

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DATA_ROOT = os.path.join(ROOT, "Content", "Data")

# prefix -> (file that declares it, field that declares it)
DECLARED_BY = {
    "Frag":    ("DT_TestimonyFragments.json", "FragmentID"),
    "Faction": ("DT_Factions.json",           "FactionID"),
    "Event":   ("DT_CampaignEvents.json",     "EventID"),
    "Deed":    ("DT_DeedTypes.json",          "DeedTypeID"),
    "Obj":     ("DT_DebateObjections.json",   "ObjectionID"),
    "Opp":     ("DT_DebateOpponents.json",    "OpponentID"),
    "Loc":     ("DT_Locations.json",          "LocationID"),
    "Enc":     ("DT_EncounterTypes.json",     "EncounterID"),
    "Cbt":     ("DT_CombatEncounters.json",   "EncounterTypeID"),
    "WM":      ("DT_WitnessMissions.json",    "MissionID"),
    "Era":     ("DT_Eras.json",               "EraID"),
    # A location declares which hub city it is and which region it sits in;
    # campaign events and encounter types then refer to those.
    "Hub":     ("DT_Locations.json",          "HubCityID"),
    "Region":  ("DT_Locations.json",          "RegionID"),
    # A route has no identity field of its own -- it is keyed by the (From, To)
    # pair -- so its row key is the only thing that names it.
    "Route":   ("DT_TravelRoutes.json",       "Name"),
}

# Dialogue node IDs are scoped to their own conversation: within a script, a
# node pointing at a node in a different script is a bug. From outside, naming
# another conversation's node is the normal thing -- that is how a Witness
# Mission opens one -- so the scoping rule applies only to references made from
# a table that declares nodes itself.
FILE_SCOPED = {"Node": "NodeID"}

# Declared in C++ at runtime (RegisterNpcProfile) or used as narrative tags, so
# there is no table to check them against. Listed so they are a deliberate
# exception rather than a silent one.
NOT_IN_CONTENT = {
    "NPC":  "registered in C++ via UReputationSubsystem::RegisterNpcProfile",
    "Char": "narrative character tags on Witness Missions",
}

ID_RE = re.compile(r"^([A-Za-z]+)_[A-Za-z0-9_]+$")


def load_tables():
    tables = {}
    for filename in sorted(os.listdir(DATA_ROOT)):
        if filename.endswith(".json"):
            tables[filename] = json.load(open(os.path.join(DATA_ROOT, filename), encoding="utf-8"))
    return tables


def walk_strings(value, path="") :
    """Yield (json path, string) for every string anywhere in the structure."""
    if isinstance(value, str):
        yield path, value
    elif isinstance(value, list):
        for index, item in enumerate(value):
            yield from walk_strings(item, f"{path}[{index}]")
    elif isinstance(value, dict):
        for key, item in value.items():
            yield from walk_strings(item, f"{path}.{key}" if path else key)


def collect_declared(tables):
    declared = defaultdict(set)
    for prefix, (filename, field) in DECLARED_BY.items():
        for row in tables.get(filename, []):
            value = row.get(field)
            if isinstance(value, str) and value:
                declared[prefix].add(value)
            elif isinstance(value, list):
                declared[prefix].update(v for v in value if isinstance(v, str) and v)
        # Some declaring fields are plural on the row (RegionIDs on a location).
        for row in tables.get(filename, []):
            plural = row.get(field + "s")
            if isinstance(plural, list):
                declared[prefix].update(v for v in plural if isinstance(v, str) and v)
    return declared


def collect_file_scoped(tables):
    scoped = defaultdict(lambda: defaultdict(set))
    for prefix, field in FILE_SCOPED.items():
        for filename, rows in tables.items():
            for row in rows:
                value = row.get(field)
                if isinstance(value, str) and value:
                    scoped[prefix][filename].add(value)
    return scoped


def main():
    listing = "--list" in sys.argv
    tables = load_tables()
    declared = collect_declared(tables)
    scoped = collect_file_scoped(tables)

    dangling, unknown_prefixes, exempt = [], defaultdict(set), defaultdict(set)

    for filename, rows in tables.items():
        for index, row in enumerate(rows):
            label = row.get("Name", f"#{index}")
            for path, value in walk_strings(row):
                match = ID_RE.match(value)
                if not match:
                    continue
                prefix = match.group(1)

                if prefix in NOT_IN_CONTENT:
                    exempt[prefix].add(value)
                    continue

                if prefix in FILE_SCOPED:
                    declares_nodes = bool(scoped[prefix].get(filename))
                    if declares_nodes:
                        if value not in scoped[prefix][filename]:
                            where = [f for f, ids in scoped[prefix].items() if value in ids]
                            hint = (f" (it is declared in {where[0]}; a script may not jump "
                                    f"into another conversation)") if where else ""
                            dangling.append(f"{filename}: row '{label}'.{path} -> '{value}' "
                                            f"is not a {FILE_SCOPED[prefix]} in this file{hint}")
                    else:
                        # An outside reference: any conversation will do, but it has
                        # to be a node that exists somewhere.
                        everywhere = set().union(*scoped[prefix].values()) if scoped[prefix] else set()
                        if value not in everywhere:
                            dangling.append(f"{filename}: row '{label}'.{path} -> '{value}' "
                                            f"is not a {FILE_SCOPED[prefix]} in any conversation")
                    continue

                if prefix not in DECLARED_BY:
                    unknown_prefixes[prefix].add(f"{filename}: row '{label}'.{path} -> {value}")
                    continue

                if value not in declared[prefix]:
                    pool = sorted(declared[prefix])
                    near = [c for c in pool if c.lower() == value.lower()]
                    if not near:
                        near = [c for c in pool
                                if c.lower().replace("_", "").startswith(value.lower().replace("_", "")[:12])]
                    hint = f" -- did you mean '{near[0]}'?" if near else ""
                    dangling.append(f"{filename}: row '{label}'.{path} -> '{value}' is not declared in "
                                    f"{DECLARED_BY[prefix][0]}{hint}")

    print("Declared:")
    for prefix in sorted(declared):
        print(f"  {prefix + '_':10} {len(declared[prefix]):3} in {DECLARED_BY[prefix][0]}")
    for prefix in sorted(scoped):
        total = sum(len(v) for v in scoped[prefix].values())
        print(f"  {prefix + '_':10} {total:3} across {len(scoped[prefix])} conversations (file-scoped)")
    for prefix in sorted(exempt):
        print(f"  {prefix + '_':10} {len(exempt[prefix]):3} not in content: {NOT_IN_CONTENT[prefix]}")
        if listing:
            for value in sorted(exempt[prefix]):
                print(f"                  {value}")

    if unknown_prefixes:
        print("\nPrefixes no table declares (add to the manifest or fix the content):")
        for prefix, uses in sorted(unknown_prefixes.items()):
            print(f"  {prefix}_  ({len(uses)} uses)")
            for use in sorted(uses)[:4]:
                print(f"    {use}")

    if dangling:
        print(f"\n{len(dangling)} dangling reference(s):")
        for item in dangling:
            print(f"  {item}")
    else:
        print("\nNo dangling references.")

    return len(dangling) + len(unknown_prefixes)


if __name__ == "__main__":
    sys.exit(min(main(), 250))
