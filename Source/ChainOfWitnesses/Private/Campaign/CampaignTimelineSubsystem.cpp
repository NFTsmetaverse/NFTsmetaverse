#include "Campaign/CampaignTimelineSubsystem.h"

void UCampaignTimelineSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UCampaignTimelineSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

void UCampaignTimelineSubsystem::SetTimelineTable(UDataTable* InTimelineTable)
{
	TimelineTable = InTimelineTable;

	ActiveEventIDs.Reset();
	EstablishedHubCityIDs.Reset();
	DestroyedHubCityIDs.Reset();
	UnlockedFactionIDs.Reset();

	AdvanceToYear(CurrentYearAD);
}

bool UCampaignTimelineSubsystem::GetEventRow(FName EventID, FCampaignEventRow& OutRow) const
{
	if (!TimelineTable)
	{
		return false;
	}

	if (const FCampaignEventRow* FoundRow = TimelineTable->FindRow<FCampaignEventRow>(EventID, TEXT("CampaignTimelineSubsystem::GetEventRow")))
	{
		OutRow = *FoundRow;
		return true;
	}

	return false;
}

bool UCampaignTimelineSubsystem::EvaluatePrerequisites(const FCampaignEventRow& Row) const
{
	for (const FName& PrereqID : Row.PrerequisiteEventIDs)
	{
		if (!ActiveEventIDs.Contains(PrereqID))
		{
			return false;
		}
	}
	return true;
}

void UCampaignTimelineSubsystem::ApplyWorldStateEffects(const FCampaignEventRow& Row)
{
	for (const ECampaignWorldStateEffect Effect : Row.WorldStateEffects)
	{
		switch (Effect)
		{
		case ECampaignWorldStateEffect::HubEstablished:
			if (!Row.AffectedHubCityID.IsNone())
			{
				EstablishedHubCityIDs.Add(Row.AffectedHubCityID);
			}
			break;

		case ECampaignWorldStateEffect::HubDestroyed:
			if (!Row.AffectedHubCityID.IsNone())
			{
				DestroyedHubCityIDs.Add(Row.AffectedHubCityID);
			}
			break;

		case ECampaignWorldStateEffect::FactionUnlock:
			for (const FName& FactionID : Row.UnlockedFactionIDs)
			{
				UnlockedFactionIDs.Add(FactionID);
			}
			break;

		case ECampaignWorldStateEffect::None:
		case ECampaignWorldStateEffect::SetPiece:
		case ECampaignWorldStateEffect::WitnessMission:
		case ECampaignWorldStateEffect::Corroboration:
		default:
			// No persistent world-state change; consumed directly by later systems
			// (Witness Mission framework, Task 8; Codex fragments, Task 2).
			break;
		}
	}
}

int32 UCampaignTimelineSubsystem::GetTotalElapsedDays() const
{
	return (CurrentYearAD - CampaignStartYearAD) * DaysPerYear + CurrentDayOfYear;
}

void UCampaignTimelineSubsystem::AdvanceDays(int32 Days)
{
	if (Days <= 0)
	{
		return;
	}

	CurrentDayOfYear += Days;

	const int32 YearsRolled = CurrentDayOfYear / DaysPerYear;
	CurrentDayOfYear %= DaysPerYear;

	// Route through AdvanceToYear so a multi-year jump still activates every event
	// it passed over, rather than only those in the year it lands in.
	AdvanceToYear(CurrentYearAD + YearsRolled);
}

void UCampaignTimelineSubsystem::SetDate(int32 YearAD, int32 DayOfYear)
{
	CurrentDayOfYear = FMath::Clamp(DayOfYear, 0, DaysPerYear - 1);

	// World state here is derived from the date, not accumulated over play, so the
	// honest way to move backwards is to throw it away and rebuild it.
	ActiveEventIDs.Reset();
	EstablishedHubCityIDs.Reset();
	DestroyedHubCityIDs.Reset();
	UnlockedFactionIDs.Reset();

	AdvanceToYear(YearAD);
}

void UCampaignTimelineSubsystem::AdvanceToYear(int32 NewYearAD)
{
	CurrentYearAD = NewYearAD;

	if (!TimelineTable)
	{
		return;
	}

	// Loop until a full pass activates nothing new, so a chain of prerequisites
	// resolves in one call regardless of row order in the table.
	bool bActivatedAnyThisPass = true;
	while (bActivatedAnyThisPass)
	{
		bActivatedAnyThisPass = false;

		TArray<FCampaignEventRow*> Rows;
		TimelineTable->GetAllRows<FCampaignEventRow>(TEXT("CampaignTimelineSubsystem::AdvanceToYear"), Rows);

		for (const FCampaignEventRow* Row : Rows)
		{
			if (!Row || ActiveEventIDs.Contains(Row->EventID))
			{
				continue;
			}

			const bool bYearReached = CurrentYearAD >= Row->YearEarliestAD;
			if (!bYearReached || !EvaluatePrerequisites(*Row))
			{
				continue;
			}

			ActiveEventIDs.Add(Row->EventID);
			bActivatedAnyThisPass = true;

			ApplyWorldStateEffects(*Row);
			OnEventActivated.Broadcast(Row->EventID);
		}
	}

	OnYearAdvanced.Broadcast(CurrentYearAD);
}

ECampaignAct UCampaignTimelineSubsystem::GetCurrentAct() const
{
	// Act boundaries per Section 6: Act I AD 30-44, Act II AD 46-62, Act III AD 55-100.
	// Acts II and III overlap (Matthew's logia and Mark both begin composition while
	// Act II's letters are still being written), so this reports furthest-reached,
	// not a mutually exclusive bracket.
	if (CurrentYearAD >= 55)
	{
		return ECampaignAct::Act3_GospelsWritten;
	}
	if (CurrentYearAD >= 46)
	{
		return ECampaignAct::Act2_TheLetters;
	}
	return ECampaignAct::Act1_JerusalemChurch;
}

bool UCampaignTimelineSubsystem::IsEventActive(FName EventID) const
{
	return ActiveEventIDs.Contains(EventID);
}

bool UCampaignTimelineSubsystem::IsHubCityEstablished(FName HubCityID) const
{
	return EstablishedHubCityIDs.Contains(HubCityID) && !DestroyedHubCityIDs.Contains(HubCityID);
}

bool UCampaignTimelineSubsystem::IsFactionUnlocked(FName FactionID) const
{
	return UnlockedFactionIDs.Contains(FactionID);
}

void UCampaignTimelineSubsystem::GetActiveEventIDs(TArray<FName>& OutEventIDs) const
{
	OutEventIDs = ActiveEventIDs.Array();
}
