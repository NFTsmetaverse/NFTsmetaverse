#include "Bootstrap/ChainBootstrapSubsystem.h"

#include "Bootstrap/ChainContentSettings.h"
#include "Campaign/CampaignTimelineSubsystem.h"
#include "Codex/CodexSubsystem.h"
#include "Combat/CombatEncounterSubsystem.h"
#include "Debate/DebateSubsystem.h"
#include "Dialogue/DialogueSubsystem.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Map/CampaignMapSubsystem.h"
#include "Reputation/ReputationSubsystem.h"
#include "Witness/WitnessMissionSubsystem.h"

DEFINE_LOG_CATEGORY(LogChainBootstrap);

void UChainBootstrapSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const UChainContentSettings* Settings = UChainContentSettings::Get();
	if (Settings && Settings->bRegisterContentOnStartup)
	{
		RegisterAllContent();
	}
}

UDataTable* UChainBootstrapSubsystem::Load(const TSoftObjectPtr<UDataTable>& Table,
	const TCHAR* Label, UScriptStruct* RowStruct)
{
	if (!Table.IsNull())
	{
		if (UDataTable* Loaded = Table.LoadSynchronous())
		{
			return Loaded;
		}
	}

	// No asset, or one that would not load. Fall back to the JSON it came from.
	return LoadFromJson(Table.GetAssetName(), Label, RowStruct);
}

UDataTable* UChainBootstrapSubsystem::LoadFromJson(const FString& AssetName,
	const TCHAR* Label, UScriptStruct* RowStruct)
{
	if (AssetName.IsEmpty() || !RowStruct)
	{
		UE_LOG(LogChainBootstrap, Warning,
			TEXT("Nothing assigned for %s, and no JSON to fall back to."), Label);
		return nullptr;
	}

	const FString Path = FPaths::ProjectContentDir() / TEXT("Data") / (AssetName + TEXT(".json"));

	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *Path))
	{
		UE_LOG(LogChainBootstrap, Warning, TEXT("No %s table: no asset, and no file at '%s'."),
			Label, *Path);
		return nullptr;
	}

	UDataTable* Table = NewObject<UDataTable>(this);
	Table->RowStruct = RowStruct;

	// Every problem here is a row the game will not have. They are warnings rather
	// than a hard failure because one malformed row should not cost the other 195.
	const TArray<FString> Problems = Table->CreateTableFromJSONString(Json);
	for (const FString& Problem : Problems)
	{
		UE_LOG(LogChainBootstrap, Warning, TEXT("%s: %s"), Label, *Problem);
	}

	UE_LOG(LogChainBootstrap, Log, TEXT("Read %s from JSON: %d rows."),
		Label, Table->GetRowMap().Num());
	return Table;
}

int32 UChainBootstrapSubsystem::RegisterAllContent()
{
	const UChainContentSettings* Settings = UChainContentSettings::Get();
	if (!Settings)
	{
		UE_LOG(LogChainBootstrap, Error, TEXT("No content settings; nothing can be registered."));
		return 0;
	}

	UGameInstance* GameInstance = GetGameInstance();
	if (!GameInstance)
	{
		return 0;
	}

	int32 Registered = 0;

	// The timeline first: every other system asks it what year it is, and a system
	// that registers content before the dates are in will read year zero.
	if (UCampaignTimelineSubsystem* Timeline = GameInstance->GetSubsystem<UCampaignTimelineSubsystem>())
	{
		if (UDataTable* Table = Load(Settings->CampaignEvents, TEXT("campaign events"), FCampaignEventRow::StaticStruct()))
		{
			Timeline->SetTimelineTable(Table);
			Registered += Table->GetRowMap().Num();
		}
	}

	if (UCodexSubsystem* Codex = GameInstance->GetSubsystem<UCodexSubsystem>())
	{
		if (UDataTable* Table = Load(Settings->TestimonyFragments, TEXT("testimony fragments"), FTestimonyFragmentDefinition::StaticStruct()))
		{
			Registered += Codex->RegisterFragmentsFromDataTable(Table);
		}
	}

	if (UCampaignMapSubsystem* Map = GameInstance->GetSubsystem<UCampaignMapSubsystem>())
	{
		if (UDataTable* Table = Load(Settings->Locations, TEXT("locations"), FCampaignLocationRow::StaticStruct()))
		{
			Registered += Map->RegisterLocationTable(Table);
		}
		if (UDataTable* Table = Load(Settings->TravelRoutes, TEXT("travel routes"), FTravelRouteRow::StaticStruct()))
		{
			Registered += Map->RegisterRouteTable(Table);
		}
		if (UDataTable* Table = Load(Settings->TravelEncounterTypes, TEXT("travel encounter types"), FEncounterTypeRow::StaticStruct()))
		{
			Registered += Map->RegisterEncounterTypeTable(Table);
		}
	}

	if (UReputationSubsystem* Reputation = GameInstance->GetSubsystem<UReputationSubsystem>())
	{
		if (UDataTable* Table = Load(Settings->Factions, TEXT("factions"), FFactionRow::StaticStruct()))
		{
			Registered += Reputation->RegisterFactionTable(Table);
		}
		if (UDataTable* Table = Load(Settings->DeedTypes, TEXT("deed types"), FDeedTypeRow::StaticStruct()))
		{
			Registered += Reputation->RegisterDeedTypeTable(Table);
		}
	}

	if (UDialogueSubsystem* Dialogue = GameInstance->GetSubsystem<UDialogueSubsystem>())
	{
		for (const TSoftObjectPtr<UDataTable>& Conversation : Settings->DialogueTables)
		{
			if (UDataTable* Table = Load(Conversation, TEXT("dialogue"), FDialogueNodeRow::StaticStruct()))
			{
				Registered += Dialogue->RegisterDialogueTable(Table);
			}
		}
	}

	if (UDebateSubsystem* Debate = GameInstance->GetSubsystem<UDebateSubsystem>())
	{
		if (UDataTable* Table = Load(Settings->DebateOpponents, TEXT("debate opponents"), FDebateOpponentRow::StaticStruct()))
		{
			Registered += Debate->RegisterOpponentTable(Table);
		}
		if (UDataTable* Table = Load(Settings->DebateObjections, TEXT("debate objections"), FDebateObjectionRow::StaticStruct()))
		{
			Registered += Debate->RegisterObjectionTable(Table);
		}
	}

	if (UWitnessMissionSubsystem* Witness = GameInstance->GetSubsystem<UWitnessMissionSubsystem>())
	{
		if (UDataTable* Table = Load(Settings->Eras, TEXT("eras"), FEraDefinitionRow::StaticStruct()))
		{
			Registered += Witness->RegisterEraTable(Table);
		}
		if (UDataTable* Table = Load(Settings->WitnessMissions, TEXT("witness missions"), FWitnessMissionRow::StaticStruct()))
		{
			Registered += Witness->RegisterMissionTable(Table);
		}
	}

	if (UCombatEncounterSubsystem* Combat = GameInstance->GetSubsystem<UCombatEncounterSubsystem>())
	{
		if (UDataTable* Table = Load(Settings->CombatEncounters, TEXT("combat encounters"), FCombatEncounterRow::StaticStruct()))
		{
			Registered += Combat->RegisterEncounterTable(Table);
		}
	}

	bContentRegistered = Registered > 0;

	UE_LOG(LogChainBootstrap, Log, TEXT("Registered %d rows of campaign content."), Registered);

	if (Registered == 0)
	{
		UE_LOG(LogChainBootstrap, Warning,
			TEXT("Nothing was registered. The DataTables are probably not assigned yet: "
				 "Project Settings -> Game -> Chain of Witnesses Content."));
	}

	// Validation has to come after everything, or it reports cross-references to
	// tables that simply have not loaded yet.
	if (Settings->bValidateContentOnStartup)
	{
		TArray<FString> Problems;
		ValidateAllContent(Problems);
	}

	ApplyStartingState();

	return Registered;
}

int32 UChainBootstrapSubsystem::ValidateAllContent(TArray<FString>& OutProblems) const
{
	OutProblems.Reset();

	const UGameInstance* GameInstance = GetGameInstance();
	if (!GameInstance)
	{
		return 0;
	}

	TArray<FString> Found;

	// The Codex reports dangling fragment references as FNames rather than
	// sentences, so it is the one that needs translating into the common form.
	if (const UCodexSubsystem* Codex = GameInstance->GetSubsystem<UCodexSubsystem>())
	{
		TArray<FName> Dangling;
		Codex->ValidateRegistry(Dangling);
		for (const FName Reference : Dangling)
		{
			OutProblems.Add(FString::Printf(
				TEXT("Codex: '%s' is referenced by a fragment but is not registered."),
				*Reference.ToString()));
		}
	}
	if (const UDialogueSubsystem* Dialogue = GameInstance->GetSubsystem<UDialogueSubsystem>())
	{
		Dialogue->ValidateDialogue(Found);
		OutProblems.Append(Found);
	}
	if (const UReputationSubsystem* Reputation = GameInstance->GetSubsystem<UReputationSubsystem>())
	{
		Reputation->ValidateReputationContent(Found);
		OutProblems.Append(Found);
	}
	if (const UCampaignMapSubsystem* Map = GameInstance->GetSubsystem<UCampaignMapSubsystem>())
	{
		Map->ValidateMapContent(Found);
		OutProblems.Append(Found);
	}
	if (const UDebateSubsystem* Debate = GameInstance->GetSubsystem<UDebateSubsystem>())
	{
		Debate->ValidateDebateContent(Found);
		OutProblems.Append(Found);
	}
	if (const UWitnessMissionSubsystem* Witness = GameInstance->GetSubsystem<UWitnessMissionSubsystem>())
	{
		Witness->ValidateWitnessContent(Found);
		OutProblems.Append(Found);
	}
	if (const UCombatEncounterSubsystem* Combat = GameInstance->GetSubsystem<UCombatEncounterSubsystem>())
	{
		Combat->ValidateCombatContent(Found);
		OutProblems.Append(Found);
	}

	if (OutProblems.Num() > 0)
	{
		UE_LOG(LogChainBootstrap, Warning, TEXT("%d content problem(s):"), OutProblems.Num());
		for (const FString& Problem : OutProblems)
		{
			UE_LOG(LogChainBootstrap, Warning, TEXT("  %s"), *Problem);
		}
	}
	else
	{
		UE_LOG(LogChainBootstrap, Log, TEXT("Content validated with no problems."));
	}

	return OutProblems.Num();
}

void UChainBootstrapSubsystem::ApplyStartingState()
{
	const UChainContentSettings* Settings = UChainContentSettings::Get();
	UGameInstance* GameInstance = GetGameInstance();
	if (!Settings || !GameInstance || Settings->StartingEraID.IsNone())
	{
		// Mode B: no frame era, so the campaign simply begins wherever the timeline
		// starts. Nothing to do.
		return;
	}

	if (UWitnessMissionSubsystem* Witness = GameInstance->GetSubsystem<UWitnessMissionSubsystem>())
	{
		Witness->BeginFrameEra(Settings->StartingEraID, Settings->StartingLocationID);

		UE_LOG(LogChainBootstrap, Log, TEXT("Began era '%s' at '%s'."),
			*Settings->StartingEraID.ToString(), *Settings->StartingLocationID.ToString());
	}
}
