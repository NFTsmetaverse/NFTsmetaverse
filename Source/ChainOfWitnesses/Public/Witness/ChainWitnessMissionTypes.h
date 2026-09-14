#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Codex/ChainCodexTypes.h"
#include "Dialogue/ChainDialogueTypes.h"
#include "Reputation/ChainReputationTypes.h"
#include "ChainWitnessMissionTypes.generated.h"

class APawn;
class APlayerController;
class UInputMappingContext;
class UWorld;

/** Which layer of the game is being played. */
UENUM(BlueprintType)
enum class EGameEra : uint8
{
	// The crusader overworld. Absent entirely in Mode B.
	Frame		UMETA(DisplayName = "Frame Era"),
	// A first-century sequence.
	Witness		UMETA(DisplayName = "Witness Era"),

	MAX			UMETA(Hidden)
};

/**
 * Section 3's structural toggle. The whole point is that this is a data choice:
 * nothing below branches on it except how a mission treats the world it leaves
 * behind.
 */
UENUM(BlueprintType)
enum class EStructuralMode : uint8
{
	// Mode A: crusader overworld plus first-century Witness Missions.
	DualEra				UMETA(DisplayName = "Mode A - Dual Era"),
	// Mode B: AD 30-100 throughout, no frame. A "mission" is simply where the
	// player already is, so nothing is sealed off and nothing is restored.
	FirstCenturyOnly	UMETA(DisplayName = "Mode B - First Century Only"),

	MAX					UMETA(Hidden)
};

/**
 * Section 5's combat retuning, as a contract rather than an implementation. No
 * combat system exists yet; this is what one will read when it does, and what a
 * designer tunes per era in the meantime.
 *
 * The two win-state flags are the substantive part: Section 5 asks for escape and
 * de-escalation to be first-class outcomes in Witness Missions, which is a
 * different game from the medieval layer rather than the same game with smaller
 * numbers.
 */
USTRUCT(BlueprintType)
struct CHAINOFWITNESSES_API FEraCombatTuning
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", meta = (ClampMin = "0.0"))
	float IncomingDamageScale = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", meta = (ClampMin = "0.0"))
	float OutgoingDamageScale = 1.f;

	// Getting away is a win, not a failure to win.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat")
	bool bEscapeIsWinState = false;

	// Talking it down likewise.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat")
	bool bDeEscalationIsWinState = false;

	// Medieval only. A fisherman does not command a line.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat")
	bool bFormationCommandEnabled = false;

	// Rough encounter size. Witness Missions want few and heavy.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", meta = (ClampMin = "0"))
	int32 TypicalEnemyCount = 4;
};

/**
 * One playable era: who the player is, how he is controlled, and how fighting
 * works. Section 3 asks for two character controllers and two combat tuning sets
 * over a shared backend; this row is that pair, and the shared backend is every
 * other subsystem in this module.
 */
USTRUCT(BlueprintType)
struct CHAINOFWITNESSES_API FEraDefinitionRow : public FTableRowBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Era")
	FName EraID;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Era")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Era")
	EGameEra Era = EGameEra::Witness;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Era")
	TSoftClassPtr<APawn> PawnClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Era")
	TSoftClassPtr<APlayerController> PlayerControllerClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Era")
	TSoftObjectPtr<UInputMappingContext> InputMappingContext;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Era")
	FEraCombatTuning CombatTuning;
};

/**
 * A playable first-century sequence reconstructing one event of the Section 6
 * spine.
 *
 * A Witness Mission is a *reconstruction*, not a second life: the player brings
 * everything his Codex holds into it, including sources written long after the
 * year he is standing in, because he is the one putting the scene together. What
 * he does inside it changes nothing in the frame era except what he comes out
 * knowing -- see bPersistsWorldState.
 */
USTRUCT(BlueprintType)
struct CHAINOFWITNESSES_API FWitnessMissionRow : public FTableRowBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission")
	FName MissionID;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission")
	FText DisplayName;

	// Row of DT_CampaignEvents this reconstructs.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission")
	FName AnchorEventID;

	// The campaign clock is pushed here for the duration, so every era-gated system
	// -- dialogue, debate opponents, encounters -- evaluates at the right year
	// without knowing a mission is running.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission")
	int32 MissionYearAD = 30;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission", meta = (ClampMin = "0", ClampMax = "364"))
	int32 MissionDayOfYear = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission")
	FName MissionLocationID;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission")
	FName EraID;

	// Peter, Mark, Luke, Paul, or an original companion.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission")
	FName PlayableCharacterID;

	// The level to travel to. The subsystem never loads this itself -- it names it
	// and broadcasts; travel and possession belong to the GameMode.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission")
	TSoftObjectPtr<UWorld> Level;

	// The scene's opening conversation, in the dialogue tables. The GameMode starts
	// it once the level is up; naming it here is what joins a mission to its script.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission")
	FName OpeningDialogueNodeID;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission")
	FName OpeningNpcID;

	// Where the opening conversation happens, which decides what the NPC will say
	// in it. A room with the door shut is not the street.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission")
	EConversationSafety OpeningSafety = EConversationSafety::Private;

	// Section 2: "when he recovers a source, the game enters a Witness Mission".
	// This is that source. NAME_None for missions gated only by the campaign act.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission")
	FName UnlockedByFragmentID;

	// What playing it yields. These survive the return to the frame era: the
	// evidence is the whole reason for going.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission")
	TArray<FName> GrantsFragmentIDs;

	// False -- the default -- seals the mission: faction standing and NPC memory are
	// snapshotted on entry and restored on return, so a reconstruction cannot change
	// the thirteenth century. True lets it write through, which is what Mode B
	// wants, where the "mission" is simply the world.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission")
	bool bPersistsWorldState = false;
};

/** How a mission ended. */
UENUM(BlueprintType)
enum class EWitnessMissionOutcome : uint8
{
	Completed	UMETA(DisplayName = "Completed"),
	// Left early. The frame is restored; nothing is granted.
	Abandoned	UMETA(DisplayName = "Abandoned"),

	MAX			UMETA(Hidden)
};

/**
 * Everything needed to put the frame era back exactly as it was left.
 *
 * The snapshots reuse each subsystem's own save payload rather than inventing a
 * parallel rollback path, so a sealed mission restores by the same code that a
 * save file does.
 */
USTRUCT(BlueprintType)
struct CHAINOFWITNESSES_API FFrameSnapshot
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "Witness")
	int32 YearAD = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Witness")
	int32 DayOfYear = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Witness")
	FName PartyLocationID;

	// The era definition to put back on, so the frame knows which controller and
	// tuning it had.
	UPROPERTY(BlueprintReadOnly, Category = "Witness")
	FName EraID;

	// Only populated for a sealed mission.
	UPROPERTY(BlueprintReadOnly, Category = "Witness")
	bool bHasWorldState = false;

	UPROPERTY(BlueprintReadOnly, Category = "Witness")
	FReputationSaveData Reputation;

	UPROPERTY(BlueprintReadOnly, Category = "Witness")
	FDialogueSaveData Dialogue;
};

/** Which mission is running, if any, and what to return to. */
USTRUCT(BlueprintType)
struct CHAINOFWITNESSES_API FWitnessMissionState
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "Witness")
	EGameEra CurrentEra = EGameEra::Frame;

	// Which era definition is live. Part of the saved state rather than re-derived,
	// so a save taken mid-mission comes back mid-mission.
	UPROPERTY(BlueprintReadOnly, Category = "Witness")
	FName CurrentEraID;

	UPROPERTY(BlueprintReadOnly, Category = "Witness")
	bool bIsMissionActive = false;

	UPROPERTY(BlueprintReadOnly, Category = "Witness")
	FName ActiveMissionID;

	UPROPERTY(BlueprintReadOnly, Category = "Witness")
	FFrameSnapshot Frame;

	// Missions played to completion, so a reconstruction already made is known to
	// have been made.
	UPROPERTY(BlueprintReadOnly, Category = "Witness")
	TArray<FName> CompletedMissionIDs;
};

/** Serializable Witness Mission state. */
USTRUCT(BlueprintType)
struct CHAINOFWITNESSES_API FWitnessMissionSaveData
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "Witness")
	FWitnessMissionState State;

	UPROPERTY(BlueprintReadOnly, Category = "Witness")
	EStructuralMode StructuralMode = EStructuralMode::DualEra;

	UPROPERTY(BlueprintReadOnly, Category = "Witness")
	int32 FrameYearAD = 1229;
};
