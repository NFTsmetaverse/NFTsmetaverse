#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "CombatTypes.generated.h"

/**
 * What kind of trouble this is. The distinction does real work: a mob and a guard
 * detail are not the same encounter with different numbers, they fail in opposite
 * directions. A mob cannot be reasoned with once it is moving but loses interest
 * quickly; disciplined men can be talked to and will not let you walk away.
 */
UENUM(BlueprintType)
enum class ECrowdKind : uint8
{
	// No command structure, enormous momentum, short attention. The Temple riot and
	// the Ephesian silversmiths.
	Mob				UMETA(DisplayName = "Mob"),
	// Auxiliaries or Temple police. Disciplined, will listen to a man whose name is
	// good, and will follow.
	Patrol			UMETA(DisplayName = "Patrol"),
	// Posted men defending a place. Hard to shift, easy to walk away from.
	Guard			UMETA(DisplayName = "Guard"),
	// Come to do one thing and not open to discussion. Stephen's stoning.
	ExecutionParty	UMETA(DisplayName = "Execution Party"),
	// Want goods, not deaths. The most reasonable people on this list.
	Bandits			UMETA(DisplayName = "Bandits"),

	MAX				UMETA(Hidden)
};

/**
 * Whether the player has committed to violence. This is a first-class variable
 * rather than a display state: drawing changes what the other side will hear, and
 * once a blade is out some encounters can no longer end any other way.
 */
UENUM(BlueprintType)
enum class ECombatPosture : uint8
{
	// Hands visible. Everything is still possible from here.
	Undrawn		UMETA(DisplayName = "Undrawn"),
	// Drawn but not yet used. Deters bandits; hardens everyone else.
	Drawn		UMETA(DisplayName = "Drawn"),
	// Blood has been drawn. Talking is mostly over.
	Engaged		UMETA(DisplayName = "Engaged"),
	// Backing away, looking for the gap.
	Withdrawing	UMETA(DisplayName = "Withdrawing"),

	MAX			UMETA(Hidden)
};

/** Where one of the other side currently stands. */
UENUM(BlueprintType)
enum class ECombatantDisposition : uint8
{
	Hostile		UMETA(DisplayName = "Hostile"),
	// Still present, no longer pressing.
	Wary		UMETA(DisplayName = "Wary"),
	// Broken, and out of the fight.
	Disengaged	UMETA(DisplayName = "Disengaged"),
	// Talked down rather than beaten.
	TalkedDown	UMETA(DisplayName = "Talked Down"),
	// Wounded out of it or killed.
	Down		UMETA(DisplayName = "Down"),

	MAX			UMETA(Hidden)
};

/**
 * How an encounter ended.
 *
 * Section 5: escape and de-escalation are first-class win states. They are listed
 * here as outcomes in their own right, not as degrees of failure, and whether they
 * count as a win in the current era is read from FEraCombatTuning rather than
 * assumed.
 */
UENUM(BlueprintType)
enum class ECombatOutcome : uint8
{
	Ongoing			UMETA(DisplayName = "Ongoing"),
	// Every hostile is down or broken.
	Overcome		UMETA(DisplayName = "Overcome"),
	// Got away.
	Escaped			UMETA(DisplayName = "Escaped"),
	// Talked out of it.
	DeEscalated		UMETA(DisplayName = "De-escalated"),
	// The player went down.
	Defeated		UMETA(DisplayName = "Defeated"),
	// Taken. Often the historically correct outcome, and not the end of anything.
	Taken			UMETA(DisplayName = "Taken"),

	MAX				UMETA(Hidden)
};

/** One of the other side. */
USTRUCT(BlueprintType)
struct CHAINOFWITNESSES_API FCombatantState
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	FName CombatantID;

	// 0..1. Pushed in by whatever owns the pawn -- a Gameplay Ability System
	// attribute set in a built-out game. The encounter layer never writes it.
	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	float HealthNormalised = 1.f;

	// How much fight is left in him, which is not the same thing as how much blood.
	// Ancient and medieval encounters were decided by this far more often than by
	// casualties, and so are these.
	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	float Resolve = 1.f;

	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	ECombatantDisposition Disposition = ECombatantDisposition::Hostile;
};

/** An authored kind of encounter. */
USTRUCT(BlueprintType)
struct CHAINOFWITNESSES_API FCombatEncounterRow : public FTableRowBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Encounter")
	FName EncounterTypeID;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Encounter", meta = (MultiLine = true))
	FText Summary;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Encounter")
	ECrowdKind CrowdKind = ECrowdKind::Patrol;

	// Whose men these are, which decides whose opinion of the player is at stake.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Encounter")
	FName FactionID;

	// Overridden downward by the era's TypicalEnemyCount where that is smaller:
	// Witness Missions want few and heavy.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Encounter", meta = (ClampMin = "1"))
	int32 CombatantCount = 3;

	// Set pieces opt out of that cap. A riot with three men in it is not a riot, and
	// the era count exists to keep ordinary encounters small, not to shrink the ones
	// whose size is the whole point.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Encounter")
	bool bIgnoreEraCombatantCap = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Encounter", meta = (ClampMin = "0.1"))
	float StartingResolve = 1.f;

	// False for men who did not come to negotiate.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Encounter")
	bool bCanBeTalkedDown = true;

	// What a persuasion attempt has to beat, before standing and posture.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Encounter", meta = (ClampMin = "0.0"))
	float DeEscalationDifficulty = 0.5f;

	// What getting away has to beat.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Encounter", meta = (ClampMin = "0.0"))
	float EscapeDifficulty = 0.5f;

	// Whether being taken alive is on the table. For most of this campaign it is,
	// and it is frequently the historically correct ending.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Encounter")
	bool bTakesPrisoners = true;

	// Recorded against the player's standing when the encounter ends, so what he did
	// here travels the roads like anything else. NAME_None for outcomes nobody sees.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Encounter")
	FName OvercomeDeedTypeID;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Encounter")
	FName DeEscalatedDeedTypeID;
};

/** What came of an attempt to talk or to run. */
USTRUCT(BlueprintType)
struct CHAINOFWITNESSES_API FCombatAttemptResult
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	bool bSucceeded = false;

	// What the attempt needed to beat, and what it was worth. Surfaced so a failure
	// can be read rather than guessed at -- the same principle as the Codex's score
	// breakdown and a debate's defeat report.
	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	float Required = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	float Achieved = 0.f;

	// Why it was impossible, rather than merely failed.
	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	FText Reason;
};

/** An encounter in progress. */
USTRUCT(BlueprintType)
struct CHAINOFWITNESSES_API FCombatEncounterState
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	bool bIsActive = false;

	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	FName EncounterTypeID;

	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	ECombatPosture Posture = ECombatPosture::Undrawn;

	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	float PlayerHealthNormalised = 1.f;

	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	TArray<FCombatantState> Combatants;

	// Rises with each failed attempt to talk. Men who have heard it once and were
	// not moved are harder to move the second time.
	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	int32 FailedDeEscalationAttempts = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	int32 FailedEscapeAttempts = 0;

	// True once anybody at all has been hurt, the player included. Reported as the
	// absence of bEndedWithoutBloodshed.
	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	bool bBloodDrawn = false;

	// True once the player has hurt one of them, which is the narrower fact and the
	// one that actually changes what a crowd will hear. Being beaten yourself does
	// not close off talking: Acts 21-22 has Paul seized and struck by the crowd in
	// the Temple and then addressing it from the steps, and they listen until he
	// reaches the sending to the Gentiles. A crowd hardens against a man who has
	// hurt one of its own, not against a man it has hurt.
	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	bool bPlayerShedBlood = false;

	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	float ElapsedSeconds = 0.f;
};

/** How it went, and whether that counted as winning. */
USTRUCT(BlueprintType)
struct CHAINOFWITNESSES_API FCombatOutcomeReport
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	ECombatOutcome Outcome = ECombatOutcome::Ongoing;

	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	FName EncounterTypeID;

	// Read from the era's tuning rather than assumed: escaping is a win in a Witness
	// Mission and a failure in a pitched medieval fight.
	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	bool bCountedAsWin = false;

	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	int32 CombatantsDown = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	int32 CombatantsBroken = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	int32 CombatantsTalkedDown = 0;

	// True where the encounter ended with nobody hurt. Worth reporting on its own:
	// in this campaign that is frequently the better result and should be visible
	// as one.
	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	bool bEndedWithoutBloodshed = false;

	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	float ElapsedSeconds = 0.f;
};
