#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/DataTable.h"
#include "TestimonyFragment.generated.h"

/**
 * Master Prompt Section 8's witness tiers. Tier is not a quality ranking -- a
 * hostile source and an eyewitness source are strong in different ways -- it
 * describes what kind of evidence the fragment is, which determines where it
 * belongs in a transmission chain.
 */
UENUM(BlueprintType)
enum class EWitnessTier : uint8
{
	// Someone positioned to have seen the event.
	Eyewitness			UMETA(DisplayName = "Eyewitness"),
	// Someone who worked directly from an eyewitness (Mark from Peter, Luke from
	// the sources he says he investigated).
	Companion			UMETA(DisplayName = "Companion"),
	// Church fathers testifying to authorship and transmission.
	Patristic			UMETA(DisplayName = "Patristic"),
	// Physical copies of the text.
	Manuscript			UMETA(DisplayName = "Manuscript"),
	// Non-Christian sources with no motive to corroborate.
	Hostile				UMETA(DisplayName = "Hostile Witness"),
	// Inscriptions and material remains.
	Archaeological		UMETA(DisplayName = "Archaeological"),

	MAX					UMETA(Hidden)
};

/**
 * The six links of a chain of custody, in order, per Section 8:
 * Event -> Eyewitness -> Oral proclamation -> Written source -> Manuscript ->
 * Patristic attestation.
 */
UENUM(BlueprintType)
enum class ETransmissionLink : uint8
{
	// That the event happened at all -- where hostile and material corroboration land.
	Event					UMETA(DisplayName = "Event"),
	// Who was positioned to see it.
	Eyewitness				UMETA(DisplayName = "Eyewitness"),
	// The fixed, memorized proclamation that circulated before anything was written.
	OralProclamation		UMETA(DisplayName = "Oral Proclamation"),
	// The document itself.
	WrittenSource			UMETA(DisplayName = "Written Source"),
	// The physical copies by which the document reaches us.
	Manuscript				UMETA(DisplayName = "Manuscript"),
	// Who, outside the document, vouches for its origin.
	PatristicAttestation	UMETA(DisplayName = "Patristic Attestation"),

	MAX						UMETA(Hidden)
};

/** Number of real (non-MAX) links in a transmission chain. */
static constexpr int32 NumTransmissionLinks = static_cast<int32>(ETransmissionLink::MAX);

/**
 * One recoverable piece of evidence. Section 8's UTestimonyFragment data model,
 * expressed as a struct so the same authored definition can live either in a
 * DataTable (bulk-authorable as text, see Content/Data/DT_TestimonyFragments.json)
 * or in a UTestimonyFragment data asset -- Section 9 asks for both to be editable
 * by a non-programmer.
 *
 * Standing rule: SourceReference cites real sources. No field here ever holds text
 * presented as scripture, a patristic quotation, or a manuscript reading.
 * ScholarlyNote is original prose describing where a dispute lies, not a quotation.
 */
USTRUCT(BlueprintType)
struct CHAINOFWITNESSES_API FTestimonyFragmentDefinition : public FTableRowBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
	FName FragmentID;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
	FText Title;

	// Citation by reference only, e.g. "Eusebius, Hist. eccl. 3.39.15". The in-game
	// reader resolves this to the actual sourced text; the Codex never stores a
	// fabricated quotation.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
	FText SourceReference;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
	EWitnessTier Tier = EWitnessTier::Patristic;

	// Master Prompt: "AttestationDate -- earliest defensible date". This is when the
	// *testimony* enters the record, not when the event it describes occurred. Papias
	// writing c. AD 110 about Mark attests an AD 55-70 event from 40-55 years out,
	// and chain scoring prices that distance in.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dating")
	int32 AttestationDateAD = 100;

	// Independent fragments that support this one. Scoring rewards corroborating
	// pairs that appear together in the same chain.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Argument")
	TArray<FName> CorroboratedBy;

	// Genuine counterarguments -- other fragments that cut against this one. A
	// debate opponent (Task 7) raises these whether or not the player has found
	// them, so an unanswered challenge is a standing liability on any chain this
	// fragment is slotted into.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Argument")
	TArray<FName> ChallengedBy;

	// Fragments this one rebuts. Slotting this fragment into a chain answers those
	// challenges for that chain.
	//
	// Beyond Section 8's literal field list, but required to make "loses strength
	// from unanswered challenges" mechanical: without an explicit rebuttal edge
	// there is no way to ever answer a challenge, and no way for the defeat screen
	// to show which link failed and what would have saved it. Kept deliberately
	// sparse -- a rebuttal must be real evidence, not an assertion. Some challenges
	// (the longer ending of Mark) have no evidential answer, and the honest design
	// is to let the chain take the hit.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Argument")
	TArray<FName> RebutsFragmentIDs;

	// True where the fragment's authenticity, integrity, or attribution is itself
	// disputed (the Testimonium Flavianum's interpolations, P52's dating spread).
	// Contested fragments still count -- they are scored at a discount, not
	// discarded, which is what the scholarship actually supports.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Argument")
	bool bIsContested = false;

	// Where the dispute actually lies. Surfaced verbatim in the fragment inspector
	// (Task 3) and by debate opponents (Task 7) -- Pillar 5: a player who learns the
	// counterarguments here is not blindsided by them later.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Argument", meta = (MultiLine = true))
	FText ScholarlyNote;
};

/**
 * Data-asset form of a testimony fragment, for designers who would rather author
 * one asset per fragment than edit a shared table. Both forms register into
 * UCodexSubsystem identically.
 */
UCLASS(BlueprintType)
class CHAINOFWITNESSES_API UTestimonyFragment : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Testimony", meta = (ShowOnlyInnerProperties))
	FTestimonyFragmentDefinition Definition;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	/**
	 * Whether a tier of evidence belongs in a given link of the chain. Slotting a
	 * mismatch is allowed -- experimenting is how the player learns the shape of a
	 * chain of custody -- but scores at a penalty rather than being blocked.
	 */
	UFUNCTION(BlueprintPure, Category = "Codex")
	static bool IsTierAppropriateForLink(EWitnessTier Tier, ETransmissionLink Link);
};
