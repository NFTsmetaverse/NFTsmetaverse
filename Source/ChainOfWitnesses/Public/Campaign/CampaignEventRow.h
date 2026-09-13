#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "CampaignEventRow.generated.h"

// Matches Master Prompt Section 6's three acts. Mode A (dual-era) and Mode B
// (first-century only, Section 3) both drive world state from this same table --
// the toggle is a data change, not a rewrite.
UENUM(BlueprintType)
enum class ECampaignAct : uint8
{
	Act1_JerusalemChurch	UMETA(DisplayName = "Act I - The Jerusalem Church (AD 30-44)"),
	Act2_TheLetters			UMETA(DisplayName = "Act II - The Letters (AD 46-62)"),
	Act3_GospelsWritten		UMETA(DisplayName = "Act III - The Gospels Written (AD 55-100)")
};

// What an event does to persistent world state once it activates. An event can
// carry more than one (e.g. Ephesus is both a hub-establishing and a set-piece row).
UENUM(BlueprintType)
enum class ECampaignWorldStateEffect : uint8
{
	None			UMETA(DisplayName = "None"),
	HubEstablished	UMETA(DisplayName = "Hub City Established"),
	HubDestroyed	UMETA(DisplayName = "Hub City Destroyed"),
	FactionUnlock	UMETA(DisplayName = "Faction Unlocked"),
	SetPiece		UMETA(DisplayName = "Set Piece"),
	WitnessMission	UMETA(DisplayName = "Witness Mission"),
	Corroboration	UMETA(DisplayName = "Corroboration Mission")
};

/**
 * One row of the Section 6 historical spine. This table is the campaign's source of
 * truth: UCampaignTimelineSubsystem drives act gating, hub-city state, and faction
 * unlocks from it, and it is the backbone that later Codex fragments (Task 2,
 * UTestimonyFragment) attach to.
 *
 * Standing rule: dates here are ranges, not single invented points, wherever the
 * underlying dating is genuinely disputed among scholars -- see bDatingIsDisputed.
 * Never add a row whose date falls outside the range given in the Master Prompt.
 */
USTRUCT(BlueprintType)
struct CHAINOFWITNESSES_API FCampaignEventRow : public FTableRowBase
{
	GENERATED_BODY()

public:
	// Stable identifier, e.g. "Event_StephenMartyred". Matches the DataTable row name.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
	FName EventID;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
	FText EventTitle;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
	ECampaignAct Act = ECampaignAct::Act1_JerusalemChurch;

	// Earliest year AD in the defensible scholarly range.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dating")
	int32 YearEarliestAD = 30;

	// Latest year AD in the defensible scholarly range. Equal to YearEarliestAD for a
	// single attested year (e.g. the Gallio inscription, AD 51-52 is the exception --
	// even "single events" often carry a defensible +/-1 year spread).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dating")
	int32 YearLatestAD = 30;

	// True when the dating -- or the underlying claim itself (e.g. Markan vs Matthean
	// priority, early vs late Luke-Acts) -- is genuinely disputed, not merely
	// imprecise. Per the standing rules, the game must surface the dispute rather
	// than silently pick a side.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dating")
	bool bDatingIsDisputed = false;

	// Required whenever bDatingIsDisputed is true: state both positions and why they
	// diverge. This is what a Codex "ScholarlyNote" (Task 2) and in-game debate
	// (Task 7) ultimately surface to the player.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dating", meta = (MultiLine = true, EditCondition = "bDatingIsDisputed"))
	FText DatingDisputeNote;

	// Real citation only -- canonical text, patristic reference, inscription, or
	// ancient historian. Never a fabricated quotation or manuscript reading.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sourcing")
	FText SourceReference;

	// Design-facing notes: what this row is *for* (hub unlock, set piece, mission
	// seed, etc.), taken from the Master Prompt's "Design use" column.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Design", meta = (MultiLine = true))
	FText DesignUse;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World State")
	TArray<ECampaignWorldStateEffect> WorldStateEffects;

	// Hub city this event establishes or destroys, if WorldStateEffects contains
	// HubEstablished or HubDestroyed. NAME_None otherwise.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World State")
	FName AffectedHubCityID;

	// Factions this event unlocks, if WorldStateEffects contains FactionUnlock.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World State")
	TArray<FName> UnlockedFactionIDs;

	// EventIDs that must already be active before this one can trigger, in addition
	// to reaching YearEarliestAD. Used for narrative causality that a bare year check
	// can't express (e.g. Mark in Rome requires Peter having already left Jerusalem).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gating")
	TArray<FName> PrerequisiteEventIDs;

	// True if this row is the trigger for a full era-swap Witness Mission (Section 2,
	// Task 8) rather than an overworld-only beat.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gating")
	bool bTriggersWitnessMission = false;
};
