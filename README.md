# Chain of Witnesses (working title)

Historical New Testament transmission campaign — UE 5.x C++ project. Full design
context lives in [`docs/MASTER_PROMPT.md`](docs/MASTER_PROMPT.md); read that
before starting new work.

## Status

Tasks 1 and 2 of the Section 10 task queue are implemented: the historical
timeline as data with the subsystem that gates world state off it, and the Codex
of Witnesses on top of that.

**Task 1 — campaign timeline**

| Path | What it is |
|---|---|
| `Source/ChainOfWitnesses/Public/Campaign/CampaignEventRow.h` | `FCampaignEventRow` — one row of the historical spine (dates, act, source citation, dispute notes, world-state effects). `ECampaignAct`, `ECampaignWorldStateEffect`. |
| `Source/ChainOfWitnesses/Public/Campaign/CampaignTimelineSubsystem.h`, `Private/.../CampaignTimelineSubsystem.cpp` | `UCampaignTimelineSubsystem` (`UGameInstanceSubsystem`) — owns the campaign clock, activates events as their year is reached and prerequisites are met, tracks established/destroyed hub cities and unlocked factions, exposes the current act. |
| `Content/Data/DT_CampaignEvents.json` | The 35-row Section 6 timeline (Acts I–III), ready to import as a DataTable with row struct `FCampaignEventRow`. |

**Task 2 — Codex of Witnesses**

| Path | What it is |
|---|---|
| `Source/ChainOfWitnesses/Public/Codex/TestimonyFragment.h` | `FTestimonyFragmentDefinition` (Section 8's data model, authorable as a DataTable row), `UTestimonyFragment` (the data-asset form of the same), `EWitnessTier`, `ETransmissionLink`, and the tier→link affinity rule. |
| `Source/ChainOfWitnesses/Public/Codex/CodexTypes.h` | `FTransmissionChain`, `FChainLinkScore`, `FChainScoreBreakdown`, `FCodexScoringRules`, `FCodexSaveData`. |
| `Source/ChainOfWitnesses/Public/Codex/CodexSubsystem.h`, `Private/.../CodexSubsystem.cpp` | `UCodexSubsystem` — fragment registry, recovery tracking, chain assembly, Attestation Strength scoring, and the save payload. |
| `Content/Data/DT_TestimonyFragments.json` | 35 seed fragments: the Mark/Peter chain, the Pauline anchors, the manuscript tradition (including the two variants the design insists on facing), the Hostile Witness set, and the inscriptions. |
| `Source/ChainOfWitnesses/Private/Tests/CodexSubsystemTests.cpp` | Automation tests for the scoring math — proximity decay, tier mismatch, hostile/corroboration bonuses, challenge penalties, save round-trip. |

**How scoring works.** A chain is anchored to a campaign event and has six link
slots (Event → Eyewitness → Oral Proclamation → Written Source → Manuscript →
Patristic Attestation). Each slotted fragment scores on how close its attestation
date sits to the anchor event — full credit inside a 30-year living-memory window,
decaying to nothing over the following 120 years — discounted if its tier doesn't
belong in that link or if the fragment is itself contested. The chain averages its
six links, then gains points for corroborating pairs and for any hostile-tier
witness, and loses points per unanswered challenge. Every number is reported in
`FChainScoreBreakdown` along with the weakest link, so a defeat can show exactly
which link failed. Weights live in `FCodexScoringRules` and are tunable without a
recompile.

Tasks 3–9 (Codex UI, dialogue, factions, campaign map, argumentation encounters,
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
3. In the editor's Content Browser, import both JSON files as DataTables:
   `Content/Data/DT_CampaignEvents.json` with row type `CampaignEventRow`, and
   `Content/Data/DT_TestimonyFragments.json` with row type
   `TestimonyFragmentDefinition`.
4. Hand the resulting assets to `UCampaignTimelineSubsystem::SetTimelineTable` and
   `UCodexSubsystem::RegisterFragmentsFromDataTable` at startup (e.g. from your
   GameMode `BeginPlay`, or the Mode A/B config asset once Section 3's toggle is
   built).
5. Run the Codex tests from **Tools → Session Frontend → Automation**, filter
   `ChainOfWitnesses.Codex`.

## Flagged design notes (standing rule 1)

**Mark's Alexandrian mission is weakly attested.** `Event_MarkAlexandrianMission`'s
`DesignUse` field calls this out: the supporting tradition is later
(Coptic/Orthodox, via Eusebius *Hist. eccl.* 2.16) rather than near-contemporary
like Papias. It's kept in the timeline because Section 6 explicitly calls for it,
but it should not be scored as equally solid to, say, Peter and Paul's martyrdom at
Rome (1 Clement 5).

**"The apostles died for what they knew to be true" needs narrowing.** Section 8
lists the apostles' willingness to die as an apologetic theme. As usually stated
this overreaches: for most of the Twelve the martyrdom accounts are late,
legendary, and mutually inconsistent. What the evidence actually supports is
narrower and still strong — James son of Zebedee (Acts 12:2), James the Just
(Josephus, *Ant.* 20.200), and Peter and Paul at Rome (1 Clement 5, with Tacitus
establishing the Neronian persecution) — plus the general point that the movement's
leadership accepted serious risk rather than recant. Task 7's debate content should
argue the narrow version, which holds, rather than the broad one, which a
well-prepared opponent will dismantle.

**One field was added beyond Section 8's data model.**
`FTestimonyFragmentDefinition::RebutsFragmentIDs`. Section 8 specifies
`ChallengedBy` but nothing that answers a challenge, which leaves "loses strength
from unanswered challenges" with no way to ever resolve. Rebuttals are kept sparse
and must be real evidence: in the seed corpus only Irenaeus (*Adv. Haer.* 5.33.4)
rebuts anything — Eusebius' dismissal of Papias. The longer ending of Mark and the
*pericope adulterae* have no evidential rebuttal, and the chains that lean on them
take the hit, which is the honest outcome.
