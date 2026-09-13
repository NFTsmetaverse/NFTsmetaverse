# Combat

Off-queue. The Section 10 task list does not contain a combat task; this was asked
for separately and is built to the constraints Section 5 and Section 9 already set
for it.

## The split, and why

Combat here is two layers, and only one of them can honestly be written outside the
editor.

**The encounter layer** — built. Who is in an encounter, how much fight is left in
each of them, whether talking or running is still possible, how it ends, and what it
costs afterwards. `UCombatEncounterSubsystem`, `FCombatEncounterRow`, and
`DT_CombatEncounters.json`. It has no dependency on a world, which is why there are
twelve automation tests over it.

**The melee layer** — not built, and deliberately not stubbed. Directional attacks,
blocking, montages, hit detection, enemy behaviour trees, formation command. All of
it needs animation assets, a skeleton, and the editor. Writing it here would produce
plausible-looking code that has never met a single asset.

The seam between them is one function each way:

```cpp
// The pawn layer, when a blow lands:
Combat->ApplyDamageToCombatant(Index, NormalisedDamage);   // he hit one of them
Combat->ApplyDamageToPlayer(NormalisedDamage);             // one of them hit him
Combat->AdvanceEncounter(DeltaSeconds);                    // from Tick

// The pawn layer, listening:
Combat->OnCombatantDispositionChanged  // this one has stopped: play the break, drop the AI out
Combat->OnEncounterEnded               // it is over, and this is how
```

Damage is normalised 0–1 — fractions of a man, not hit points — because the era
tuning, the content tables and the attribute set all speak in those terms, and a
designer changing what a wound is worth should change it in one place.

`UChainAttributeSet` is the Gameplay Ability System half Section 9 asks for, and the
part of GAS that is pure C++: Health, MaxHealth, Resolve, MaxResolve, and the Damage
and ResolveLoss meta-attributes, with clamping and depletion detection in
`PostGameplayEffectExecute`. Abilities, effects and cues are editor assets and are
not here.

## What the design is actually claiming

Section 5 asks for sparse, lethal encounters in which **escape and de-escalation are
first-class win states**. Those three words are the whole system:

**Men break before they die.** Every combatant carries `Resolve` alongside `Health`,
and the ordinary way an encounter ends is that the people in it stop, not that they
die. Killing one of them takes `ResolveLostPerCasualty` off everyone still standing,
multiplied by who they are. Against bandits, two bodies out of four ends it with the
survivors untouched. Against an execution party, two bodies move the remaining four
almost not at all.

| Crowd | Casualty shock | Escape modifier | Reads as |
|---|---|---|---|
| Mob | ×1.3 | −0.2 | Enormous momentum, no perimeter, short memory |
| Patrol | ×0.8 | +0.2 | Disciplined, will listen, will follow |
| Guard | ×0.7 | −0.2 | Hard to shift, easy to walk away from |
| Execution party | ×0.3 | +0.3 | Came to do one thing |
| Bandits | ×1.5 | 0.0 | Here for profit; a dead companion changes the arithmetic |

**A crowd burns itself out, and far too slowly to wait for.** Only `ECrowdKind::Mob`
decays, at 0.004 resolve per second: a crowd at full pitch takes something over four
minutes to go home. A first-century player absorbs about two solid blows before he
is down. Waiting is a real option and a bad one, which is the intended shape.

**Talking is priced, and standing is most of the price.** A de-escalation attempt
beats `DeEscalationDifficulty`, plus `0.2` per previous failure, plus `0.3` for
having drawn and `0.6` for having used it. Against that the player brings his
rhetoric plus his faction standing at `0.01` per point. Twenty points of standing
with the faction whose men these are is worth `0.2` — routinely the difference. A
near miss (75% of the requirement) peels the least committed man off the back of the
crowd rather than achieving nothing, because a crowd is not one mind.

**Being taken is an ending, not a loss.** `bTakesPrisoners` decides whether zero
health is `Taken` or `Defeated`. For most of this campaign it is `Taken`, and that is
frequently the historically correct outcome: Paul wrote from custody.

**The era decides what counted.** `FEraCombatTuning` (Task 8) is read here for the
first time. In the first century, `bEscapeIsWinState` and `bDeEscalationIsWinState`
are both true and incoming damage is scaled ×2.5. In the AD 1229 frame, neither is a
win and a man in mail takes the same blow at ×1. This is why there is one combat
system and not two.

**It travels.** `OvercomeDeedTypeID` and `DeEscalatedDeedTypeID` are recorded through
`UReputationSubsystem` at the party's current location, so a body in the Temple court
reaches Antioch on the reputation system's own timetable. The overcome deed is gated
on the player having actually shed blood — a crowd that lost interest and went home
is also "overcome", and he did none of that.

## Content

`Content/Data/DT_CombatEncounters.json`, twelve rows. Four of them set
`bIgnoreEraCombatantCap` because their size is the point: a riot with three men in it
is not a riot.

Five deed types were added to `DT_DeedTypes.json`:
`Deed_ShedBloodInThePrecinct`, `Deed_ShedBloodInTheStreet`, `Deed_TalkedDownACrowd`,
`Deed_ResistedArrest`, `Deed_TalkedPastAPatrol`.

## Historical flags

Standing Rule 1 of the master prompt: flag anything believed inaccurate or
overstated before writing code around it.

**The Temple riot, and what a crowd will hear.** An earlier draft of this system had
a crowd stop listening the moment anyone was hurt. Acts 21–22 says otherwise: Paul is
seized and beaten in the Temple court, and then addresses the crowd from the steps of
the Antonia, and they listen until he reaches the sending to the Gentiles. The rule
now keys on whether *he* has put one of them down, not on whether he has been
knocked about, which is both the better mechanic and the one the record supports.

**The Ephesian silversmiths.** In Acts 19 the riot is dispersed by the city clerk,
and Paul is specifically kept away from the theatre by the disciples and by friendly
Asiarchs. `Cbt_SilversmithsRiot` lets the player try to talk it down himself. That is
a game affordance, not the narrative of Acts 19, and it should not be dressed up as
one in mission text.

**Stoning, and who could carry out a death sentence.** Whether the Sanhedrin held
capital jurisdiction under direct Roman rule is genuinely disputed and the sources
pull against each other: John 18:31 has the Jewish authorities say they may not put
anyone to death; the Talmud (y. Sanhedrin 1.1; b. Sanhedrin 41a) says capital
jurisdiction was removed "forty years before the destruction of the Temple"; and yet
Acts 7 has Stephen stoned, and Josephus (*Antiquities* 20.200) has James executed by
Ananus during a gap between prefects — for which Ananus was deposed, which cuts both
ways. The defensible reading is that formal capital jurisdiction was constrained,
while lynchings and executions in the absence of a governor happened. The game should
present this as contested rather than settled. `Cbt_StoningParty` is deliberately
written as men acting on a sentence decided elsewhere, without specifying the court.

The stoning *procedure* set out in Mishnah *Sanhedrin* 6 should not be treated as a
description of first-century practice: the Mishnah is codified around AD 200 and is
in large part idealised legal reconstruction.

**Sicarii.** Well attested. Josephus, *Jewish War* 2.254–257 and *Antiquities*
20.186–187, describes assassins with concealed daggers working festival crowds. The
encounter is modelled on that and needs no hedging.

**Everything numeric is invented.** The decay rates, shock multipliers, escape
thresholds and standing weights are game tuning. They are chosen so that the
behaviour they produce matches what the sources describe qualitatively — crowds
disperse, disciplined men pursue, bandits reconsider after a casualty — and no
further claim is made for them.

## Not built

- The melee layer, as above.
- Ranged and thrown attacks. A stoning is currently modelled as damage arriving, not
  as stones with trajectories.
- `bFormationCommandEnabled` is read from the era tuning and nothing acts on it yet.
  It belongs to the frame era's Templar encounters and needs the pawn layer.
- Wounds that persist between encounters. `PlayerHealthNormalised` resets each time;
  a campaign-level injury model would belong with the party state on the map, not
  here.
- Nothing in this document has been compiled. See the note in the README.
