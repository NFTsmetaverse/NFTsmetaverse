#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "ChainContentSettings.generated.h"

class UDataTable;

/**
 * Where the campaign's content lives, as a Project Settings page.
 *
 * Every subsystem in this project starts empty and is filled by a Register*Table
 * call. Until something makes those calls the game has no factions, no fragments,
 * no roads and no dialogue, however much of it is sitting in Content/Data. This is
 * the thing that makes them.
 *
 * It is a UDeveloperSettings rather than a hand-placed asset so that it appears in
 * Project Settings -> Game -> Chain of Witnesses Content on its own, with no
 * Blueprint to create and nothing to remember to place in a level. Assign the
 * imported DataTables once and they are wired for every map, every play session
 * and every packaged build.
 *
 * The comment above each property names the row struct to import that JSON as.
 * Tools/datatable_lint.py prints the same mapping, derived independently from the
 * headers, so the two can be checked against each other.
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Chain of Witnesses Content"))
class CHAINOFWITNESSES_API UChainContentSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UChainContentSettings();

	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	/** The settings object, or nullptr if the CDO is not available yet. */
	static const UChainContentSettings* Get();

	// --- Campaign -----------------------------------------------------------

	// DT_CampaignEvents.json -- row struct CampaignEventRow.
	UPROPERTY(config, EditAnywhere, Category = "Campaign", meta = (AllowedClasses = "/Script/Engine.DataTable"))
	TSoftObjectPtr<UDataTable> CampaignEvents;

	// --- Codex --------------------------------------------------------------

	// DT_TestimonyFragments.json -- row struct TestimonyFragmentDefinition.
	UPROPERTY(config, EditAnywhere, Category = "Codex", meta = (AllowedClasses = "/Script/Engine.DataTable"))
	TSoftObjectPtr<UDataTable> TestimonyFragments;

	// --- Map ----------------------------------------------------------------

	// DT_Locations.json -- row struct CampaignLocationRow.
	UPROPERTY(config, EditAnywhere, Category = "Map", meta = (AllowedClasses = "/Script/Engine.DataTable"))
	TSoftObjectPtr<UDataTable> Locations;

	// DT_TravelRoutes.json -- row struct TravelRouteRow.
	UPROPERTY(config, EditAnywhere, Category = "Map", meta = (AllowedClasses = "/Script/Engine.DataTable"))
	TSoftObjectPtr<UDataTable> TravelRoutes;

	// DT_EncounterTypes.json -- row struct EncounterTypeRow. Travel encounters, not
	// combat ones; the two are different tables and different systems.
	UPROPERTY(config, EditAnywhere, Category = "Map", meta = (AllowedClasses = "/Script/Engine.DataTable"))
	TSoftObjectPtr<UDataTable> TravelEncounterTypes;

	// --- Reputation ---------------------------------------------------------

	// DT_Factions.json -- row struct FactionRow.
	UPROPERTY(config, EditAnywhere, Category = "Reputation", meta = (AllowedClasses = "/Script/Engine.DataTable"))
	TSoftObjectPtr<UDataTable> Factions;

	// DT_DeedTypes.json -- row struct DeedTypeRow.
	UPROPERTY(config, EditAnywhere, Category = "Reputation", meta = (AllowedClasses = "/Script/Engine.DataTable"))
	TSoftObjectPtr<UDataTable> DeedTypes;

	// --- Dialogue -----------------------------------------------------------

	// One entry per conversation: DT_Dialogue_JerusalemHandoff.json and
	// DT_Dialogue_RomeAD65.json today -- row struct DialogueNodeRow. A list rather
	// than a fixed pair because conversations are the thing there will be most of.
	UPROPERTY(config, EditAnywhere, Category = "Dialogue", meta = (AllowedClasses = "/Script/Engine.DataTable"))
	TArray<TSoftObjectPtr<UDataTable>> DialogueTables;

	// --- Debate -------------------------------------------------------------

	// DT_DebateOpponents.json -- row struct DebateOpponentRow.
	UPROPERTY(config, EditAnywhere, Category = "Debate", meta = (AllowedClasses = "/Script/Engine.DataTable"))
	TSoftObjectPtr<UDataTable> DebateOpponents;

	// DT_DebateObjections.json -- row struct DebateObjectionRow.
	UPROPERTY(config, EditAnywhere, Category = "Debate", meta = (AllowedClasses = "/Script/Engine.DataTable"))
	TSoftObjectPtr<UDataTable> DebateObjections;

	// --- Witness Missions ---------------------------------------------------

	// DT_Eras.json -- row struct EraDefinitionRow.
	UPROPERTY(config, EditAnywhere, Category = "Witness Missions", meta = (AllowedClasses = "/Script/Engine.DataTable"))
	TSoftObjectPtr<UDataTable> Eras;

	// DT_WitnessMissions.json -- row struct WitnessMissionRow.
	UPROPERTY(config, EditAnywhere, Category = "Witness Missions", meta = (AllowedClasses = "/Script/Engine.DataTable"))
	TSoftObjectPtr<UDataTable> WitnessMissions;

	// --- Combat -------------------------------------------------------------

	// DT_CombatEncounters.json -- row struct CombatEncounterRow.
	UPROPERTY(config, EditAnywhere, Category = "Combat", meta = (AllowedClasses = "/Script/Engine.DataTable"))
	TSoftObjectPtr<UDataTable> CombatEncounters;

	// --- Startup behaviour --------------------------------------------------

	// The era to start in, and where. Mode A (the framed campaign) begins in the
	// crusader frame; leave the era empty for Mode B, which starts in the first
	// century with no frame at all.
	UPROPERTY(config, EditAnywhere, Category = "Startup")
	FName StartingEraID = TEXT("Era_Frame");

	UPROPERTY(config, EditAnywhere, Category = "Startup")
	FName StartingLocationID = TEXT("Loc_Jerusalem");

	// Runs every subsystem's content validator after registration and logs what it
	// finds. On by default: the validators exist precisely to catch the content
	// errors that are otherwise silent, and startup is when they are cheapest.
	UPROPERTY(config, EditAnywhere, Category = "Startup")
	bool bValidateContentOnStartup = true;

	// Registration happens automatically when the game instance starts. Turn this
	// off to drive UChainBootstrapSubsystem::RegisterAllContent() yourself.
	UPROPERTY(config, EditAnywhere, Category = "Startup")
	bool bRegisterContentOnStartup = true;
};
