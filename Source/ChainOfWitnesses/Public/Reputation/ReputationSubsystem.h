#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Reputation/ReputationTypes.h"
#include "ReputationSubsystem.generated.h"

class UDataTable;
class UCampaignTimelineSubsystem;
class UCampaignMapSubsystem;

DECLARE_LOG_CATEGORY_EXTERN(LogReputation, Log, All);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDeedRecorded, FName, DeedID, FName, DeedTypeID);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnFactionReputationChanged, FName, FactionID, float, NewReputation);

/**
 * Faction and per-NPC reputation with memory and word-of-mouth (Task 10.5).
 *
 * Section 5 asks to exceed per-clan relation integers on three counts, and each is
 * a separate mechanism here:
 *
 *   - Per-faction AND per-named-NPC. Factions hold a reputation; named NPCs hold a
 *     personal standing on top of it, so a man can think well of you while his
 *     party does not.
 *   - Memory of specific acts. Every change comes from a recorded FReputationDeed
 *     that names what the player did, where, and when. The numbers are derived from
 *     the record, never set blind.
 *   - Word-of-mouth along trade routes. An account leaves the place it happened and
 *     travels the road and sea network at the speed of the people carrying it, so
 *     an NPC in Rome has not heard about last week's trouble in Jerusalem. Sea legs
 *     wait out the closed sailing season.
 *
 * Standing answers "what does this man think of you"; UDialogueSubsystem's trust
 * answers "how far has he warmed to you in conversation". They are deliberately
 * separate: Peter may like you personally and still not risk anything while your
 * name is bad in the city.
 */
UCLASS()
class CHAINOFWITNESSES_API UReputationSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// --- Content ------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "Reputation|Content")
	bool RegisterFaction(const FFactionRow& Faction);

	UFUNCTION(BlueprintCallable, Category = "Reputation|Content")
	bool RegisterDeedType(const FDeedTypeRow& DeedType);

	// Bulk wrappers over the two above. Each returns the number of rows added.
	UFUNCTION(BlueprintCallable, Category = "Reputation|Content")
	int32 RegisterFactionTable(const UDataTable* FactionTable);

	UFUNCTION(BlueprintCallable, Category = "Reputation|Content")
	int32 RegisterDeedTypeTable(const UDataTable* DeedTypeTable);

	/** Named NPCs must be profiled before their location or faction can matter. */
	UFUNCTION(BlueprintCallable, Category = "Reputation|Content")
	bool RegisterNpcProfile(const FNpcProfile& Profile);

	UFUNCTION(BlueprintCallable, Category = "Reputation|Content")
	bool GetNpcProfile(FName NpcID, FNpcProfile& OutProfile) const;

	/** So other systems can lint their own content against the faction list. */
	UFUNCTION(BlueprintPure, Category = "Reputation|Content")
	bool IsFactionRegistered(FName FactionID) const { return Factions.Contains(FactionID); }

	UFUNCTION(BlueprintPure, Category = "Reputation|Content")
	bool IsDeedTypeRegistered(FName DeedTypeID) const { return DeedTypes.Contains(DeedTypeID); }

	/** Reports deed impacts and NPC profiles naming factions nobody registered. */
	UFUNCTION(BlueprintCallable, Category = "Reputation|Content")
	void ValidateReputationContent(TArray<FString>& OutProblems) const;

	// --- Deeds --------------------------------------------------------------

	/**
	 * Records something the player did. Applies faction impacts and witness
	 * standing immediately, then works out where and when the account of it will
	 * arrive. Returns the new deed's ID, or NAME_None if the type is unregistered.
	 */
	UFUNCTION(BlueprintCallable, Category = "Reputation")
	FName RecordDeed(FName DeedTypeID, FName OriginLocationID, const TArray<FName>& WitnessNpcIDs,
		float Magnitude = 1.f);

	UFUNCTION(BlueprintCallable, Category = "Reputation")
	bool GetDeed(FName DeedID, FReputationDeed& OutDeed) const;

	/** True if he saw it, or if the account has reached where he lives by now. */
	UFUNCTION(BlueprintPure, Category = "Reputation")
	bool DoesNpcKnowOfDeed(FName NpcID, FName DeedID) const;

	/** Everything this NPC could bring up, newest first. The memory, as content. */
	UFUNCTION(BlueprintCallable, Category = "Reputation")
	void GetDeedsKnownToNpc(FName NpcID, TArray<FReputationDeed>& OutDeeds) const;

	/** INDEX_NONE if the account never gets there. */
	UFUNCTION(BlueprintPure, Category = "Reputation")
	int32 GetNewsArrivalDay(FName DeedID, FName LocationID) const;

	/** True once the account has reached the location on the current campaign day. */
	UFUNCTION(BlueprintPure, Category = "Reputation")
	bool HasNewsReached(FName DeedID, FName LocationID) const;

	// --- Standing -----------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "Reputation")
	float GetFactionReputation(FName FactionID) const;

	UFUNCTION(BlueprintPure, Category = "Reputation")
	float GetNpcPersonalStanding(FName NpcID) const;

	/**
	 * What this man actually thinks of you: his own view of you, plus his faction's,
	 * weighted by FactionWeight. An unprofiled NPC has no faction and returns only
	 * his personal standing.
	 */
	UFUNCTION(BlueprintPure, Category = "Reputation")
	float GetEffectiveStanding(FName NpcID) const;

	// --- Save/load ----------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "Reputation|Save")
	FReputationSaveData CaptureSaveData() const;

	/** Replaces standings and deeds. Registered content is left alone. */
	UFUNCTION(BlueprintCallable, Category = "Reputation|Save")
	void RestoreFromSaveData(const FReputationSaveData& SaveData);

	/** Test seam; in a running game both resolve from the owning GameInstance. */
	void SetDependenciesForTesting(UCampaignTimelineSubsystem* InTimeline, UCampaignMapSubsystem* InMap);

	// How much an NPC's faction colours his view of the player, relative to his own
	// dealings with him.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reputation", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FactionWeight = 0.5f;

	UPROPERTY(BlueprintAssignable, Category = "Reputation")
	FOnDeedRecorded OnDeedRecorded;

	UPROPERTY(BlueprintAssignable, Category = "Reputation")
	FOnFactionReputationChanged OnFactionReputationChanged;

private:
	UCampaignTimelineSubsystem* GetTimeline() const;
	UCampaignMapSubsystem* GetMap() const;
	int32 GetCurrentCampaignDay() const;

	/** Applies a change to one faction, then to everyone who has a view on that faction. */
	void ApplyFactionImpact(FName FactionID, float Delta);

	void AddToFactionReputation(FName FactionID, float Delta);
	void ModifyNpcStanding(FName NpcID, float Delta);

	const FReputationDeed* FindDeed(FName DeedID) const;

	UPROPERTY()
	TMap<FName, FFactionRow> Factions;

	UPROPERTY()
	TMap<FName, FDeedTypeRow> DeedTypes;

	UPROPERTY()
	TMap<FName, FNpcProfile> NpcProfiles;

	UPROPERTY()
	TArray<FFactionStanding> FactionStandings;

	UPROPERTY()
	TArray<FNpcStanding> NpcStandings;

	UPROPERTY()
	TArray<FReputationDeed> Deeds;

	UPROPERTY()
	int32 NextDeedIndex = 0;

	UPROPERTY()
	TObjectPtr<UCampaignTimelineSubsystem> TimelineOverride = nullptr;

	UPROPERTY()
	TObjectPtr<UCampaignMapSubsystem> MapOverride = nullptr;
};
