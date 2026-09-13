#include "Map/CampaignMapSubsystem.h"

#include "Algo/Reverse.h"
#include "Campaign/CampaignTimelineSubsystem.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "Reputation/ReputationSubsystem.h"

DEFINE_LOG_CATEGORY(LogCampaignMap);

void UCampaignMapSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Any fixed seed will do; what matters is that it is saved, so reloading before
	// a leg replays the same road rather than rerolling for a kinder one.
	EncounterStream.Initialize(20300414);
}

void UCampaignMapSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

void UCampaignMapSubsystem::SetDependenciesForTesting(UCampaignTimelineSubsystem* InTimeline,
	UReputationSubsystem* InReputation)
{
	TimelineOverride = InTimeline;
	ReputationOverride = InReputation;
}

UCampaignTimelineSubsystem* UCampaignMapSubsystem::GetTimeline() const
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

UReputationSubsystem* UCampaignMapSubsystem::GetReputation() const
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

int32 UCampaignMapSubsystem::GetCurrentDay() const
{
	const UCampaignTimelineSubsystem* Timeline = GetTimeline();
	return Timeline ? Timeline->GetTotalElapsedDays() : 0;
}

void UCampaignMapSubsystem::AdvanceClock(int32 Days)
{
	if (UCampaignTimelineSubsystem* Timeline = GetTimeline())
	{
		Timeline->AdvanceDays(Days);
	}
}

// --- Content ----------------------------------------------------------------

bool UCampaignMapSubsystem::RegisterLocation(const FCampaignLocationRow& Location)
{
	if (Location.LocationID.IsNone() || Locations.Contains(Location.LocationID))
	{
		return false;
	}

	Locations.Add(Location.LocationID, Location);
	return true;
}

bool UCampaignMapSubsystem::RegisterRoute(const FTravelRouteRow& Route)
{
	if (Route.FromLocationID.IsNone() || Route.ToLocationID.IsNone() || Route.TravelDays <= 0)
	{
		return false;
	}

	Routes.Add(Route);
	return true;
}

bool UCampaignMapSubsystem::RegisterEncounterType(const FEncounterTypeRow& EncounterType)
{
	if (EncounterType.EncounterID.IsNone() || EncounterTypes.Contains(EncounterType.EncounterID))
	{
		return false;
	}

	EncounterTypes.Add(EncounterType.EncounterID, EncounterType);
	return true;
}

int32 UCampaignMapSubsystem::RegisterLocationTable(const UDataTable* LocationTable)
{
	if (!LocationTable)
	{
		return 0;
	}

	TArray<FCampaignLocationRow*> Rows;
	LocationTable->GetAllRows<FCampaignLocationRow>(TEXT("UCampaignMapSubsystem::RegisterLocationTable"), Rows);

	int32 AddedCount = 0;
	for (const FCampaignLocationRow* Row : Rows)
	{
		if (Row && RegisterLocation(*Row))
		{
			++AddedCount;
		}
	}

	UE_LOG(LogCampaignMap, Log, TEXT("Registered %d locations."), AddedCount);
	return AddedCount;
}

int32 UCampaignMapSubsystem::RegisterRouteTable(const UDataTable* RouteTable)
{
	if (!RouteTable)
	{
		return 0;
	}

	TArray<FTravelRouteRow*> Rows;
	RouteTable->GetAllRows<FTravelRouteRow>(TEXT("UCampaignMapSubsystem::RegisterRouteTable"), Rows);

	int32 AddedCount = 0;
	for (const FTravelRouteRow* Row : Rows)
	{
		if (Row && RegisterRoute(*Row))
		{
			++AddedCount;
		}
	}

	UE_LOG(LogCampaignMap, Log, TEXT("Registered %d travel routes."), AddedCount);
	return AddedCount;
}

int32 UCampaignMapSubsystem::RegisterEncounterTypeTable(const UDataTable* EncounterTable)
{
	if (!EncounterTable)
	{
		return 0;
	}

	TArray<FEncounterTypeRow*> Rows;
	EncounterTable->GetAllRows<FEncounterTypeRow>(TEXT("UCampaignMapSubsystem::RegisterEncounterTypeTable"), Rows);

	int32 AddedCount = 0;
	for (const FEncounterTypeRow* Row : Rows)
	{
		if (Row && RegisterEncounterType(*Row))
		{
			++AddedCount;
		}
	}

	UE_LOG(LogCampaignMap, Log, TEXT("Registered %d encounter types."), AddedCount);
	return AddedCount;
}

bool UCampaignMapSubsystem::GetLocation(FName LocationID, FCampaignLocationRow& OutLocation) const
{
	if (const FCampaignLocationRow* Found = Locations.Find(LocationID))
	{
		OutLocation = *Found;
		return true;
	}

	return false;
}

void UCampaignMapSubsystem::ValidateMapContent(TArray<FString>& OutProblems) const
{
	OutProblems.Reset();

	for (const FTravelRouteRow& Route : Routes)
	{
		const FCampaignLocationRow* From = Locations.Find(Route.FromLocationID);
		const FCampaignLocationRow* To = Locations.Find(Route.ToLocationID);

		if (!From)
		{
			OutProblems.Add(FString::Printf(TEXT("Route starts at unregistered location '%s'."),
				*Route.FromLocationID.ToString()));
		}

		if (!To)
		{
			OutProblems.Add(FString::Printf(TEXT("Route ends at unregistered location '%s'."),
				*Route.ToLocationID.ToString()));
		}

		if (Route.bIsSeaRoute)
		{
			if (From && !From->bIsPort)
			{
				OutProblems.Add(FString::Printf(TEXT("Sea route sets out from '%s', which is not a port."),
					*Route.FromLocationID.ToString()));
			}

			if (To && !To->bIsPort)
			{
				OutProblems.Add(FString::Printf(TEXT("Sea route lands at '%s', which is not a port."),
					*Route.ToLocationID.ToString()));
			}
		}
	}

	for (const TPair<FName, FEncounterTypeRow>& Pair : EncounterTypes)
	{
		if (!Pair.Value.bOnLandRoutes && !Pair.Value.bOnSeaRoutes)
		{
			OutProblems.Add(FString::Printf(TEXT("Encounter '%s' can happen on neither land nor sea."),
				*Pair.Key.ToString()));
		}
	}
}

// --- Graph ------------------------------------------------------------------

bool UCampaignMapSubsystem::IsSailingSeasonClosed(int32 DayOfYear) const
{
	// A misconfigured season (opening on or after it closes) is treated as open all
	// year rather than stranding every ship in the world.
	if (SailingSeasonOpensDayOfYear >= SailingSeasonClosesDayOfYear)
	{
		return false;
	}

	return DayOfYear >= SailingSeasonClosesDayOfYear || DayOfYear < SailingSeasonOpensDayOfYear;
}

float UCampaignMapSubsystem::GetSeaRiskMultiplier(int32 DayOfYear, bool bOutOfSeason) const
{
	if (bOutOfSeason || IsSailingSeasonClosed(DayOfYear))
	{
		return OutOfSeasonRiskMultiplier;
	}

	// Risk climbs toward either end of the open season rather than stepping from
	// safe to lethal on one particular morning.
	const int32 MarginToClose = SailingSeasonClosesDayOfYear - DayOfYear;
	const int32 MarginFromOpen = DayOfYear - SailingSeasonOpensDayOfYear;
	const int32 Margin = FMath::Min(MarginToClose, MarginFromOpen);

	if (Margin >= SeaShoulderDays)
	{
		return 1.f;
	}

	const float Alpha = static_cast<float>(FMath::Max(0, Margin)) / static_cast<float>(FMath::Max(1, SeaShoulderDays));
	return FMath::Lerp(ShoulderRiskMultiplier, 1.f, Alpha);
}

int32 UCampaignMapSubsystem::ComputeLegArrival(int32 DepartureDay, const FTravelRouteRow& Route,
	bool bAllowOutOfSeason) const
{
	if (!Route.bIsSeaRoute || bAllowOutOfSeason)
	{
		return DepartureDay + Route.TravelDays;
	}

	const int32 DaysPerYear = UCampaignTimelineSubsystem::DaysPerYear;
	const int32 DayOfYear = ((DepartureDay % DaysPerYear) + DaysPerYear) % DaysPerYear;

	if (!IsSailingSeasonClosed(DayOfYear))
	{
		return DepartureDay + Route.TravelDays;
	}

	int32 EffectiveDeparture = DepartureDay;
	if (DayOfYear >= SailingSeasonClosesDayOfYear)
	{
		// Shut for the winter: wait out the rest of this year, then until spring.
		EffectiveDeparture += (DaysPerYear - DayOfYear) + SailingSeasonOpensDayOfYear;
	}
	else
	{
		EffectiveDeparture += SailingSeasonOpensDayOfYear - DayOfYear;
	}

	return EffectiveDeparture + Route.TravelDays;
}

const FTravelRouteRow* UCampaignMapSubsystem::FindRoute(FName FromLocationID, FName ToLocationID) const
{
	const FTravelRouteRow* Best = nullptr;

	for (const FTravelRouteRow& Route : Routes)
	{
		const bool bForward = Route.FromLocationID == FromLocationID && Route.ToLocationID == ToLocationID;
		const bool bBackward = Route.bIsBidirectional
			&& Route.ToLocationID == FromLocationID
			&& Route.FromLocationID == ToLocationID;

		if ((bForward || bBackward) && (!Best || Route.TravelDays < Best->TravelDays))
		{
			Best = &Route;
		}
	}

	return Best;
}

void UCampaignMapSubsystem::ComputeArrivalTimes(FName OriginLocationID, int32 StartDay, int32 MaxTravelDays,
	TArray<FTravelArrival>& OutArrivals) const
{
	OutArrivals.Reset();

	if (OriginLocationID.IsNone())
	{
		return;
	}

	TMap<FName, int32> EarliestArrival;
	EarliestArrival.Add(OriginLocationID, StartDay);

	TSet<FName> Settled;

	// Dijkstra with a linear frontier scan. The graph is a handful of cities, so a
	// heap would cost more in indirection than it saves. Edge cost depends on when
	// the traveller reaches the port, but waiting for the season never makes an
	// earlier departure arrive later, so the usual argument still holds.
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

			const int32 Arrival = ComputeLegArrival(CurrentDay, Route, /*bAllowOutOfSeason=*/false);

			// Beyond the budget: word stops being worth repeating before it gets
			// this far, and a traveller would not make the journey either.
			if (Arrival - StartDay > MaxTravelDays)
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
		FTravelArrival& Arrival = OutArrivals.AddDefaulted_GetRef();
		Arrival.LocationID = Entry.Key;
		Arrival.ArrivalDay = Entry.Value;
	}
}

bool UCampaignMapSubsystem::FindPath(FName OriginLocationID, FName DestinationLocationID, int32 StartDay,
	bool bAllowOutOfSeason, TArray<FName>& OutPath, int32& OutArrivalDay) const
{
	OutPath.Reset();
	OutArrivalDay = INDEX_NONE;

	if (OriginLocationID.IsNone() || DestinationLocationID.IsNone())
	{
		return false;
	}

	if (OriginLocationID == DestinationLocationID)
	{
		OutArrivalDay = StartDay;
		return true;
	}

	TMap<FName, int32> EarliestArrival;
	TMap<FName, FName> Predecessor;
	TSet<FName> Settled;

	EarliestArrival.Add(OriginLocationID, StartDay);

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

		if (Current.IsNone() || Current == DestinationLocationID)
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

			const int32 Arrival = ComputeLegArrival(CurrentDay, Route, bAllowOutOfSeason);

			int32* Existing = EarliestArrival.Find(Neighbour);
			if (!Existing)
			{
				EarliestArrival.Add(Neighbour, Arrival);
				Predecessor.Add(Neighbour, Current);
			}
			else if (Arrival < *Existing)
			{
				*Existing = Arrival;
				Predecessor.Add(Neighbour, Current);
			}
		}
	}

	const int32* DestinationDay = EarliestArrival.Find(DestinationLocationID);
	if (!DestinationDay)
	{
		return false;
	}

	OutArrivalDay = *DestinationDay;

	FName Step = DestinationLocationID;
	while (Step != OriginLocationID)
	{
		OutPath.Add(Step);

		const FName* Previous = Predecessor.Find(Step);
		if (!Previous)
		{
			OutPath.Reset();
			OutArrivalDay = INDEX_NONE;
			return false;
		}

		Step = *Previous;
	}

	Algo::Reverse(OutPath);
	return true;
}

// --- The party --------------------------------------------------------------

void UCampaignMapSubsystem::SetPartyLocation(FName LocationID)
{
	StopTravelling();
	Party.CurrentLocationID = LocationID;
}

bool UCampaignMapSubsystem::SetDestination(FName DestinationLocationID, bool bRiskOutOfSeasonSailing)
{
	if (DestinationLocationID.IsNone() || Party.CurrentLocationID.IsNone())
	{
		return false;
	}

	if (DestinationLocationID == Party.CurrentLocationID)
	{
		StopTravelling();
		return true;
	}

	TArray<FName> Path;
	int32 ArrivalDay = INDEX_NONE;

	if (!FindPath(Party.CurrentLocationID, DestinationLocationID, GetCurrentDay(),
		bRiskOutOfSeasonSailing, Path, ArrivalDay))
	{
		UE_LOG(LogCampaignMap, Warning, TEXT("No route from '%s' to '%s'."),
			*Party.CurrentLocationID.ToString(), *DestinationLocationID.ToString());
		return false;
	}

	Party.DestinationLocationID = DestinationLocationID;
	Party.RemainingPath = MoveTemp(Path);
	Party.bIsTravelling = true;
	Party.bRiskOutOfSeasonSailing = bRiskOutOfSeasonSailing;

	BeginNextLeg();
	return Party.bIsTravelling;
}

void UCampaignMapSubsystem::StopTravelling()
{
	Party.bIsTravelling = false;
	Party.NextLocationID = NAME_None;
	Party.DestinationLocationID = NAME_None;
	Party.RemainingPath.Reset();
	Party.DaysRemainingOnLeg = 0;
	Party.DaysWaitingInPort = 0;
	Party.bCurrentLegIsSea = false;
	Party.bRiskOutOfSeasonSailing = false;
}

void UCampaignMapSubsystem::BeginNextLeg()
{
	if (Party.RemainingPath.Num() == 0)
	{
		Party.bIsTravelling = false;
		Party.NextLocationID = NAME_None;
		return;
	}

	const FName NextLocationID = Party.RemainingPath[0];
	Party.RemainingPath.RemoveAt(0);

	const FTravelRouteRow* Route = FindRoute(Party.CurrentLocationID, NextLocationID);
	if (!Route)
	{
		UE_LOG(LogCampaignMap, Error, TEXT("No leg from '%s' to '%s'; the party stops where it stands."),
			*Party.CurrentLocationID.ToString(), *NextLocationID.ToString());
		StopTravelling();
		return;
	}

	Party.NextLocationID = NextLocationID;
	Party.bCurrentLegIsSea = Route->bIsSeaRoute;

	const int32 Now = GetCurrentDay();
	const int32 Arrival = ComputeLegArrival(Now, *Route, Party.bRiskOutOfSeasonSailing);

	// The wait for the season and the crossing itself are tracked apart, because
	// nothing happens to a party sitting in harbour.
	Party.DaysRemainingOnLeg = Route->TravelDays;
	Party.DaysWaitingInPort = FMath::Max(0, (Arrival - Now) - Route->TravelDays);
}

bool UCampaignMapSubsystem::IsEncounterEligible(const FEncounterTypeRow& EncounterType, FName RegionID,
	int32 CurrentYearAD) const
{
	if (EncounterType.EarliestYearAD != INDEX_NONE && CurrentYearAD < EncounterType.EarliestYearAD)
	{
		return false;
	}

	if (EncounterType.LatestYearAD != INDEX_NONE && CurrentYearAD > EncounterType.LatestYearAD)
	{
		return false;
	}

	if (EncounterType.RegionIDs.Num() > 0 && !EncounterType.RegionIDs.Contains(RegionID))
	{
		return false;
	}

	if (EncounterType.Conditions.Num() > 0)
	{
		const UReputationSubsystem* Reputation = GetReputation();
		if (!Reputation)
		{
			// Fail closed: an encounter that never fires is a visible gap; one that
			// fires because its condition could not be checked is not.
			return false;
		}

		for (const FEncounterCondition& Condition : EncounterType.Conditions)
		{
			const float Standing = Reputation->GetFactionReputation(Condition.FactionID);
			if (Standing < Condition.MinReputation || Standing > Condition.MaxReputation)
			{
				return false;
			}
		}
	}

	return true;
}

bool UCampaignMapSubsystem::RollEncounter(FName& OutEncounterID)
{
	OutEncounterID = NAME_None;

	if (EncounterTypes.Num() == 0)
	{
		return false;
	}

	const UCampaignTimelineSubsystem* Timeline = GetTimeline();
	const int32 CurrentYearAD = Timeline ? Timeline->GetCurrentYearAD() : 0;
	const int32 DayOfYear = Timeline ? Timeline->GetCurrentDayOfYear() : 0;

	const FCampaignLocationRow* Origin = Locations.Find(Party.CurrentLocationID);
	const FName RegionID = Origin ? Origin->RegionID : NAME_None;
	const float DangerModifier = Origin ? Origin->DangerModifier : 1.f;

	const float SeaRisk = Party.bCurrentLegIsSea
		? GetSeaRiskMultiplier(DayOfYear, Party.bRiskOutOfSeasonSailing)
		: 1.f;

	TArray<TPair<FName, float>> Eligible;
	float TotalChance = 0.f;

	for (const TPair<FName, FEncounterTypeRow>& Pair : EncounterTypes)
	{
		const FEncounterTypeRow& EncounterType = Pair.Value;

		const bool bRouteMatches = Party.bCurrentLegIsSea ? EncounterType.bOnSeaRoutes : EncounterType.bOnLandRoutes;
		if (!bRouteMatches || !IsEncounterEligible(EncounterType, RegionID, CurrentYearAD))
		{
			continue;
		}

		float Chance = EncounterType.ChancePerDay * DangerModifier;
		if (EncounterType.bScalesWithSeaSeason)
		{
			Chance *= SeaRisk;
		}

		if (Chance <= 0.f)
		{
			continue;
		}

		Eligible.Add(TPair<FName, float>(EncounterType.EncounterID, Chance));
		TotalChance += Chance;
	}

	if (Eligible.Num() == 0 || TotalChance <= 0.f)
	{
		return false;
	}

	if (EncounterStream.FRand() >= FMath::Min(TotalChance, 1.f))
	{
		return false;
	}

	// Pick in proportion against the unclamped total, so which encounter fires does
	// not depend on the order the table happens to iterate in.
	float Pick = EncounterStream.FRand() * TotalChance;
	for (const TPair<FName, float>& Entry : Eligible)
	{
		Pick -= Entry.Value;
		if (Pick <= 0.f)
		{
			OutEncounterID = Entry.Key;
			return true;
		}
	}

	OutEncounterID = Eligible.Last().Key;
	return true;
}

FTravelResult UCampaignMapSubsystem::AdvanceDays(int32 Days)
{
	FTravelResult Result;
	Result.LocationID = Party.CurrentLocationID;

	if (Days <= 0)
	{
		Result.Outcome = Party.bIsTravelling ? ETravelOutcome::Travelling : ETravelOutcome::Idle;
		return Result;
	}

	int32 Remaining = Days;

	while (Remaining > 0)
	{
		if (!Party.bIsTravelling)
		{
			// Sitting still. Time passes and nothing else does.
			AdvanceClock(Remaining);
			Result.DaysElapsed += Remaining;
			Remaining = 0;
			break;
		}

		if (Party.DaysWaitingInPort > 0)
		{
			const int32 Step = FMath::Min(Remaining, Party.DaysWaitingInPort);
			AdvanceClock(Step);
			Party.DaysWaitingInPort -= Step;
			Remaining -= Step;
			Result.DaysElapsed += Step;

			if (Party.DaysWaitingInPort > 0)
			{
				Result.Outcome = ETravelOutcome::WaitingForSailingSeason;
				return Result;
			}

			continue;
		}

		// On the road, a day at a time, so that something can interrupt mid-leg.
		while (Remaining > 0 && Party.DaysRemainingOnLeg > 0)
		{
			AdvanceClock(1);
			--Party.DaysRemainingOnLeg;
			--Remaining;
			++Result.DaysElapsed;

			FName EncounterID;
			if (RollEncounter(EncounterID))
			{
				Result.Outcome = ETravelOutcome::Encounter;
				Result.EncounterID = EncounterID;
				Result.DaysRemaining = Remaining;
				Result.LocationID = Party.CurrentLocationID;

				OnEncounterTriggered.Broadcast(EncounterID, Party.CurrentLocationID);
				return Result;
			}
		}

		if (Party.DaysRemainingOnLeg > 0)
		{
			// Out of days, still between places.
			break;
		}

		// Reached the far end of this leg.
		Party.CurrentLocationID = Party.NextLocationID;
		Party.NextLocationID = NAME_None;
		Result.LocationID = Party.CurrentLocationID;

		if (Party.RemainingPath.Num() == 0)
		{
			StopTravelling();
			Result.Outcome = ETravelOutcome::Arrived;
			Result.DaysRemaining = Remaining;

			OnPartyArrived.Broadcast(Result.LocationID);
			return Result;
		}

		// Passing through is not a reason to stop.
		OnWaypointReached.Broadcast(Party.CurrentLocationID);
		BeginNextLeg();

		if (!Party.bIsTravelling)
		{
			Result.DaysRemaining = Remaining;
			break;
		}
	}

	Result.Outcome = Party.bIsTravelling ? ETravelOutcome::Travelling : ETravelOutcome::Idle;
	return Result;
}

// --- Save/load --------------------------------------------------------------

FCampaignMapSaveData UCampaignMapSubsystem::CaptureSaveData() const
{
	FCampaignMapSaveData SaveData;
	SaveData.Party = Party;
	SaveData.EncounterSeed = EncounterStream.GetCurrentSeed();

	return SaveData;
}

void UCampaignMapSubsystem::RestoreFromSaveData(const FCampaignMapSaveData& SaveData)
{
	Party = SaveData.Party;
	EncounterStream.Initialize(SaveData.EncounterSeed);
}
