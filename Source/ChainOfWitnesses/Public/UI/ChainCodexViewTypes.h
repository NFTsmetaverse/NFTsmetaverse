#pragma once

#include "CoreMinimal.h"
#include "Codex/TestimonyFragment.h"
#include "ChainCodexViewTypes.generated.h"

/**
 * How strong something is, in words rather than a number.
 *
 * A bare float tells the player nothing about whether he is in trouble. The band is
 * what a colour or a label is driven from, and it exists here rather than in the
 * widget so that every screen describes the same score the same way.
 */
UENUM(BlueprintType)
enum class EAttestationBand : uint8
{
	// Nothing in it.
	Empty			UMETA(DisplayName = "Empty"),
	// Something, but it will not survive being argued with.
	Thin			UMETA(DisplayName = "Thin"),
	// Defensible, with work still to do.
	Serviceable		UMETA(DisplayName = "Serviceable"),
	// Holds.
	Strong			UMETA(DisplayName = "Strong"),

	MAX				UMETA(Hidden)
};

/**
 * One fragment, ready to draw.
 *
 * SourceReference is the real citation and the reason the inspector exists: the
 * player is shown where a thing actually comes from, and the sourced reader opens
 * the text itself. Nothing here is ever a fabricated quotation -- Title and
 * ScholarlyNote are original description, the citation is a pointer.
 */
USTRUCT(BlueprintType)
struct CHAINOFWITNESSES_API FCodexFragmentView
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "Codex UI")
	FName FragmentID;

	UPROPERTY(BlueprintReadOnly, Category = "Codex UI")
	FText Title;

	// e.g. "Papias, via Eusebius, Hist. eccl. 3.39.15".
	UPROPERTY(BlueprintReadOnly, Category = "Codex UI")
	FText SourceReference;

	UPROPERTY(BlueprintReadOnly, Category = "Codex UI")
	EWitnessTier Tier = EWitnessTier::Patristic;

	UPROPERTY(BlueprintReadOnly, Category = "Codex UI")
	FText TierName;

	// "AD 110".
	UPROPERTY(BlueprintReadOnly, Category = "Codex UI")
	FText AttestationDateText;

	UPROPERTY(BlueprintReadOnly, Category = "Codex UI")
	int32 AttestationDateAD = 0;

	// Where the dispute actually lies. Shown, not hidden.
	UPROPERTY(BlueprintReadOnly, Category = "Codex UI")
	FText ScholarlyNote;

	UPROPERTY(BlueprintReadOnly, Category = "Codex UI")
	bool bIsContested = false;

	UPROPERTY(BlueprintReadOnly, Category = "Codex UI")
	bool bIsRecovered = false;

	UPROPERTY(BlueprintReadOnly, Category = "Codex UI")
	bool bIsSlotted = false;

	// Valid only when bIsSlotted.
	UPROPERTY(BlueprintReadOnly, Category = "Codex UI")
	ETransmissionLink SlottedInLink = ETransmissionLink::Event;

	// False when the ID was slotted but no definition is registered -- a content
	// error the screen should show rather than draw as a blank.
	UPROPERTY(BlueprintReadOnly, Category = "Codex UI")
	bool bIsKnownFragment = false;
};

/** One link of the chain-building screen. */
USTRUCT(BlueprintType)
struct CHAINOFWITNESSES_API FCodexLinkView
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "Codex UI")
	ETransmissionLink Link = ETransmissionLink::Event;

	UPROPERTY(BlueprintReadOnly, Category = "Codex UI")
	FText LinkName;

	// What this link is for, in one line. The chain's shape is the thing the player
	// is learning, so the screen should say it rather than assume it.
	UPROPERTY(BlueprintReadOnly, Category = "Codex UI")
	FText LinkDescription;

	UPROPERTY(BlueprintReadOnly, Category = "Codex UI")
	bool bIsFilled = false;

	// 0..1.
	UPROPERTY(BlueprintReadOnly, Category = "Codex UI")
	float Score = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Codex UI")
	EAttestationBand Band = EAttestationBand::Empty;

	// The link an opponent will go for first. Worth marking on the screen before the
	// player finds out in a debate.
	UPROPERTY(BlueprintReadOnly, Category = "Codex UI")
	bool bIsWeakestLink = false;

	UPROPERTY(BlueprintReadOnly, Category = "Codex UI")
	TArray<FCodexFragmentView> SlottedFragments;

	UPROPERTY(BlueprintReadOnly, Category = "Codex UI")
	int32 UnansweredChallengeCount = 0;
};

/** One objection against the chain, and what would meet it. */
USTRUCT(BlueprintType)
struct CHAINOFWITNESSES_API FCodexChallengeView
{
	GENERATED_BODY()

public:
	// The objection itself, which is another piece of evidence.
	UPROPERTY(BlueprintReadOnly, Category = "Codex UI")
	FCodexFragmentView Challenger;

	// The slotted fragment it cuts against, and where that sits.
	UPROPERTY(BlueprintReadOnly, Category = "Codex UI")
	FName ChallengedFragmentID;

	UPROPERTY(BlueprintReadOnly, Category = "Codex UI")
	ETransmissionLink AgainstLink = ETransmissionLink::Event;

	UPROPERTY(BlueprintReadOnly, Category = "Codex UI")
	bool bIsAnswered = false;

	// Rebuttals the player holds and could slot.
	UPROPERTY(BlueprintReadOnly, Category = "Codex UI")
	TArray<FCodexFragmentView> AvailableAnswers;

	// Rebuttals that exist and the player has not found. The reading list, and the
	// next set of missions.
	UPROPERTY(BlueprintReadOnly, Category = "Codex UI")
	TArray<FCodexFragmentView> MissingAnswers;

	// True when nothing in the corpus answers this. Some objections have no
	// evidential reply -- the longer ending of Mark is one -- and the screen should
	// say so rather than imply the player is missing something.
	UPROPERTY(BlueprintReadOnly, Category = "Codex UI")
	bool bHasNoKnownAnswer = false;
};

/** Everything the chain-building screen draws. */
USTRUCT(BlueprintType)
struct CHAINOFWITNESSES_API FCodexChainView
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "Codex UI")
	FName AnchorEventID;

	UPROPERTY(BlueprintReadOnly, Category = "Codex UI")
	FText AnchorTitle;

	// "AD 55-70", or "AD 65" where the range is a single year.
	UPROPERTY(BlueprintReadOnly, Category = "Codex UI")
	FText AnchorDateText;

	UPROPERTY(BlueprintReadOnly, Category = "Codex UI")
	bool bChainExists = false;

	// 0..100. The campaign's real progression meter.
	UPROPERTY(BlueprintReadOnly, Category = "Codex UI")
	float AttestationStrength = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Codex UI")
	EAttestationBand Band = EAttestationBand::Empty;

	UPROPERTY(BlueprintReadOnly, Category = "Codex UI")
	bool bIsComplete = false;

	UPROPERTY(BlueprintReadOnly, Category = "Codex UI")
	ETransmissionLink WeakestLink = ETransmissionLink::Event;

	// Always six, in transmission order.
	UPROPERTY(BlueprintReadOnly, Category = "Codex UI")
	TArray<FCodexLinkView> Links;

	UPROPERTY(BlueprintReadOnly, Category = "Codex UI")
	int32 CorroborationCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Codex UI")
	bool bHasHostileConfirmation = false;

	// Answered and unanswered both, so the screen can show what has been met as well
	// as what is outstanding.
	UPROPERTY(BlueprintReadOnly, Category = "Codex UI")
	TArray<FCodexChallengeView> Challenges;

	UPROPERTY(BlueprintReadOnly, Category = "Codex UI")
	int32 UnansweredChallengeCount = 0;

	// Slotted IDs with no registered definition. A content error, surfaced.
	UPROPERTY(BlueprintReadOnly, Category = "Codex UI")
	TArray<FName> UnknownFragmentIDs;
};
