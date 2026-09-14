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

`EngineAssociation` is empty, so the engine asks which install to use and writes
the answer itself. Nothing to edit.

**This project was written against UE 5.4 and is being opened on 5.8** — four minor
versions and roughly two years of API drift. Build on 5.8 anyway: targeting a
two-year-old engine for a project that has not shipped anything would be the wrong
trade, and the drift is a one-time cost you pay now rather than later.

What that means in practice is that some of the first build's errors will be
version differences rather than mistakes in this code, and from the error text
alone you often cannot tell which. That is expected. Send me the log and I will
sort them into the two piles.

Two things have already been changed for the jump:

- **`Target.cs` pinned `EngineIncludeOrderVersion.Unreal5_4`.** UBT retires those
  enum values a few releases after they ship, and `Target.cs` is C# compiled
  *before* any C++ — a stale value fails the build at step zero with an error that
  looks nothing like a code problem. Both targets now use `Latest`.
- **`EAutomationTestFlags` was spelled out in all nine test suites**, 55 times. It
  has already changed shape once in UE 5's life (namespace of integer constants →
  enum class). It is now `CHAIN_TEST_FLAGS`, defined once in
  `Private/Tests/ChainAutomationFlags.h`, so if 5.8 moved it again that is one edit
  rather than fifty-five.

I could not verify either against a real 5.8 install — I have no engine here, and
5.8 is past what I know reliably. They are hedges against the failure modes I can
reason about, not confirmed fixes.

You will need Visual Studio 2022 with the *Game development with C++* workload
(Windows) or current Xcode (macOS).

## 2. Build it, and expect errors

Right-click the `.uproject` → **Generate Visual Studio project files**, open the
solution, build. Or just double-click the `.uproject` and let it offer to rebuild.

**Expect a few dozen errors on the first build, and more on 5.8 than on 5.4.** That is the normal outcome for
13,000 lines written without a compiler, not a sign anything is badly wrong. The
mechanical reflection rules are already checked (`python3 Tools/verify.py`), so
what is left is the category a linter cannot reach:

| Likely | Why |
|---|---|
| Missing engine includes | I included what the code needs; UE's own headers move between versions |
| Signature drift | `PostGameplayEffectExecute`, `NativeConstruct`, subsystem overrides |
| Automation test flags | `EAutomationTestFlags` changed shape after 5.4 — now centralised in one header |
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
`Content/Maps/L_Bootstrap`, and in **Project Settings → Maps & Modes** set:

- `GameDefaultMap` and `EditorStartupMap` → `L_Bootstrap`
- **Default GameMode** → `ChainGameMode`

The GameMode matters as much as the level. It brings up `AChainPlayerController`
and `AChainHUD`, which are what make the project playable without an editor-built
interface; without it you get an empty level and a registered-but-invisible
simulation.

Press Play and watch the Output Log. You are looking for:

```
LogChainBootstrap: Registered 196 rows of campaign content.
LogChainBootstrap: Content validated with no problems.
LogChainBootstrap: Began era 'Era_Frame' at 'Loc_Jerusalem'.
LogChainFlow: Ready. Open the console (~) and type ChainHelp, or press H.
```

That is the whole simulation coming up. If it says `Registered 0 rows`, step 4 is
incomplete and the log says so explicitly.

## 5a. Actually play it

The HUD draws the current state and the keys drive it:

| Key | Does |
|---|---|
| `1`–`9` | say the numbered line |
| `C` | open and close the Codex |
| `Esc` | back out one layer |
| `Tab` | status |
| `H` | the command list |
| `~` | console |

The fastest way to see the game work end to end is the Rome slice. Open the
console and type:

```
ChainSlice
```

That grants the source which opens the reconstruction, drops you into Rome in
AD 65, and opens the scene with Peter. Then play it with the number keys. Locked
lines are shown greyed with the reason they are shut — that is the disclosure
system working, not a bug. Press `C` as you go and the fragments appear in the
Codex as the conversation yields them.

`ChainHelp` lists everything else: `ChainTravel`, `ChainWait`, `ChainDebate`,
`ChainFight`, `ChainRecover` and the rest. Those commands are the whole simulation
exposed to the keyboard, which is what lets the design be judged before any art
exists.

## 6. Run the tests

**Tools → Session Frontend → Automation**, filter `ChainOfWitnesses`. Fifty-nine
tests across ten suites. They have never run. Some will fail, and the failures
are worth reading rather than deleting — they are the first real feedback this
design has ever had.

---

## What comes after

At this point the game is playable as text and nothing is drawn. In rough order of
value:

1. **A Codex screen.** `docs/CODEX_UI.md` is a finished designer contract: three
   Widget Blueprints reparented to C++ bases, no `BindWidget` properties, one
   event each. `AChainHUD` already renders the same data, so this is a
   presentation upgrade rather than new logic.
2. **A dialogue widget** replacing the HUD's text block. The Jerusalem and Rome
   conversations are written and playable today.
3. **The campaign map**, which is the system with the most emergent behaviour
   already in it: news travelling the roads, the sailing season closing.
4. **A pawn and the melee layer**, which is the biggest single piece of remaining
   work and needs animation assets. `docs/COMBAT.md` documents the seam.

Be realistic about proportion: the simulation is perhaps a sixth of a finished
game of this scope, and the remaining five sixths is art, animation, level design,
audio and UI — the parts that need an editor, assets and, honestly, other people.

What exists now is the text prototype that proves the design before anyone spends
a day on a mesh.
