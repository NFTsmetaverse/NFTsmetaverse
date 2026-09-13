#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Codex/CodexTypes.h"
#include "Codex/TestimonyFragment.h"
#include "CodexSubsystem.generated.h"

class UDataTable;
class UCampaignTimelineSubsystem;

DECLARE_LOG_CATEGORY_EXTERN(LogCodex, Log, All);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnFragmentRecovered, FName, FragmentID);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnChainChanged, FName, AnchorEventID);

/**
 * The Codex of Witnesses (Task 10.2) -- the game's actual progression system.
 *
 * Holds the registry of authored fragments, tracks which the player has recovered,
 * owns the transmission chains he assembles from them, and scores those chains for
 * Attestation Strength. Task 3's UI reads FChainScoreBreakdown; Task 7's
 * argumentation encounters use chains as ammunition and the breakdown's weakest
 * link as the opponent's line of attack.
 *
 * Scoring is deliberately a pure function of chain + registry (the anchor year is
 * cached on the chain), so it can be evaluated and tested without a world.
 */
UCLASS()
class CHAINOFWITNESSES_API UCodexSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// --- Registry -----------------------------------------------------------
	// Registration is explicit rather than an Asset Manager scan: load policy for
	// fragment assets belongs to whoever owns startup flow, and a table-driven
	// project may never use the asset form at all.

	UFUNCTION(BlueprintCallable, Category = "Codex|Registry")
	bool RegisterFragmentDefinition(const FTestimonyFragmentDefinition& Definition);

	UFUNCTION(BlueprintCallable, Category = "Codex|Registry")
	bool RegisterFragment(const UTestimonyFragment* Fragment);

	/** Registers every row of a DT_TestimonyFragments-shaped table. Returns rows added. */
	UFUNCTION(BlueprintCallable, Category = "Codex|Registry")
	int32 RegisterFragmentsFromDataTable(const UDataTable* FragmentTable);

	UFUNCTION(BlueprintCallable, Category = "Codex|Registry")
	bool GetFragmentDefinition(FName FragmentID, FTestimonyFragmentDefinition& OutDefinition) const;

	UFUNCTION(BlueprintPure, Category = "Codex|Registry")
	bool IsFragmentRegistered(FName FragmentID) const;

	/**
	 * Reports authored cross-references that point at fragments no one registered.
	 * Content check for the editor and automated tests -- a dangling ChallengedBy
	 * would otherwise silently never be answerable.
	 */
	UFUNCTION(BlueprintCallable, Category = "Codex|Registry")
	void ValidateRegistry(TArray<FName>& OutDanglingReferences) const;

	// --- Recovery -----------------------------------------------------------

	/** Section 8 step 1: recover a fragment through play. */
	UFUNCTION(BlueprintCallable, Category = "Codex")
	bool RecoverFragment(FName FragmentID);

	UFUNCTION(BlueprintPure, Category = "Codex")
	bool IsFragmentRecovered(FName FragmentID) const;

	UFUNCTION(BlueprintCallable, Category = "Codex")
	void GetRecoveredFragmentIDs(TArray<FName>& OutFragmentIDs) const;

	// --- Chain assembly -----------------------------------------------------

	/**
	 * Opens a chain for a campaign event, resolving its anchor year from the
	 * timeline subsystem (Task 1). Fails if that lookup fails -- use
	 * CreateChainWithAnchorYear to supply the year directly.
	 */
	UFUNCTION(BlueprintCallable, Category = "Codex|Chains")
	bool CreateChain(FName AnchorEventID);

	UFUNCTION(BlueprintCallable, Category = "Codex|Chains")
	bool CreateChainWithAnchorYear(FName AnchorEventID, int32 AnchorYearAD);

	/** Section 8 step 2. The fragment must be recovered, and may occupy only one link per chain. */
	UFUNCTION(BlueprintCallable, Category = "Codex|Chains")
	bool SlotFragment(FName AnchorEventID, ETransmissionLink Link, FName FragmentID);

	UFUNCTION(BlueprintCallable, Category = "Codex|Chains")
	bool UnslotFragment(FName AnchorEventID, FName FragmentID);

	UFUNCTION(BlueprintCallable, Category = "Codex|Chains")
	bool GetChain(FName AnchorEventID, FTransmissionChain& OutChain) const;

	UFUNCTION(BlueprintCallable, Category = "Codex|Chains")
	void GetChainAnchorEventIDs(TArray<FName>& OutAnchorEventIDs) const;

	// --- Scoring ------------------------------------------------------------

	/** Section 8 step 3. Safe to call on a missing chain: returns bChainExists false. */
	UFUNCTION(BlueprintCallable, Category = "Codex|Scoring")
	FChainScoreBreakdown ScoreChain(FName AnchorEventID) const;

	/** Mean Attestation Strength across every open chain -- the campaign meter. */
	UFUNCTION(BlueprintCallable, Category = "Codex|Scoring")
	float GetCampaignAttestationStrength() const;

	// By value: UFUNCTIONs cannot return a struct by const reference.
	UFUNCTION(BlueprintPure, Category = "Codex|Scoring")
	FCodexScoringRules GetScoringRules() const { return ScoringRules; }

	UFUNCTION(BlueprintCallable, Category = "Codex|Scoring")
	void SetScoringRules(const FCodexScoringRules& InRules) { ScoringRules = InRules; }

	// --- Save/load ----------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "Codex|Save")
	FCodexSaveData CaptureSaveData() const;

	/** Replaces all recovery and chain state. The fragment registry is authored content and is left alone. */
	UFUNCTION(BlueprintCallable, Category = "Codex|Save")
	void RestoreFromSaveData(const FCodexSaveData& SaveData);

	/** Test seam; in a running game the timeline resolves from the owning GameInstance. */
	void SetTimelineForTesting(UCampaignTimelineSubsystem* InTimeline);

	UPROPERTY(BlueprintAssignable, Category = "Codex")
	FOnFragmentRecovered OnFragmentRecovered;

	UPROPERTY(BlueprintAssignable, Category = "Codex")
	FOnChainChanged OnChainChanged;

private:
	UCampaignTimelineSubsystem* GetTimeline() const;

	FTransmissionChain* FindChain(FName AnchorEventID);
	const FTransmissionChain* FindChain(FName AnchorEventID) const;

	/** 0..1 by distance from the anchor year, per FCodexScoringRules. */
	float ComputeProximityScore(int32 AttestationDateAD, int32 AnchorYearAD) const;

	/** True if any fragment slotted in the chain rebuts ChallengerID. */
	bool IsChallengeAnswered(FName ChallengerID, const FTransmissionChain& Chain) const;

	int32 CountCorroboratingPairs(const FTransmissionChain& Chain) const;

	UPROPERTY()
	TMap<FName, FTestimonyFragmentDefinition> FragmentRegistry;

	UPROPERTY()
	TSet<FName> RecoveredFragmentIDs;

	UPROPERTY()
	TArray<FTransmissionChain> Chains;

	UPROPERTY()
	FCodexScoringRules ScoringRules;

	UPROPERTY()
	TObjectPtr<UCampaignTimelineSubsystem> TimelineOverride = nullptr;
};
