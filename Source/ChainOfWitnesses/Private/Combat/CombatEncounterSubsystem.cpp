#include "Combat/CombatEncounterSubsystem.h"

#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "Map/CampaignMapSubsystem.h"
#include "Reputation/ReputationSubsystem.h"
#include "Witness/WitnessMissionSubsystem.h"

#define LOCTEXT_NAMESPACE "ChainOfWitnesses.Combat"

DEFINE_LOG_CATEGORY(LogCombat);

namespace
{
	/**
	 * How hard one man going down hits the rest of them.
	 *
	 * A mob is held together by nothing but its own momentum and the first body
	 * takes a great deal of that away; a bandit is doing this for profit and a dead
	 * companion changes the arithmetic immediately. Men who came to carry out a
	 * sentence are the opposite case: they expected resistance and have orders.
	 */
	float CasualtyShockMultiplier(ECrowdKind CrowdKind)
	{
		switch (CrowdKind)
		{
		case ECrowdKind::Mob:			return 1.3f;
		case ECrowdKind::Patrol:		return 0.8f;
		case ECrowdKind::Guard:			return 0.7f;
		case ECrowdKind::ExecutionParty:	return 0.3f;
		case ECrowdKind::Bandits:		return 1.5f;
		default:				return 1.f;
		}
	}

	/**
	 * Added to the difficulty of getting away. Negative is easier.
	 *
	 * A crowd is disorganised and has no perimeter, so slipping it is mostly a matter
	 * of nerve; posted guards are defending a place rather than pursuing, and will
	 * usually let a man walk away from it. A patrol's whole function is to follow.
	 */
	float EscapeDifficultyModifier(ECrowdKind CrowdKind)
	{
		switch (CrowdKind)
		{
		case ECrowdKind::Mob:			return -0.2f;
		case ECrowdKind::Patrol:		return 0.2f;
		case ECrowdKind::Guard:			return -0.2f;
		case ECrowdKind::ExecutionParty:	return 0.3f;
		case ECrowdKind::Bandits:		return 0.f;
		default:				return 0.f;
		}
	}
}

void UCombatEncounterSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UCombatEncounterSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

void UCombatEncounterSubsystem::SetDependenciesForTesting(UReputationSubsystem* InReputation,
	UCampaignMapSubsystem* InMap, UWitnessMissionSubsystem* InWitness)
{
	ReputationOverride = InReputation;
	MapOverride = InMap;
	WitnessOverride = InWitness;
}

UReputationSubsystem* UCombatEncounterSubsystem::GetReputation() const
{
	if (ReputationOverride)
	{
		return ReputationOverride;
	}

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		return GameInstance->GetSubsystem<UReputationSubsystem>();
	}

	return nullptr;
}

UCampaignMapSubsystem* UCombatEncounterSubsystem::GetMap() const
{
	if (MapOverride)
	{
		return MapOverride;
	}

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		return GameInstance->GetSubsystem<UCampaignMapSubsystem>();
	}

	return nullptr;
}

UWitnessMissionSubsystem* UCombatEncounterSubsystem::GetWitness() const
{
	if (WitnessOverride)
	{
		return WitnessOverride;
	}

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		return GameInstance->GetSubsystem<UWitnessMissionSubsystem>();
	}

	return nullptr;
}

FEraCombatTuning UCombatEncounterSubsystem::GetTuning() const
{
	if (const UWitnessMissionSubsystem* Witness = GetWitness())
	{
		return Witness->GetActiveCombatTuning();
	}

	// Defaults are the frame era's shape, not the first century's: a system with no
	// era set should not silently make the player harder to kill.
	return FEraCombatTuning();
}

// --- Content ----------------------------------------------------------------

bool UCombatEncounterSubsystem::RegisterEncounterType(const FCombatEncounterRow& EncounterType)
{
	if (EncounterType.EncounterTypeID.IsNone() || EncounterTypes.Contains(EncounterType.EncounterTypeID))
	{
		return false;
	}

	EncounterTypes.Add(EncounterType.EncounterTypeID, EncounterType);
	return true;
}

int32 UCombatEncounterSubsystem::RegisterEncounterTable(const UDataTable* EncounterTable)
{
	if (!EncounterTable)
	{
		return 0;
	}

	TArray<FCombatEncounterRow*> Rows;
	EncounterTable->GetAllRows<FCombatEncounterRow>(TEXT("UCombatEncounterSubsystem::RegisterEncounterTable"), Rows);

	int32 AddedCount = 0;
	for (const FCombatEncounterRow* Row : Rows)
	{
		if (Row && RegisterEncounterType(*Row))
		{
			++AddedCount;
		}
	}

	UE_LOG(LogCombat, Log, TEXT("Registered %d encounter types."), AddedCount);
	return AddedCount;
}

bool UCombatEncounterSubsystem::GetEncounterType(FName EncounterTypeID, FCombatEncounterRow& OutEncounterType) const
{
	if (const FCombatEncounterRow* Row = EncounterTypes.Find(EncounterTypeID))
	{
		OutEncounterType = *Row;
		return true;
	}

	OutEncounterType = FCombatEncounterRow();
	return false;
}

void UCombatEncounterSubsystem::ValidateCombatContent(TArray<FString>& OutProblems) const
{
	OutProblems.Reset();

	const UReputationSubsystem* Reputation = GetReputation();

	for (const TPair<FName, FCombatEncounterRow>& Pair : EncounterTypes)
	{
		const FCombatEncounterRow& Row = Pair.Value;

		if (Row.Summary.IsEmpty())
		{
			OutProblems.Add(FString::Printf(TEXT("Encounter '%s' has no summary."), *Pair.Key.ToString()));
		}

		// An encounter nobody can talk down or run from is a scripted death, and
		// should be a set piece rather than an encounter type.
		if (!Row.bCanBeTalkedDown && !Row.bTakesPrisoners && Row.EscapeDifficulty > 1.5f)
		{
			OutProblems.Add(FString::Printf(
				TEXT("Encounter '%s' cannot be talked down, takes no prisoners and is near-impossible to escape."),
				*Pair.Key.ToString()));
		}

		if (Reputation)
		{
			if (!Row.FactionID.IsNone() && !Reputation->IsFactionRegistered(Row.FactionID))
			{
				OutProblems.Add(FString::Printf(TEXT("Encounter '%s' belongs to unregistered faction '%s'."),
					*Pair.Key.ToString(), *Row.FactionID.ToString()));
			}

			// A deed type nobody registered means the encounter resolves silently and
			// nothing that happens in it ever reaches anyone.
			if (!Row.OvercomeDeedTypeID.IsNone() && !Reputation->IsDeedTypeRegistered(Row.OvercomeDeedTypeID))
			{
				OutProblems.Add(FString::Printf(TEXT("Encounter '%s' records unregistered deed type '%s'."),
					*Pair.Key.ToString(), *Row.OvercomeDeedTypeID.ToString()));
			}

			if (!Row.DeEscalatedDeedTypeID.IsNone() && !Reputation->IsDeedTypeRegistered(Row.DeEscalatedDeedTypeID))
			{
				OutProblems.Add(FString::Printf(TEXT("Encounter '%s' records unregistered deed type '%s'."),
					*Pair.Key.ToString(), *Row.DeEscalatedDeedTypeID.ToString()));
			}
		}
	}
}

// --- Running an encounter ---------------------------------------------------

bool UCombatEncounterSubsystem::BeginEncounter(FName EncounterTypeID)
{
	const FCombatEncounterRow* Row = EncounterTypes.Find(EncounterTypeID);
	if (!Row)
	{
		UE_LOG(LogCombat, Warning, TEXT("No such encounter type '%s'."), *EncounterTypeID.ToString());
		return false;
	}

	if (State.bIsActive)
	{
		// Whatever was happening is over; it did not end well for anybody watching.
		EndEncounter(ECombatOutcome::Escaped);
	}

	const FEraCombatTuning Tuning = GetTuning();

	State = FCombatEncounterState();
	State.bIsActive = true;
	State.EncounterTypeID = EncounterTypeID;

	// The era caps the count downward only. A Witness Mission wants few and heavy;
	// it should never inflate an authored street encounter into a battle. Set pieces
	// whose size is the point say so on the row and are left alone.
	const int32 CombatantCount = Row->bIgnoreEraCombatantCap
		? FMath::Max(1, Row->CombatantCount)
		: FMath::Max(1, FMath::Min(Row->CombatantCount, Tuning.TypicalEnemyCount));

	for (int32 Index = 0; Index < CombatantCount; ++Index)
	{
		FCombatantState& Combatant = State.Combatants.AddDefaulted_GetRef();
		Combatant.CombatantID = FName(*FString::Printf(TEXT("%s_%d"), *EncounterTypeID.ToString(), Index));
		Combatant.Resolve = Row->StartingResolve;
	}

	Report = FCombatOutcomeReport();

	OnEncounterBegun.Broadcast(EncounterTypeID);
	return true;
}

bool UCombatEncounterSubsystem::SetPosture(ECombatPosture NewPosture)
{
	if (!State.bIsActive || NewPosture == ECombatPosture::MAX)
	{
		return false;
	}

	// Engaged is not a choice; it is a description of what has already happened, and
	// a man who has used a blade cannot put the fact away with the blade. Being hit
	// himself closes nothing off -- a man being stoned may still draw.
	if (State.bPlayerShedBlood && (NewPosture == ECombatPosture::Undrawn || NewPosture == ECombatPosture::Drawn))
	{
		return false;
	}

	State.Posture = NewPosture;
	return true;
}

void UCombatEncounterSubsystem::ApplyDamageToCombatant(int32 CombatantIndex, float NormalisedDamage)
{
	if (!State.bIsActive || !State.Combatants.IsValidIndex(CombatantIndex) || NormalisedDamage <= 0.f)
	{
		return;
	}

	FCombatantState& Combatant = State.Combatants[CombatantIndex];
	if (Combatant.Disposition == ECombatantDisposition::Down)
	{
		return;
	}

	const FEraCombatTuning Tuning = GetTuning();
	const float Damage = NormalisedDamage * Tuning.OutgoingDamageScale;

	Combatant.HealthNormalised = FMath::Max(0.f, Combatant.HealthNormalised - Damage);

	// Hurting a man discourages him whether or not it stops him, and the record of
	// who started it is now settled.
	Combatant.Resolve = FMath::Max(0.f, Combatant.Resolve - ResolveLostPerWound * Damage);

	State.bBloodDrawn = true;
	State.bPlayerShedBlood = true;
	State.Posture = ECombatPosture::Engaged;

	if (Combatant.HealthNormalised <= 0.f)
	{
		SetDisposition(CombatantIndex, ECombatantDisposition::Down);
		ApplyCasualtyShock(CombatantIndex);
	}

	ResolveDispositions();
}

void UCombatEncounterSubsystem::ApplyDamageToPlayer(float NormalisedDamage)
{
	if (!State.bIsActive || NormalisedDamage <= 0.f)
	{
		return;
	}

	const FEraCombatTuning Tuning = GetTuning();
	State.PlayerHealthNormalised = FMath::Max(0.f,
		State.PlayerHealthNormalised - NormalisedDamage * Tuning.IncomingDamageScale);

	State.bBloodDrawn = true;

	if (State.PlayerHealthNormalised <= 0.f)
	{
		const FCombatEncounterRow* Row = EncounterTypes.Find(State.EncounterTypeID);

		// Being taken is the historically ordinary ending, and it is not the end of
		// anything: Paul wrote from custody. Only men with no use for a prisoner
		// finish the job.
		EndEncounter((Row && Row->bTakesPrisoners) ? ECombatOutcome::Taken : ECombatOutcome::Defeated);
	}
}

FCombatAttemptResult UCombatEncounterSubsystem::AttemptDeEscalation(float PersuasionStrength)
{
	FCombatAttemptResult Result;

	if (!State.bIsActive)
	{
		Result.Reason = LOCTEXT("NoEncounter", "There is nothing to talk your way out of.");
		return Result;
	}

	const FCombatEncounterRow* Row = EncounterTypes.Find(State.EncounterTypeID);
	if (!Row)
	{
		return Result;
	}

	if (!Row->bCanBeTalkedDown)
	{
		Result.Reason = LOCTEXT("WillNotListen", "They did not come here to be spoken to.");
		return Result;
	}

	// A crowd will hear a man it has just manhandled -- that is Acts 21-22 -- but not
	// one who has put one of its own on the ground. The rule keys on what he did,
	// not on what was done to him.
	if (Row->CrowdKind == ECrowdKind::Mob && State.bPlayerShedBlood)
	{
		Result.Reason = LOCTEXT("MobPastTalking", "They will not hear a man standing over one of their own.");
		return Result;
	}

	if (!HasHostilesRemaining())
	{
		Result.Reason = LOCTEXT("NobodyLeft", "Nobody is still pressing.");
		return Result;
	}

	float Required = Row->DeEscalationDifficulty
		+ State.FailedDeEscalationAttempts * DeEscalationHardeningPerFailure;

	// Drawn steel is an argument of its own and it argues against you.
	if (State.Posture == ECombatPosture::Drawn)
	{
		Required += DrawnPersuasionPenalty;
	}
	else if (State.Posture == ECombatPosture::Engaged)
	{
		Required += DrawnPersuasionPenalty * 2.f;
	}

	float Achieved = PersuasionStrength;

	// Who you are to them counts for as much as what you say. This is the point at
	// which the reputation system pays for itself in combat.
	if (const UReputationSubsystem* Reputation = GetReputation())
	{
		if (!Row->FactionID.IsNone())
		{
			Achieved += Reputation->GetFactionReputation(Row->FactionID) * StandingPersuasionWeight;
		}
	}

	Result.Required = Required;
	Result.Achieved = Achieved;
	Result.bSucceeded = Achieved >= Required;

	if (!Result.bSucceeded)
	{
		// Men who have heard it once and were not moved are harder to move again.
		++State.FailedDeEscalationAttempts;

		// But a crowd is not one mind. An attempt that nearly landed takes the least
		// committed man out of it, which makes the next thing the player tries --
		// usually running -- measurably easier.
		if (Required > 0.f && Achieved >= Required * PartialPersuasionFraction)
		{
			int32 LeastCommittedIndex = INDEX_NONE;
			float LowestResolve = TNumericLimits<float>::Max();

			for (int32 Index = 0; Index < State.Combatants.Num(); ++Index)
			{
				const FCombatantState& Combatant = State.Combatants[Index];
				if (Combatant.Disposition == ECombatantDisposition::Hostile && Combatant.Resolve < LowestResolve)
				{
					LowestResolve = Combatant.Resolve;
					LeastCommittedIndex = Index;
				}
			}

			if (LeastCommittedIndex != INDEX_NONE)
			{
				SetDisposition(LeastCommittedIndex, ECombatantDisposition::Wary);
				Result.Reason = LOCTEXT("SomeWaver", "Most of them are not moved. One of them steps back.");

				// Peeling the last one off is a result in its own right.
				ResolveDispositions();
				return Result;
			}
		}

		Result.Reason = LOCTEXT("NotConvinced", "They are not moved.");
		return Result;
	}

	for (int32 Index = 0; Index < State.Combatants.Num(); ++Index)
	{
		if (State.Combatants[Index].Disposition == ECombatantDisposition::Hostile
			|| State.Combatants[Index].Disposition == ECombatantDisposition::Wary)
		{
			SetDisposition(Index, ECombatantDisposition::TalkedDown);
		}
	}

	EndEncounter(ECombatOutcome::DeEscalated);
	return Result;
}

FCombatAttemptResult UCombatEncounterSubsystem::AttemptEscape(float AgilityStrength)
{
	FCombatAttemptResult Result;

	if (!State.bIsActive)
	{
		Result.Reason = LOCTEXT("NoEncounterToLeave", "There is nothing to get away from.");
		return Result;
	}

	const FCombatEncounterRow* Row = EncounterTypes.Find(State.EncounterTypeID);
	if (!Row)
	{
		return Result;
	}

	int32 HostileCount = 0;
	for (const FCombatantState& Combatant : State.Combatants)
	{
		if (Combatant.Disposition == ECombatantDisposition::Hostile)
		{
			++HostileCount;
		}
	}

	// Every pair of hands still coming for you is another way this goes wrong.
	float Required = Row->EscapeDifficulty
		+ HostileCount * EscapeDifficultyPerHostile
		+ EscapeDifficultyModifier(Row->CrowdKind);

	// Committed men do not disengage cleanly.
	if (State.Posture == ECombatPosture::Engaged)
	{
		Required += EngagedEscapePenalty;
	}

	Result.Required = Required;
	Result.Achieved = AgilityStrength;
	Result.bSucceeded = AgilityStrength >= Required;

	if (!Result.bSucceeded)
	{
		++State.FailedEscapeAttempts;

		// A failed break for it leaves you moving backwards with your hands full.
		State.Posture = ECombatPosture::Withdrawing;
		Result.Reason = LOCTEXT("NoGap", "There is no gap.");
		return Result;
	}

	EndEncounter(ECombatOutcome::Escaped);
	return Result;
}

void UCombatEncounterSubsystem::AdvanceEncounter(float DeltaSeconds)
{
	if (!State.bIsActive || DeltaSeconds <= 0.f)
	{
		return;
	}

	State.ElapsedSeconds += DeltaSeconds;

	const FCombatEncounterRow* Row = EncounterTypes.Find(State.EncounterTypeID);

	// A crowd burns itself out. Nothing else does -- disciplined men will wait all
	// day, which is why running works on a mob and patience works on nobody.
	if (Row && Row->CrowdKind == ECrowdKind::Mob && MobResolveDecayPerSecond > 0.f)
	{
		const float Decay = MobResolveDecayPerSecond * DeltaSeconds;

		for (FCombatantState& Combatant : State.Combatants)
		{
			if (Combatant.Disposition == ECombatantDisposition::Hostile)
			{
				Combatant.Resolve = FMath::Max(0.f, Combatant.Resolve - Decay);
			}
		}
	}

	ResolveDispositions();
}

bool UCombatEncounterSubsystem::HasHostilesRemaining() const
{
	for (const FCombatantState& Combatant : State.Combatants)
	{
		if (Combatant.Disposition == ECombatantDisposition::Hostile)
		{
			return true;
		}
	}

	return false;
}

void UCombatEncounterSubsystem::SetDisposition(int32 CombatantIndex, ECombatantDisposition NewDisposition)
{
	if (!State.Combatants.IsValidIndex(CombatantIndex))
	{
		return;
	}

	FCombatantState& Combatant = State.Combatants[CombatantIndex];
	if (Combatant.Disposition == NewDisposition)
	{
		return;
	}

	Combatant.Disposition = NewDisposition;
	OnCombatantDispositionChanged.Broadcast(CombatantIndex, NewDisposition);
}

void UCombatEncounterSubsystem::ApplyCasualtyShock(int32 FallenIndex)
{
	const FCombatEncounterRow* Row = EncounterTypes.Find(State.EncounterTypeID);
	const float Shock = ResolveLostPerCasualty * (Row ? CasualtyShockMultiplier(Row->CrowdKind) : 1.f);

	for (int32 Index = 0; Index < State.Combatants.Num(); ++Index)
	{
		if (Index == FallenIndex)
		{
			continue;
		}

		FCombatantState& Combatant = State.Combatants[Index];
		if (Combatant.Disposition != ECombatantDisposition::Hostile
			&& Combatant.Disposition != ECombatantDisposition::Wary)
		{
			continue;
		}

		Combatant.Resolve = FMath::Max(0.f, Combatant.Resolve - Shock);
	}
}

void UCombatEncounterSubsystem::ResolveDispositions()
{
	if (!State.bIsActive)
	{
		return;
	}

	for (int32 Index = 0; Index < State.Combatants.Num(); ++Index)
	{
		const FCombatantState& Combatant = State.Combatants[Index];
		if (Combatant.Disposition != ECombatantDisposition::Hostile)
		{
			continue;
		}

		if (Combatant.Resolve <= BreakThreshold)
		{
			// He has not been beaten; he has stopped. Most encounters end here, and
			// the ones that end the other way cost the player something.
			SetDisposition(Index, ECombatantDisposition::Disengaged);
		}
	}

	if (!HasHostilesRemaining())
	{
		EndEncounter(ECombatOutcome::Overcome);
	}
}

void UCombatEncounterSubsystem::BuildReport(ECombatOutcome Outcome)
{
	const FEraCombatTuning Tuning = GetTuning();

	Report = FCombatOutcomeReport();
	Report.Outcome = Outcome;
	Report.EncounterTypeID = State.EncounterTypeID;
	Report.ElapsedSeconds = State.ElapsedSeconds;
	Report.bEndedWithoutBloodshed = !State.bBloodDrawn;

	for (const FCombatantState& Combatant : State.Combatants)
	{
		switch (Combatant.Disposition)
		{
		case ECombatantDisposition::Down:	++Report.CombatantsDown; break;
		case ECombatantDisposition::Disengaged:	++Report.CombatantsBroken; break;
		case ECombatantDisposition::TalkedDown:	++Report.CombatantsTalkedDown; break;
		default: break;
		}
	}

	// Whether getting out of it counted is the era's judgement, not this system's.
	// In a Witness Mission escape is the objective; in the frame era it is a rout.
	switch (Outcome)
	{
	case ECombatOutcome::Overcome:
		Report.bCountedAsWin = true;
		break;
	case ECombatOutcome::Escaped:
		Report.bCountedAsWin = Tuning.bEscapeIsWinState;
		break;
	case ECombatOutcome::DeEscalated:
		Report.bCountedAsWin = Tuning.bDeEscalationIsWinState;
		break;
	default:
		Report.bCountedAsWin = false;
		break;
	}
}

void UCombatEncounterSubsystem::EndEncounter(ECombatOutcome Outcome)
{
	if (!State.bIsActive || Outcome == ECombatOutcome::Ongoing)
	{
		return;
	}

	// Clear first: the deed below can reach code that asks whether an encounter is
	// running, and ending one twice through a delegate would be worse than untidy.
	State.bIsActive = false;

	BuildReport(Outcome);

	// What he did here travels the roads like anything else he does. A street full
	// of witnesses is the most reliable courier in the game.
	const FCombatEncounterRow* Row = EncounterTypes.Find(State.EncounterTypeID);
	if (Row)
	{
		FName DeedTypeID = NAME_None;

		// An overcome deed describes violence -- a body in the precinct, a man who
		// fought the men sent to take him. A crowd that lost interest and went home
		// is also "overcome" and the player did not do any of that, so the deed is
		// gated on blood rather than on the outcome name.
		if (Outcome == ECombatOutcome::Overcome && State.bPlayerShedBlood)
		{
			DeedTypeID = Row->OvercomeDeedTypeID;
		}
		else if (Outcome == ECombatOutcome::DeEscalated)
		{
			DeedTypeID = Row->DeEscalatedDeedTypeID;
		}

		if (!DeedTypeID.IsNone())
		{
			if (UReputationSubsystem* Reputation = GetReputation())
			{
				const UCampaignMapSubsystem* Map = GetMap();
				const FName LocationID = Map ? Map->GetPartyLocation() : NAME_None;

				Reputation->RecordDeed(DeedTypeID, LocationID, TArray<FName>());
			}
		}
	}

	OnEncounterEnded.Broadcast(Outcome);
}

#undef LOCTEXT_NAMESPACE
