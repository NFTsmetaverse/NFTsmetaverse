#include "Witness/WitnessMissionSubsystem.h"

#include "Campaign/CampaignTimelineSubsystem.h"
#include "Codex/CodexSubsystem.h"
#include "Dialogue/DialogueSubsystem.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "Map/CampaignMapSubsystem.h"
#include "Reputation/ReputationSubsystem.h"

DEFINE_LOG_CATEGORY(LogWitnessMission);

void UWitnessMissionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UWitnessMissionSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

void UWitnessMissionSubsystem::SetDependenciesForTesting(UCodexSubsystem* InCodex,
	UCampaignTimelineSubsystem* InTimeline, UReputationSubsystem* InReputation,
	UCampaignMapSubsystem* InMap, UDialogueSubsystem* InDialogue)
{
	CodexOverride = InCodex;
	TimelineOverride = InTimeline;
	ReputationOverride = InReputation;
	MapOverride = InMap;
	DialogueOverride = InDialogue;
}

UCodexSubsystem* UWitnessMissionSubsystem::GetCodex() const
{
	if (CodexOverride)
	{
		return CodexOverride;
	}

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		return GameInstance->GetSubsystem<UCodexSubsystem>();
	}

	return nullptr;
}

UCampaignTimelineSubsystem* UWitnessMissionSubsystem::GetTimeline() const
{
	if (TimelineOverride)
	{
		return TimelineOverride;
	}

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		return GameInstance->GetSubsystem<UCampaignTimelineSubsystem>();
	}

	return nullptr;
}

UReputationSubsystem* UWitnessMissionSubsystem::GetReputation() const
{
	if (ReputationOverride)
	{
		return ReputationOverride;
	}

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		return GameInstance->GetSubsystem<UReputationSubsystem>();
	}

	return nullptr;
}

UCampaignMapSubsystem* UWitnessMissionSubsystem::GetMap() const
{
	if (MapOverride)
	{
		return MapOverride;
	}

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		return GameInstance->GetSubsystem<UCampaignMapSubsystem>();
	}

	return nullptr;
}

UDialogueSubsystem* UWitnessMissionSubsystem::GetDialogue() const
{
	if (DialogueOverride)
	{
		return DialogueOverride;
	}

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		return GameInstance->GetSubsystem<UDialogueSubsystem>();
	}

	return nullptr;
}

// --- Content ----------------------------------------------------------------

bool UWitnessMissionSubsystem::RegisterEra(const FEraDefinitionRow& Era)
{
	if (Era.EraID.IsNone() || Eras.Contains(Era.EraID))
	{
		return false;
	}

	Eras.Add(Era.EraID, Era);
	return true;
}

bool UWitnessMissionSubsystem::RegisterMission(const FWitnessMissionRow& Mission)
{
	if (Mission.MissionID.IsNone() || Missions.Contains(Mission.MissionID))
	{
		return false;
	}

	Missions.Add(Mission.MissionID, Mission);
	return true;
}

int32 UWitnessMissionSubsystem::RegisterEraTable(const UDataTable* EraTable)
{
	if (!EraTable)
	{
		return 0;
	}

	TArray<FEraDefinitionRow*> Rows;
	EraTable->GetAllRows<FEraDefinitionRow>(TEXT("UWitnessMissionSubsystem::RegisterEraTable"), Rows);

	int32 AddedCount = 0;
	for (const FEraDefinitionRow* Row : Rows)
	{
		if (Row && RegisterEra(*Row))
		{
			++AddedCount;
		}
	}

	UE_LOG(LogWitnessMission, Log, TEXT("Registered %d eras."), AddedCount);
	return AddedCount;
}

int32 UWitnessMissionSubsystem::RegisterMissionTable(const UDataTable* MissionTable)
{
	if (!MissionTable)
	{
		return 0;
	}

	TArray<FWitnessMissionRow*> Rows;
	MissionTable->GetAllRows<FWitnessMissionRow>(TEXT("UWitnessMissionSubsystem::RegisterMissionTable"), Rows);

	int32 AddedCount = 0;
	for (const FWitnessMissionRow* Row : Rows)
	{
		if (Row && RegisterMission(*Row))
		{
			++AddedCount;
		}
	}

	UE_LOG(LogWitnessMission, Log, TEXT("Registered %d witness missions."), AddedCount);
	return AddedCount;
}

bool UWitnessMissionSubsystem::GetEra(FName EraID, FEraDefinitionRow& OutEra) const
{
	if (const FEraDefinitionRow* Found = Eras.Find(EraID))
	{
		OutEra = *Found;
		return true;
	}

	return false;
}

bool UWitnessMissionSubsystem::GetMission(FName MissionID, FWitnessMissionRow& OutMission) const
{
	if (const FWitnessMissionRow* Found = Missions.Find(MissionID))
	{
		OutMission = *Found;
		return true;
	}

	return false;
}

void UWitnessMissionSubsystem::ValidateWitnessContent(TArray<FString>& OutProblems) const
{
	OutProblems.Reset();

	const UCodexSubsystem* Codex = GetCodex();
	const UCampaignTimelineSubsystem* Timeline = GetTimeline();

	for (const TPair<FName, FWitnessMissionRow>& Pair : Missions)
	{
		const FWitnessMissionRow& Mission = Pair.Value;
		const FString MissionName = Mission.MissionID.ToString();

		if (!Eras.Contains(Mission.EraID))
		{
			OutProblems.Add(FString::Printf(TEXT("Mission '%s' uses unregistered era '%s'."),
				*MissionName, *Mission.EraID.ToString()));
		}

		if (Timeline && !Mission.AnchorEventID.IsNone())
		{
			FCampaignEventRow AnchorRow;
			if (!Timeline->GetEventRow(Mission.AnchorEventID, AnchorRow))
			{
				OutProblems.Add(FString::Printf(TEXT("Mission '%s' anchors to unknown event '%s'."),
					*MissionName, *Mission.AnchorEventID.ToString()));
			}
			else if (Mission.MissionYearAD < AnchorRow.YearEarliestAD
				|| Mission.MissionYearAD > AnchorRow.YearLatestAD)
			{
				// The spine is the source of truth on dates; a mission set outside its
				// own anchor's defensible range is a content error, not a liberty.
				OutProblems.Add(FString::Printf(
					TEXT("Mission '%s' is set in AD %d, outside its anchor's range of AD %d-%d."),
					*MissionName, Mission.MissionYearAD, AnchorRow.YearEarliestAD, AnchorRow.YearLatestAD));
			}
		}

		if (Codex)
		{
			if (!Mission.UnlockedByFragmentID.IsNone()
				&& !Codex->IsFragmentRegistered(Mission.UnlockedByFragmentID))
			{
				OutProblems.Add(FString::Printf(TEXT("Mission '%s' is unlocked by unregistered fragment '%s'."),
					*MissionName, *Mission.UnlockedByFragmentID.ToString()));
			}

			for (const FName FragmentID : Mission.GrantsFragmentIDs)
			{
				if (!Codex->IsFragmentRegistered(FragmentID))
				{
					OutProblems.Add(FString::Printf(TEXT("Mission '%s' grants unregistered fragment '%s'."),
						*MissionName, *FragmentID.ToString()));
				}

				if (FragmentID == Mission.UnlockedByFragmentID)
				{
					OutProblems.Add(FString::Printf(
						TEXT("Mission '%s' is unlocked by the same fragment it grants."), *MissionName));
				}
			}
		}
	}
}

// --- Mode and era -----------------------------------------------------------

void UWitnessMissionSubsystem::SetStructuralMode(EStructuralMode Mode)
{
	if (State.bIsMissionActive)
	{
		UE_LOG(LogWitnessMission, Warning, TEXT("Refusing to change structural mode mid-mission."));
		return;
	}

	StructuralMode = Mode;

	if (Mode == EStructuralMode::FirstCenturyOnly)
	{
		// Mode B has no frame at all, so the player is in the first century from the
		// opening scene and never leaves it.
		State.CurrentEra = EGameEra::Witness;
	}
}

void UWitnessMissionSubsystem::SetEra(EGameEra NewEra, FName EraID)
{
	State.CurrentEra = NewEra;
	State.CurrentEraID = EraID;

	OnEraChanged.Broadcast(NewEra, EraID);
}

void UWitnessMissionSubsystem::BeginFrameEra(FName FrameEraID, FName StartingLocationID)
{
	if (StructuralMode == EStructuralMode::FirstCenturyOnly)
	{
		// Nothing to begin. Mode B's opening year is whatever the campaign is set to.
		return;
	}

	if (UCampaignTimelineSubsystem* Timeline = GetTimeline())
	{
		Timeline->SetDate(FrameYearAD, 0);
	}

	if (!StartingLocationID.IsNone())
	{
		if (UCampaignMapSubsystem* Map = GetMap())
		{
			Map->SetPartyLocation(StartingLocationID);
		}
	}

	SetEra(EGameEra::Frame, FrameEraID);
}

FEraCombatTuning UWitnessMissionSubsystem::GetActiveCombatTuning() const
{
	if (const FEraDefinitionRow* Era = Eras.Find(State.CurrentEraID))
	{
		return Era->CombatTuning;
	}

	return FEraCombatTuning();
}

// --- Missions ---------------------------------------------------------------

bool UWitnessMissionSubsystem::IsMissionAvailable(FName MissionID) const
{
	const FWitnessMissionRow* Mission = Missions.Find(MissionID);
	if (!Mission)
	{
		return false;
	}

	// Section 2: recovering the source is what opens the reconstruction.
	if (!Mission->UnlockedByFragmentID.IsNone())
	{
		const UCodexSubsystem* Codex = GetCodex();
		if (!Codex || !Codex->IsFragmentRecovered(Mission->UnlockedByFragmentID))
		{
			return false;
		}
	}

	// In Mode B nothing is being reconstructed -- the player is living it -- so the
	// scene has to have come round. In Mode A the frame sits after all of it.
	if (StructuralMode == EStructuralMode::FirstCenturyOnly)
	{
		const UCampaignTimelineSubsystem* Timeline = GetTimeline();
		if (!Timeline || Timeline->GetCurrentYearAD() < Mission->MissionYearAD)
		{
			return false;
		}
	}

	return true;
}

void UWitnessMissionSubsystem::GetAvailableMissionIDs(TArray<FName>& OutMissionIDs) const
{
	OutMissionIDs.Reset();

	for (const TPair<FName, FWitnessMissionRow>& Pair : Missions)
	{
		if (IsMissionAvailable(Pair.Key))
		{
			OutMissionIDs.Add(Pair.Key);
		}
	}
}

FFrameSnapshot UWitnessMissionSubsystem::CaptureFrame(bool bIncludeWorldState) const
{
	FFrameSnapshot Snapshot;
	Snapshot.EraID = State.CurrentEraID;

	if (const UCampaignTimelineSubsystem* Timeline = GetTimeline())
	{
		Snapshot.YearAD = Timeline->GetCurrentYearAD();
		Snapshot.DayOfYear = Timeline->GetCurrentDayOfYear();
	}

	if (const UCampaignMapSubsystem* Map = GetMap())
	{
		Snapshot.PartyLocationID = Map->GetPartyLocation();
	}

	if (bIncludeWorldState)
	{
		Snapshot.bHasWorldState = true;

		// Reusing each subsystem's save payload rather than a parallel rollback path:
		// a sealed mission unwinds by exactly the code a save file restores through.
		if (const UReputationSubsystem* Reputation = GetReputation())
		{
			Snapshot.Reputation = Reputation->CaptureSaveData();
		}

		if (const UDialogueSubsystem* Dialogue = GetDialogue())
		{
			Snapshot.Dialogue = Dialogue->CaptureSaveData();
		}
	}

	return Snapshot;
}

void UWitnessMissionSubsystem::RestoreFrame(const FFrameSnapshot& Snapshot)
{
	if (UCampaignTimelineSubsystem* Timeline = GetTimeline())
	{
		Timeline->SetDate(Snapshot.YearAD, Snapshot.DayOfYear);
	}

	if (UCampaignMapSubsystem* Map = GetMap())
	{
		Map->SetPartyLocation(Snapshot.PartyLocationID);
	}

	if (Snapshot.bHasWorldState)
	{
		if (UReputationSubsystem* Reputation = GetReputation())
		{
			Reputation->RestoreFromSaveData(Snapshot.Reputation);
		}

		if (UDialogueSubsystem* Dialogue = GetDialogue())
		{
			Dialogue->RestoreFromSaveData(Snapshot.Dialogue);
		}
	}
}

bool UWitnessMissionSubsystem::EnterMission(FName MissionID)
{
	if (State.bIsMissionActive)
	{
		UE_LOG(LogWitnessMission, Warning, TEXT("A mission is already running."));
		return false;
	}

	const FWitnessMissionRow* Mission = Missions.Find(MissionID);
	if (!Mission)
	{
		UE_LOG(LogWitnessMission, Warning, TEXT("No such mission '%s'."), *MissionID.ToString());
		return false;
	}

	if (!IsMissionAvailable(MissionID))
	{
		UE_LOG(LogWitnessMission, Warning, TEXT("Mission '%s' is not available yet."), *MissionID.ToString());
		return false;
	}

	// Mode B has no frame to seal off, and its "missions" are simply where the party
	// already is, so neither the snapshot nor the clock push applies there.
	const bool bIsDualEra = StructuralMode == EStructuralMode::DualEra;
	const bool bSealed = bIsDualEra && !Mission->bPersistsWorldState;

	State.Frame = CaptureFrame(bSealed);
	State.ActiveMissionID = MissionID;
	State.bIsMissionActive = true;

	if (bIsDualEra)
	{
		if (UCampaignTimelineSubsystem* Timeline = GetTimeline())
		{
			Timeline->SetDate(Mission->MissionYearAD, Mission->MissionDayOfYear);
		}

		if (!Mission->MissionLocationID.IsNone())
		{
			if (UCampaignMapSubsystem* Map = GetMap())
			{
				Map->SetPartyLocation(Mission->MissionLocationID);
			}
		}
	}

	SetEra(EGameEra::Witness, Mission->EraID);

	// The GameMode listens for this and does the travelling and possessing; the
	// level and the classes to use are on the mission and era rows.
	OnMissionEntered.Broadcast(MissionID);

	return true;
}

bool UWitnessMissionSubsystem::CompleteMission()
{
	return EndMission(EWitnessMissionOutcome::Completed);
}

bool UWitnessMissionSubsystem::AbandonMission()
{
	return EndMission(EWitnessMissionOutcome::Abandoned);
}

bool UWitnessMissionSubsystem::EndMission(EWitnessMissionOutcome Outcome)
{
	if (!State.bIsMissionActive)
	{
		return false;
	}

	const FName MissionID = State.ActiveMissionID;
	const FWitnessMissionRow* Mission = Missions.Find(MissionID);

	if (Outcome == EWitnessMissionOutcome::Completed && Mission)
	{
		// Granted before the frame is put back, because the Codex is deliberately not
		// part of what unwinds. Everything else about the reconstruction is undone;
		// what the player came out knowing is the whole point of having gone.
		if (UCodexSubsystem* Codex = GetCodex())
		{
			for (const FName FragmentID : Mission->GrantsFragmentIDs)
			{
				Codex->RecoverFragment(FragmentID);
			}
		}

		State.CompletedMissionIDs.AddUnique(MissionID);
	}

	if (StructuralMode == EStructuralMode::DualEra)
	{
		RestoreFrame(State.Frame);
		SetEra(EGameEra::Frame, State.Frame.EraID);
	}

	State.bIsMissionActive = false;
	State.ActiveMissionID = NAME_None;
	State.Frame = FFrameSnapshot();

	OnMissionEnded.Broadcast(MissionID, Outcome);

	return true;
}

bool UWitnessMissionSubsystem::HasCompletedMission(FName MissionID) const
{
	return State.CompletedMissionIDs.Contains(MissionID);
}

// --- Save/load --------------------------------------------------------------

FWitnessMissionSaveData UWitnessMissionSubsystem::CaptureSaveData() const
{
	FWitnessMissionSaveData SaveData;
	SaveData.State = State;
	SaveData.StructuralMode = StructuralMode;
	SaveData.FrameYearAD = FrameYearAD;

	return SaveData;
}

void UWitnessMissionSubsystem::RestoreFromSaveData(const FWitnessMissionSaveData& SaveData)
{
	// The live era travels inside State, so nothing has to be re-derived here.
	State = SaveData.State;
	StructuralMode = SaveData.StructuralMode;
	FrameYearAD = SaveData.FrameYearAD;
}
