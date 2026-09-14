# Opening this for the first time

This repository **is** the Unreal project. There is nothing to paste into anything:
clone it, open `ChainOfWitnesses.uproject`, and the engine finds the module, the
config and the content.

What you have is the simulation layer — the rules, the content and the tests.
What does not exist yet is a single `.uasset`: no level, no pawn, no widget, no
mesh, no animation. `Content/` holds fourteen JSON files and nothing else. The
steps below are the shortest honest path from that to something that runs.

---

## 1. Engine version

`EngineAssociation` is deliberately empty, so the engine will ask which install to
use and write the answer itself. No editing required.

**But read this before you build.** Every line here was written against the UE 5.4
API by someone with no compiler. On a newer engine you are stacking version drift
on top of a first compile, and when an error appears you will not know which of the
two caused it. Two options, both fine:

- **Use 5.4** if you still can. Clean separation: any error is this project's fault.
- **Use your latest** and accept that some errors will be API changes rather than
  bugs. Tell me the exact version and I can pre-empt the known ones.

Either way you need Visual Studio 2022 with the *Game development with C++* workload
(Windows) or Xcode (macOS).

## 2. Build it, and expect errors

Right-click the `.uproject` → **Generate Visual Studio project files**, open the
solution, build. Or just double-click the `.uproject` and let it offer to rebuild.

**Expect a few dozen errors on the first build.** That is the normal outcome for
13,000 lines written without a compiler, not a sign anything is badly wrong. The
mechanical reflection rules are already checked (`python3 Tools/verify.py`), so
what is left is the category a linter cannot reach:

| Likely | Why |
|---|---|
| Missing engine includes | I included what the code needs; UE's own headers move between versions |
| Signature drift | `PostGameplayEffectExecute`, `NativeConstruct`, subsystem overrides |
| Automation test flags | `EAutomationTestFlags` changed shape after 5.4 |
| GAS macros | `ATTRIBUTE_ACCESSORS` and the attribute set boilerplate |

Save the full build log and send it to me. Working through a compile log is
something I can do well from here, and it is the fastest way through this step.

## 3. Import the fourteen tables

In the Content Browser, make a `Content/Data` folder, drag each JSON in, and choose
**DataTable** with the row struct below. This mapping is not from memory —
`Tools/datatable_lint.py` derives it from the headers and prints it.

| File | Row struct |
|---|---|
| `DT_CampaignEvents.json` | `CampaignEventRow` |
| `DT_TestimonyFragments.json` | `TestimonyFragmentDefinition` |
| `DT_Locations.json` | `CampaignLocationRow` |
| `DT_TravelRoutes.json` | `TravelRouteRow` |
| `DT_EncounterTypes.json` | `EncounterTypeRow` |
| `DT_Factions.json` | `FactionRow` |
| `DT_DeedTypes.json` | `DeedTypeRow` |
| `DT_Dialogue_JerusalemHandoff.json` | `DialogueNodeRow` |
| `DT_Dialogue_RomeAD65.json` | `DialogueNodeRow` |
| `DT_DebateOpponents.json` | `DebateOpponentRow` |
| `DT_DebateObjections.json` | `DebateObjectionRow` |
| `DT_Eras.json` | `EraDefinitionRow` |
| `DT_WitnessMissions.json` | `WitnessMissionRow` |
| `DT_CombatEncounters.json` | `CombatEncounterRow` |

If the import dialogue does not offer a struct, the module has not compiled yet —
the row structs are C++ types and do not exist until it does.

## 4. Assign them

**Project Settings → Game → Chain of Witnesses Content.** Assign each imported
table to its slot; dialogue is a list, so add both conversations to it.

That page is the whole wiring step. Before it existed, every subsystem in this
project stayed empty forever: the game would open, compile, run, and have no
factions, no fragments, no roads and no conversations, with nothing in the log to
say why. That is the worst failure mode in the design, because it looks exactly
like working software.

You can also set where the campaign begins here. The defaults are `Era_Frame` and
`Loc_Jerusalem` — the crusader frame, which is Mode A. Clear `StartingEraID` for
Mode B, which starts in the first century with no frame at all.

## 5. Make something to run

Still no level, so: **File → New Level → Empty Level**, save as
`Content/Maps/L_Bootstrap`, and set it as both `GameDefaultMap` and
`EditorStartupMap` in **Project Settings → Maps & Modes**.

Press Play and watch the Output Log. You are looking for:

```
LogChainBootstrap: Registered 196 rows of campaign content.
LogChainBootstrap: Content validated with no problems.
LogChainBootstrap: Began era 'Era_Frame' at 'Loc_Jerusalem'.
```

That line is the whole simulation coming up. If it says `Registered 0 rows`, step 4
is incomplete and the log says so explicitly.

## 6. Run the tests

**Tools → Session Frontend → Automation**, filter `ChainOfWitnesses`. Roughly sixty
tests across eleven suites. They have never run. Some will fail, and the failures
are worth reading rather than deleting — they are the first real feedback this
design has ever had.

---

## What comes after

At this point the rules layer is live and nothing is visible. In rough order of
value:

1. **A Codex screen.** `docs/CODEX_UI.md` is a finished designer contract: three
   Widget Blueprints reparented to C++ bases, no `BindWidget` properties, one
   event each. This is the fastest route to seeing the project's actual subject
   on screen.
2. **A dialogue widget** driving `UDialogueSubsystem` — the Jerusalem and Rome
   conversations are written and playable as text today.
3. **The campaign map**, which is the system with the most emergent behaviour
   already in it: news travelling the roads, the sailing season closing.
4. **A pawn and the melee layer**, which is the biggest single piece of remaining
   work and needs animation assets. `docs/COMBAT.md` documents the seam.

Be realistic about proportion: the simulation is perhaps a sixth of a finished
game of this scope, and the remaining five sixths is art, animation, level design,
audio and UI — the parts that need an editor, assets and, honestly, other people.

A text-only prototype of steps 1–3, though, is genuinely reachable alone, and it
is the version worth building first: it would prove the design before anyone spends
a day on a mesh.
