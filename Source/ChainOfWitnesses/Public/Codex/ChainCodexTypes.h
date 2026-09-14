#pragma once

#include "CoreMinimal.h"
#include "Codex/TestimonyFragment.h"
#include "ChainCodexTypes.generated.h"

/** One link's worth of slotted evidence within a chain. */
USTRUCT(BlueprintType)
struct CHAINOFWITNESSES_API FTransmissionLinkSlot
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "Codex")
	ETransmissionLink Link = ETransmissionLink::Event;

	// More than one fragment per link is the point: two independent written sources
	// in the same link is the multiple-attestation argument made mechanical.
	UPROPERTY(BlueprintReadOnly, Category = "Codex")
	TArray<FName> FragmentIDs;
};

/**
 * A chain of custody the player has assembled for one campaign event.
 *
 * AnchorYearAD is captured when the chain is created rather than looked up at
 * score time, so scoring stays a pure function of the chain plus the fragment
 * registry -- testable without a world, and stable across save/load even if the
 * timeline table is re-tuned mid-campaign.
 */
USTRUCT(BlueprintType)
struct CHAINOFWITNESSES_API FTransmissionChain
{
	GENERATED_BODY()

public:
	// Row name in DT_CampaignEvents -- the event whose transmission this chain traces.
	UPROPERTY(BlueprintReadOnly, Category = "Codex")
	FName AnchorEventID;

	// Earliest defensible year of the anchor event, from FCampaignEventRow.
	// INDEX_NONE if it could not be resolved.
	UPROPERTY(BlueprintReadOnly, Category = "Codex")
	int32 AnchorYearAD = INDEX_NONE;

	// Always NumTransmissionLinks entries, in transmission order.
	UPROPERTY(BlueprintReadOnly, Category = "Codex")
	TArray<FTransmissionLinkSlot> Slots;

	/** Fills Slots with one empty slot per link, in order. */
	void InitialiseSlots();

	FTransmissionLinkSlot* FindSlot(ETransmissionLink Link);
	const FTransmissionLinkSlot* FindSlot(ETransmissionLink Link) const;

	/** True if the fragment is slotted anywhere in this chain. */
	bool ContainsFragment(FName FragmentID) const;
};

/** Per-link result, so a defeat can name the link that failed. */
USTRUCT(BlueprintType)
struct CHAINOFWITNESSES_API FChainLinkScore
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "Codex")
	ETransmissionLink Link = ETransmissionLink::Event;

	UPROPERTY(BlueprintReadOnly, Category = "Codex")
	bool bIsFilled = false;

	UPROPERTY(BlueprintReadOnly, Category = "Codex")
	int32 FragmentCount = 0;

	// Best single fragment in this link, 0..1, after proximity, tier fit, and
	// contested discounts.
	UPROPERTY(BlueprintReadOnly, Category = "Codex")
	float BestFragmentScore = 0.f;

	// Link total, 0..1, including the bonus for additional independent fragments.
	UPROPERTY(BlueprintReadOnly, Category = "Codex")
	float LinkScore = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Codex")
	TArray<FName> UnansweredChallenges;
};

/**
 * The full, legible result of scoring a chain. Section 8: "If the chain is thin,
 * the player loses, and the loss is legible -- he can see precisely which link
 * failed." Everything needed for that screen is here; nothing is a bare number.
 */
USTRUCT(BlueprintType)
struct CHAINOFWITNESSES_API FChainScoreBreakdown
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "Codex")
	FName AnchorEventID;

	UPROPERTY(BlueprintReadOnly, Category = "Codex")
	bool bChainExists = false;

	// 0..100. The campaign's real progression meter (Pillar 1).
	UPROPERTY(BlueprintReadOnly, Category = "Codex")
	float AttestationStrength = 0.f;

	// True when every link holds at least one fragment.
	UPROPERTY(BlueprintReadOnly, Category = "Codex")
	bool bIsComplete = false;

	// Lowest-scoring link; ties resolve to the earliest link in transmission order,
	// since that is the one an opponent attacks first.
	UPROPERTY(BlueprintReadOnly, Category = "Codex")
	ETransmissionLink WeakestLink = ETransmissionLink::Event;

	UPROPERTY(BlueprintReadOnly, Category = "Codex")
	TArray<FChainLinkScore> LinkScores;

	// Pairs of slotted fragments where one names the other in CorroboratedBy.
	UPROPERTY(BlueprintReadOnly, Category = "Codex")
	int32 IndependentCorroborationCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Codex")
	bool bHasHostileConfirmation = false;

	// Union of every link's unanswered challenges, deduplicated.
	UPROPERTY(BlueprintReadOnly, Category = "Codex")
	TArray<FName> UnansweredChallenges;

	// Slotted IDs with no registered definition -- a content error, surfaced rather
	// than silently scored as zero.
	UPROPERTY(BlueprintReadOnly, Category = "Codex")
	TArray<FName> UnknownFragmentIDs;
};

/**
 * Designer-tunable scoring weights. Section 8 fixes what raises and lowers
 * Attestation Strength -- independent corroboration, source proximity, hostile
 * confirmation, unanswered challenges -- but not the numbers; these are meant to
 * be iterated on without a recompile.
 */
USTRUCT(BlueprintType)
struct CHAINOFWITNESSES_API FCodexScoringRules
{
	GENERATED_BODY()

public:
	// Attestation within this many years of the event scores full proximity: the
	// window in which the people who were there are still alive to contradict it.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scoring", meta = (ClampMin = "0"))
	int32 LivingMemoryYears = 30;

	// Years beyond the living-memory window over which proximity decays to zero.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scoring", meta = (ClampMin = "1"))
	int32 AttestationFalloffYears = 120;

	// Added per additional fragment in the same link, beyond the first.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scoring", meta = (ClampMin = "0.0"))
	float IndependentFragmentBonus = 0.1f;

	// Applied when a fragment's tier does not belong in the link it was slotted into.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scoring", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MismatchedTierMultiplier = 0.5f;

	// Applied to fragments whose own authenticity is disputed.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scoring", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ContestedFragmentMultiplier = 0.75f;

	// Points per corroborating pair present in the chain.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scoring", meta = (ClampMin = "0.0"))
	float CorroborationPairBonus = 3.f;

	// Points for any hostile-tier fragment in the chain -- a source with no motive
	// to help is worth more than another friendly one.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scoring", meta = (ClampMin = "0.0"))
	float HostileConfirmationBonus = 8.f;

	// Points lost per distinct unanswered challenge.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scoring", meta = (ClampMin = "0.0"))
	float UnansweredChallengePenalty = 10.f;
};

/**
 * Serializable Codex state. Section 9 requires the save system to carry Codex
 * state alongside NPC memory, faction relations, and era flags; this is the Codex's
 * payload, which the game-wide USaveGame composes once that exists (Task 8+).
 */
USTRUCT(BlueprintType)
struct CHAINOFWITNESSES_API FCodexSaveData
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "Codex")
	TArray<FName> RecoveredFragmentIDs;

	UPROPERTY(BlueprintReadOnly, Category = "Codex")
	TArray<FTransmissionChain> Chains;
};
