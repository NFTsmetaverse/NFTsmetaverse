#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Debate/ChainDebateTypes.h"
#include "Codex/ChainCodexTypes.h"
#include "DebateSubsystem.generated.h"

class UDataTable;
class UCodexSubsystem;
class UCampaignTimelineSubsystem;
class UReputationSubsystem;
class UCampaignMapSubsystem;

DECLARE_LOG_CATEGORY_EXTERN(LogDebate, Log, All);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDebateStarted, FName, OpponentID, FName, AnchorEventID);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnObjectionRaised, FName, ObjectionID);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDebateEnded, EDebateOutcome, Outcome);

/**
 * Argumentation encounters (Task 10.7).
 *
 * Section 8 asks for structured debate that uses the player's assembled chains as
 * ammunition, and for a defeat he can read. Both come from one decision: every
 * objection attacks a named link of the transmission chain. The link score the
 * player built in the Codex is the defence he has here, so the six-link breakdown
 * from Task 2 is literally the surface the opponent probes -- and the weakest link
 * is what loses him arguments.
 *
 * The opponent is not a random-objection dispenser. Each round he leads with
 * whatever the chain is worst at, which is what a prepared opponent does.
 *
 * Three ways to meet an objection: rest on the chain (worth exactly that link's
 * score), cite something that answers it directly (a full answer, once per
 * citation per debate), or concede the point. Conceding is sometimes correct --
 * the longer ending of Mark has no evidential answer, and pretending otherwise is
 * how a case falls apart later.
 */
UCLASS()
class CHAINOFWITNESSES_API UDebateSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// --- Content ------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "Debate|Content")
	bool RegisterOpponent(const FDebateOpponentRow& Opponent);

	UFUNCTION(BlueprintCallable, Category = "Debate|Content")
	bool RegisterObjection(const FDebateObjectionRow& Objection);

	// Bulk wrappers over the two above. Each returns the number of rows added.
	UFUNCTION(BlueprintCallable, Category = "Debate|Content")
	int32 RegisterOpponentTable(const UDataTable* OpponentTable);

	UFUNCTION(BlueprintCallable, Category = "Debate|Content")
	int32 RegisterObjectionTable(const UDataTable* ObjectionTable);

	/** Reports opponents citing unregistered objections, and objections answered by unknown fragments. */
	UFUNCTION(BlueprintCallable, Category = "Debate|Content")
	void ValidateDebateContent(TArray<FString>& OutProblems) const;

	// --- Running a debate ---------------------------------------------------

	/**
	 * Opens an argument about one event's transmission. Fails if the opponent is
	 * unregistered, outside his era, or has nothing era-appropriate to say.
	 */
	UFUNCTION(BlueprintCallable, Category = "Debate")
	bool StartDebate(FName OpponentID, FName AnchorEventID);

	UFUNCTION(BlueprintPure, Category = "Debate")
	bool IsDebateActive() const { return State.bIsActive; }

	UFUNCTION(BlueprintCallable, Category = "Debate")
	bool GetCurrentObjection(FDebateObjectionView& OutView) const;

	/** Rest on the chain as assembled. Worth exactly the targeted link's score. */
	UFUNCTION(BlueprintCallable, Category = "Debate")
	bool RespondWithChain();

	/** Cite something that answers this objection directly. Each citation carries once. */
	UFUNCTION(BlueprintCallable, Category = "Debate")
	bool RespondWithFragment(FName FragmentID);

	/** Give the point away rather than defend what cannot be defended. */
	UFUNCTION(BlueprintCallable, Category = "Debate")
	bool ConcedePoint();

	/** Walk out. Counts as a loss for reputation, without the transcript of one. */
	UFUNCTION(BlueprintCallable, Category = "Debate")
	void AbandonDebate();

	UFUNCTION(BlueprintPure, Category = "Debate")
	FDebateState GetDebateState() const { return State; }

	/** Valid once the debate has ended. */
	UFUNCTION(BlueprintPure, Category = "Debate")
	FDebateOutcomeReport GetOutcomeReport() const { return Report; }

	/** Test seam; in a running game all four resolve from the owning GameInstance. */
	void SetDependenciesForTesting(UCodexSubsystem* InCodex, UCampaignTimelineSubsystem* InTimeline,
		UReputationSubsystem* InReputation, UCampaignMapSubsystem* InMap);

	// Multiplies what the chain is worth on a link carrying a challenge the player
	// has never answered. An opponent who knows the objection exists will use it.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debate", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float UnansweredChallengeDefencePenalty = 0.5f;

	// What a direct citation is worth. Above the 0..1 of a link score, because
	// naming the source that answers the objection is stronger than gesturing at
	// the shape of the argument.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debate", meta = (ClampMin = "0.0"))
	float DirectAnswerDefence = 1.25f;

	// Added when the cited fragment is not merely held but placed in the chain
	// under argument.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debate", meta = (ClampMin = "0.0"))
	float SlottedFragmentBonus = 0.25f;

	UPROPERTY(BlueprintAssignable, Category = "Debate")
	FOnDebateStarted OnDebateStarted;

	UPROPERTY(BlueprintAssignable, Category = "Debate")
	FOnObjectionRaised OnObjectionRaised;

	UPROPERTY(BlueprintAssignable, Category = "Debate")
	FOnDebateEnded OnDebateEnded;

private:
	UCodexSubsystem* GetCodex() const;
	UCampaignTimelineSubsystem* GetTimeline() const;
	UReputationSubsystem* GetReputation() const;
	UCampaignMapSubsystem* GetMap() const;

	int32 GetCurrentYearAD() const;

	bool IsObjectionEligible(const FDebateObjectionRow& Objection) const;

	/** The link score for one link of the chain under argument, after challenge penalties. */
	float GetLinkDefence(ETransmissionLink Link, bool& bOutHasUnansweredChallenge) const;

	/**
	 * Picks the next objection: the eligible, unused one aimed at whichever link the
	 * chain is weakest at. Deterministic, because "he goes for your weak point" is
	 * the characterisation, not a die roll.
	 */
	FName SelectNextObjection() const;

	/** Shared tail of the three response paths. */
	bool ResolveRound(EDebateResponseKind ResponseKind, FName FragmentID, float Defence);

	void EndDebate(EDebateOutcome Outcome);

	/** Fills the report's teaching half: the landed objections and what would have met them. */
	void BuildOutcomeReport(EDebateOutcome Outcome);

	UPROPERTY()
	TMap<FName, FDebateOpponentRow> Opponents;

	UPROPERTY()
	TMap<FName, FDebateObjectionRow> Objections;

	UPROPERTY()
	FDebateState State;

	UPROPERTY()
	FDebateOutcomeReport Report;

	// Captured at the start of the debate, so every round is judged against the case
	// the player walked in with rather than one that shifts underneath him.
	UPROPERTY()
	FChainScoreBreakdown ChainAtStart;

	UPROPERTY()
	TObjectPtr<UCodexSubsystem> CodexOverride = nullptr;

	UPROPERTY()
	TObjectPtr<UCampaignTimelineSubsystem> TimelineOverride = nullptr;

	UPROPERTY()
	TObjectPtr<UReputationSubsystem> ReputationOverride = nullptr;

	UPROPERTY()
	TObjectPtr<UCampaignMapSubsystem> MapOverride = nullptr;
};
