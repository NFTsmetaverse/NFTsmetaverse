#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ChainGameFlowSubsystem.generated.h"

class UCampaignMapSubsystem;
class UCampaignTimelineSubsystem;
class UCodexSubsystem;
class UCombatEncounterSubsystem;
class UDebateSubsystem;
class UDialogueSubsystem;
class UReputationSubsystem;
class UWitnessMissionSubsystem;

DECLARE_LOG_CATEGORY_EXTERN(LogChainFlow, Log, All);

/** Which of the game's modes currently owns the screen and the input. */
UENUM(BlueprintType)
enum class EChainScreen : uint8
{
	/** No encounter running: the campaign map, travelling or standing somewhere. */
	Campaign,
	Dialogue,
	Debate,
	Combat,
	/** An overlay the player opens over any of the above. */
	Codex,
	MAX UMETA(Hidden)
};

/**
 * The game, as distinct from the systems.
 *
 * The nine subsystems each know a set of rules and nothing about the order things
 * happen in. That is the right split, but it leaves a hole: registering content and
 * knowing the rules of dialogue does not tell anyone that a campaign opens in
 * Jerusalem in 1229, that recovering Papias is what opens Rome, or that entering a
 * mission should also open its scene. Before this existed the only code that knew
 * the sequence was the vertical slice automation test, which meant the game was
 * playable by the test runner and by nothing else.
 *
 * This subsystem holds that sequence. It is deliberately thin -- it calls the
 * subsystems, it does not reimplement them -- and it is the single place the
 * HUD, the controller and the console commands all go through, so those three
 * cannot drift apart.
 *
 * The current screen is derived from subsystem state rather than stored, because a
 * stored copy is a copy that can be wrong: if a debate is running, the player is in
 * a debate, and there is no second opinion to reconcile.
 */
UCLASS()
class CHAINOFWITNESSES_API UChainGameFlowSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// --- Campaign -----------------------------------------------------------

	/**
	 * Opens a campaign in the frame era. Safe to call twice; the second call resets
	 * the player to the starting era and place without touching recovered evidence.
	 */
	UFUNCTION(BlueprintCallable, Category = "Chain|Flow")
	bool StartNewCampaign();

	/**
	 * Enters a Witness Mission and opens its authored scene in one step, which is
	 * what every caller actually wants: a mission whose conversation has not started
	 * is a player standing in an empty room in the wrong century.
	 */
	UFUNCTION(BlueprintCallable, Category = "Chain|Flow")
	bool OpenMission(FName MissionID);

	/**
	 * Grants the mission's unlock fragment first, then opens it. This is the
	 * developer shortcut into a scene -- it skips the frame-era recovery that would
	 * normally earn entry, so it is for testing a scene, not for playing the game.
	 */
	UFUNCTION(BlueprintCallable, Category = "Chain|Flow")
	bool ForceOpenMission(FName MissionID);

	UFUNCTION(BlueprintCallable, Category = "Chain|Flow")
	bool CompleteMission();

	// --- Dialogue -----------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "Chain|Flow")
	bool StartConversation(FName NpcID, FName RootNodeID);

	/**
	 * Selects a dialogue option by the index the player sees. Returns false and
	 * leaves the conversation untouched when the option is locked or out of range,
	 * so a mistyped key cannot advance the scene.
	 */
	UFUNCTION(BlueprintCallable, Category = "Chain|Flow")
	bool Say(int32 OptionIndex);

	// --- Travel -------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "Chain|Flow")
	bool TravelTo(FName DestinationLocationID, bool bRiskOutOfSeasonSailing = false);

	/** Moves the clock and the party together, which is the only correct way to move either. */
	UFUNCTION(BlueprintCallable, Category = "Chain|Flow")
	void AdvanceDays(int32 Days);

	// --- Encounters ---------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "Chain|Flow")
	bool StartDebate(FName OpponentID, FName AnchorEventID);

	UFUNCTION(BlueprintCallable, Category = "Chain|Flow")
	bool StartCombat(FName EncounterTypeID);

	// --- Screen -------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "Chain|Flow")
	EChainScreen GetScreen() const;

	UFUNCTION(BlueprintCallable, Category = "Chain|Flow")
	void ToggleCodex();

	UFUNCTION(BlueprintPure, Category = "Chain|Flow")
	bool IsCodexOpen() const { return bCodexOpen; }

	/**
	 * Closes whatever overlay or encounter is on top, innermost first. Returns false
	 * when there was nothing to back out of.
	 */
	UFUNCTION(BlueprintCallable, Category = "Chain|Flow")
	bool Back();

	// --- Reporting ----------------------------------------------------------

	/**
	 * One human-readable line per fact worth knowing right now. Shared by the HUD
	 * and the console so the two can never disagree about the state of the game.
	 */
	UFUNCTION(BlueprintCallable, Category = "Chain|Flow")
	void GetStatusLines(TArray<FString>& OutLines) const;

	/** The recovered evidence and what it scores, as display lines. */
	UFUNCTION(BlueprintCallable, Category = "Chain|Flow")
	void GetCodexLines(TArray<FString>& OutLines) const;

	/** Set by the last failed call, so the HUD can say why nothing happened. */
	UFUNCTION(BlueprintPure, Category = "Chain|Flow")
	FString GetLastMessage() const { return LastMessage; }

	UCodexSubsystem* GetCodex() const;
	UDialogueSubsystem* GetDialogue() const;
	UWitnessMissionSubsystem* GetWitness() const;
	UCampaignMapSubsystem* GetMap() const;
	UCampaignTimelineSubsystem* GetTimeline() const;
	UDebateSubsystem* GetDebate() const;
	UCombatEncounterSubsystem* GetCombat() const;
	UReputationSubsystem* GetReputation() const;

	/** Test seam; in a running game all eight resolve from the owning GameInstance. */
	void SetDependenciesForTesting(UCodexSubsystem* InCodex, UDialogueSubsystem* InDialogue,
		UWitnessMissionSubsystem* InWitness, UCampaignMapSubsystem* InMap,
		UCampaignTimelineSubsystem* InTimeline, UDebateSubsystem* InDebate,
		UCombatEncounterSubsystem* InCombat, UReputationSubsystem* InReputation);

private:
	void Report(const FString& Message);

	/** Opens the scene a mission names, if it names one. */
	bool StartMissionOpeningScene(FName MissionID);

	UPROPERTY()
	bool bCodexOpen = false;

	UPROPERTY()
	FString LastMessage;

	UPROPERTY()
	TObjectPtr<UCodexSubsystem> CodexOverride = nullptr;

	UPROPERTY()
	TObjectPtr<UDialogueSubsystem> DialogueOverride = nullptr;

	UPROPERTY()
	TObjectPtr<UWitnessMissionSubsystem> WitnessOverride = nullptr;

	UPROPERTY()
	TObjectPtr<UCampaignMapSubsystem> MapOverride = nullptr;

	UPROPERTY()
	TObjectPtr<UCampaignTimelineSubsystem> TimelineOverride = nullptr;

	UPROPERTY()
	TObjectPtr<UDebateSubsystem> DebateOverride = nullptr;

	UPROPERTY()
	TObjectPtr<UCombatEncounterSubsystem> CombatOverride = nullptr;

	UPROPERTY()
	TObjectPtr<UReputationSubsystem> ReputationOverride = nullptr;
};
