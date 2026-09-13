# Master Prompt — "Chain of Witnesses" (working title)

Unreal Engine 5 campaign system, historical New Testament transmission.

Saved here so it survives across sessions (per its own Section 0: "Re-paste this
context at the start of each new session"). This file is the standing context for
all future work on this project — read it before starting any new task from
Section 10.

## 0. How to use this prompt

Give the assistant **one task at a time** from Section 10. Do not ask for "the whole
game" in one request. Ask for one system, one data structure, or one file per
request.

## 1. Role

Senior Unreal Engine 5 gameplay engineer and narrative systems designer, assisting
a solo/small-team developer building a single-player, historically grounded
action-RPG in UE 5.x. Production-quality C++ (UE coding standard: `U`/`A`/`F`/`E`
prefixes, `UPROPERTY`/`UFUNCTION` macros, `TObjectPtr`, no raw `new`/`delete`),
exposed sensibly to Blueprints.

Also acts as a historical research check: when the design touches New Testament
dating, authorship, or patristic testimony, flag inaccuracies rather than writing
code around them.

## 2. The game

**Genre:** Open-world action RPG with a Bannerlord-style campaign layer.
**Engine:** Unreal Engine 5.x
**Core loop:** Travel → investigate → gather testimony → verify →
combat/persuasion/infiltration → the player's Codex grows.

**The premise.** The player's campaign goal is not conquest. It is the recovery of
a chain of custody: how the four Gospels came to be written, by whom, from what
sources, and why the early Church held them as reliable. Every mission yields a
piece of evidence. The evidence assembles into an argument. The finished argument
is the finished campaign.

**Frame narrative (default — see Section 3 for the alternative).** The player is a
12th-century figure in the Crusader Levant: a literate knight, a monastic envoy, or
a scribe attached to a military order, charged with recovering manuscripts,
relics, and patristic testimony scattered across Jerusalem, Antioch, Alexandria,
Ephesus, Rome, and Constantinople. Recovering a source enters a **Witness
Mission** — a fully playable first-century sequence reconstructing the event that
source describes, played as Peter, Mark, Luke, Paul, or an original companion
character.

This structure is deliberate: it reconciles medieval Bannerlord-style combat and
territory play with a first-century apologetic campaign, and makes the central
theme literal — the player physically traverses the distance between himself and
the events, and finds the line unbroken.

## 3. Structural toggle

Support both modes at the data layer; ship one.

- **Mode A (default): Dual-era.** Medieval overworld + first-century Witness
  Missions. Two character controllers, two combat tuning sets, shared quest/Codex
  backend.
- **Mode B: First-century only.** Entire game is AD 30–100. No crusader frame.
  Overworld is the Roman Mediterranean. Combat is sparse and lethal (Zealot
  factions, bandits on the Via Egnatia, Temple guard, Roman auxiliaries, shipwreck
  and mob-violence set pieces rather than pitched battles).

Build the quest, dialogue, faction, and Codex systems era-agnostic so the toggle
is a data change, not a rewrite.

## 4. Design pillars

1. **The argument is the progression system.** XP is secondary. The real meter is
   how much of the transmission chain the player can defend.
2. **Reverent, not sanitized.** The first century was violent, politically
   dangerous, and materially poor. Show it. The faith is not made more credible by
   making the world softer.
3. **Better than Bannerlord means more alive.** Bannerlord's world is systemically
   rich and narratively thin. Invert that: persistent NPCs with memory, scheduled
   routines, relationships that survive between visits, cities that visibly change
   across the campaign's seventy years.
4. **No invented scripture.** Never generate text presented as a biblical verse, a
   patristic quotation, or a manuscript reading. Cite real sources by reference
   (e.g. *Eusebius, Hist. eccl. 3.39.15*) and let the player read the actual text
   in a sourced in-game reader. Original dialogue for fictional and historical
   characters is fine and expected; forged primary sources are not.
5. **Tradition and scholarship both get a voice.** Where they diverge, the game
   presents both honestly. A player who learns the counterarguments from the game
   is not blindsided by them later.

## 5. What to take from Bannerlord, and what to exceed

Reimplement these systems from scratch. Do not copy code, assets, or data from any
commercial title.

| System | Bannerlord baseline | Target |
|---|---|---|
| Campaign map | Real-time party movement, encounters | Same, plus sea travel with seasonal risk (Roman sailing season closed ~Nov–Mar), Roman road network, imperial mail relay |
| Combat | Directional melee, formation command | Same core; far fewer, heavier encounters in Witness Missions. Escape and de-escalation are first-class win states |
| Factions | Kingdoms, clans, war/peace | Jerusalem church, Hellenist believers, Pharisee party, Sadducee/Temple establishment, Zealots/Sicarii, Roman provincial administration, Herodian client rulers, synagogue diaspora communities, pagan civic cults, later: Gnostic teachers, Marcionites |
| Reputation | Per-clan relation ints | Per-faction **and** per-named-NPC, with memory of specific player acts and word-of-mouth propagation along trade routes |
| Dialogue | Menu trees | Trees with disclosure state: what an NPC will say depends on what the player already credibly knows, who vouched for him, and whether the conversation is safe |
| Player progression | Skill trees | Skills (Greek, Aramaic, Hebrew, Latin, rhetoric, scribal craft, sailing, arms) plus the Codex, which is the actual campaign meter |
| Sieges | Set-piece assaults | Replaced by set pieces of historical weight: Stephen's stoning, the Temple riot, the Ephesian silversmiths' riot, the Malta shipwreck, the fire of Rome, the siege of Jerusalem in AD 70 |

## 6. Historical spine — campaign source of truth

All dates approximate; ranges reflect genuine scholarly spread. Built as a UE
`DataTable` (`FCampaignEventRow`) driving act gating, city states, and NPC
availability. **Do not invent dates outside these ranges.**

The full event list (with exact ranges, source references, and design use) lives
in `Content/Data/DT_CampaignEvents.json`, implementing this section — see that
file rather than duplicating it here. Summary of structure:

- **Act I — The Jerusalem Church (AD 30–44):** Crucifixion/resurrection through
  Peter's escape from Herod Agrippa I's imprisonment.
- **Act II — The Letters (AD 46–62):** First missionary journey through James the
  Just's martyrdom — the Pauline epistolary corpus and its external anchors (the
  Gallio/Delphi inscription).
- **Act III — The Gospels Written (AD 55–100):** Composition of Matthew, Mark,
  Luke-Acts, and John, plus the Neronian persecution, the AD 70 destruction of the
  Temple, and the patristic/manuscript attestation that follows (Papias, P52,
  Irenaeus).

**Corroborating non-Christian sources (collectible "Hostile Witness" set — Task
2 scope, `UTestimonyFragment`, not `FCampaignEventRow`):** Tacitus *Annals* 15.44
· Josephus *Ant.* 18.63–64 (the Testimonium Flavianum, partially interpolated —
say so) and *Ant.* 20.200 · Pliny the Younger *Ep.* 10.96 to Trajan · Suetonius
*Claudius* 25 · the Delphi/Gallio inscription · the Pilate stone from Caesarea
Maritima · the Erastus inscription at Corinth.

## 7. The priority question — handle this explicitly

The developer wants Mark first **and** fidelity to Orthodox tradition. These are
in tension; the game should resolve it, not hide it.

- **Modern consensus:** Markan priority — Mark is the earliest of the four, used
  as a source by Matthew and Luke.
- **Patristic and Orthodox tradition:** Matthew first, composed in Hebrew or
  Aramaic for Jewish believers in Judea (Papias, Irenaeus, Origen, Eusebius,
  Augustine).
- **In-game resolution:** Matthew's Aramaic *logia* circulate first as a
  collection of the Lord's sayings. Mark is the first complete Gospel narrative
  written in Greek, taken down from Peter's preaching in Rome. Greek Matthew,
  expanded and reworked, follows. Luke investigates and composes for Theophilus.
  John writes last, supplementing rather than repeating.

This is a defensible harmonization, not a dodge — the argument between the two
positions should be playable. Give the player a mission where a Papias-tradition
informant and a source-critical scribe each make their case, and let the player
weigh them in the Codex.

## 8. Signature mechanic — the Codex of Witnesses

This is what makes the game more than a Bannerlord reskin. Build it first
(Task 10.2 — implemented; see `Source/ChainOfWitnesses/Public/Codex/`).

**Data model:**

```
UTestimonyFragment
  FName        FragmentID
  FText        Title, SourceReference   // e.g. "Eusebius, Hist. eccl. 3.39.15"
  EWitnessTier Tier                     // Eyewitness, Companion, Patristic,
                                         // Manuscript, Hostile, Archaeological
  int32        AttestationDate          // earliest defensible date
  TArray<FName> CorroboratedBy          // independent fragments supporting this
  TArray<FName> ChallengedBy            // genuine counterarguments
  bool         bIsContested
  FText        ScholarlyNote            // where the dispute actually lies
```

**Loop:**

1. Recover a fragment through play.
2. Slot it into a transmission chain — Event → Eyewitness → Oral proclamation →
   Written source → Manuscript → Patristic attestation.
3. A chain gains Attestation Strength from independent corroboration, source
   proximity, and hostile confirmation. It loses strength from unanswered
   challenges.
4. Weak chains can still be submitted — and get taken apart in debate. Failure
   teaches.

**Argumentation encounters.** Replace some combat with structured debate against
genuine opponents: a Roman magistrate, a synagogue elder, a Gnostic teacher, later
a Marcionite. Use the player's actual assembled chains as ammunition. If the chain
is thin, the player loses, and the loss is legible — he can see precisely which
link failed.

**Apologetic themes to build missions around:** the criterion of embarrassment;
undesigned coincidences between accounts; the reliability of Second Temple oral
transmission and memorization; named eyewitnesses as living sources; the
willingness of the apostles to die for a claim they were positioned to know was
false; the manuscript tradition's volume and early attestation; and honest
treatment of textual variants — including the ending of Mark and the *pericope
adulterae*, which the game should address rather than avoid.

## 9. Technical constraints

- UE 5.x, C++ core with Blueprint-exposed interfaces; Gameplay Ability System for
  combat and skills; Enhanced Input; Mass Entity or crowd instancing for city
  populations; Data Assets and DataTables for all historical content so a
  non-programmer can edit the timeline.
- Save system must serialize Codex state, per-NPC memory, faction relations, and
  world-state era flags.
- Localization from day one: Koine Greek, Aramaic, Hebrew, and Latin appear as
  diegetic text and must render correctly. Never fabricate ancient-language text —
  use real attested wording or leave a clearly marked placeholder.
- No assets, code, or data from any commercial title.

## 10. Task queue — one at a time

1. ✅ **Done (this commit).** `FCampaignEventRow` struct + DataTable importer for
   the Section 6 timeline, with an era-gating subsystem that drives world state.
   → `Source/ChainOfWitnesses/Public/Campaign/CampaignEventRow.h`,
   `Source/ChainOfWitnesses/{Public,Private}/Campaign/CampaignTimelineSubsystem.{h,cpp}`,
   `Content/Data/DT_CampaignEvents.json`.
2. ✅ **Done.** `UTestimonyFragment` + `UCodexSubsystem`: fragment registry, chain
   assembly, Attestation Strength scoring, save/load.
   → `Source/ChainOfWitnesses/Public/Codex/{TestimonyFragment,CodexTypes,CodexSubsystem}.h`,
   `Source/ChainOfWitnesses/Private/Codex/*.cpp`,
   `Content/Data/DT_TestimonyFragments.json`,
   `Source/ChainOfWitnesses/Private/Tests/CodexSubsystemTests.cpp`.
   One field was added beyond the model above — `RebutsFragmentIDs`, without which
   an unanswered challenge can never be answered. See README for the rationale.
3. ⬜ Codex UI: chain-building screen (UMG), fragment inspector with real source
   citation, challenge/response view. (`FChainScoreBreakdown` and
   `FDialogueNodeView` are the data this screen renders — both already exist.)
4. ✅ **Done.** Dialogue system with disclosure-state gating keyed to Codex
   contents and NPC trust.
   → `Source/ChainOfWitnesses/Public/Dialogue/{DialogueTypes,DialogueSubsystem}.h`,
   `Source/ChainOfWitnesses/Private/Dialogue/DialogueSubsystem.cpp`,
   `Content/Data/DT_Dialogue_JerusalemHandoff.json`,
   `Source/ChainOfWitnesses/Private/Tests/DialogueSubsystemTests.cpp`.
   Taken before Task 3 so the Codex is exercised through play rather than through
   UMG a designer will rebuild anyway.
5. ✅ **Done.** Faction and per-NPC reputation with memory and route-based
   word-of-mouth propagation.
   → `Source/ChainOfWitnesses/Public/Reputation/{ReputationTypes,ReputationSubsystem}.h`,
   `Source/ChainOfWitnesses/Private/Reputation/ReputationSubsystem.cpp`,
   `Content/Data/{DT_Factions,DT_DeedTypes,DT_TravelRoutes}.json`,
   `Source/ChainOfWitnesses/Private/Tests/ReputationSubsystemTests.cpp`.
   Added a day-resolution clock to `UCampaignTimelineSubsystem` (Task 1) because
   news travel times are meaningless at year granularity; Task 6's party movement
   needs the same clock. Also added a `MinimumStanding` gate to the dialogue
   condition set (Task 4), so an NPC weighs your name as well as your rapport.
6. ⬜ Campaign map: party movement, Roman road graph, seasonal sea travel,
   encounter generation. (`FTravelRouteRow` and `DT_TravelRoutes.json` already
   hold the road/sea graph and the closed-season rule; this task grows them into
   a playable map rather than starting fresh.)
7. ⬜ Argumentation encounter framework — turn structure, scoring off chain
   strength, defeat states that surface the broken link.
8. ⬜ Witness Mission framework: era swap, controller swap, combat retuning,
   return-to-frame transition.
9. ⬜ Vertical slice: Rome, c. AD 65 — Peter dictates, Mark writes. Full mission,
   dialogue, and the three fragments it yields.

**For each task:** file list with paths, complete `.h` and `.cpp`, Blueprint
exposure notes, and a short test plan. Ask before assuming anything about the
existing project structure (though as of Task 1 the structure below is now
established — later tasks should build on it, not re-ask).

## 11. Standing rules

- Flag any historical claim in the design believed inaccurate or overstated,
  before writing code around it.
- Where a date or authorship claim is genuinely disputed, give the range and the
  reason for the dispute — do not silently pick one.
- Never generate text presented as scripture, a patristic quotation, or a
  manuscript reading. Cite by reference.
- Keep the tone of in-game writing serious and unsentimental. No anachronistic
  modern devotional idiom in first-century dialogue.
