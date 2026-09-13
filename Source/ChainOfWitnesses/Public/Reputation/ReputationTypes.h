#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "ReputationTypes.generated.h"

/** How one faction weighs the player's standing with another. */
USTRUCT(BlueprintType)
struct CHAINOFWITNESSES_API FFactionAttitude
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Faction")
	FName TowardFactionID;

	// -1 reads the other faction's gain as an equal loss; +1 as an equal gain; 0 is
	// indifference. This is what stops reputation being eleven unrelated integers:
	// feeding the poor in Jerusalem is not neutral to the men who run the Temple.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Faction", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
	float Multiplier = 0.f;
};

/** One of the Section 5 factions. */
USTRUCT(BlueprintType)
struct CHAINOFWITNESSES_API FFactionRow : public FTableRowBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Faction")
	FName FactionID;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Faction")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Faction", meta = (MultiLine = true))
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Faction")
	TArray<FFactionAttitude> AttitudesToward;

	// Timeline row (Task 1) this faction comes into play with. NAME_None for
	// factions present from the campaign's opening year.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Faction")
	FName UnlockedByEventID;
};

/** What one kind of act does to the player's standing. */
USTRUCT(BlueprintType)
struct CHAINOFWITNESSES_API FFactionImpact
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Deed")
	FName FactionID;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Deed")
	float Delta = 0.f;
};

/**
 * An authored kind of act. Runtime records (FReputationDeed) point back at one of
 * these, so an NPC remembers a specific thing the player did rather than only a
 * number that moved.
 */
USTRUCT(BlueprintType)
struct CHAINOFWITNESSES_API FDeedTypeRow : public FTableRowBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Deed")
	FName DeedTypeID;

	// How someone who has heard of it would recount it. Surfaced by dialogue and the
	// reputation screen -- this is the memory, in words.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Deed", meta = (MultiLine = true))
	FText Summary;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Deed")
	TArray<FFactionImpact> FactionImpacts;

	// Applied to the personal standing of anyone who saw it happen. Witnesses react
	// to the act; everyone else reacts to the account of it.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Deed")
	float WitnessStandingDelta = 0.f;

	// How far the account travels before it stops being worth repeating, in days of
	// travel from where it happened. A kindness in a Jerusalem back street does not
	// reach Rome; refusing to sacrifice in front of a magistrate does.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Deed", meta = (ClampMin = "0"))
	int32 NewsReachDays = 30;

	// False for acts only the witnesses ever know about: the account never leaves
	// the place it happened, whatever NewsReachDays says.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Deed")
	bool bTravelsByWordOfMouth = true;
};

/**
 * A road or sea leg between two locations. Task 6 owns the campaign map proper;
 * this is the same graph at the resolution news needs, and is meant to be the seed
 * that grows into it rather than a parallel structure.
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
	// roughly November to March, and a message that arrives at the wrong end of
	// October waits for spring.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Route")
	bool bIsSeaRoute = false;

	// Almost always true. False models a leg that is only practical one way.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Route")
	bool bIsBidirectional = true;
};

/** Where an NPC lives and who he belongs to. */
USTRUCT(BlueprintType)
struct CHAINOFWITNESSES_API FNpcProfile
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC")
	FName NpcID;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC")
	FName FactionID;

	// Determines which accounts have reached him.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC")
	FName HomeLocationID;
};

/** When an account of a deed reaches one place. */
USTRUCT(BlueprintType)
struct CHAINOFWITNESSES_API FDeedArrival
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "Reputation")
	FName LocationID;

	// Absolute campaign day, matching UCampaignTimelineSubsystem::GetTotalElapsedDays.
	UPROPERTY(BlueprintReadOnly, Category = "Reputation")
	int32 ArrivalDay = 0;
};

/** A specific thing the player did, and how far the account of it has spread. */
USTRUCT(BlueprintType)
struct CHAINOFWITNESSES_API FReputationDeed
{
	GENERATED_BODY()

public:
	// Unique per occurrence: the player may shelter believers more than once.
	UPROPERTY(BlueprintReadOnly, Category = "Reputation")
	FName DeedID;

	UPROPERTY(BlueprintReadOnly, Category = "Reputation")
	FName DeedTypeID;

	UPROPERTY(BlueprintReadOnly, Category = "Reputation")
	FName OriginLocationID;

	UPROPERTY(BlueprintReadOnly, Category = "Reputation")
	int32 OccurredOnDay = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Reputation")
	int32 OccurredInYearAD = 0;

	// Scales the deed type's impacts. 1.0 is the authored weight.
	UPROPERTY(BlueprintReadOnly, Category = "Reputation")
	float Magnitude = 1.f;

	// Saw it themselves, and so know of it regardless of where news has reached.
	UPROPERTY(BlueprintReadOnly, Category = "Reputation")
	TArray<FName> WitnessNpcIDs;

	// Every place the account reaches, and when. Places beyond the deed type's
	// NewsReachDays are absent: the story dies out before it gets there.
	UPROPERTY(BlueprintReadOnly, Category = "Reputation")
	TArray<FDeedArrival> Arrivals;
};

/** Personal standing with one named NPC, as distinct from his faction's view. */
USTRUCT(BlueprintType)
struct CHAINOFWITNESSES_API FNpcStanding
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "Reputation")
	FName NpcID;

	UPROPERTY(BlueprintReadOnly, Category = "Reputation")
	float Standing = 0.f;
};

/** Standing with one faction. */
USTRUCT(BlueprintType)
struct CHAINOFWITNESSES_API FFactionStanding
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "Reputation")
	FName FactionID;

	UPROPERTY(BlueprintReadOnly, Category = "Reputation")
	float Reputation = 0.f;
};

/**
 * Serializable reputation state -- Section 9's "faction relations" and the deed
 * half of "per-NPC memory". Conversational rapport lives in FDialogueSaveData;
 * these are the acts the world remembers.
 */
USTRUCT(BlueprintType)
struct CHAINOFWITNESSES_API FReputationSaveData
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "Reputation")
	TArray<FFactionStanding> FactionStandings;

	UPROPERTY(BlueprintReadOnly, Category = "Reputation")
	TArray<FNpcStanding> NpcStandings;

	UPROPERTY(BlueprintReadOnly, Category = "Reputation")
	TArray<FReputationDeed> Deeds;

	// Keeps generated DeedIDs unique and stable across a save/load cycle.
	UPROPERTY(BlueprintReadOnly, Category = "Reputation")
	int32 NextDeedIndex = 0;
};
