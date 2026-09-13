# Chain of Witnesses (working title)

Historical New Testament transmission campaign — UE 5.x C++ project. Full design
context lives in [`docs/MASTER_PROMPT.md`](docs/MASTER_PROMPT.md); read that
before starting new work. Art direction for the Mode A player character is in
[`docs/CHARACTER_FRAME_PROTAGONIST.md`](docs/CHARACTER_FRAME_PROTAGONIST.md).

## Status

All of the Section 10 task queue except Task 3 (the Codex UI) is implemented: the
historical timeline as data with the subsystem that gates world state off it, the
Codex of Witnesses on top of that, dialogue gated on what the player can credibly
claim, a reputation system whose news travels no faster than a man on a road, the
campaign map that road belongs to, the debates where the Codex is put under
pressure, and the Witness Mission framework that carries the player between the
two eras, and the AD 65 vertical slice that runs through all of it.

Task 3 (the Codex UI) is the one left. UMG widgets are binary assets a designer
builds in-editor, so the useful part from here is the data they render, which
`FChainScoreBreakdown`, `FDialogueNodeView` and `FDebateObjectionView` already
provide.

**Nothing here has been compiled.** There is no UE toolchain in the environment
this was written in. Roughly 9,000 lines of C++ and eight automation suites are
waiting on a first build; expect real errors on the first pass.

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
| `Source/ChainOfWitnesses/Public/Reputation/ReputationTypes.h` | `FFactionRow` (with inter-faction attitudes), `FDeedTypeRow`, `FReputationDeed`, `FNpcProfile`, and the save payload. |
| `Source/ChainOfWitnesses/Public/Reputation/ReputationSubsystem.h`, `Private/.../ReputationSubsystem.cpp` | `UReputationSubsystem` — faction and per-NPC standing, recorded deeds, and news propagation across the road and sea network. |
| `Content/Data/DT_Factions.json` | The twelve Section 5 factions, each with how it reads the others. |
| `Content/Data/DT_DeedTypes.json` | Ten acts, from sheltering believers to informing on them, with who they please and how far the account travels. |
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

**Task 6 — the campaign map**

| Path | What it is |
|---|---|
| `Source/ChainOfWitnesses/Public/Map/CampaignMapTypes.h` | `FCampaignLocationRow`, `FTravelRouteRow`, `FEncounterTypeRow`, `FPartyState`, `FTravelResult`, `ETravelOutcome`. |
| `Source/ChainOfWitnesses/Public/Map/CampaignMapSubsystem.h`, `Private/.../CampaignMapSubsystem.cpp` | `UCampaignMapSubsystem` — the travel graph and its pathfinder, party movement, the sailing season, and encounter generation. |
| `Content/Data/DT_Locations.json`, `DT_TravelRoutes.json` | Nine cities — region, port status, controlling faction, and the hub ID the timeline razes in AD 70 — and the twelve legs joining them. |
| `Content/Data/DT_EncounterTypes.json` | Thirteen things that can happen on the road, from Sicarii to a synagogue that takes in travellers. |
| `Source/ChainOfWitnesses/Private/Tests/CampaignMapSubsystemTests.cpp` | Pathfinding, party movement, encounter interruption, the closed season, save round-trip. |

**One road network.** The graph moved here out of `UReputationSubsystem`, which now
asks the map how far word has travelled. Both the party and the news use the same
pathfinder and the same seasonal cost function.

**Time and movement are the same thing.** `AdvanceDays` is what moves the campaign
clock while the party is on the road, and it stops the moment something happens,
returning the unspent days so the caller can resume after resolving it. Passing
through a city broadcasts an event but does not halt the journey.

**The sailing season is a real decision, not decoration.** Routes are costed by
arrival time rather than distance, so a shut sea lane changes which way is fastest.
Setting out from Jerusalem for Rome:

| Departing | Waiting for the season | Sailing regardless |
|---|---|---|
| High summer | 27 days, via Alexandria | 27 days, via Alexandria |
| Late October | **43 days**, one long crossing from Caesarea | **27 days**, at 8× storm risk |

Nobody authored that late-October reroute — the pathfinder drops the Alexandria hop
because the second sea leg would be caught by the closing lanes, and takes the
single direct crossing instead. Insisting on sailing is what `Acts 27` was, and the
risk multiplier makes the wreck the expected outcome rather than bad luck.

**Task 7 — argumentation encounters**

| Path | What it is |
|---|---|
| `Source/ChainOfWitnesses/Public/Debate/DebateTypes.h` | `FDebateObjectionRow`, `FDebateOpponentRow`, `FDebateObjectionView`, `FDebateRoundRecord`, `FDebateOutcomeReport`, `FDebateState`. |
| `Source/ChainOfWitnesses/Public/Debate/DebateSubsystem.h`, `Private/.../DebateSubsystem.cpp` | `UDebateSubsystem` — objection selection, the three ways to answer, scoring off link strength, and the outcome report. |
| `Content/Data/DT_DebateObjections.json` | Sixteen real counterarguments, each aimed at a named link. |
| `Content/Data/DT_DebateOpponents.json` | Six opponents — magistrate, synagogue elder, source-critical scribe, tradition keeper, Gnostic, Marcionite — each attacking a different part of the chain. |
| `Source/ChainOfWitnesses/Private/Tests/DebateSubsystemTests.cpp` | Weakest-link probing, chain-vs-citation defence, challenge penalties, the defeat report, early wins, era gating, deed recording. |

**The chain is the ammunition, literally.** Every objection attacks a named link of
the transmission chain, so the link score built in the Codex *is* the defence
available here. That single decision is what makes Section 8's two requirements —
"use the player's actual assembled chains" and "the loss is legible" — the same
mechanism rather than two features.

The opponent is not a random-objection dispenser: each round he leads with whatever
the chain is worst at, deterministically. Three ways to meet an objection — rest on
the chain (worth exactly that link's score), cite a source that answers it directly
(stronger, and each citation carries only once per debate), or concede. Conceding is
sometimes the correct move: the longer ending of Mark has no evidential answer, and
pretending otherwise is how a case comes apart later.

A link carrying a challenge the player never answered is worth **half** in debate —
so Task 2's `ChallengedBy` data is what an opponent reaches for. And a defeat
returns a transcript naming the link that failed plus the fragments that would have
met the objections that landed and which the player has not found. That list is the
next set of missions.

**Task 8 — the Witness Mission framework**

| Path | What it is |
|---|---|
| `Source/ChainOfWitnesses/Public/Witness/WitnessMissionTypes.h` | `FEraDefinitionRow`, `FEraCombatTuning`, `FWitnessMissionRow`, `FFrameSnapshot`, `EGameEra`, `EStructuralMode`. |
| `Source/ChainOfWitnesses/Public/Witness/WitnessMissionSubsystem.h`, `Private/.../WitnessMissionSubsystem.cpp` | `UWitnessMissionSubsystem` — era and mode state, mission availability, entering and leaving, and the frame snapshot. |
| `Content/Data/DT_Eras.json` | The crusader frame and the first century, each with its pawn/controller/input references and combat tuning. |
| `Content/Data/DT_WitnessMissions.json` | Six missions from the empty tomb to Peter dictating in Rome, each anchored to a timeline row and validated against its date range. |
| `Source/ChainOfWitnesses/Private/Tests/WitnessMissionSubsystemTests.cpp` | Era swap, the sealed reconstruction, unlock-by-source, Mode B, knowledge carry-through, save round-trip. |

**The clock is the whole trick.** Entering a mission pushes the campaign clock to
the mission's year and the party to its location; leaving pops both back. Every
era-gated system already built — dialogue windows, debate opponents, road
encounters — therefore behaves correctly inside a mission without knowing missions
exist. That needed one addition to Task 1: `SetDate`, because the clock now has to
run backwards from 1229 to AD 65 and forwards again, and events activate
cumulatively so going back re-derives world state rather than trying to undo it.

**What this subsystem does not do** is travel or possess. Those need a World and a
loaded map, which is GameMode work; the subsystem names the level and the classes
and broadcasts. That separation is what lets the whole framework be tested without
a world.

**A mission is sealed.** Faction standing and NPC memory are snapshotted on entry
and restored on return — using each subsystem's own save payload, so a mission
unwinds through exactly the code a save file restores through. Recovered fragments
are the one deliberate exception, and the reason for going.

**Task 9 — the vertical slice**

| Path | What it is |
|---|---|
| `Content/Data/DT_Dialogue_RomeAD65.json` | Rome, AD 65, after the fire. Ten nodes; the player is Mark, taking dictation. |
| `Content/Data/DT_WitnessMissions.json` | `WM_PeterDictatesMark` — unlocked by recovering Papias' testimony, opening on `Node_Rome_Room`. |
| `Source/ChainOfWitnesses/Private/Tests/VerticalSliceTests.cpp` | The module's end-to-end integration test: frame era → mission → conversation → three fragments → frame restored → chain scored. |

**The scene.** Peter has been asked to let his preaching be written down and does
not want to be. His objection is the one Papias himself records holding — that a
living voice can be questioned and a book cannot. Pressing him on why the account
does not run in sequence is what produces the admission that it does not, which is
the slice's central fragment: the same sentence that vouches for Mark concedes a
defect in him, and a fabricated credential does not come qualified.

Asking whether his own failure in the courtyard stays in the text is the criterion
of embarrassment, played rather than explained. He leaves it in.

**The three fragments** — Peter preaching in Rome, Mark writing accurately but not
in order, and the Gospel as a written document — are all granted through the
dialogue, so playing the scene is what yields them rather than completing it.

**What the test proves.** That the seven subsystems compose. The frame era hands
off, the clock moves to AD 65, the persecution-gated line opens because world state
resolves at the mission's year, disclosure earned in one exchange unlocks the next,
the frame comes back with Peter's regard for Mark rolled back and the evidence kept,
and the three fragments assemble into a chain that scores — weakly, with the Event
link still empty and two objections outstanding. One evening in Rome is a start, not
a case.

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
   `DT_TravelRoutes.json` with `TravelRouteRow`, `DT_Locations.json` with
   `CampaignLocationRow`, and `DT_EncounterTypes.json` with `EncounterTypeRow`.
4. Hand the resulting assets to `UCampaignTimelineSubsystem::SetTimelineTable`,
   `UCodexSubsystem::RegisterFragmentsFromDataTable`,
   `UDialogueSubsystem::RegisterDialogueTable`, the two
   `UReputationSubsystem::Register*Table` entry points, and the three
   `UCampaignMapSubsystem::Register*Table` ones, and the two
   `UDebateSubsystem::Register*Table` ones at startup (e.g. from your GameMode
   `BeginPlay`, or the Mode A/B config asset once Section 3's toggle is built).
   NPCs also need `RegisterNpcProfile` before their city or faction can matter to
   what they have heard. Note that routes now register on the **map**, not on
   reputation.
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

**Resolved: a Witness Mission is a reconstruction.** The question of what the
player knows inside a first-century scene is settled in favour of the frame
character performing the reconstruction, so his whole Codex is available there —
including sources written long after the year he is standing in. The scene stays
honest because era gating still evaluates at the mission's year: `EarliestYearAD`,
`LatestYearAD` and `RequiredActiveEventIDs` all resolve to AD 65 inside an AD 65
mission, so no NPC refers to something that has not happened to him. Recorded in
`FDialogueConditionSet` and in the character brief.

**Resolved: the frame era is c. AD 1229.** Chosen against the character reference's
armour, which reads about a century later than a 12th-century setting would allow.
See `docs/CHARACTER_FRAME_PROTAGONIST.md` for the audit and the reasoning; the year
lives in `UWitnessMissionSubsystem::FrameYearAD`.

**One field was added beyond Section 8's data model.**
`FTestimonyFragmentDefinition::RebutsFragmentIDs`. Section 8 specifies
`ChallengedBy` but nothing that answers a challenge, which leaves "loses strength
from unanswered challenges" with no way to ever resolve. Rebuttals are kept sparse
and must be real evidence: in the seed corpus only Irenaeus (*Adv. Haer.* 5.33.4)
rebuts anything — Eusebius' dismissal of Papias. The longer ending of Mark and the
*pericope adulterae* have no evidential rebuttal, and the chains that lean on them
take the hit, which is the honest outcome.
