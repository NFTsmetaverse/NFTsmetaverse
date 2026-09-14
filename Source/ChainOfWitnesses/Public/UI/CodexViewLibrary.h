#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "UI/ChainCodexViewTypes.h"
#include "CodexViewLibrary.generated.h"

class UCodexSubsystem;
class UCampaignTimelineSubsystem;

/**
 * Turns Codex state into something a screen can draw (Task 10.3).
 *
 * Kept as a function library rather than folded into the widgets because the
 * projection is the part worth testing: it is pure, it has no widget in it, and it
 * is the same whether the chain-building screen, the inspector, or a debrief after
 * a lost debate is asking. The widgets below own refresh timing and nothing else.
 *
 * Nothing here computes a score. Scoring belongs to UCodexSubsystem and is read
 * from FChainScoreBreakdown -- if the UI recomputed anything, the number on the
 * screen and the number in the argument could drift apart.
 */
UCLASS()
class CHAINOFWITNESSES_API UCodexViewLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * One fragment as the inspector shows it. Returns a view with bIsKnownFragment
	 * false if the ID is not registered, rather than an empty row.
	 */
	UFUNCTION(BlueprintCallable, Category = "Codex UI")
	static FCodexFragmentView MakeFragmentView(const UCodexSubsystem* Codex, FName FragmentID);

	/**
	 * The whole chain-building screen for one anchor event. The timeline is optional
	 * and only supplies the anchor's title and dates.
	 */
	UFUNCTION(BlueprintCallable, Category = "Codex UI")
	static FCodexChainView MakeChainView(const UCodexSubsystem* Codex,
		const UCampaignTimelineSubsystem* Timeline, FName AnchorEventID);

	/**
	 * Recovered fragments not already in this chain, for the "what can go here"
	 * list. bOnlyTierAppropriate filters to what actually belongs in the link;
	 * passing false returns everything, because slotting a mismatch is allowed and
	 * is how the player learns the shape of a chain.
	 */
	UFUNCTION(BlueprintCallable, Category = "Codex UI")
	static void GetSlottableFragments(const UCodexSubsystem* Codex, FName AnchorEventID,
		ETransmissionLink Link, bool bOnlyTierAppropriate, TArray<FCodexFragmentView>& OutFragments);

	/** Every objection against the chain, answered and unanswered alike. */
	UFUNCTION(BlueprintCallable, Category = "Codex UI")
	static void GetChainChallenges(const UCodexSubsystem* Codex, FName AnchorEventID,
		TArray<FCodexChallengeView>& OutChallenges);

	// --- Display helpers ----------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "Codex UI")
	static FText GetLinkName(ETransmissionLink Link);

	UFUNCTION(BlueprintPure, Category = "Codex UI")
	static FText GetLinkDescription(ETransmissionLink Link);

	UFUNCTION(BlueprintPure, Category = "Codex UI")
	static FText GetTierName(EWitnessTier Tier);

	UFUNCTION(BlueprintPure, Category = "Codex UI")
	static FText GetBandName(EAttestationBand Band);

	/** For a 0..1 link score. */
	UFUNCTION(BlueprintPure, Category = "Codex UI")
	static EAttestationBand BandForLinkScore(float Score);

	/** For a 0..100 chain strength. */
	UFUNCTION(BlueprintPure, Category = "Codex UI")
	static EAttestationBand BandForAttestationStrength(float Strength);

	/** "AD 110". Years are not thousands-separated. */
	UFUNCTION(BlueprintPure, Category = "Codex UI")
	static FText FormatYearAD(int32 YearAD);

	/** "AD 55-70", or "AD 65" where the two are equal. */
	UFUNCTION(BlueprintPure, Category = "Codex UI")
	static FText FormatYearRangeAD(int32 EarliestAD, int32 LatestAD);
};
