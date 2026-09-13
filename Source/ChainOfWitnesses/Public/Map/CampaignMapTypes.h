#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "CampaignMapTypes.generated.h"

/**
 * A place on the campaign map. Locations are the nodes of the road and sea graph;
 * FTravelRouteRow supplies the edges.
 */
USTRUCT(BlueprintType)
struct CHAINOFWITNESSES_API FCampaignLocationRow : public FTableRowBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Location")
	FName LocationID;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Location")
	FText DisplayName;

	// Groups locations for encounter filtering: bandits on the Via Egnatia are not
	// the same threat as the Temple guard in Judaea.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Location")
	FName RegionID;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Location")
	bool bIsPort = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Location")
	FName ControllingFactionID;

	// The hub ID the Section 6 timeline establishes or destroys for this place
	// (Hub_Jerusalem and the like). The timeline speaks in hubs and the map speaks
	// in locations; this is the join, so AD 70 can close Jerusalem without the two
	// tables having to agree on one naming scheme.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Location")
	FName HubCityID;

	// A station of the imperial post. Section 5 lists the relay as something the
	// campaign map should model; for now it marks where official news moves fastest.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Location")
	bool bIsRelayStation = false;

	// Scales the chance of an encounter on legs setting out from here.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Location", meta = (ClampMin = "0.0"))
	float DangerModifier = 1.f;
};

/**
 * A road or sea leg between two locations.
 *
 * Owned by the map, which is the only place that knows how long a journey takes.
 * UReputationSubsystem asks the map how far word has travelled rather than keeping
 * its own copy of the network.
 */
USTRUCT(BlueprintType)
struct CHAINOFWITNESSES_API FTravelRouteRow : public FTableRowBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Route")
	FName FromLocationID;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Route")
	FName ToLocationID;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Route", meta = (ClampMin = "1"))
	int32 TravelDays = 1;

	// Sea legs do not run in winter. Section 5: the Roman sailing season closed
	// roughly November to March, and anything waiting at the wrong end of October
	// waits for spring -- unless someone insists on sailing anyway.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Route")
	bool bIsSeaRoute = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Route")
	bool bIsBidirectional = true;
};

/** When something travelling the network reaches one place. */
USTRUCT(BlueprintType)
struct CHAINOFWITNESSES_API FTravelArrival
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "Map")
	FName LocationID;

	// Absolute campaign day, matching UCampaignTimelineSubsystem::GetTotalElapsedDays.
	UPROPERTY(BlueprintReadOnly, Category = "Map")
	int32 ArrivalDay = 0;
};

/** Gates an encounter on where the player stands with someone. */
USTRUCT(BlueprintType)
struct CHAINOFWITNESSES_API FEncounterCondition
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Encounter")
	FName FactionID;

	// The band of standing in which this encounter is possible. The defaults are far
	// outside any reachable reputation, so an unset bound is simply no bound.
	// Zealots stopping a traveller because his name is good with Rome is the case
	// this exists for.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Encounter")
	float MinReputation = -99999.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Encounter")
	float MaxReputation = 99999.f;
};

/** Something that can happen on the road. */
USTRUCT(BlueprintType)
struct CHAINOFWITNESSES_API FEncounterTypeRow : public FTableRowBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Encounter")
	FName EncounterID;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Encounter", meta = (MultiLine = true))
	FText Summary;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Encounter")
	bool bOnLandRoutes = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Encounter")
	bool bOnSeaRoutes = false;

	// Empty means anywhere. Otherwise the leg must set out from one of these regions.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Encounter")
	TArray<FName> RegionIDs;

	// Campaign-year window. INDEX_NONE disables either bound -- Sicarii on the roads
	// are a fact of the 50s and 60s, not of AD 33.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Encounter")
	int32 EarliestYearAD = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Encounter")
	int32 LatestYearAD = INDEX_NONE;

	// Per day of travel, before the location's danger modifier and, at sea, the
	// seasonal one.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Encounter", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ChancePerDay = 0.02f;

	// All must hold for the encounter to be eligible.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Encounter")
	TArray<FEncounterCondition> Conditions;

	// Storms and wreck: rises steeply as the season closes in, and far higher again
	// for anyone at sea outside it.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Encounter")
	bool bScalesWithSeaSeason = false;

	// False for meetings that are not a threat -- a believer on the same road, a
	// courier, a caravan worth travelling with.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Encounter")
	bool bIsHostile = true;
};

/** What came of advancing the campaign clock. */
UENUM(BlueprintType)
enum class ETravelOutcome : uint8
{
	// Not travelling; the days simply passed.
	Idle					UMETA(DisplayName = "Idle"),
	// Still on the road with days left to go. Intermediate stops are reported by
	// OnWaypointReached rather than by halting: passing through a city is not a
	// reason to stop travelling.
	Travelling				UMETA(DisplayName = "Travelling"),
	// Reached the destination; the party is no longer travelling.
	Arrived					UMETA(DisplayName = "Arrived"),
	// Interrupted. The unspent days are reported so the caller can resume.
	Encounter				UMETA(DisplayName = "Encounter"),
	// In port, waiting for the lanes to open.
	WaitingForSailingSeason	UMETA(DisplayName = "Waiting for the Sailing Season"),

	MAX						UMETA(Hidden)
};

/** The result of one call to advance the campaign clock. */
USTRUCT(BlueprintType)
struct CHAINOFWITNESSES_API FTravelResult
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "Map")
	ETravelOutcome Outcome = ETravelOutcome::Idle;

	UPROPERTY(BlueprintReadOnly, Category = "Map")
	int32 DaysElapsed = 0;

	// Unspent days, when an encounter cut the journey short.
	UPROPERTY(BlueprintReadOnly, Category = "Map")
	int32 DaysRemaining = 0;

	// Where the party now stands, or the leg's origin if it is still between places.
	UPROPERTY(BlueprintReadOnly, Category = "Map")
	FName LocationID;

	// Set only when Outcome is Encounter.
	UPROPERTY(BlueprintReadOnly, Category = "Map")
	FName EncounterID;
};

/** Where the party is and where it is going. */
USTRUCT(BlueprintType)
struct CHAINOFWITNESSES_API FPartyState
{
	GENERATED_BODY()

public:
	// The last place the party actually stood. While on a leg this is its origin.
	UPROPERTY(BlueprintReadOnly, Category = "Map")
	FName CurrentLocationID;

	UPROPERTY(BlueprintReadOnly, Category = "Map")
	FName NextLocationID;

	UPROPERTY(BlueprintReadOnly, Category = "Map")
	FName DestinationLocationID;

	// Hops still to make after NextLocationID, in order.
	UPROPERTY(BlueprintReadOnly, Category = "Map")
	TArray<FName> RemainingPath;

	// Days still to spend in port before this leg can start. No encounters are rolled
	// against these -- the party is sitting still.
	UPROPERTY(BlueprintReadOnly, Category = "Map")
	int32 DaysWaitingInPort = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Map")
	int32 DaysRemainingOnLeg = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Map")
	bool bIsTravelling = false;

	// Set when the player chose to sail out of season rather than wait, as the owner
	// of Paul's ship did. The lanes open, and the risk multiplies.
	UPROPERTY(BlueprintReadOnly, Category = "Map")
	bool bRiskOutOfSeasonSailing = false;

	UPROPERTY(BlueprintReadOnly, Category = "Map")
	bool bCurrentLegIsSea = false;
};

/** Serializable map state. */
USTRUCT(BlueprintType)
struct CHAINOFWITNESSES_API FCampaignMapSaveData
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "Map")
	FPartyState Party;

	// Saved so that reloading and re-travelling the same leg does not reroll into a
	// different set of encounters.
	UPROPERTY(BlueprintReadOnly, Category = "Map")
	int32 EncounterSeed = 0;
};
