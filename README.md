# Chain of Witnesses (working title)

Historical New Testament transmission campaign — UE 5.x C++ project. Full design
context lives in [`docs/MASTER_PROMPT.md`](docs/MASTER_PROMPT.md); read that
before starting new work.

## Status

Tasks 1, 2, 4 and 5 of the Section 10 task queue are implemented: the historical
timeline as data with the subsystem that gates world state off it, the Codex of
Witnesses on top of that, dialogue gated on what the player can credibly claim,
and a reputation system whose news travels no faster than a man on a road.

Task 3 (the Codex UI) is deliberately out of order — UMG widgets are binary
assets a designer builds in-editor, so the useful part from here is the data it
renders, which `FChainScoreBreakdown` and `FDialogueNodeView` already provide.

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

**Task 4 — dialogue with disclosure-state gating**

| Path | What it is |
|---|---|
| `Source/ChainOfWitnesses/Public/Dialogue/DialogueTypes.h` | `FDialogueNodeRow`, `FDialogueOption`, `FDialogueConditionSet`, `FDialogueEffects`, the view structs the UI renders, and `EConversationSafety` / `EDisclosureLevel` / `EDialogueGate`. |
| `Source/ChainOfWitnesses/Public/Dialogue/DialogueSubsystem.h`, `Private/.../DialogueSubsystem.cpp` | `UDialogueSubsystem` — dialogue content registry, conversation traversal, condition evaluation, per-NPC trust/disclosure/vouching, and the save payload. |
| `Content/Data/DT_Dialogue_JerusalemHandoff.json` | Jerusalem, c. AD 35: Saul's first meeting with Peter (Gal 1:18–19). Ten nodes exercising every gate. |
| `Source/ChainOfWitnesses/Private/Tests/DialogueSubsystemTests.cpp` | Automation tests for each gate, fallback traversal, hidden options, and the save round-trip. |

**What gates a line.** Section 5 asks for trees where what an NPC will say depends
on what the player already credibly knows, who vouched for him, and whether the
conversation is safe. Those are the three axes, plus trust and the campaign year:

- *Credibly knows* → the Codex. Holding fragments is the weak form; having a chain
  that scores above a threshold is the strong one, so Task 2's Attestation Strength
  is what actually opens doors.
- *Vouched for* → vouches recorded between NPCs, the mechanic the early Church
  actually ran on. A vouch can be to one person or general.
- *Safe* → `EConversationSafety`, set per meeting by the caller. The same man says
  different things in the Temple courtyard and behind a shut door.
- *Era* → the campaign timeline, so no one refers to the Temple's fall in AD 50.

Locked options are reported with their gate rather than silently dropped — the
player learns what the world runs on — unless the author hides them, for lines that
would themselves give away something unearned. Conditions are re-checked on
selection: the view is a presentation, never the authority.

**Task 5 — reputation, memory, and word of mouth**

| Path | What it is |
|---|---|
| `Source/ChainOfWitnesses/Public/Reputation/ReputationTypes.h` | `FFactionRow` (with inter-faction attitudes), `FDeedTypeRow`, `FTravelRouteRow`, `FReputationDeed`, `FNpcProfile`, and the save payload. |
| `Source/ChainOfWitnesses/Public/Reputation/ReputationSubsystem.h`, `Private/.../ReputationSubsystem.cpp` | `UReputationSubsystem` — faction and per-NPC standing, recorded deeds, and news propagation across the road and sea network. |
| `Content/Data/DT_Factions.json` | The twelve Section 5 factions, each with how it reads the others. |
| `Content/Data/DT_DeedTypes.json` | Ten acts, from sheltering believers to informing on them, with who they please and how far the account travels. |
| `Content/Data/DT_TravelRoutes.json` | Nine locations, twelve legs, road and sea. |
| `Source/ChainOfWitnesses/Private/Tests/ReputationSubsystemTests.cpp` | Faction bleed, propagation timing, the closed sailing season, who remembers what, save round-trip. |

**How reputation moves.** Nothing sets a number directly. The player does something;
a `FReputationDeed` records what, where, and when; the deed's authored type says
which factions care and by how much. Each faction impact then bleeds into every
other faction by its attitude — helping the Jerusalem church is not neutral to the
men who run the Temple. Witnesses take the act personally, on top of the faction
move.

**How news travels.** An account leaves the place it happened and spreads over the
route graph by time-dependent Dijkstra, and an NPC knows about a deed only once it
has reached the city he lives in — or if he saw it himself. Sea legs wait out the
closed sailing season, which is not decoration: a deed in Jerusalem reaches Rome in
27 days in summer and 147 in winter, and Antioch reroutes overland via Damascus when
the lanes shut. Deed types that stay local never reach Rome at all, and a kept
confidence never leaves the room.

Tasks 3 and 6–9 (Codex UI, campaign map, argumentation encounters, Witness
Missions, the AD 65 vertical slice) are not started — see the task queue in the
master prompt doc.

## Project layout

Standard UE5 C++ project: `ChainOfWitnesses.uproject`, `Source/`, `Content/`,
`Config/`. No `Binaries/`/`Intermediate/`/`Saved/` are committed (see
`.gitignore`) — those are generated the first time the project is opened/built.

**This repo does not include the Unreal Engine itself.** To work with it:

1. Install UE 5.4+ (this project targets `EngineAssociation: 5.4`).
2. Right-click `ChainOfWitnesses.uproject` → *Generate Visual Studio project
   files* (or the equivalent for your platform/IDE), then open and build.
3. In the editor's Content Browser, import the JSON files as DataTables:
   `DT_CampaignEvents.json` with row type `CampaignEventRow`,
   `DT_TestimonyFragments.json` with row type `TestimonyFragmentDefinition`, and
   `DT_Dialogue_JerusalemHandoff.json` with row type `DialogueNodeRow`,
   `DT_Factions.json` with `FactionRow`, `DT_DeedTypes.json` with `DeedTypeRow`,
   and `DT_TravelRoutes.json` with `TravelRouteRow`.
4. Hand the resulting assets to `UCampaignTimelineSubsystem::SetTimelineTable`,
   `UCodexSubsystem::RegisterFragmentsFromDataTable`,
   `UDialogueSubsystem::RegisterDialogueTable`, and the three
   `UReputationSubsystem::Register*Table` entry points at startup (e.g. from your
   GameMode `BeginPlay`, or the Mode A/B config asset once Section 3's toggle is
   built). NPCs also need `RegisterNpcProfile` before their city or faction can
   matter to what they have heard.
5. Run the tests from **Tools → Session Frontend → Automation**, filter
   `ChainOfWitnesses`.

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

**A design question the dialogue gating surfaces, and you should decide.** When a
dialogue option requires a Codex fragment, what does that mean inside a
first-century Witness Mission? `Frag_WomenFirstAtTheTomb` carries an
`AttestationDateAD` of 55 — the year the written testimony enters the record — but
the conversation with Peter is set c. AD 35. The seed content treats a required
fragment as *the player can credibly show he already knows this*, which is coherent
because the knowledge plainly predates the writing. That reading breaks down if
someone authors an option requiring a second-century patristic source in a
first-century scene: the player would be citing Papias at a man who died decades
before Papias was born. Nothing in the code prevents it. Three options: leave it as
an authoring rule (current state, documented in `FDialogueConditionSet`), add a
per-conversation cutoff year that hard-fails late fragments, or treat Witness
Missions as explicitly the frame character's reconstruction, where knowing the later
source is fine. Worth settling before Task 9 authors the vertical slice.

**One field was added beyond Section 8's data model.**
`FTestimonyFragmentDefinition::RebutsFragmentIDs`. Section 8 specifies
`ChallengedBy` but nothing that answers a challenge, which leaves "loses strength
from unanswered challenges" with no way to ever resolve. Rebuttals are kept sparse
and must be real evidence: in the seed corpus only Irenaeus (*Adv. Haer.* 5.33.4)
rebuts anything — Eusebius' dismissal of Papias. The longer ending of Mark and the
*pericope adulterae* have no evidential rebuttal, and the chains that lean on them
take the hit, which is the honest outcome.
