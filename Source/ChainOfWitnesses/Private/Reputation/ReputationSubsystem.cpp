#include "Reputation/ReputationSubsystem.h"

#include "Campaign/CampaignTimelineSubsystem.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"

DEFINE_LOG_CATEGORY(LogReputation);

void UReputationSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UReputationSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

void UReputationSubsystem::SetTimelineForTesting(UCampaignTimelineSubsystem* InTimeline)
{
	TimelineOverride = InTimeline;
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

bool UReputationSubsystem::RegisterRoute(const FTravelRouteRow& Route)
{
	if (Route.FromLocationID.IsNone() || Route.ToLocationID.IsNone())
	{
		return false;
	}

	Routes.Add(Route);
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

int32 UReputationSubsystem::RegisterRouteTable(const UDataTable* RouteTable)
{
	if (!RouteTable)
	{
		return 0;
	}

	TArray<FTravelRouteRow*> Rows;
	RouteTable->GetAllRows<FTravelRouteRow>(TEXT("UReputationSubsystem::RegisterRouteTable"), Rows);

	int32 AddedCount = 0;
	for (const FTravelRouteRow* Row : Rows)
	{
		if (Row && RegisterRoute(*Row))
		{
			++AddedCount;
		}
	}

	UE_LOG(LogReputation, Log, TEXT("Registered %d travel routes."), AddedCount);
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

	for (const FTravelRouteRow& Route : Routes)
	{
		if (Route.FromLocationID == Route.ToLocationID)
		{
			OutProblems.Add(FString::Printf(TEXT("Route at '%s' leads to itself."),
				*Route.FromLocationID.ToString()));
		}

		if (Route.TravelDays <= 0)
		{
			OutProblems.Add(FString::Printf(TEXT("Route '%s' -> '%s' takes no time."),
				*Route.FromLocationID.ToString(), *Route.ToLocationID.ToString()));
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

// --- Word of mouth ----------------------------------------------------------

int32 UReputationSubsystem::ComputeLegArrival(int32 DepartureDay, const FTravelRouteRow& Route) const
{
	if (!Route.bIsSeaRoute)
	{
		return DepartureDay + Route.TravelDays;
	}

	// A misconfigured season (opens on or after it closes) would make the wrap
	// below nonsense; treat it as open all year rather than silently stranding news.
	if (SailingSeasonOpensDayOfYear >= SailingSeasonClosesDayOfYear)
	{
		return DepartureDay + Route.TravelDays;
	}

	const int32 DaysPerYear = UCampaignTimelineSubsystem::DaysPerYear;
	const int32 DayOfYear = ((DepartureDay % DaysPerYear) + DaysPerYear) % DaysPerYear;

	int32 EffectiveDeparture = DepartureDay;

	if (DayOfYear >= SailingSeasonClosesDayOfYear)
	{
		// Shut for the winter: wait out the rest of this year, then until spring.
		EffectiveDeparture += (DaysPerYear - DayOfYear) + SailingSeasonOpensDayOfYear;
	}
	else if (DayOfYear < SailingSeasonOpensDayOfYear)
	{
		EffectiveDeparture += (SailingSeasonOpensDayOfYear - DayOfYear);
	}

	return EffectiveDeparture + Route.TravelDays;
}

void UReputationSubsystem::PropagateNews(FName OriginLocationID, int32 OriginDay, int32 MaxTravelDays,
	TArray<FDeedArrival>& OutArrivals) const
{
	OutArrivals.Reset();

	if (OriginLocationID.IsNone())
	{
		return;
	}

	TMap<FName, int32> EarliestArrival;
	EarliestArrival.Add(OriginLocationID, OriginDay);

	TSet<FName> Settled;

	// Dijkstra with a linear frontier scan. The graph is a handful of cities, and a
	// heap would cost more in indirection than it saves. Edge cost depends on when
	// the carrier reaches the port, but waiting for the sailing season never makes
	// an earlier departure arrive later, so the usual argument still holds.
	for (;;)
	{
		FName Current = NAME_None;
		int32 CurrentDay = TNumericLimits<int32>::Max();

		for (const TPair<FName, int32>& Entry : EarliestArrival)
		{
			if (!Settled.Contains(Entry.Key) && Entry.Value < CurrentDay)
			{
				Current = Entry.Key;
				CurrentDay = Entry.Value;
			}
		}

		if (Current.IsNone())
		{
			break;
		}

		Settled.Add(Current);

		for (const FTravelRouteRow& Route : Routes)
		{
			FName Neighbour = NAME_None;
			if (Route.FromLocationID == Current)
			{
				Neighbour = Route.ToLocationID;
			}
			else if (Route.bIsBidirectional && Route.ToLocationID == Current)
			{
				Neighbour = Route.FromLocationID;
			}

			if (Neighbour.IsNone() || Settled.Contains(Neighbour))
			{
				continue;
			}

			const int32 Arrival = ComputeLegArrival(CurrentDay, Route);

			// The account stops being worth repeating before it gets this far.
			if (Arrival - OriginDay > MaxTravelDays)
			{
				continue;
			}

			if (int32* Existing = EarliestArrival.Find(Neighbour))
			{
				*Existing = FMath::Min(*Existing, Arrival);
			}
			else
			{
				EarliestArrival.Add(Neighbour, Arrival);
			}
		}
	}

	OutArrivals.Reserve(EarliestArrival.Num());
	for (const TPair<FName, int32>& Entry : EarliestArrival)
	{
		FDeedArrival& Arrival = OutArrivals.AddDefaulted_GetRef();
		Arrival.LocationID = Entry.Key;
		Arrival.ArrivalDay = Entry.Value;
	}
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
		PropagateNews(OriginLocationID, CurrentDay, DeedType->NewsReachDays, Deed.Arrivals);
	}
	else if (!OriginLocationID.IsNone())
	{
		// Known where it happened and nowhere else, however striking it was.
		FDeedArrival& Here = Deed.Arrivals.AddDefaulted_GetRef();
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

	for (const FDeedArrival& Arrival : Deed->Arrivals)
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
