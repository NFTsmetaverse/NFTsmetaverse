# Tools

Static checks for a project that has never been compiled.

```
python3 Tools/verify.py
```

No dependencies beyond a Python 3 interpreter, and no Unreal Engine. That is
the whole point: there was no UE toolchain in the environment this was written
in, so roughly 13,000 lines of C++ and fourteen DataTables have never been
through UnrealHeaderTool, a compiler, or the DataTable importer. These cover
the failures that can be found without them.

**They are not a build.** They cannot tell you the project compiles. They can
tell you it will not fail for any of the specific, mechanical reasons below.

## What each one checks

### `uht_lint.py` — the reflection boundary

The rules UHT enforces, which are invisible to an ordinary C++ reading:

- every reflected type has `GENERATED_BODY()`, and its declaration's own body is
  what gets scanned (a fixed-size window runs on into the next declaration and
  finds *its* `GENERATED_BODY` instead)
- `.generated.h` is present, correctly named, and is the **last** include
- `UENUM(BlueprintType)` is `uint8`-based
- `UFUNCTION` does not return a reference or const reference, which UHT rejects
  outright — this project hit that one for real
- no unreflectable type (`std::`, `TSharedPtr`, `TStrongObjectPtr`, `TFunction`,
  `TOptional`, raw `TCHAR*`) in a `UPROPERTY` or a `UFUNCTION` signature
- `UPROPERTY` is neither `const` nor `static`
- dynamic delegate parameters are not references

Plus the one class of ordinary C++ error that reflection code makes easy: using
a type from another header **by value** without including it. Pointers and
references are satisfied by a forward declaration and are not flagged. This
project hit that one for real too, in `CombatEncounterSubsystem.h`.

### `datatable_lint.py` — content against the C++ that receives it

UE's DataTable JSON importer is quiet about nearly everything. An unknown key is
ignored. A missing key silently takes the C++ default. **A misspelled enumerator
imports as the enum's zero value** — so a row meant to read `ExecutionParty`
becomes `Mob`, and nothing anywhere says so.

This parses the row structs out of the headers and checks each table against
the one it will be imported as: unknown keys, type mismatches (including
`bool`/number confusion in both directions), bad enumerator names with a
"did you mean", duplicate row keys, and `Name` disagreeing with the row's own
ID field.

It also prints which struct each table matched, which independently confirms
the import instructions in the main README.

### `crossref_lint.py` — IDs against the table that declares them

`datatable_lint` cannot see that a dialogue node grants `Frag_PapiasOnMark` when
the fragment is spelt `Frag_PapiasOnMatthewLogia`: both are perfectly good
`FName`s. That failure is silent at import *and* at runtime — the node simply
grants nothing, forever.

Every ID prefix in the project maps to the table that declares it. The manifest
at the top of the file is deliberately explicit rather than inferred, because
two of the mappings are not obvious (a **location** declares the hub city and
the region it belongs to) and two kinds of ID are not declared in content at all
(`NPC_` is registered in C++; `Char_` is a narrative tag). Dialogue node IDs are
scoped to their own conversation — a script may not jump into another one — but
a Witness Mission naming another conversation's opening node is normal and
allowed.

## On trusting a clean run

Each checker was written, then run against deliberately corrupted fixtures
before its clean result was believed. That order matters: the first version of
`uht_lint` reported zero findings because nothing matched, and the first version
of `datatable_lint` reported zero because any `UPROPERTY` carrying a
`meta = (...)` clause was silently excluded from every check — which was most of
the numeric fields in the project.

If you add a rule, corrupt something and watch it fail first.
