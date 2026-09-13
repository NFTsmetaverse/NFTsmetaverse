#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/CodexViewTypes.h"
#include "CodexWidgets.generated.h"

class UCodexSubsystem;
class UCampaignTimelineSubsystem;

/**
 * Shared base for the Codex screens (Task 10.3).
 *
 * These are deliberately thin. They resolve the subsystems, keep themselves current
 * when the Codex changes, and hand a finished view to Blueprint -- nothing else.
 * There are no BindWidget declarations, because that would force the designer to
 * name things this header's way; the visual tree is entirely his, and the contract
 * between us is the view struct and one event.
 *
 * The refresh binding is the real value here: keeping a chain screen in step with
 * a Codex that changes underneath it is tedious to wire in Blueprint and easy to
 * get subtly wrong.
 */
UCLASS(Abstract)
class CHAINOFWITNESSES_API UCodexWidgetBase : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/** Rebuilds this screen's view from current Codex state. */
	UFUNCTION(BlueprintCallable, Category = "Codex UI")
	virtual void Refresh();

protected:
	UCodexSubsystem* GetCodex() const;
	UCampaignTimelineSubsystem* GetTimeline() const;

	UFUNCTION()
	void HandleChainChanged(FName AnchorEventID);

	UFUNCTION()
	void HandleFragmentRecovered(FName FragmentID);
};

/**
 * Section 8's chain-building screen: the six links, what is in them, what may go in
 * them, and what the whole thing is currently worth.
 */
UCLASS(Abstract)
class CHAINOFWITNESSES_API UCodexChainScreenBase : public UCodexWidgetBase
{
	GENERATED_BODY()

public:
	/** Which event's transmission this screen is showing. */
	UFUNCTION(BlueprintCallable, Category = "Codex UI")
	void SetAnchorEvent(FName InAnchorEventID);

	UFUNCTION(BlueprintPure, Category = "Codex UI")
	FName GetAnchorEvent() const { return AnchorEventID; }

	virtual void Refresh() override;

	/** Candidates for a link, closest attestation first. */
	UFUNCTION(BlueprintCallable, Category = "Codex UI")
	void GetSlottableFragments(ETransmissionLink Link, bool bOnlyTierAppropriate,
		TArray<FCodexFragmentView>& OutFragments) const;

	// Slotting and unslotting go through here rather than straight to the subsystem
	// so the screen is never showing a chain it has already changed.
	UFUNCTION(BlueprintCallable, Category = "Codex UI")
	bool SlotFragment(ETransmissionLink Link, FName FragmentID);

	UFUNCTION(BlueprintCallable, Category = "Codex UI")
	bool UnslotFragment(FName FragmentID);

	/** Draw here. Called on Refresh, and whenever the Codex changes underneath. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Codex UI")
	void OnChainViewRefreshed(const FCodexChainView& View);

	UPROPERTY(BlueprintReadOnly, Category = "Codex UI")
	FCodexChainView ChainView;

private:
	UPROPERTY()
	FName AnchorEventID;
};

/**
 * The fragment inspector. Its job is the citation: the player is shown exactly
 * where a thing comes from, and where the scholarship disagrees about it, rather
 * than a summary that asks to be trusted.
 */
UCLASS(Abstract)
class CHAINOFWITNESSES_API UFragmentInspectorBase : public UCodexWidgetBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Codex UI")
	void SetFragment(FName InFragmentID);

	UFUNCTION(BlueprintPure, Category = "Codex UI")
	FName GetFragment() const { return FragmentID; }

	virtual void Refresh() override;

	UFUNCTION(BlueprintImplementableEvent, Category = "Codex UI")
	void OnFragmentViewRefreshed(const FCodexFragmentView& View);

	UPROPERTY(BlueprintReadOnly, Category = "Codex UI")
	FCodexFragmentView FragmentView;

	// What supports this fragment and what cuts against it, so the inspector can be
	// navigated outward rather than read in isolation.
	UPROPERTY(BlueprintReadOnly, Category = "Codex UI")
	TArray<FCodexFragmentView> Corroborations;

	UPROPERTY(BlueprintReadOnly, Category = "Codex UI")
	TArray<FCodexFragmentView> Challenges;

private:
	UPROPERTY()
	FName FragmentID;
};

/**
 * The challenge and response view: every objection standing against a chain, what
 * would meet it, and -- honestly -- which ones nothing in the record answers.
 */
UCLASS(Abstract)
class CHAINOFWITNESSES_API UChallengeResponseViewBase : public UCodexWidgetBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Codex UI")
	void SetAnchorEvent(FName InAnchorEventID);

	UFUNCTION(BlueprintPure, Category = "Codex UI")
	FName GetAnchorEvent() const { return AnchorEventID; }

	virtual void Refresh() override;

	UFUNCTION(BlueprintImplementableEvent, Category = "Codex UI")
	void OnChallengesRefreshed();

	UPROPERTY(BlueprintReadOnly, Category = "Codex UI")
	TArray<FCodexChallengeView> Challenges;

	UPROPERTY(BlueprintReadOnly, Category = "Codex UI")
	int32 UnansweredCount = 0;

	// Objections with no evidential reply anywhere in the corpus. Shown apart from
	// the rest, because "you have not found the answer yet" and "there is no answer"
	// are different things to tell a player.
	UPROPERTY(BlueprintReadOnly, Category = "Codex UI")
	int32 UnanswerableCount = 0;

private:
	UPROPERTY()
	FName AnchorEventID;
};
