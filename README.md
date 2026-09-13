# Chain of Witnesses (working title)

Historical New Testament transmission campaign — UE 5.x C++ project. Full design
context lives in [`docs/MASTER_PROMPT.md`](docs/MASTER_PROMPT.md); read that
before starting new work.

## Status

Task 1 of the Section 10 task queue is implemented: the Section 6 historical
timeline as data, plus the subsystem that gates world state off it.

| Path | What it is |
|---|---|
| `Source/ChainOfWitnesses/Public/Campaign/CampaignEventRow.h` | `FCampaignEventRow` — one row of the historical spine (dates, act, source citation, dispute notes, world-state effects). `ECampaignAct`, `ECampaignWorldStateEffect`. |
| `Source/ChainOfWitnesses/Public/Campaign/CampaignTimelineSubsystem.h`, `Private/.../CampaignTimelineSubsystem.cpp` | `UCampaignTimelineSubsystem` (`UGameInstanceSubsystem`) — owns the campaign clock, activates events as their year is reached and prerequisites are met, tracks established/destroyed hub cities and unlocked factions, exposes the current act. |
| `Content/Data/DT_CampaignEvents.json` | The 35-row Section 6 timeline (Acts I–III), ready to import as a DataTable with row struct `FCampaignEventRow`. |

Tasks 2–9 (Codex, dialogue, factions, campaign map, argumentation encounters,
Witness Missions, the AD 65 vertical slice) are not started — see the task queue
in the master prompt doc.

## Project layout

Standard UE5 C++ project: `ChainOfWitnesses.uproject`, `Source/`, `Content/`,
`Config/`. No `Binaries/`/`Intermediate/`/`Saved/` are committed (see
`.gitignore`) — those are generated the first time the project is opened/built.

**This repo does not include the Unreal Engine itself.** To work with it:

1. Install UE 5.4+ (this project targets `EngineAssociation: 5.4`).
2. Right-click `ChainOfWitnesses.uproject` → *Generate Visual Studio project
   files* (or the equivalent for your platform/IDE), then open and build.
3. In the editor's Content Browser, import `Content/Data/DT_CampaignEvents.json`
   as a DataTable with row type `CampaignEventRow`.
4. Hand the resulting `UDataTable` asset to `UCampaignTimelineSubsystem::SetTimelineTable`
   at startup (e.g. from your GameMode `BeginPlay`, or the Mode A/B config asset
   once Section 3's toggle is built).

## A flagged design note (standing rule 1)

`Event_MarkAlexandrianMission`'s `DesignUse` field calls out that Mark's mission to
Alexandria is attested far more weakly than his Gospel's composition in Rome — the
supporting tradition is later (Coptic/Orthodox, via Eusebius *Hist. eccl.* 2.16)
rather than near-contemporary like Papias. It's kept in the timeline because
Section 6 explicitly calls for it, but the Codex's Attestation Strength scoring
(Task 2) should weight it accordingly once that system exists, rather than
treating it as equally solid to, say, Peter and Paul's martyrdom at Rome (1
Clement, Eusebius).
