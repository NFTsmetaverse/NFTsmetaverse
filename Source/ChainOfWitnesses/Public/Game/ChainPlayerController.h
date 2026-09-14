#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ChainPlayerController.generated.h"

class UChainGameFlowSubsystem;

/**
 * Input and the console shell.
 *
 * Two surfaces onto the same flow subsystem. The keys are how the game is played --
 * number keys pick what to say, C opens the codex, Escape backs out. The Chain*
 * console commands are how it is driven and tested: they reach parts of the game
 * that have no key yet, and they take arguments, which keys cannot.
 *
 * Both go through UChainGameFlowSubsystem rather than touching the nine subsystems
 * directly, so there is one definition of what an action means regardless of which
 * surface asked for it.
 *
 * Input is bound with BindKey against literal keys rather than Enhanced Input
 * actions. Enhanced Input would need InputAction and InputMappingContext assets,
 * which only exist once someone has made them in the editor -- and the point of
 * this layer is that the game is playable the moment it compiles.
 */
UCLASS()
class CHAINOFWITNESSES_API AChainPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AChainPlayerController();

	// --- Console: campaign --------------------------------------------------

	/** Lists every Chain command with a one-line description. */
	UFUNCTION(Exec)
	void ChainHelp();

	UFUNCTION(Exec)
	void ChainStatus();

	UFUNCTION(Exec)
	void ChainNew();

	UFUNCTION(Exec)
	void ChainCodex();

	/** Every mission the content declares, and whether it is open. */
	UFUNCTION(Exec)
	void ChainMissions();

	UFUNCTION(Exec)
	void ChainOpen(const FString& MissionID);

	/** Straight into the Rome AD 65 slice, granting the source that opens it. */
	UFUNCTION(Exec)
	void ChainSlice();

	UFUNCTION(Exec)
	void ChainComplete();

	// --- Console: dialogue --------------------------------------------------

	UFUNCTION(Exec)
	void ChainTalk(const FString& NpcID, const FString& RootNodeID);

	/** Takes the number shown on screen, which is one-based. */
	UFUNCTION(Exec)
	void ChainSay(int32 DisplayNumber);

	// --- Console: travel ----------------------------------------------------

	UFUNCTION(Exec)
	void ChainTravel(const FString& LocationID);

	UFUNCTION(Exec)
	void ChainWait(int32 Days);

	// --- Console: evidence --------------------------------------------------

	UFUNCTION(Exec)
	void ChainRecover(const FString& FragmentID);

	// --- Console: debate ----------------------------------------------------

	UFUNCTION(Exec)
	void ChainDebate(const FString& OpponentID, const FString& AnchorEventID);

	/** Answers the standing objection with the whole chain. */
	UFUNCTION(Exec)
	void ChainAnswer();

	/** Answers it with one named fragment instead. */
	UFUNCTION(Exec)
	void ChainAnswerWith(const FString& FragmentID);

	UFUNCTION(Exec)
	void ChainConcede();

	// --- Console: combat ----------------------------------------------------

	UFUNCTION(Exec)
	void ChainFight(const FString& EncounterTypeID);

	/** 0 undrawn, 1 drawn, 2 engaged, 3 withdrawing. */
	UFUNCTION(Exec)
	void ChainPosture(int32 Posture);

	UFUNCTION(Exec)
	void ChainStrike(int32 CombatantIndex);

	UFUNCTION(Exec)
	void ChainEscape();

	UFUNCTION(Exec)
	void ChainTalkDown();

protected:
	virtual void SetupInputComponent() override;
	virtual void BeginPlay() override;

private:
	UChainGameFlowSubsystem* Flow() const;

	/** Echoes to the log and to the on-screen message line in one step. */
	void Echo(const FString& Line);
	void EchoLines(const TArray<FString>& Lines);

	/** Shared by the number keys and by ChainSay: one-based in, zero-based out. */
	void SelectByDisplayNumber(int32 DisplayNumber);

	void OnOption1();
	void OnOption2();
	void OnOption3();
	void OnOption4();
	void OnOption5();
	void OnOption6();
	void OnOption7();
	void OnOption8();
	void OnOption9();
	void OnToggleCodex();
	void OnBack();
	void OnStatus();
	void OnHelp();
};
