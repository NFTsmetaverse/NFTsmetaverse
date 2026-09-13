#include "Reputation/ReputationSubsystem.h"

#include "Campaign/CampaignTimelineSubsystem.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "Map/CampaignMapSubsystem.h"

DEFINE_LOG_CATEGORY(LogReputation);

void UReputationSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UReputationSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

void UReputationSubsystem::SetDependenciesForTesting(UCampaignTimelineSubsystem* InTimeline,
	UCampaignMapSubsystem* InMap)
{
	TimelineOverride = InTimeline;
	MapOverride = InMap;
}

UCampaignTimelineSubsystem* UReputationSubsystem::GetTimeline() const
{
	if (TimelineOverride)
	{
		return TimelineOverride;
	}

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		return GameInstance->GetSubsystem<UCampaignTimelineSubsystem>();
	}

	return nullptr;
}

UCampaignMapSubsystem* UReputationSubsystem::GetMap() const
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

int32 UReputationSubsystem::GetCurrentCampaignDay() const
{
	const UCampaignTimelineSubsystem* Timeline = GetTimeline();
	return Timeline ? Timeline->GetTotalElapsedDays() : 0;
}

// --- Content ----------------------------------------------------------------

bool UReputationSubsystem::RegisterFaction(const FFactionRow& Faction)
{
	if (Faction.FactionID.IsNone() || Factions.Contains(Faction.FactionID))
	{
		return false;
	}

	Factions.Add(Faction.FactionID, Faction);
	return true;
}

bool UReputationSubsystem::RegisterDeedType(const FDeedTypeRow& DeedType)
{
	if (DeedType.DeedTypeID.IsNone() || DeedTypes.Contains(DeedType.DeedTypeID))
	{
		return false;
	}

	DeedTypes.Add(DeedType.DeedTypeID, DeedType);
	return true;
}

int32 UReputationSubsystem::RegisterFactionTable(const UDataTable* FactionTable)
{
	if (!FactionTable)
	{
		return 0;
	}

	TArray<FFactionRow*> Rows;
	FactionTable->GetAllRows<FFactionRow>(TEXT("UReputationSubsystem::RegisterFactionTable"), Rows);

	int32 AddedCount = 0;
	for (const FFactionRow* Row : Rows)
	{
		if (Row && RegisterFaction(*Row))
		{
			++AddedCount;
		}
	}

	UE_LOG(LogReputation, Log, TEXT("Registered %d factions."), AddedCount);
	return AddedCount;
}

int32 UReputationSubsystem::RegisterDeedTypeTable(const UDataTable* DeedTypeTable)
{
	if (!DeedTypeTable)
	{
		return 0;
	}

	TArray<FDeedTypeRow*> Rows;
	DeedTypeTable->GetAllRows<FDeedTypeRow>(TEXT("UReputationSubsystem::RegisterDeedTypeTable"), Rows);

	int32 AddedCount = 0;
	for (const FDeedTypeRow* Row : Rows)
	{
		if (Row && RegisterDeedType(*Row))
		{
			++AddedCount;
		}
	}

	UE_LOG(LogReputation, Log, TEXT("Registered %d deed types."), AddedCount);
	return AddedCount;
}

bool UReputationSubsystem::RegisterNpcProfile(const FNpcProfile& Profile)
{
	if (Profile.NpcID.IsNone())
	{
		return false;
	}

	NpcProfiles.Add(Profile.NpcID, Profile);
	return true;
}

bool UReputationSubsystem::GetNpcProfile(FName NpcID, FNpcProfile& OutProfile) const
{
	if (const FNpcProfile* Found = NpcProfiles.Find(NpcID))
	{
		OutProfile = *Found;
		return true;
	}

	return false;
}

void UReputationSubsystem::ValidateReputationContent(TArray<FString>& OutProblems) const
{
	OutProblems.Reset();

	for (const TPair<FName, FFactionRow>& Pair : Factions)
	{
		for (const FFactionAttitude& Attitude : Pair.Value.AttitudesToward)
		{
			if (!Factions.Contains(Attitude.TowardFactionID))
			{
				OutProblems.Add(FString::Printf(TEXT("Faction '%s' holds an attitude toward unregistered faction '%s'."),
					*Pair.Key.ToString(), *Attitude.TowardFactionID.ToString()));
			}
		}
	}

	for (const TPair<FName, FDeedTypeRow>& Pair : DeedTypes)
	{
		for (const FFactionImpact& Impact : Pair.Value.FactionImpacts)
		{
			if (!Factions.Contains(Impact.FactionID))
			{
				OutProblems.Add(FString::Printf(TEXT("Deed type '%s' affects unregistered faction '%s'."),
					*Pair.Key.ToString(), *Impact.FactionID.ToString()));
			}
		}
	}

	for (const TPair<FName, FNpcProfile>& Pair : NpcProfiles)
	{
		if (!Pair.Value.FactionID.IsNone() && !Factions.Contains(Pair.Value.FactionID))
		{
			OutProblems.Add(FString::Printf(TEXT("NPC '%s' belongs to unregistered faction '%s'."),
				*Pair.Key.ToString(), *Pair.Value.FactionID.ToString()));
		}
	}
}

// --- Standing ---------------------------------------------------------------

void UReputationSubsystem::AddToFactionReputation(FName FactionID, float Delta)
{
	if (FactionID.IsNone() || FMath::IsNearlyZero(Delta))
	{
		return;
	}

	FFactionStanding* Standing = FactionStandings.FindByPredicate([FactionID](const FFactionStanding& Entry)
	{
		return Entry.FactionID == FactionID;
	});

	if (!Standing)
	{
		Standing = &FactionStandings.AddDefaulted_GetRef();
		Standing->FactionID = FactionID;
	}

	Standing->Reputation += Delta;
	OnFactionReputationChanged.Broadcast(FactionID, Standing->Reputation);
}

void UReputationSubsystem::ApplyFactionImpact(FName FactionID, float Delta)
{
	if (FactionID.IsNone() || FMath::IsNearlyZero(Delta))
	{
		return;
	}

	AddToFactionReputation(FactionID, Delta);

	// Nothing the player does lands on one faction alone. Everyone who holds a view
	// on this faction revises their own opinion in proportion to it.
	for (const TPair<FName, FFactionRow>& Pair : Factions)
	{
		if (Pair.Key == FactionID)
		{
			continue;
		}

		for (const FFactionAttitude& Attitude : Pair.Value.AttitudesToward)
		{
			if (Attitude.TowardFactionID == FactionID && !FMath::IsNearlyZero(Attitude.Multiplier))
			{
				AddToFactionReputation(Pair.Key, Delta * Attitude.Multiplier);
				break;
			}
		}
	}
}

void UReputationSubsystem::ModifyNpcStanding(FName NpcID, float Delta)
{
	if (NpcID.IsNone() || FMath::IsNearlyZero(Delta))
	{
		return;
	}

	FNpcStanding* Standing = NpcStandings.FindByPredicate([NpcID](const FNpcStanding& Entry)
	{
		return Entry.NpcID == NpcID;
	});

	if (!Standing)
	{
		Standing = &NpcStandings.AddDefaulted_GetRef();
		Standing->NpcID = NpcID;
	}

	Standing->Standing += Delta;
}

float UReputationSubsystem::GetFactionReputation(FName FactionID) const
{
	const FFactionStanding* Standing = FactionStandings.FindByPredicate([FactionID](const FFactionStanding& Entry)
	{
		return Entry.FactionID == FactionID;
	});

	return Standing ? Standing->Reputation : 0.f;
}

float UReputationSubsystem::GetNpcPersonalStanding(FName NpcID) const
{
	const FNpcStanding* Standing = NpcStandings.FindByPredicate([NpcID](const FNpcStanding& Entry)
	{
		return Entry.NpcID == NpcID;
	});

	return Standing ? Standing->Standing : 0.f;
}

float UReputationSubsystem::GetEffectiveStanding(FName NpcID) const
{
	const float Personal = GetNpcPersonalStanding(NpcID);

	const FNpcProfile* Profile = NpcProfiles.Find(NpcID);
	if (!Profile || Profile->FactionID.IsNone())
	{
		return Personal;
	}

	return Personal + GetFactionReputation(Profile->FactionID) * FactionWeight;
}

// --- Deeds ------------------------------------------------------------------

FName UReputationSubsystem::RecordDeed(FName DeedTypeID, FName OriginLocationID,
	const TArray<FName>& WitnessNpcIDs, float Magnitude)
{
	const FDeedTypeRow* DeedType = DeedTypes.Find(DeedTypeID);
	if (!DeedType)
	{
		UE_LOG(LogReputation, Warning, TEXT("Cannot record unregistered deed type '%s'."),
			*DeedTypeID.ToString());
		return NAME_None;
	}

	const int32 CurrentDay = GetCurrentCampaignDay();
	const UCampaignTimelineSubsystem* Timeline = GetTimeline();

	FReputationDeed Deed;
	Deed.DeedID = FName(*FString::Printf(TEXT("%s_%d"), *DeedTypeID.ToString(), NextDeedIndex++));
	Deed.DeedTypeID = DeedTypeID;
	Deed.OriginLocationID = OriginLocationID;
	Deed.OccurredOnDay = CurrentDay;
	Deed.OccurredInYearAD = Timeline ? Timeline->GetCurrentYearAD() : 0;
	Deed.Magnitude = Magnitude;
	Deed.WitnessNpcIDs = WitnessNpcIDs;

	if (DeedType->bTravelsByWordOfMouth)
	{
		if (const UCampaignMapSubsystem* Map = GetMap())
		{
			Map->ComputeArrivalTimes(OriginLocationID, CurrentDay, DeedType->NewsReachDays, Deed.Arrivals);
		}
		else
		{
			// Without the road network nothing can be said about where word gets to,
			// so it goes no further than the place it happened.
			UE_LOG(LogReputation, Warning,
				TEXT("No campaign map available; '%s' will be known only where it happened."),
				*DeedTypeID.ToString());

			if (!OriginLocationID.IsNone())
			{
				FTravelArrival& Here = Deed.Arrivals.AddDefaulted_GetRef();
				Here.LocationID = OriginLocationID;
				Here.ArrivalDay = CurrentDay;
			}
		}
	}
	else if (!OriginLocationID.IsNone())
	{
		// Known where it happened and nowhere else, however striking it was.
		FTravelArrival& Here = Deed.Arrivals.AddDefaulted_GetRef();
		Here.LocationID = OriginLocationID;
		Here.ArrivalDay = CurrentDay;
	}

	for (const FFactionImpact& Impact : DeedType->FactionImpacts)
	{
		ApplyFactionImpact(Impact.FactionID, Impact.Delta * Magnitude);
	}

	// Witnesses react to the act itself. Everyone else will react to the account of
	// it, if and when it reaches them.
	for (const FName WitnessID : WitnessNpcIDs)
	{
		ModifyNpcStanding(WitnessID, DeedType->WitnessStandingDelta * Magnitude);
	}

	const FName RecordedID = Deed.DeedID;
	Deeds.Add(MoveTemp(Deed));

	OnDeedRecorded.Broadcast(RecordedID, DeedTypeID);
	return RecordedID;
}

const FReputationDeed* UReputationSubsystem::FindDeed(FName DeedID) const
{
	return Deeds.FindByPredicate([DeedID](const FReputationDeed& Deed)
	{
		return Deed.DeedID == DeedID;
	});
}

bool UReputationSubsystem::GetDeed(FName DeedID, FReputationDeed& OutDeed) const
{
	if (const FReputationDeed* Found = FindDeed(DeedID))
	{
		OutDeed = *Found;
		return true;
	}

	return false;
}

int32 UReputationSubsystem::GetNewsArrivalDay(FName DeedID, FName LocationID) const
{
	const FReputationDeed* Deed = FindDeed(DeedID);
	if (!Deed)
	{
		return INDEX_NONE;
	}

	for (const FTravelArrival& Arrival : Deed->Arrivals)
	{
		if (Arrival.LocationID == LocationID)
		{
			return Arrival.ArrivalDay;
		}
	}

	return INDEX_NONE;
}

bool UReputationSubsystem::HasNewsReached(FName DeedID, FName LocationID) const
{
	const int32 ArrivalDay = GetNewsArrivalDay(DeedID, LocationID);
	return ArrivalDay != INDEX_NONE && GetCurrentCampaignDay() >= ArrivalDay;
}

bool UReputationSubsystem::DoesNpcKnowOfDeed(FName NpcID, FName DeedID) const
{
	const FReputationDeed* Deed = FindDeed(DeedID);
	if (!Deed)
	{
		return false;
	}

	// Seeing it beats hearing about it.
	if (Deed->WitnessNpcIDs.Contains(NpcID))
	{
		return true;
	}

	const FNpcProfile* Profile = NpcProfiles.Find(NpcID);
	if (!Profile)
	{
		return false;
	}

	return HasNewsReached(DeedID, Profile->HomeLocationID);
}

void UReputationSubsystem::GetDeedsKnownToNpc(FName NpcID, TArray<FReputationDeed>& OutDeeds) const
{
	OutDeeds.Reset();

	// Newest first: the thing he brings up is the thing he heard most recently.
	for (int32 Index = Deeds.Num() - 1; Index >= 0; --Index)
	{
		if (DoesNpcKnowOfDeed(NpcID, Deeds[Index].DeedID))
		{
			OutDeeds.Add(Deeds[Index]);
		}
	}
}

// --- Save/load --------------------------------------------------------------

FReputationSaveData UReputationSubsystem::CaptureSaveData() const
{
	FReputationSaveData SaveData;
	SaveData.FactionStandings = FactionStandings;
	SaveData.NpcStandings = NpcStandings;
	SaveData.Deeds = Deeds;
	SaveData.NextDeedIndex = NextDeedIndex;

	return SaveData;
}

void UReputationSubsystem::RestoreFromSaveData(const FReputationSaveData& SaveData)
{
	FactionStandings = SaveData.FactionStandings;
	NpcStandings = SaveData.NpcStandings;
	Deeds = SaveData.Deeds;
	NextDeedIndex = SaveData.NextDeedIndex;
}
