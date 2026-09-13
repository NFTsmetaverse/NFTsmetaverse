#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Witness/WitnessMissionTypes.h"
#include "WitnessMissionSubsystem.generated.h"

class UDataTable;
class UCodexSubsystem;
class UCampaignTimelineSubsystem;
class UReputationSubsystem;
class UCampaignMapSubsystem;
class UDialogueSubsystem;

DECLARE_LOG_CATEGORY_EXTERN(LogWitnessMission, Log, All);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEraChanged, EGameEra, NewEra, FName, EraID);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWitnessMissionEntered, FName, MissionID);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnWitnessMissionEnded, FName, MissionID, EWitnessMissionOutcome, Outcome);

/**
 * The Witness Mission framework (Task 10.8): era swap, controller swap, combat
 * retuning, and the return to the frame.
 *
 * **What this owns.** State and policy: which era is live, which mission is
 * running, which pawn and controller and input context that era wants, what the
 * combat tuning currently is, and how to put the frame era back afterwards.
 *
 * **What this does not own.** Level travel and possession. Those need a World and
 * a loaded map, which is GameMode territory; this subsystem names the level and
 * the classes and broadcasts, and the GameMode does the work. Keeping it that way
 * is what lets the whole framework be tested without a world.
 *
 * **The clock is the trick.** Entering a mission pushes the campaign clock to the
 * mission's year and the party to its location; leaving pops both back. Every
 * era-gated system already built -- dialogue windows, debate opponents, road
 * encounters -- therefore evaluates correctly inside a mission without knowing
 * missions exist.
 *
 * **A mission is a reconstruction.** The player brings his whole Codex into it,
 * including sources written after the year he is standing in, because he is the
 * one assembling the scene. What he does inside changes nothing in the frame era
 * except what he comes out knowing: faction standing and NPC memory are
 * snapshotted on entry and restored on return. Fragments are the deliberate
 * exception, and the point of going.
 *
 * **Mode B.** With EStructuralMode::FirstCenturyOnly there is no frame to seal off
 * or return to, so entering a mission is simply arriving somewhere and nothing is
 * snapshotted. Section 3's toggle is this flag and nothing else.
 */
UCLASS()
class CHAINOFWITNESSES_API UWitnessMissionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// --- Content ------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "Witness|Content")
	bool RegisterEra(const FEraDefinitionRow& Era);

	UFUNCTION(BlueprintCallable, Category = "Witness|Content")
	bool RegisterMission(const FWitnessMissionRow& Mission);

	UFUNCTION(BlueprintCallable, Category = "Witness|Content")
	int32 RegisterEraTable(const UDataTable* EraTable);

	UFUNCTION(BlueprintCallable, Category = "Witness|Content")
	int32 RegisterMissionTable(const UDataTable* MissionTable);

	UFUNCTION(BlueprintCallable, Category = "Witness|Content")
	bool GetEra(FName EraID, FEraDefinitionRow& OutEra) const;

	UFUNCTION(BlueprintCallable, Category = "Witness|Content")
	bool GetMission(FName MissionID, FWitnessMissionRow& OutMission) const;

	/** Reports missions naming unregistered eras, anchors, or fragments. */
	UFUNCTION(BlueprintCallable, Category = "Witness|Content")
	void ValidateWitnessContent(TArray<FString>& OutProblems) const;

	// --- Mode and era -------------------------------------------------------

	/** Section 3's toggle. Set once at startup, before any mission runs. */
	UFUNCTION(BlueprintCallable, Category = "Witness")
	void SetStructuralMode(EStructuralMode Mode);

	UFUNCTION(BlueprintPure, Category = "Witness")
	EStructuralMode GetStructuralMode() const { return StructuralMode; }

	/**
	 * Puts the player in the frame era at FrameYearAD. Mode A startup calls this;
	 * in Mode B it does nothing, because there is no frame.
	 */
	UFUNCTION(BlueprintCallable, Category = "Witness")
	void BeginFrameEra(FName FrameEraID, FName StartingLocationID);

	UFUNCTION(BlueprintPure, Category = "Witness")
	EGameEra GetCurrentEra() const { return State.CurrentEra; }

	UFUNCTION(BlueprintPure, Category = "Witness")
	FName GetCurrentEraID() const { return State.CurrentEraID; }

	/** What a combat system should be reading right now. */
	UFUNCTION(BlueprintPure, Category = "Witness")
	FEraCombatTuning GetActiveCombatTuning() const;

	// --- Missions -----------------------------------------------------------

	/** True once the source it reconstructs has been recovered and its era has come. */
	UFUNCTION(BlueprintCallable, Category = "Witness")
	bool IsMissionAvailable(FName MissionID) const;

	UFUNCTION(BlueprintCallable, Category = "Witness")
	void GetAvailableMissionIDs(TArray<FName>& OutMissionIDs) const;

	/**
	 * Pushes the clock and the party into the mission, sealing the frame era's
	 * world state unless the mission says otherwise. Broadcasts so the GameMode can
	 * travel and possess.
	 */
	UFUNCTION(BlueprintCallable, Category = "Witness")
	bool EnterMission(FName MissionID);

	/** Grants what the mission yields, then returns to the frame. */
	UFUNCTION(BlueprintCallable, Category = "Witness")
	bool CompleteMission();

	/** Returns to the frame with nothing gained. */
	UFUNCTION(BlueprintCallable, Category = "Witness")
	bool AbandonMission();

	UFUNCTION(BlueprintPure, Category = "Witness")
	bool IsMissionActive() const { return State.bIsMissionActive; }

	UFUNCTION(BlueprintPure, Category = "Witness")
	FName GetActiveMissionID() const { return State.ActiveMissionID; }

	UFUNCTION(BlueprintPure, Category = "Witness")
	bool HasCompletedMission(FName MissionID) const;

	UFUNCTION(BlueprintPure, Category = "Witness")
	FWitnessMissionState GetMissionState() const { return State; }

	// --- Save/load ----------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "Witness|Save")
	FWitnessMissionSaveData CaptureSaveData() const;

	UFUNCTION(BlueprintCallable, Category = "Witness|Save")
	void RestoreFromSaveData(const FWitnessMissionSaveData& SaveData);

	/** Test seam; in a running game all five resolve from the owning GameInstance. */
	void SetDependenciesForTesting(UCodexSubsystem* InCodex, UCampaignTimelineSubsystem* InTimeline,
		UReputationSubsystem* InReputation, UCampaignMapSubsystem* InMap, UDialogueSubsystem* InDialogue);

	// The year the crusader frame is set in. AD 1229: Frederick II's treaty has just
	// returned Jerusalem to Christian hands on terms the Templars opposed, and the
	// Khwarazmian sack of 1244 is the clock on anything left in the city.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Witness")
	int32 FrameYearAD = 1229;

	UPROPERTY(BlueprintAssignable, Category = "Witness")
	FOnEraChanged OnEraChanged;

	UPROPERTY(BlueprintAssignable, Category = "Witness")
	FOnWitnessMissionEntered OnMissionEntered;

	UPROPERTY(BlueprintAssignable, Category = "Witness")
	FOnWitnessMissionEnded OnMissionEnded;

private:
	UCodexSubsystem* GetCodex() const;
	UCampaignTimelineSubsystem* GetTimeline() const;
	UReputationSubsystem* GetReputation() const;
	UCampaignMapSubsystem* GetMap() const;
	UDialogueSubsystem* GetDialogue() const;

	void SetEra(EGameEra NewEra, FName EraID);

	/** Records where and when to come back to, and what the world looked like. */
	FFrameSnapshot CaptureFrame(bool bIncludeWorldState) const;

	void RestoreFrame(const FFrameSnapshot& Snapshot);

	/** Shared tail of CompleteMission and AbandonMission. */
	bool EndMission(EWitnessMissionOutcome Outcome);

	UPROPERTY()
	TMap<FName, FEraDefinitionRow> Eras;

	UPROPERTY()
	TMap<FName, FWitnessMissionRow> Missions;

	UPROPERTY()
	FWitnessMissionState State;

	UPROPERTY()
	EStructuralMode StructuralMode = EStructuralMode::DualEra;

	UPROPERTY()
	TObjectPtr<UCodexSubsystem> CodexOverride = nullptr;

	UPROPERTY()
	TObjectPtr<UCampaignTimelineSubsystem> TimelineOverride = nullptr;

	UPROPERTY()
	TObjectPtr<UReputationSubsystem> ReputationOverride = nullptr;

	UPROPERTY()
	TObjectPtr<UCampaignMapSubsystem> MapOverride = nullptr;

	UPROPERTY()
	TObjectPtr<UDialogueSubsystem> DialogueOverride = nullptr;
};
