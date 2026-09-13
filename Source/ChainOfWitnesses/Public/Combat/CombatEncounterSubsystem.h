#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Combat/CombatTypes.h"
#include "Witness/WitnessMissionTypes.h"
#include "CombatEncounterSubsystem.generated.h"

class UDataTable;
class UReputationSubsystem;
class UCampaignMapSubsystem;
class UWitnessMissionSubsystem;

DECLARE_LOG_CATEGORY_EXTERN(LogCombat, Log, All);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEncounterBegun, FName, EncounterTypeID);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCombatantDispositionChanged, int32, CombatantIndex, ECombatantDisposition, Disposition);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEncounterEnded, ECombatOutcome, Outcome);

/**
 * Combat encounters: what an encounter is, how it can end, and which endings count.
 *
 * **What this owns.** The state of an encounter and its outcome. Who is still in
 * it, how much fight they have left, whether the player has drawn, whether talking
 * or running is still possible, and what it costs him afterwards.
 *
 * **What this does not own.** Swinging anything. Directional melee, animation,
 * hit detection and enemy AI live in the pawn layer and drive this by pushing
 * damage in; the split is the same one used for Witness Missions, where the
 * subsystem holds state and policy and the GameMode does the moving. It is also
 * what lets the whole thing be tested without a world.
 *
 * **Why resolve rather than attrition.** Section 5 asks for sparse, lethal
 * encounters where escape and de-escalation are first-class win states. Men here
 * break before they die, as they historically did: killing four opponents is one
 * way through an encounter and usually the worst one, because a body is a deed and
 * deeds travel. The lethality itself comes from the era's FEraCombatTuning (Task 8),
 * which this is the first system to read.
 */
UCLASS()
class CHAINOFWITNESSES_API UCombatEncounterSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// --- Content ------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "Combat|Content")
	bool RegisterEncounterType(const FCombatEncounterRow& EncounterType);

	UFUNCTION(BlueprintCallable, Category = "Combat|Content")
	int32 RegisterEncounterTable(const UDataTable* EncounterTable);

	UFUNCTION(BlueprintCallable, Category = "Combat|Content")
	bool GetEncounterType(FName EncounterTypeID, FCombatEncounterRow& OutEncounterType) const;

	/** Reports encounters naming factions or deed types nobody registered. */
	UFUNCTION(BlueprintCallable, Category = "Combat|Content")
	void ValidateCombatContent(TArray<FString>& OutProblems) const;

	// --- Running an encounter -----------------------------------------------

	/** Opens an encounter. Combatant count is capped by the era's TypicalEnemyCount. */
	UFUNCTION(BlueprintCallable, Category = "Combat")
	bool BeginEncounter(FName EncounterTypeID);

	UFUNCTION(BlueprintPure, Category = "Combat")
	bool IsEncounterActive() const { return State.bIsActive; }

	UFUNCTION(BlueprintPure, Category = "Combat")
	FCombatEncounterState GetEncounterState() const { return State; }

	/** Drawing hardens most people and reassures nobody. Cannot be undone after blood. */
	UFUNCTION(BlueprintCallable, Category = "Combat")
	bool SetPosture(ECombatPosture NewPosture);

	/**
	 * Pushed in by the melee layer. Scaled by the era's OutgoingDamageScale, and
	 * costs the target resolve as well as health -- being hit is discouraging.
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void ApplyDamageToCombatant(int32 CombatantIndex, float NormalisedDamage);

	/** Likewise, scaled by IncomingDamageScale. This is where lethality lives. */
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void ApplyDamageToPlayer(float NormalisedDamage);

	/**
	 * Try to talk them out of it. PersuasionStrength is what the player brings --
	 * rhetoric, in a built-out skill system. Standing with their faction counts for
	 * as much, and having a blade out counts against.
	 *
	 * A failure is not free: it hardens them, and the next attempt is harder.
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat")
	FCombatAttemptResult AttemptDeEscalation(float PersuasionStrength);

	/** Try to get away. Being committed makes it harder; a mob makes it easier. */
	UFUNCTION(BlueprintCallable, Category = "Combat")
	FCombatAttemptResult AttemptEscape(float AgilityStrength);

	/** Advances resolve and checks for an ending. Driven by the pawn layer's tick. */
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void AdvanceEncounter(float DeltaSeconds);

	/** Ends it deliberately, e.g. when a set piece takes over. */
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void EndEncounter(ECombatOutcome Outcome);

	UFUNCTION(BlueprintPure, Category = "Combat")
	FCombatOutcomeReport GetOutcomeReport() const { return Report; }

	/** True while any of them is still pressing. */
	UFUNCTION(BlueprintPure, Category = "Combat")
	bool HasHostilesRemaining() const;

	/** Test seam; in a running game all three resolve from the owning GameInstance. */
	void SetDependenciesForTesting(UReputationSubsystem* InReputation, UCampaignMapSubsystem* InMap,
		UWitnessMissionSubsystem* InWitness);

	// Resolve lost by everyone still standing when one of them goes down. The single
	// biggest lever on how encounters actually end.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat", meta = (ClampMin = "0.0"))
	float ResolveLostPerCasualty = 0.35f;

	// Resolve lost by a man who is hit himself.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat", meta = (ClampMin = "0.0"))
	float ResolveLostPerWound = 0.4f;

	// At or below this, a man stops pressing.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BreakThreshold = 0.25f;

	// A mob burns itself out, but not quickly: at this rate a crowd at full pitch
	// takes something over four minutes to lose interest, which is far longer than
	// a man can stand in front of one. Waiting is a real option and a bad one.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat", meta = (ClampMin = "0.0"))
	float MobResolveDecayPerSecond = 0.004f;

	// Added to the difficulty of talking, per failed attempt.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat", meta = (ClampMin = "0.0"))
	float DeEscalationHardeningPerFailure = 0.2f;

	// How much a point of faction standing is worth when talking. Standing runs in
	// tens, so this is deliberately small.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	float StandingPersuasionWeight = 0.01f;

	// Penalty to talking for having drawn, and again for having used it.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat", meta = (ClampMin = "0.0"))
	float DrawnPersuasionPenalty = 0.3f;

	// A failed attempt that came this close to the mark still peels the least
	// committed man off the back of the crowd. A crowd is not one mind, and the
	// people furthest from the front are always the first to remember they have
	// somewhere to be.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float PartialPersuasionFraction = 0.75f;

	// Added to the difficulty of getting away, per man still coming for you.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat", meta = (ClampMin = "0.0"))
	float EscapeDifficultyPerHostile = 0.1f;

	// Added again once it is hand to hand. Disengaging from a committed man is the
	// hardest thing on this list and the most frequently attempted.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat", meta = (ClampMin = "0.0"))
	float EngagedEscapePenalty = 0.3f;

	UPROPERTY(BlueprintAssignable, Category = "Combat")
	FOnEncounterBegun OnEncounterBegun;

	UPROPERTY(BlueprintAssignable, Category = "Combat")
	FOnCombatantDispositionChanged OnCombatantDispositionChanged;

	UPROPERTY(BlueprintAssignable, Category = "Combat")
	FOnEncounterEnded OnEncounterEnded;

private:
	UReputationSubsystem* GetReputation() const;
	UCampaignMapSubsystem* GetMap() const;
	UWitnessMissionSubsystem* GetWitness() const;

	/** The era's combat tuning, or defaults where no era is set. */
	FEraCombatTuning GetTuning() const;

	void SetDisposition(int32 CombatantIndex, ECombatantDisposition NewDisposition);

	/** Everyone still standing loses heart when one of them goes down. */
	void ApplyCasualtyShock(int32 FallenIndex);

	/** Breaks anyone whose resolve has gone, and ends the encounter if nobody is left. */
	void ResolveDispositions();

	void BuildReport(ECombatOutcome Outcome);

	UPROPERTY()
	TMap<FName, FCombatEncounterRow> EncounterTypes;

	UPROPERTY()
	FCombatEncounterState State;

	UPROPERTY()
	FCombatOutcomeReport Report;

	UPROPERTY()
	TObjectPtr<UReputationSubsystem> ReputationOverride = nullptr;

	UPROPERTY()
	TObjectPtr<UCampaignMapSubsystem> MapOverride = nullptr;

	UPROPERTY()
	TObjectPtr<UWitnessMissionSubsystem> WitnessOverride = nullptr;
};
