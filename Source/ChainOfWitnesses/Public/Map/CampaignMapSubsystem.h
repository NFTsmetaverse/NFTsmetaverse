#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Map/ChainCampaignMapTypes.h"
#include "Math/RandomStream.h"
#include "CampaignMapSubsystem.generated.h"

class UDataTable;
class UCampaignTimelineSubsystem;
class UReputationSubsystem;

DECLARE_LOG_CATEGORY_EXTERN(LogCampaignMap, Log, All);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPartyArrived, FName, LocationID);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWaypointReached, FName, LocationID);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEncounterTriggered, FName, EncounterID, FName, LocationID);

/**
 * The campaign map (Task 10.6): the road and sea network, the party moving on it,
 * and what happens along the way.
 *
 * This subsystem owns the travel graph, because it is the only place that knows how
 * long a journey takes. UReputationSubsystem asks it how far word has travelled
 * rather than keeping a second copy of the network and a second pathfinder.
 *
 * Time and movement are the same thing here, as in any campaign layer: AdvanceDays
 * is what moves the clock while the party is on the road, and it stops the moment
 * something happens, reporting the unspent days so the caller can resume after
 * resolving it.
 *
 * Sea travel is seasonal in two senses. Out of season the lanes are shut and a party
 * waits in port, which is why the Damascus road can beat the Antioch crossing in
 * December. A party that insists on sailing anyway gets through -- at a risk
 * multiplier that makes Acts 27 the expected outcome rather than bad luck.
 */
UCLASS()
class CHAINOFWITNESSES_API UCampaignMapSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// --- Content ------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "Map|Content")
	bool RegisterLocation(const FCampaignLocationRow& Location);

	UFUNCTION(BlueprintCallable, Category = "Map|Content")
	bool RegisterRoute(const FTravelRouteRow& Route);

	UFUNCTION(BlueprintCallable, Category = "Map|Content")
	bool RegisterEncounterType(const FEncounterTypeRow& EncounterType);

	// Bulk wrappers over the three above. Each returns the number of rows added.
	UFUNCTION(BlueprintCallable, Category = "Map|Content")
	int32 RegisterLocationTable(const UDataTable* LocationTable);

	UFUNCTION(BlueprintCallable, Category = "Map|Content")
	int32 RegisterRouteTable(const UDataTable* RouteTable);

	UFUNCTION(BlueprintCallable, Category = "Map|Content")
	int32 RegisterEncounterTypeTable(const UDataTable* EncounterTable);

	UFUNCTION(BlueprintCallable, Category = "Map|Content")
	bool GetLocation(FName LocationID, FCampaignLocationRow& OutLocation) const;

	/** Reports routes naming unregistered locations, and sea legs from landlocked places. */
	UFUNCTION(BlueprintCallable, Category = "Map|Content")
	void ValidateMapContent(TArray<FString>& OutProblems) const;

	// --- Graph queries ------------------------------------------------------

	/**
	 * When a traveller leaving on DepartureDay reaches the far end of one leg. A sea
	 * leg starting in the closed season waits for spring unless bAllowOutOfSeason.
	 */
	UFUNCTION(BlueprintPure, Category = "Map|Graph")
	int32 ComputeLegArrival(int32 DepartureDay, const FTravelRouteRow& Route, bool bAllowOutOfSeason) const;

	/**
	 * Earliest arrival at every location reachable within MaxTravelDays of setting
	 * out. This is what word of mouth spreads along (Task 5).
	 */
	UFUNCTION(BlueprintCallable, Category = "Map|Graph")
	void ComputeArrivalTimes(FName OriginLocationID, int32 StartDay, int32 MaxTravelDays,
		TArray<FTravelArrival>& OutArrivals) const;

	/**
	 * Quickest route by arrival time, not by distance -- a shut sea lane makes the
	 * long way round the fast one. OutPath lists the hops after the origin.
	 */
	UFUNCTION(BlueprintCallable, Category = "Map|Graph")
	bool FindPath(FName OriginLocationID, FName DestinationLocationID, int32 StartDay,
		bool bAllowOutOfSeason, TArray<FName>& OutPath, int32& OutArrivalDay) const;

	UFUNCTION(BlueprintPure, Category = "Map|Graph")
	bool IsSailingSeasonClosed(int32 DayOfYear) const;

	/** Multiplies storm and wreck chances: 1 in high summer, rising as the season shuts. */
	UFUNCTION(BlueprintPure, Category = "Map|Graph")
	float GetSeaRiskMultiplier(int32 DayOfYear, bool bOutOfSeason) const;

	// --- The party ----------------------------------------------------------

	/** Places the party without travelling. Use at campaign start or after a Witness Mission. */
	UFUNCTION(BlueprintCallable, Category = "Map|Party")
	void SetPartyLocation(FName LocationID);

	/**
	 * Plots a course. Pass bRiskOutOfSeasonSailing to put to sea in winter rather
	 * than wait it out. Fails if there is no route.
	 */
	UFUNCTION(BlueprintCallable, Category = "Map|Party")
	bool SetDestination(FName DestinationLocationID, bool bRiskOutOfSeasonSailing = false);

	UFUNCTION(BlueprintCallable, Category = "Map|Party")
	void StopTravelling();

	/**
	 * Advances the campaign clock and the party with it, stopping early on an
	 * encounter or on arrival. This is the campaign layer's heartbeat: while the
	 * party is on the road, this is what moves time.
	 */
	UFUNCTION(BlueprintCallable, Category = "Map|Party")
	FTravelResult AdvanceDays(int32 Days);

	UFUNCTION(BlueprintPure, Category = "Map|Party")
	FPartyState GetPartyState() const { return Party; }

	UFUNCTION(BlueprintPure, Category = "Map|Party")
	FName GetPartyLocation() const { return Party.CurrentLocationID; }

	UFUNCTION(BlueprintPure, Category = "Map|Party")
	bool IsPartyTravelling() const { return Party.bIsTravelling; }

	// --- Save/load ----------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "Map|Save")
	FCampaignMapSaveData CaptureSaveData() const;

	/** Replaces party state and the encounter stream. Registered content is left alone. */
	UFUNCTION(BlueprintCallable, Category = "Map|Save")
	void RestoreFromSaveData(const FCampaignMapSaveData& SaveData);

	/** Test seam; in a running game both resolve from the owning GameInstance. */
	void SetDependenciesForTesting(UCampaignTimelineSubsystem* InTimeline, UReputationSubsystem* InReputation);

	// Day of year on or after which sea legs stop running, and the day they resume.
	// Roughly 1 November and 10 March -- the mare clausum of Roman practice.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map|Sailing", meta = (ClampMin = "0", ClampMax = "364"))
	int32 SailingSeasonClosesDayOfYear = 304;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map|Sailing", meta = (ClampMin = "0", ClampMax = "364"))
	int32 SailingSeasonOpensDayOfYear = 68;

	// Days either side of the open season over which risk ramps up to ShoulderRiskMultiplier.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map|Sailing", meta = (ClampMin = "1"))
	int32 SeaShoulderDays = 30;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map|Sailing", meta = (ClampMin = "1.0"))
	float ShoulderRiskMultiplier = 3.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map|Sailing", meta = (ClampMin = "1.0"))
	float OutOfSeasonRiskMultiplier = 8.f;

	UPROPERTY(BlueprintAssignable, Category = "Map")
	FOnPartyArrived OnPartyArrived;

	UPROPERTY(BlueprintAssignable, Category = "Map")
	FOnWaypointReached OnWaypointReached;

	UPROPERTY(BlueprintAssignable, Category = "Map")
	FOnEncounterTriggered OnEncounterTriggered;

private:
	UCampaignTimelineSubsystem* GetTimeline() const;
	UReputationSubsystem* GetReputation() const;

	int32 GetCurrentDay() const;
	void AdvanceClock(int32 Days);

	const FTravelRouteRow* FindRoute(FName FromLocationID, FName ToLocationID) const;

	/** Sets up the next leg from RemainingPath, including any wait for the season. */
	void BeginNextLeg();

	/** One day's roll. Returns true and names the encounter when something happens. */
	bool RollEncounter(FName& OutEncounterID);

	bool IsEncounterEligible(const FEncounterTypeRow& EncounterType, FName RegionID, int32 CurrentYearAD) const;

	UPROPERTY()
	TMap<FName, FCampaignLocationRow> Locations;

	UPROPERTY()
	TArray<FTravelRouteRow> Routes;

	UPROPERTY()
	TMap<FName, FEncounterTypeRow> EncounterTypes;

	UPROPERTY()
	FPartyState Party;

	// Seeded and saved, so reloading before a leg replays the same road.
	UPROPERTY()
	FRandomStream EncounterStream;

	UPROPERTY()
	TObjectPtr<UCampaignTimelineSubsystem> TimelineOverride = nullptr;

	UPROPERTY()
	TObjectPtr<UReputationSubsystem> ReputationOverride = nullptr;
};
