#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "DialogueTypes.generated.h"

/**
 * Where the conversation is happening. Ordered least to most secure, and compared
 * by value, so a condition asking for Guarded is also satisfied in Private.
 *
 * Safety is a property of the meeting, not of the NPC: the same man says different
 * things in the Temple courtyard and behind a shut door. The caller sets it when
 * starting the conversation.
 */
UENUM(BlueprintType)
enum class EConversationSafety : uint8
{
	// Street, market, Temple court. Assume someone is listening.
	Public		UMETA(DisplayName = "Public"),
	// A workshop, a ship's deck, a shared room. Known company, but not sealed.
	Guarded		UMETA(DisplayName = "Guarded"),
	// A private house with the door shut and someone watching the street.
	Private		UMETA(DisplayName = "Private"),

	MAX			UMETA(Hidden)
};

/**
 * How far an NPC will go with this player. This is the "disclosure state" of
 * Section 5's dialogue row: it ratchets upward through conversation and persists
 * between visits, so a breakthrough is not re-earned every time the player walks
 * back into the city.
 */
UENUM(BlueprintType)
enum class EDisclosureLevel : uint8
{
	// Directions and nothing else.
	Stranger		UMETA(DisplayName = "Stranger"),
	// Will discuss the movement in general terms.
	Acquaintance	UMETA(DisplayName = "Acquaintance"),
	// Will name names and say what he personally saw.
	Confidant		UMETA(DisplayName = "Confidant"),
	// Will hand over sources, letters, and introductions.
	Insider			UMETA(DisplayName = "Insider"),

	MAX				UMETA(Hidden)
};

/**
 * Why a line is unavailable. Surfaced to the player rather than hidden, on the same
 * principle as the Codex's score breakdown: a gate the player can see is a gate
 * that teaches him what the world runs on.
 */
UENUM(BlueprintType)
enum class EDialogueGate : uint8
{
	None					UMETA(DisplayName = "Available"),
	OutsideEra				UMETA(DisplayName = "Not yet, or no longer, true"),
	MissingWorldState		UMETA(DisplayName = "Has not happened yet"),
	Unsafe					UMETA(DisplayName = "Not where he can be overheard"),
	NoVoucher				UMETA(DisplayName = "No one has answered for you"),
	InsufficientStanding	UMETA(DisplayName = "Your name is not good enough here"),
	InsufficientTrust		UMETA(DisplayName = "He does not trust you that far"),
	InsufficientDisclosure	UMETA(DisplayName = "He has not opened up that far"),
	UnknownFragment			UMETA(DisplayName = "You cannot show that you know this"),
	WeakChain				UMETA(DisplayName = "You cannot yet defend this"),

	MAX						UMETA(Hidden)
};

/**
 * Everything that can gate a line. All conditions must pass; the first failure
 * encountered is the one reported, in the order evaluated by
 * UDialogueSubsystem::EvaluateConditions (world facts first, then the player's
 * standing, then what he can demonstrate).
 *
 * Authoring note on RequiredFragmentIDs in a first-century Witness Mission: a
 * required fragment means the player can credibly show he already knows the thing,
 * not that he is brandishing a document. A fragment whose AttestationDateAD falls
 * after the conversation's year is still legitimate where the knowledge plainly
 * predates the writing -- but citing a second-century source at a first-century
 * NPC is not, and nothing here prevents it. Author accordingly.
 */
USTRUCT(BlueprintType)
struct CHAINOFWITNESSES_API FDialogueConditionSet
{
	GENERATED_BODY()

public:
	// Fragments the player must have recovered.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Codex")
	TArray<FName> RequiredFragmentIDs;

	// "What the player already credibly knows": not merely holding fragments, but
	// having assembled them into a chain that would survive being argued with.
	// NAME_None disables the check.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Codex")
	FName CredibleChainAnchorEventID;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Codex", meta = (ClampMin = "0.0", ClampMax = "100.0"))
	float MinimumChainStrength = 0.f;

	// Conversational rapport with this NPC, built up over the tree itself.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Standing")
	float MinimumTrust = 0.f;

	// What he has heard about the player, from UReputationSubsystem: his own
	// dealings plus his faction's view. Distinct from trust -- a man can like you
	// and still not risk anything while your name is bad in his city.
	//
	// Opt-in rather than defaulted, because standing legitimately goes negative and
	// a default threshold of zero would silently gate every authored line behind
	// having no enemies.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Standing")
	bool bCheckStanding = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Standing", meta = (EditCondition = "bCheckStanding"))
	float MinimumStanding = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Standing")
	EDisclosureLevel MinimumDisclosure = EDisclosureLevel::Stranger;

	// "Who vouched for him." Any one of these suffices. A voucher counts if he has
	// spoken for the player to this NPC specifically or vouched generally.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Standing")
	TArray<FName> AcceptedVoucherIDs;

	// "Whether the conversation is safe."
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Setting")
	EConversationSafety MinimumSafety = EConversationSafety::Public;

	// Campaign-year window, from UCampaignTimelineSubsystem. INDEX_NONE disables
	// either bound.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Setting")
	int32 EarliestYearAD = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Setting")
	int32 LatestYearAD = INDEX_NONE;

	// Timeline events that must already have fired -- an NPC cannot refer to the
	// Temple's destruction in AD 50.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Setting")
	TArray<FName> RequiredActiveEventIDs;
};

/** What saying, or hearing, a line changes. */
USTRUCT(BlueprintType)
struct CHAINOFWITNESSES_API FDialogueEffects
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effects")
	float TrustDelta = 0.f;

	// Only ever raises: disclosure does not un-happen. Leave at Stranger for no change.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effects")
	EDisclosureLevel RaiseDisclosureTo = EDisclosureLevel::Stranger;

	// Testimony recovered by being told it. Recovery still goes through the Codex,
	// so an unregistered ID fails loudly rather than silently.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effects")
	TArray<FName> GrantsFragmentIDs;

	// The speaking NPC vouches for the player to these NPCs. An entry of NAME_None
	// is a general vouch, good with anyone who accepts this voucher.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effects")
	TArray<FName> VouchesForPlayerTo;
};

/** One thing the player can say. */
USTRUCT(BlueprintType)
struct CHAINOFWITNESSES_API FDialogueOption
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
	FText PlayerLine;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
	FDialogueConditionSet Conditions;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
	FDialogueEffects Effects;

	// NAME_None ends the conversation.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
	FName NextNodeID;

	// True: shown greyed with its gate reason, which teaches the player what the
	// world runs on. False: hidden entirely -- use it where the line itself would
	// give away something the player has not earned.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
	bool bShowWhenLocked = true;
};

/** One node of a dialogue tree. Nodes with no reachable options end the conversation. */
USTRUCT(BlueprintType)
struct CHAINOFWITNESSES_API FDialogueNodeRow : public FTableRowBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
	FName NodeID;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
	FName SpeakerID;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue", meta = (MultiLine = true))
	FText Line;

	// Checked on entry. If unmet, traversal diverts to FallbackNodeID -- this is how
	// an NPC greets a returning player differently without duplicating the tree.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
	FDialogueConditionSet EntryConditions;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
	FName FallbackNodeID;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
	FDialogueEffects EntryEffects;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
	TArray<FDialogueOption> Options;
};

/** An option as presented to the UI, with its gate resolved. */
USTRUCT(BlueprintType)
struct CHAINOFWITNESSES_API FDialogueOptionView
{
	GENERATED_BODY()

public:
	// Index into the node's authored Options array; pass to SelectOption.
	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
	int32 OptionIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
	FText PlayerLine;

	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
	bool bIsAvailable = false;

	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
	EDialogueGate Gate = EDialogueGate::None;
};

/** The current node as presented to the UI. */
USTRUCT(BlueprintType)
struct CHAINOFWITNESSES_API FDialogueNodeView
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
	FName NodeID;

	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
	FName SpeakerID;

	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
	FText Line;

	// Includes locked options whose bShowWhenLocked is true.
	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
	TArray<FDialogueOptionView> Options;

	// True when nothing selectable remains.
	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
	bool bIsTerminal = true;
};

/** Persistent standing with one NPC. */
USTRUCT(BlueprintType)
struct CHAINOFWITNESSES_API FNpcDialogueState
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
	FName NpcID;

	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
	float Trust = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
	EDisclosureLevel Disclosure = EDisclosureLevel::Stranger;

	// NPCs who have spoken for the player to this one.
	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
	TArray<FName> VouchersToThisNpc;
};

/**
 * Serializable dialogue state. Section 9's save requirement calls this per-NPC
 * memory; Task 5 will widen it to remembered acts and word-of-mouth propagation,
 * at which point this payload grows rather than being replaced.
 */
USTRUCT(BlueprintType)
struct CHAINOFWITNESSES_API FDialogueSaveData
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
	TArray<FNpcDialogueState> NpcStates;

	// Vouchers who have spoken for the player generally rather than to one NPC.
	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
	TArray<FName> GeneralVouchers;
};
