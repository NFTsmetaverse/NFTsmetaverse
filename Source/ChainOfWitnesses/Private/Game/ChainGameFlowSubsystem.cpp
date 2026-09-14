#include "Game/ChainGameFlowSubsystem.h"

#include "Game/ChainNpc.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"

#include "Bootstrap/ChainContentSettings.h"
#include "Campaign/CampaignTimelineSubsystem.h"
#include "Codex/CodexSubsystem.h"
#include "Codex/ChainCodexTypes.h"
#include "Combat/CombatEncounterSubsystem.h"
#include "Combat/ChainCombatTypes.h"
#include "Debate/DebateSubsystem.h"
#include "Debate/ChainDebateTypes.h"
#include "Dialogue/DialogueSubsystem.h"
#include "Dialogue/ChainDialogueTypes.h"
#include "Engine/GameInstance.h"
#include "Map/CampaignMapSubsystem.h"
#include "Map/ChainCampaignMapTypes.h"
#include "Reputation/ReputationSubsystem.h"
#include "UI/CodexViewLibrary.h"
#include "UI/ChainCodexViewTypes.h"
#include "Witness/WitnessMissionSubsystem.h"
#include "Witness/ChainWitnessMissionTypes.h"

DEFINE_LOG_CATEGORY(LogChainFlow);

namespace ChainFlowPrivate
{
	/**
	 * Enum names come from reflection rather than a hand-written switch. A switch
	 * here would be a second copy of every enumerator in the project, and the copy
	 * would rot the first time one was renamed.
	 */
	template <typename TEnum>
	FString EnumName(TEnum Value)
	{
		if (const UEnum* Enum = StaticEnum<TEnum>())
		{
			return Enum->GetNameStringByValue(static_cast<int64>(Value));
		}
		return TEXT("?");
	}

	/** Locations are referred to by ID everywhere; this is the only place that prefers the display name. */
	FString LocationLabel(const UCampaignMapSubsystem* Map, FName LocationID)
	{
		if (LocationID.IsNone())
		{
			return TEXT("nowhere");
		}

		FCampaignLocationRow Row;
		if (Map && Map->GetLocation(LocationID, Row) && !Row.DisplayName.IsEmpty())
		{
			return Row.DisplayName.ToString();
		}
		return LocationID.ToString();
	}
}

UCodexSubsystem* UChainGameFlowSubsystem::GetCodex() const
{
	if (CodexOverride)
	{
		return CodexOverride;
	}

	UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UCodexSubsystem>() : nullptr;
}

UDialogueSubsystem* UChainGameFlowSubsystem::GetDialogue() const
{
	if (DialogueOverride)
	{
		return DialogueOverride;
	}

	UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UDialogueSubsystem>() : nullptr;
}

UWitnessMissionSubsystem* UChainGameFlowSubsystem::GetWitness() const
{
	if (WitnessOverride)
	{
		return WitnessOverride;
	}

	UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UWitnessMissionSubsystem>() : nullptr;
}

UCampaignMapSubsystem* UChainGameFlowSubsystem::GetMap() const
{
	if (MapOverride)
	{
		return MapOverride;
	}

	UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UCampaignMapSubsystem>() : nullptr;
}

UCampaignTimelineSubsystem* UChainGameFlowSubsystem::GetTimeline() const
{
	if (TimelineOverride)
	{
		return TimelineOverride;
	}

	UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UCampaignTimelineSubsystem>() : nullptr;
}

UDebateSubsystem* UChainGameFlowSubsystem::GetDebate() const
{
	if (DebateOverride)
	{
		return DebateOverride;
	}

	UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UDebateSubsystem>() : nullptr;
}

UCombatEncounterSubsystem* UChainGameFlowSubsystem::GetCombat() const
{
	if (CombatOverride)
	{
		return CombatOverride;
	}

	UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UCombatEncounterSubsystem>() : nullptr;
}

UReputationSubsystem* UChainGameFlowSubsystem::GetReputation() const
{
	if (ReputationOverride)
	{
		return ReputationOverride;
	}

	UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UReputationSubsystem>() : nullptr;
}


void UChainGameFlowSubsystem::SetDependenciesForTesting(UCodexSubsystem* InCodex,
	UDialogueSubsystem* InDialogue, UWitnessMissionSubsystem* InWitness, UCampaignMapSubsystem* InMap,
	UCampaignTimelineSubsystem* InTimeline, UDebateSubsystem* InDebate,
	UCombatEncounterSubsystem* InCombat, UReputationSubsystem* InReputation)
{
	CodexOverride = InCodex;
	DialogueOverride = InDialogue;
	WitnessOverride = InWitness;
	MapOverride = InMap;
	TimelineOverride = InTimeline;
	DebateOverride = InDebate;
	CombatOverride = InCombat;
	ReputationOverride = InReputation;
}

AChainNpc* UChainGameFlowSubsystem::PlaceNpc(FName NpcID, FName RootNodeID,
	EConversationSafety Safety)
{
	UGameInstance* GI = GetGameInstance();
	UWorld* World = GI ? GI->GetWorld() : nullptr;
	if (!World)
	{
		return nullptr;
	}

	APawn* Player = World->GetFirstPlayerController()
		? World->GetFirstPlayerController()->GetPawn()
		: nullptr;
	if (!Player)
	{
		return nullptr;
	}

	// Three metres ahead and facing you: close enough to be obviously the thing to
	// walk to, far enough that you are not standing inside them.
	const FVector Where = Player->GetActorLocation() + Player->GetActorForwardVector() * 300.f;
	const FRotator Facing = (Player->GetActorLocation() - Where).Rotation();

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	AChainNpc* Npc = World->SpawnActor<AChainNpc>(AChainNpc::StaticClass(), Where, Facing, Params);
	if (!Npc)
	{
		return nullptr;
	}

	Npc->NpcID = NpcID;
	Npc->RootNodeID = RootNodeID;
	Npc->Safety = Safety;
	Npc->DisplayName = FText::FromName(NpcID);
	PlacedNpcs.Add(Npc);
	return Npc;
}

void UChainGameFlowSubsystem::Report(const FString& Message)
{
	LastMessage = Message;
	UE_LOG(LogChainFlow, Log, TEXT("%s"), *Message);
}

// --- Campaign ---------------------------------------------------------------

bool UChainGameFlowSubsystem::StartNewCampaign()
{
	UWitnessMissionSubsystem* Witness = GetWitness();
	if (!Witness)
	{
		Report(TEXT("No witness mission subsystem; cannot start a campaign."));
		return false;
	}

	const UChainContentSettings* Settings = UChainContentSettings::Get();
	const FName EraID = Settings ? Settings->StartingEraID : FName(TEXT("Era_Frame"));
	const FName LocationID = Settings ? Settings->StartingLocationID : FName(TEXT("Loc_Jerusalem"));

	Witness->BeginFrameEra(EraID, LocationID);

	const UCampaignTimelineSubsystem* Timeline = GetTimeline();
	const int32 Year = Timeline ? Timeline->GetCurrentYearAD() : 0;

	Report(FString::Printf(TEXT("Campaign opened in %s, AD %d."),
		*ChainFlowPrivate::LocationLabel(GetMap(), LocationID), Year));
	return true;
}

bool UChainGameFlowSubsystem::StartMissionOpeningScene(FName MissionID)
{
	UWitnessMissionSubsystem* Witness = GetWitness();
	UDialogueSubsystem* Dialogue = GetDialogue();
	if (!Witness || !Dialogue)
	{
		return false;
	}

	FWitnessMissionRow Mission;
	if (!Witness->GetMission(MissionID, Mission))
	{
		return false;
	}

	// A mission is allowed to have no authored opening scene; that is a mission the
	// player walks into, not a failure.
	if (Mission.OpeningDialogueNodeID.IsNone() || Mission.OpeningNpcID.IsNone())
	{
		return false;
	}

	return Dialogue->StartConversation(Mission.OpeningNpcID, Mission.OpeningDialogueNodeID,
		Mission.OpeningSafety);
}

bool UChainGameFlowSubsystem::OpenMission(FName MissionID)
{
	UWitnessMissionSubsystem* Witness = GetWitness();
	if (!Witness)
	{
		Report(TEXT("No witness mission subsystem."));
		return false;
	}

	FWitnessMissionRow Mission;
	if (!Witness->GetMission(MissionID, Mission))
	{
		Report(FString::Printf(TEXT("No such mission: %s"), *MissionID.ToString()));
		return false;
	}

	if (!Witness->IsMissionAvailable(MissionID))
	{
		Report(FString::Printf(
			TEXT("%s is shut. It opens when its source is recovered%s."),
			*MissionID.ToString(),
			Mission.UnlockedByFragmentID.IsNone()
				? TEXT("")
				: *FString::Printf(TEXT(" (%s)"), *Mission.UnlockedByFragmentID.ToString())));
		return false;
	}

	if (!Witness->EnterMission(MissionID))
	{
		Report(FString::Printf(TEXT("Could not enter %s."), *MissionID.ToString()));
		return false;
	}

	const UCampaignTimelineSubsystem* Timeline = GetTimeline();
	const FString Where = ChainFlowPrivate::LocationLabel(GetMap(), Mission.MissionLocationID);
	const int32 Year = Timeline ? Timeline->GetCurrentYearAD() : Mission.MissionYearAD;

	// When the speaker can be put in the world, walking to them is the game and
	// opening the scene for the player would skip it. When there is no world to put
	// them in -- a headless run, the automation tests, console-only play -- the
	// scene has to open directly or the mission is unreachable.
	const AChainNpc* Placed = PlaceNpc(Mission.OpeningNpcID, Mission.OpeningDialogueNodeID,
		Mission.OpeningSafety);

	if (Placed)
	{
		Report(FString::Printf(TEXT("%s, AD %d. %s is waiting."),
			*Where, Year, *Mission.OpeningNpcID.ToString()));
	}
	else if (StartMissionOpeningScene(MissionID))
	{
		Report(FString::Printf(TEXT("%s, AD %d. The scene opens."), *Where, Year));
	}
	else
	{
		Report(FString::Printf(TEXT("%s, AD %d. No opening scene authored."), *Where, Year));
	}
	return true;
}

bool UChainGameFlowSubsystem::ForceOpenMission(FName MissionID)
{
	UWitnessMissionSubsystem* Witness = GetWitness();
	UCodexSubsystem* Codex = GetCodex();
	if (!Witness || !Codex)
	{
		Report(TEXT("Subsystems unavailable."));
		return false;
	}

	FWitnessMissionRow Mission;
	if (!Witness->GetMission(MissionID, Mission))
	{
		Report(FString::Printf(TEXT("No such mission: %s"), *MissionID.ToString()));
		return false;
	}

	if (!Mission.UnlockedByFragmentID.IsNone() && !Codex->IsFragmentRecovered(Mission.UnlockedByFragmentID))
	{
		Codex->RecoverFragment(Mission.UnlockedByFragmentID);
		Report(FString::Printf(TEXT("Granted %s to open the mission."),
			*Mission.UnlockedByFragmentID.ToString()));
	}

	return OpenMission(MissionID);
}

bool UChainGameFlowSubsystem::CompleteMission()
{
	UWitnessMissionSubsystem* Witness = GetWitness();
	if (!Witness || !Witness->IsMissionActive())
	{
		Report(TEXT("No mission is running."));
		return false;
	}

	if (UDialogueSubsystem* Dialogue = GetDialogue())
	{
		if (Dialogue->IsConversationActive())
		{
			Dialogue->EndConversation();
		}
	}

	for (AChainNpc* Npc : PlacedNpcs)
	{
		if (Npc)
		{
			Npc->Destroy();
		}
	}
	PlacedNpcs.Reset();

	const FName Finished = Witness->GetActiveMissionID();
	if (!Witness->CompleteMission())
	{
		Report(TEXT("The mission would not complete."));
		return false;
	}

	const UCampaignTimelineSubsystem* Timeline = GetTimeline();
	Report(FString::Printf(TEXT("%s complete. Back in the frame era, AD %d."),
		*Finished.ToString(), Timeline ? Timeline->GetCurrentYearAD() : 0));
	return true;
}

// --- Dialogue ---------------------------------------------------------------

bool UChainGameFlowSubsystem::StartConversation(FName NpcID, FName RootNodeID,
	EConversationSafety Safety)
{
	UDialogueSubsystem* Dialogue = GetDialogue();
	if (!Dialogue)
	{
		Report(TEXT("No dialogue subsystem."));
		return false;
	}

	if (!Dialogue->StartConversation(NpcID, RootNodeID, Safety))
	{
		Report(FString::Printf(TEXT("%s will not open that conversation here."), *NpcID.ToString()));
		return false;
	}

	Report(FString::Printf(TEXT("Talking to %s."), *NpcID.ToString()));
	return true;
}

bool UChainGameFlowSubsystem::Say(int32 OptionIndex)
{
	UDialogueSubsystem* Dialogue = GetDialogue();
	if (!Dialogue || !Dialogue->IsConversationActive())
	{
		Report(TEXT("Nobody is talking."));
		return false;
	}

	FDialogueNodeView View;
	Dialogue->GetCurrentNodeView(View);

	// Locked options are shown as well as selectable ones, so the index the player
	// typed has to be checked against what is actually open, not just the range.
	const FDialogueOptionView* Chosen = View.Options.FindByPredicate(
		[OptionIndex](const FDialogueOptionView& Option) { return Option.OptionIndex == OptionIndex; });

	if (!Chosen)
	{
		Report(FString::Printf(TEXT("There is no option %d."), OptionIndex + 1));
		return false;
	}

	if (!Chosen->bIsAvailable)
	{
		Report(FString::Printf(TEXT("That line is closed to you: %s"),
			*ChainFlowPrivate::EnumName(Chosen->Gate)));
		return false;
	}

	if (!Dialogue->SelectOption(OptionIndex))
	{
		Report(TEXT("The line would not play."));
		return false;
	}

	if (!Dialogue->IsConversationActive())
	{
		Report(TEXT("The conversation is over."));
	}
	return true;
}

// --- Travel -----------------------------------------------------------------

bool UChainGameFlowSubsystem::TravelTo(FName DestinationLocationID, bool bRiskOutOfSeasonSailing)
{
	UCampaignMapSubsystem* Map = GetMap();
	if (!Map)
	{
		Report(TEXT("No map subsystem."));
		return false;
	}

	FCampaignLocationRow Row;
	if (!Map->GetLocation(DestinationLocationID, Row))
	{
		Report(FString::Printf(TEXT("No such place: %s"), *DestinationLocationID.ToString()));
		return false;
	}

	if (!Map->SetDestination(DestinationLocationID, bRiskOutOfSeasonSailing))
	{
		Report(FString::Printf(TEXT("No route to %s from here%s."),
			*ChainFlowPrivate::LocationLabel(Map, DestinationLocationID),
			bRiskOutOfSeasonSailing ? TEXT("") : TEXT(" this season")));
		return false;
	}

	Report(FString::Printf(TEXT("Bound for %s."),
		*ChainFlowPrivate::LocationLabel(Map, DestinationLocationID)));
	return true;
}

void UChainGameFlowSubsystem::AdvanceDays(int32 Days)
{
	if (Days <= 0)
	{
		Report(TEXT("Days must be positive."));
		return;
	}

	UCampaignTimelineSubsystem* Timeline = GetTimeline();
	UCampaignMapSubsystem* Map = GetMap();

	if (Timeline)
	{
		Timeline->AdvanceDays(Days);
	}

	if (Map)
	{
		const FTravelResult Result = Map->AdvanceDays(Days);
		if (!Result.EncounterID.IsNone())
		{
			Report(FString::Printf(TEXT("%d days: %s on the road (%s)."),
				Result.DaysElapsed,
				*ChainFlowPrivate::EnumName(Result.Outcome),
				*Result.EncounterID.ToString()));
			return;
		}

		Report(FString::Printf(TEXT("%d days: %s. %s."),
			Result.DaysElapsed,
			*ChainFlowPrivate::EnumName(Result.Outcome),
			*ChainFlowPrivate::LocationLabel(Map, Map->GetPartyLocation())));
		return;
	}

	Report(FString::Printf(TEXT("%d days pass."), Days));
}

// --- Encounters -------------------------------------------------------------

bool UChainGameFlowSubsystem::StartDebate(FName OpponentID, FName AnchorEventID)
{
	UDebateSubsystem* Debate = GetDebate();
	if (!Debate)
	{
		Report(TEXT("No debate subsystem."));
		return false;
	}

	if (!Debate->StartDebate(OpponentID, AnchorEventID))
	{
		Report(FString::Printf(TEXT("%s will not be drawn on %s."),
			*OpponentID.ToString(), *AnchorEventID.ToString()));
		return false;
	}

	Report(FString::Printf(TEXT("%s challenges you on %s."),
		*OpponentID.ToString(), *AnchorEventID.ToString()));
	return true;
}

bool UChainGameFlowSubsystem::StartCombat(FName EncounterTypeID)
{
	UCombatEncounterSubsystem* Combat = GetCombat();
	if (!Combat)
	{
		Report(TEXT("No combat subsystem."));
		return false;
	}

	if (!Combat->BeginEncounter(EncounterTypeID))
	{
		Report(FString::Printf(TEXT("No such encounter: %s"), *EncounterTypeID.ToString()));
		return false;
	}

	Report(FString::Printf(TEXT("Trouble: %s."), *EncounterTypeID.ToString()));
	return true;
}

// --- Screen -----------------------------------------------------------------

EChainScreen UChainGameFlowSubsystem::GetScreen() const
{
	if (bCodexOpen)
	{
		return EChainScreen::Codex;
	}

	const UCombatEncounterSubsystem* Combat = GetCombat();
	if (Combat && Combat->IsEncounterActive())
	{
		return EChainScreen::Combat;
	}

	const UDebateSubsystem* Debate = GetDebate();
	if (Debate && Debate->IsDebateActive())
	{
		return EChainScreen::Debate;
	}

	const UDialogueSubsystem* Dialogue = GetDialogue();
	if (Dialogue && Dialogue->IsConversationActive())
	{
		return EChainScreen::Dialogue;
	}

	return EChainScreen::Campaign;
}

void UChainGameFlowSubsystem::ToggleCodex()
{
	bCodexOpen = !bCodexOpen;
}

bool UChainGameFlowSubsystem::Back()
{
	if (bCodexOpen)
	{
		bCodexOpen = false;
		return true;
	}

	// Combat is deliberately not backed out of. Leaving a fight is a move the combat
	// system arbitrates -- escape, de-escalation, defeat -- not a menu action.
	if (const UCombatEncounterSubsystem* Combat = GetCombat())
	{
		if (Combat->IsEncounterActive())
		{
			Report(TEXT("You cannot simply walk out of a fight."));
			return false;
		}
	}

	if (UDebateSubsystem* Debate = GetDebate())
	{
		if (Debate->IsDebateActive())
		{
			Debate->AbandonDebate();
			Report(TEXT("You break off the argument."));
			return true;
		}
	}

	if (UDialogueSubsystem* Dialogue = GetDialogue())
	{
		if (Dialogue->IsConversationActive())
		{
			Dialogue->EndConversation();
			Report(TEXT("You take your leave."));
			return true;
		}
	}

	return false;
}

// --- Reporting --------------------------------------------------------------

void UChainGameFlowSubsystem::GetStatusLines(TArray<FString>& OutLines) const
{
	const UCampaignTimelineSubsystem* Timeline = GetTimeline();
	const UCampaignMapSubsystem* Map = GetMap();
	const UWitnessMissionSubsystem* Witness = GetWitness();
	const UCodexSubsystem* Codex = GetCodex();

	if (Timeline)
	{
		OutLines.Add(FString::Printf(TEXT("AD %d, day %d  |  %s"),
			Timeline->GetCurrentYearAD(),
			Timeline->GetCurrentDayOfYear(),
			*ChainFlowPrivate::EnumName(Timeline->GetCurrentAct())));
	}

	if (Map)
	{
		const FPartyState Party = Map->GetPartyState();
		if (Party.bIsTravelling)
		{
			OutLines.Add(FString::Printf(TEXT("On the road to %s  |  %d days out"),
				*ChainFlowPrivate::LocationLabel(Map, Party.DestinationLocationID),
				Party.DaysRemainingOnLeg));
		}
		else
		{
			OutLines.Add(FString::Printf(TEXT("At %s"),
				*ChainFlowPrivate::LocationLabel(Map, Party.CurrentLocationID)));
		}
	}

	if (Witness)
	{
		if (Witness->IsMissionActive())
		{
			OutLines.Add(FString::Printf(TEXT("Witness Mission: %s"),
				*Witness->GetActiveMissionID().ToString()));
		}
		else
		{
			OutLines.Add(FString::Printf(TEXT("Era: %s"),
				*ChainFlowPrivate::EnumName(Witness->GetCurrentEra())));
		}
	}

	if (Codex)
	{
		TArray<FName> Recovered;
		Codex->GetRecoveredFragmentIDs(Recovered);
		const float Strength = Codex->GetCampaignAttestationStrength();
		OutLines.Add(FString::Printf(TEXT("Evidence: %d fragments  |  attestation %.2f (%s)"),
			Recovered.Num(),
			Strength,
			*UCodexViewLibrary::GetBandName(
				UCodexViewLibrary::BandForAttestationStrength(Strength)).ToString()));
	}
}

void UChainGameFlowSubsystem::GetCodexLines(TArray<FString>& OutLines) const
{
	const UCodexSubsystem* Codex = GetCodex();
	if (!Codex)
	{
		OutLines.Add(TEXT("No codex."));
		return;
	}

	TArray<FName> Recovered;
	Codex->GetRecoveredFragmentIDs(Recovered);

	OutLines.Add(FString::Printf(TEXT("--- Recovered testimony (%d) ---"), Recovered.Num()));
	if (Recovered.Num() == 0)
	{
		OutLines.Add(TEXT("Nothing yet. The chain begins where you find it."));
	}

	for (const FName& FragmentID : Recovered)
	{
		FTestimonyFragmentDefinition Definition;
		if (Codex->GetFragmentDefinition(FragmentID, Definition))
		{
			OutLines.Add(FString::Printf(TEXT("  %s  [%s, %s]%s"),
				*FragmentID.ToString(),
				*UCodexViewLibrary::GetTierName(Definition.Tier).ToString(),
				*UCodexViewLibrary::FormatYearAD(Definition.AttestationDateAD).ToString(),
				Definition.bIsContested ? TEXT("  (contested)") : TEXT("")));
		}
		else
		{
			OutLines.Add(FString::Printf(TEXT("  %s"), *FragmentID.ToString()));
		}
	}

	TArray<FName> Anchors;
	Codex->GetChainAnchorEventIDs(Anchors);
	if (Anchors.Num() > 0)
	{
		OutLines.Add(FString::Printf(TEXT("--- Chains (%d) ---"), Anchors.Num()));
	}

	for (const FName& AnchorID : Anchors)
	{
		const FChainScoreBreakdown Score = Codex->ScoreChain(AnchorID);
		OutLines.Add(FString::Printf(TEXT("  %s  strength %.2f  weakest: %s%s"),
			*AnchorID.ToString(),
			Score.AttestationStrength,
			*UCodexViewLibrary::GetLinkName(Score.WeakestLink).ToString(),
			Score.bIsComplete ? TEXT("  (complete)") : TEXT("")));
	}
}
