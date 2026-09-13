#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Dialogue/DialogueTypes.h"
#include "DialogueSubsystem.generated.h"

class UDataTable;
class UCodexSubsystem;
class UCampaignTimelineSubsystem;

DECLARE_LOG_CATEGORY_EXTERN(LogDialogue, Log, All);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnConversationStarted, FName, NpcID, FName, RootNodeID);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDialogueNodeEntered, FName, NodeID);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnConversationEnded, FName, NpcID);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnNpcTrustChanged, FName, NpcID, float, NewTrust);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnNpcDisclosureChanged, FName, NpcID, EDisclosureLevel, NewLevel);

/**
 * Dialogue with disclosure-state gating (Task 10.4).
 *
 * Section 5 asks for trees where "what an NPC will say depends on what the player
 * already credibly knows, who vouched for him, and whether the conversation is
 * safe". Those are the three axes here, plus per-NPC trust and the campaign year:
 *
 *   - Credibly knows -> the Codex (Task 2). Holding fragments is the weak form;
 *     having a chain that scores above a threshold is the strong one.
 *   - Vouched for     -> vouches recorded between NPCs, the mechanic the early
 *     Church actually ran on (Barnabas speaking for Saul, letters of commendation).
 *   - Safe            -> EConversationSafety, set by the caller per meeting.
 *   - Era             -> the campaign timeline (Task 1).
 *
 * Locked options are reported with their gate rather than silently dropped, so the
 * player learns what the world runs on. Conditions are re-checked on selection:
 * the view is a presentation, never the authority.
 */
UCLASS()
class CHAINOFWITNESSES_API UDialogueSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// --- Content ------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "Dialogue|Content")
	bool RegisterNode(const FDialogueNodeRow& Node);

	/** Bulk wrapper over RegisterNode for an FDialogueNodeRow table. Returns nodes added. */
	UFUNCTION(BlueprintCallable, Category = "Dialogue|Content")
	int32 RegisterDialogueTable(const UDataTable* DialogueTable);

	UFUNCTION(BlueprintCallable, Category = "Dialogue|Content")
	bool GetNode(FName NodeID, FDialogueNodeRow& OutNode) const;

	/**
	 * Reports authored references that go nowhere: options pointing at missing
	 * nodes, fallbacks pointing at missing nodes, and effects granting fragments the
	 * Codex has never heard of. Content check for the editor and tests.
	 */
	UFUNCTION(BlueprintCallable, Category = "Dialogue|Content")
	void ValidateDialogue(TArray<FString>& OutProblems) const;

	// --- Conversation flow --------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "Dialogue")
	bool StartConversation(FName NpcID, FName RootNodeID, EConversationSafety Safety);

	UFUNCTION(BlueprintPure, Category = "Dialogue")
	bool IsConversationActive() const { return bConversationActive; }

	UFUNCTION(BlueprintCallable, Category = "Dialogue")
	bool GetCurrentNodeView(FDialogueNodeView& OutView) const;

	/** Index is into the node's authored Options array, as reported by the view. */
	UFUNCTION(BlueprintCallable, Category = "Dialogue")
	bool SelectOption(int32 OptionIndex);

	UFUNCTION(BlueprintCallable, Category = "Dialogue")
	void EndConversation();

	UFUNCTION(BlueprintPure, Category = "Dialogue")
	FName GetCurrentNpcID() const { return CurrentNpcID; }

	UFUNCTION(BlueprintPure, Category = "Dialogue")
	EConversationSafety GetCurrentSafety() const { return CurrentSafety; }

	// --- Standing -----------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "Dialogue|Standing")
	float GetTrust(FName NpcID) const;

	UFUNCTION(BlueprintCallable, Category = "Dialogue|Standing")
	void ModifyTrust(FName NpcID, float Delta);

	UFUNCTION(BlueprintPure, Category = "Dialogue|Standing")
	EDisclosureLevel GetDisclosure(FName NpcID) const;

	/** Raises only; disclosure does not un-happen. */
	UFUNCTION(BlueprintCallable, Category = "Dialogue|Standing")
	void RaiseDisclosure(FName NpcID, EDisclosureLevel NewLevel);

	/** TargetNpcID of NAME_None records a general vouch, good with anyone. */
	UFUNCTION(BlueprintCallable, Category = "Dialogue|Standing")
	void RecordVouch(FName VoucherNpcID, FName TargetNpcID);

	UFUNCTION(BlueprintPure, Category = "Dialogue|Standing")
	bool HasVouch(FName VoucherNpcID, FName TargetNpcID) const;

	// --- Gating -------------------------------------------------------------

	/** Returns true if every condition passes; otherwise OutGate names the first failure. */
	UFUNCTION(BlueprintCallable, Category = "Dialogue|Gating")
	bool EvaluateConditions(const FDialogueConditionSet& Conditions, FName NpcID,
		EConversationSafety Safety, EDialogueGate& OutGate) const;

	// --- Save/load ----------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "Dialogue|Save")
	FDialogueSaveData CaptureSaveData() const;

	/** Replaces all per-NPC standing. Registered dialogue content is left alone. */
	UFUNCTION(BlueprintCallable, Category = "Dialogue|Save")
	void RestoreFromSaveData(const FDialogueSaveData& SaveData);

	/**
	 * Test seam. In a running game both dependencies resolve from the owning
	 * GameInstance; a standalone test has no GameInstance and injects them here.
	 */
	void SetDependenciesForTesting(UCodexSubsystem* InCodex, UCampaignTimelineSubsystem* InTimeline);

	UPROPERTY(BlueprintAssignable, Category = "Dialogue")
	FOnConversationStarted OnConversationStarted;

	UPROPERTY(BlueprintAssignable, Category = "Dialogue")
	FOnDialogueNodeEntered OnNodeEntered;

	UPROPERTY(BlueprintAssignable, Category = "Dialogue")
	FOnConversationEnded OnConversationEnded;

	UPROPERTY(BlueprintAssignable, Category = "Dialogue")
	FOnNpcTrustChanged OnTrustChanged;

	UPROPERTY(BlueprintAssignable, Category = "Dialogue")
	FOnNpcDisclosureChanged OnDisclosureChanged;

private:
	UCodexSubsystem* GetCodex() const;
	UCampaignTimelineSubsystem* GetTimeline() const;

	FNpcDialogueState& FindOrAddNpcState(FName NpcID);
	const FNpcDialogueState* FindNpcState(FName NpcID) const;

	/** Follows entry-condition fallbacks, applies entry effects, and broadcasts. */
	bool EnterNode(FName NodeID);

	void ApplyEffects(const FDialogueEffects& Effects, FName SpeakerNpcID);

	UPROPERTY()
	TMap<FName, FDialogueNodeRow> Nodes;

	UPROPERTY()
	TArray<FNpcDialogueState> NpcStates;

	UPROPERTY()
	TSet<FName> GeneralVouchers;

	UPROPERTY()
	bool bConversationActive = false;

	UPROPERTY()
	FName CurrentNpcID;

	UPROPERTY()
	FName CurrentNodeID;

	UPROPERTY()
	EConversationSafety CurrentSafety = EConversationSafety::Public;

	UPROPERTY()
	TObjectPtr<UCodexSubsystem> CodexOverride = nullptr;

	UPROPERTY()
	TObjectPtr<UCampaignTimelineSubsystem> TimelineOverride = nullptr;
};
