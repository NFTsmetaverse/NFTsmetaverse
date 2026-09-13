#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Codex/TestimonyFragment.h"
#include "DebateTypes.generated.h"

/** Who is arguing. Colours the tone; the mechanics come from his objections. */
UENUM(BlueprintType)
enum class EDebateOpponentKind : uint8
{
	// Cares about public order and legal fact, not about theology.
	RomanMagistrate			UMETA(DisplayName = "Roman Magistrate"),
	// Argues from inside the tradition, which makes him the hardest to dismiss.
	SynagogueElder			UMETA(DisplayName = "Synagogue Elder"),
	// Second century: offers a higher knowledge and is embarrassed by bodies.
	GnosticTeacher			UMETA(DisplayName = "Gnostic Teacher"),
	// Second century: cuts the Hebrew scriptures away and keeps an edited Luke.
	Marcionite				UMETA(DisplayName = "Marcionite"),
	// Modern source criticism given a first-century voice, per Section 7.
	SourceCriticalScribe	UMETA(DisplayName = "Source-Critical Scribe"),
	// The other side of Section 7's argument, arguing from the patristic tradition.
	TraditionInformant		UMETA(DisplayName = "Tradition Informant"),

	MAX						UMETA(Hidden)
};

/** How the player met an objection. */
UENUM(BlueprintType)
enum class EDebateResponseKind : uint8
{
	// Rest on the chain as assembled. The defence is that link's score, no better.
	LeanOnChain		UMETA(DisplayName = "Lean on the Chain"),
	// Cite one thing that answers this objection directly. Each may be used once.
	PresentFragment	UMETA(DisplayName = "Present a Fragment"),
	// Give the point away. Sometimes the honest move, and always cheaper than
	// defending something indefensible.
	Concede			UMETA(DisplayName = "Concede the Point"),

	MAX				UMETA(Hidden)
};

UENUM(BlueprintType)
enum class EDebateOutcome : uint8
{
	InProgress	UMETA(DisplayName = "In Progress"),
	// The opponent's composure broke: he grants the case.
	Won			UMETA(DisplayName = "Won"),
	// The case came apart. The report names where.
	Lost		UMETA(DisplayName = "Lost"),
	// Rounds ran out with neither side moved far enough.
	Drawn		UMETA(DisplayName = "Drawn"),
	Abandoned	UMETA(DisplayName = "Abandoned"),

	MAX			UMETA(Hidden)
};

/**
 * One genuine counterargument.
 *
 * Every objection attacks a named link of the transmission chain, which is what
 * makes the Codex the ammunition rather than a separate scoring system bolted to a
 * debate: the link score the player built in Task 2 is the defence he has here.
 *
 * Standing rule: an objection states a real scholarly or historical challenge in
 * the opponent's own words. It never quotes a source, and it is never a straw man
 * -- Pillar 5 is that a player who meets the counterarguments here is not
 * blindsided by them later, which only works if they are the strong versions.
 */
USTRUCT(BlueprintType)
struct CHAINOFWITNESSES_API FDebateObjectionRow : public FTableRowBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objection")
	FName ObjectionID;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objection", meta = (MultiLine = true))
	FText Statement;

	// The link of the chain this goes after.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objection")
	ETransmissionLink TargetsLink = ETransmissionLink::Event;

	// Fragments that meet this objection head on. Presenting one of these is a
	// full answer; leaning on the chain is only as good as the link.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objection")
	TArray<FName> AnsweredByFragmentIDs;

	// How strong the targeted link must be for a general appeal to the chain to
	// hold. Above 1 means nothing short of a direct citation will do.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objection", meta = (ClampMin = "0.0"))
	float RequiredLinkStrength = 0.5f;

	// How much ground this objection moves, won or lost.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objection", meta = (ClampMin = "0.0"))
	float Weight = 10.f;

	// What he says when the answer lands.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objection", meta = (MultiLine = true))
	FText ConcededResponse;

	// What he says when it does not.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objection", meta = (MultiLine = true))
	FText PressedResponse;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objection")
	int32 EarliestYearAD = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objection")
	int32 LatestYearAD = INDEX_NONE;
};

/** Someone worth arguing with. */
USTRUCT(BlueprintType)
struct CHAINOFWITNESSES_API FDebateOpponentRow : public FTableRowBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Opponent")
	FName OpponentID;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Opponent")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Opponent")
	EDebateOpponentKind Kind = EDebateOpponentKind::SynagogueElder;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Opponent")
	FName FactionID;

	// What he knows to ask. He leads with whatever the player's chain is worst at.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Opponent")
	TArray<FName> ObjectionIDs;

	// How far the player must move him before he grants the case -- and, the other
	// way, how far the player can be pushed before his own case collapses.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Opponent", meta = (ClampMin = "1.0"))
	float Composure = 30.f;

	// Multiplies what a failed answer costs. A hostile magistrate is not a patient
	// interlocutor.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Opponent", meta = (ClampMin = "0.1"))
	float Aggression = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Opponent", meta = (ClampMin = "1"))
	int32 MaxRounds = 5;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Opponent")
	int32 EarliestYearAD = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Opponent")
	int32 LatestYearAD = INDEX_NONE;

	// Recorded against the player's reputation when the debate ends, so an argument
	// in a public place travels like anything else he does. NAME_None for a
	// conversation nobody else hears.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Opponent")
	FName VictoryDeedTypeID;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Opponent")
	FName DefeatDeedTypeID;
};

/** The objection on the table, and what the player has to meet it with. */
USTRUCT(BlueprintType)
struct CHAINOFWITNESSES_API FDebateObjectionView
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "Debate")
	FName ObjectionID;

	UPROPERTY(BlueprintReadOnly, Category = "Debate")
	FText Statement;

	UPROPERTY(BlueprintReadOnly, Category = "Debate")
	ETransmissionLink TargetsLink = ETransmissionLink::Event;

	// What leaning on the chain would be worth against this, and what it needs to
	// be. Shown before committing: the player can read his own Codex, so hiding the
	// arithmetic would only add tedium.
	UPROPERTY(BlueprintReadOnly, Category = "Debate")
	float ChainDefence = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Debate")
	float RequiredLinkStrength = 0.f;

	// True when the targeted link carries a challenge the player has never answered,
	// which halves whatever the chain is worth here.
	UPROPERTY(BlueprintReadOnly, Category = "Debate")
	bool bLinkHasUnansweredChallenge = false;

	// Recovered, not yet spent this debate, and a direct answer to this objection.
	UPROPERTY(BlueprintReadOnly, Category = "Debate")
	TArray<FName> AvailableAnswerFragmentIDs;
};

/** What happened in one exchange. */
USTRUCT(BlueprintType)
struct CHAINOFWITNESSES_API FDebateRoundRecord
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "Debate")
	FName ObjectionID;

	UPROPERTY(BlueprintReadOnly, Category = "Debate")
	ETransmissionLink TargetedLink = ETransmissionLink::Event;

	UPROPERTY(BlueprintReadOnly, Category = "Debate")
	EDebateResponseKind ResponseKind = EDebateResponseKind::Concede;

	// NAME_None unless the player cited something.
	UPROPERTY(BlueprintReadOnly, Category = "Debate")
	FName FragmentUsedID;

	UPROPERTY(BlueprintReadOnly, Category = "Debate")
	float Defence = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Debate")
	float ConvictionDelta = 0.f;

	// False when the objection stood.
	UPROPERTY(BlueprintReadOnly, Category = "Debate")
	bool bAnswered = false;

	// What he said back.
	UPROPERTY(BlueprintReadOnly, Category = "Debate")
	FText OpponentReply;
};

/**
 * The legible loss Section 8 asks for: not a verdict but a transcript, naming the
 * link that failed and what would have held it.
 */
USTRUCT(BlueprintType)
struct CHAINOFWITNESSES_API FDebateOutcomeReport
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "Debate")
	EDebateOutcome Outcome = EDebateOutcome::InProgress;

	UPROPERTY(BlueprintReadOnly, Category = "Debate")
	FName OpponentID;

	UPROPERTY(BlueprintReadOnly, Category = "Debate")
	FName AnchorEventID;

	UPROPERTY(BlueprintReadOnly, Category = "Debate")
	float FinalConviction = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Debate")
	TArray<FDebateRoundRecord> Rounds;

	// The link the chain was weakest at, from the Codex's own breakdown. This is
	// the answer to "why did I lose".
	UPROPERTY(BlueprintReadOnly, Category = "Debate")
	ETransmissionLink WeakestLink = ETransmissionLink::Event;

	UPROPERTY(BlueprintReadOnly, Category = "Debate")
	TArray<FName> ObjectionsThatLanded;

	// Fragments that would have answered the objections which landed, and that the
	// player has not recovered. Failure teaches: this is the reading list, and the
	// next set of missions.
	UPROPERTY(BlueprintReadOnly, Category = "Debate")
	TArray<FName> WouldHaveAnsweredFragmentIDs;
};

/** A debate in progress. */
USTRUCT(BlueprintType)
struct CHAINOFWITNESSES_API FDebateState
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "Debate")
	bool bIsActive = false;

	UPROPERTY(BlueprintReadOnly, Category = "Debate")
	FName OpponentID;

	// The chain under argument. A debate is always about one event's transmission.
	UPROPERTY(BlueprintReadOnly, Category = "Debate")
	FName AnchorEventID;

	UPROPERTY(BlueprintReadOnly, Category = "Debate")
	FName CurrentObjectionID;

	UPROPERTY(BlueprintReadOnly, Category = "Debate")
	int32 RoundIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Debate")
	int32 MaxRounds = 5;

	// Positive is ground gained. The opponent's composure is the threshold in both
	// directions.
	UPROPERTY(BlueprintReadOnly, Category = "Debate")
	float Conviction = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Debate")
	TArray<FName> UsedObjectionIDs;

	// A citation carries once. Making the same appeal twice in one argument is
	// weaker, not stronger.
	UPROPERTY(BlueprintReadOnly, Category = "Debate")
	TArray<FName> SpentFragmentIDs;

	UPROPERTY(BlueprintReadOnly, Category = "Debate")
	TArray<FDebateRoundRecord> Rounds;
};
