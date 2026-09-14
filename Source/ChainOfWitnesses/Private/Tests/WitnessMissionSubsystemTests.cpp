#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "ChainAutomationFlags.h"
#include "ChainTestOuter.h"

#include "Campaign/CampaignTimelineSubsystem.h"
#include "Codex/CodexSubsystem.h"
#include "Dialogue/DialogueSubsystem.h"
#include "Map/CampaignMapSubsystem.h"
#include "Reputation/ReputationSubsystem.h"
#include "UObject/StrongObjectPtr.h"
#include "Witness/WitnessMissionSubsystem.h"

namespace WitnessTestHelpers
{
	const TCHAR* const FrameEra = TEXT("Era_Frame");
	const TCHAR* const WitnessEra = TEXT("Era_FirstCentury");
	const TCHAR* const Mission = TEXT("WM_Test");
	const TCHAR* const Jerusalem = TEXT("Loc_Jerusalem");
	const TCHAR* const Rome = TEXT("Loc_Rome");
	const TCHAR* const Church = TEXT("Faction_Church");

	constexpr int32 FrameYear = 1229;
	constexpr int32 MissionYear = 65;

	struct FWitnessFixture
	{
		TStrongObjectPtr<UWitnessMissionSubsystem> Witness;
		TStrongObjectPtr<UCodexSubsystem> Codex;
		TStrongObjectPtr<UCampaignTimelineSubsystem> Timeline;
		TStrongObjectPtr<UReputationSubsystem> Reputation;
		TStrongObjectPtr<UCampaignMapSubsystem> Map;
		TStrongObjectPtr<UDialogueSubsystem> Dialogue;

		FWitnessFixture()
			: Witness(NewObject<UWitnessMissionSubsystem>(ChainTestOuter()))
			, Codex(NewObject<UCodexSubsystem>(ChainTestOuter()))
			, Timeline(NewObject<UCampaignTimelineSubsystem>(ChainTestOuter()))
			, Reputation(NewObject<UReputationSubsystem>(ChainTestOuter()))
			, Map(NewObject<UCampaignMapSubsystem>(ChainTestOuter()))
			, Dialogue(NewObject<UDialogueSubsystem>(ChainTestOuter()))
		{
			Witness->SetDependenciesForTesting(Codex.Get(), Timeline.Get(), Reputation.Get(),
				Map.Get(), Dialogue.Get());
			Reputation->SetDependenciesForTesting(Timeline.Get(), Map.Get());
			Map->SetDependenciesForTesting(Timeline.Get(), Reputation.Get());
			Dialogue->SetDependenciesForTesting(Codex.Get(), Timeline.Get(), Reputation.Get());

			AddEra(FrameEra, EGameEra::Frame, /*bFormation=*/true, /*bEscapeWins=*/false, 1.f);
			AddEra(WitnessEra, EGameEra::Witness, /*bFormation=*/false, /*bEscapeWins=*/true, 2.5f);

			FFactionRow ChurchRow;
			ChurchRow.FactionID = Church;
			Reputation->RegisterFaction(ChurchRow);
		}

		void AddEra(const TCHAR* EraID, EGameEra Era, bool bFormation, bool bEscapeWins, float IncomingScale)
		{
			FEraDefinitionRow Row;
			Row.EraID = EraID;
			Row.Era = Era;
			Row.CombatTuning.bFormationCommandEnabled = bFormation;
			Row.CombatTuning.bEscapeIsWinState = bEscapeWins;
			Row.CombatTuning.IncomingDamageScale = IncomingScale;

			Witness->RegisterEra(Row);
		}

		void AddMission(const TCHAR* MissionID, const TArray<FName>& Grants,
			FName UnlockedBy = NAME_None, bool bPersists = false)
		{
			FWitnessMissionRow Row;
			Row.MissionID = MissionID;
			Row.MissionYearAD = MissionYear;
			Row.MissionDayOfYear = 150;
			Row.MissionLocationID = Rome;
			Row.EraID = WitnessEra;
			Row.GrantsFragmentIDs = Grants;
			Row.UnlockedByFragmentID = UnlockedBy;
			Row.bPersistsWorldState = bPersists;

			Witness->RegisterMission(Row);
		}

		void AddFragment(const TCHAR* ID)
		{
			FTestimonyFragmentDefinition Definition;
			Definition.FragmentID = ID;
			Definition.Tier = EWitnessTier::Companion;
			Definition.AttestationDateAD = 110;
			Codex->RegisterFragmentDefinition(Definition);
		}

		/** Puts the player in the frame era at Jerusalem, as Mode A startup would. */
		void BeginFrame()
		{
			Map->SetPartyLocation(Jerusalem);
			Witness->BeginFrameEra(FrameEra, Jerusalem);
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWitnessEraSwapTest,
	"ChainOfWitnesses.Witness.EnteringAMissionMovesTheClockAndTheControls",
	CHAIN_TEST_FLAGS)

bool FWitnessEraSwapTest::RunTest(const FString& Parameters)
{
	using namespace WitnessTestHelpers;

	FWitnessFixture Fixture;
	Fixture.AddFragment(TEXT("Frag_Yield"));
	Fixture.AddMission(Mission, { TEXT("Frag_Yield") });
	Fixture.BeginFrame();

	TestTrue(TEXT("Starts in the frame era"), Fixture.Witness->GetCurrentEra() == EGameEra::Frame);
	TestEqual(TEXT("In 1229"), Fixture.Timeline->GetCurrentYearAD(), FrameYear);
	TestTrue(TEXT("With the medieval tuning"),
		Fixture.Witness->GetActiveCombatTuning().bFormationCommandEnabled);

	TestTrue(TEXT("Mission entered"), Fixture.Witness->EnterMission(Mission));

	TestTrue(TEXT("Now in the witness era"), Fixture.Witness->GetCurrentEra() == EGameEra::Witness);
	TestEqual(TEXT("And in AD 65"), Fixture.Timeline->GetCurrentYearAD(), MissionYear);
	TestEqual(TEXT("On the right day"), Fixture.Timeline->GetCurrentDayOfYear(), 150);
	TestTrue(TEXT("At the mission's location"), Fixture.Map->GetPartyLocation() == FName(Rome));

	// Section 5's retuning: a different game, not the same game with smaller numbers.
	const FEraCombatTuning Tuning = Fixture.Witness->GetActiveCombatTuning();
	TestFalse(TEXT("No formation command in the first century"), Tuning.bFormationCommandEnabled);
	TestTrue(TEXT("Escape is a win state"), Tuning.bEscapeIsWinState);
	TestEqual(TEXT("And it is far more lethal"), Tuning.IncomingDamageScale, 2.5f, 0.01f);

	TestTrue(TEXT("Completed"), Fixture.Witness->CompleteMission());

	TestTrue(TEXT("Back in the frame era"), Fixture.Witness->GetCurrentEra() == EGameEra::Frame);
	TestEqual(TEXT("Back in 1229"), Fixture.Timeline->GetCurrentYearAD(), FrameYear);
	TestTrue(TEXT("Back in Jerusalem"), Fixture.Map->GetPartyLocation() == FName(Jerusalem));
	TestTrue(TEXT("With the medieval tuning again"),
		Fixture.Witness->GetActiveCombatTuning().bFormationCommandEnabled);

	// The evidence is the one thing that survives the return.
	TestTrue(TEXT("The fragment came back with him"),
		Fixture.Codex->IsFragmentRecovered(TEXT("Frag_Yield")));
	TestTrue(TEXT("And the mission is recorded as played"),
		Fixture.Witness->HasCompletedMission(Mission));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWitnessSealedMissionTest,
	"ChainOfWitnesses.Witness.AReconstructionChangesNothingButWhatIsLearned",
	CHAIN_TEST_FLAGS)

bool FWitnessSealedMissionTest::RunTest(const FString& Parameters)
{
	using namespace WitnessTestHelpers;

	FWitnessFixture Fixture;
	Fixture.AddFragment(TEXT("Frag_Yield"));
	Fixture.AddMission(Mission, { TEXT("Frag_Yield") });

	FDeedTypeRow Deed;
	Deed.DeedTypeID = TEXT("Deed_Test");
	Deed.bTravelsByWordOfMouth = false;
	FFactionImpact Impact;
	Impact.FactionID = Church;
	Impact.Delta = 25.f;
	Deed.FactionImpacts.Add(Impact);
	Fixture.Reputation->RegisterDeedType(Deed);

	Fixture.BeginFrame();

	// Standing the player built up in the thirteenth century.
	Fixture.Dialogue->ModifyTrust(TEXT("NPC_Preceptor"), 40.f);
	Fixture.Reputation->RecordDeed(TEXT("Deed_Test"), Jerusalem, TArray<FName>());

	const float FrameStanding = Fixture.Reputation->GetFactionReputation(Church);
	TestEqual(TEXT("Frame standing established"), FrameStanding, 25.f, 0.01f);

	Fixture.Witness->EnterMission(Mission);

	// Things done inside the reconstruction.
	Fixture.Reputation->RecordDeed(TEXT("Deed_Test"), Rome, TArray<FName>());
	Fixture.Dialogue->ModifyTrust(TEXT("NPC_Peter"), 60.f);
	TestEqual(TEXT("They register while the mission runs"),
		Fixture.Reputation->GetFactionReputation(Church), 50.f, 0.01f);

	Fixture.Witness->CompleteMission();

	// ...and are gone, because a reconstruction cannot change the thirteenth century.
	TestEqual(TEXT("Faction standing is as it was"),
		Fixture.Reputation->GetFactionReputation(Church), FrameStanding, 0.01f);
	TestEqual(TEXT("Trust earned inside the reconstruction did not follow him out"),
		Fixture.Dialogue->GetTrust(TEXT("NPC_Peter")), 0.f, 0.01f);
	TestEqual(TEXT("Trust earned in the frame era survived"),
		Fixture.Dialogue->GetTrust(TEXT("NPC_Preceptor")), 40.f, 0.01f);

	// The single deliberate exception.
	TestTrue(TEXT("The evidence is the exception"),
		Fixture.Codex->IsFragmentRecovered(TEXT("Frag_Yield")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWitnessUnlockAndAbandonTest,
	"ChainOfWitnesses.Witness.RecoveringTheSourceOpensTheReconstruction",
	CHAIN_TEST_FLAGS)

bool FWitnessUnlockAndAbandonTest::RunTest(const FString& Parameters)
{
	using namespace WitnessTestHelpers;

	FWitnessFixture Fixture;
	Fixture.AddFragment(TEXT("Frag_Source"));
	Fixture.AddFragment(TEXT("Frag_Yield"));
	Fixture.AddMission(Mission, { TEXT("Frag_Yield") }, /*UnlockedBy=*/TEXT("Frag_Source"));
	Fixture.BeginFrame();

	// Section 2: the reconstruction opens when the source that describes it is found.
	TestFalse(TEXT("Shut until the source is recovered"), Fixture.Witness->IsMissionAvailable(Mission));
	TestFalse(TEXT("And cannot be entered"), Fixture.Witness->EnterMission(Mission));

	Fixture.Codex->RecoverFragment(TEXT("Frag_Source"));
	TestTrue(TEXT("Open once it is"), Fixture.Witness->IsMissionAvailable(Mission));

	TArray<FName> Available;
	Fixture.Witness->GetAvailableMissionIDs(Available);
	TestEqual(TEXT("And it is listed"), Available.Num(), 1);

	TestTrue(TEXT("Entered"), Fixture.Witness->EnterMission(Mission));
	TestFalse(TEXT("Cannot enter a second while one runs"), Fixture.Witness->EnterMission(Mission));

	TestTrue(TEXT("Abandoned"), Fixture.Witness->AbandonMission());

	// Walking out yields nothing, but still puts the frame back.
	TestFalse(TEXT("Nothing was learned"), Fixture.Codex->IsFragmentRecovered(TEXT("Frag_Yield")));
	TestFalse(TEXT("And it does not count as played"), Fixture.Witness->HasCompletedMission(Mission));
	TestEqual(TEXT("But the frame is restored"), Fixture.Timeline->GetCurrentYearAD(), FrameYear);
	TestTrue(TEXT("In the frame era"), Fixture.Witness->GetCurrentEra() == EGameEra::Frame);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWitnessModeBTest,
	"ChainOfWitnesses.Witness.ModeBHasNoFrameToReturnTo",
	CHAIN_TEST_FLAGS)

bool FWitnessModeBTest::RunTest(const FString& Parameters)
{
	using namespace WitnessTestHelpers;

	FWitnessFixture Fixture;
	Fixture.AddFragment(TEXT("Frag_Yield"));
	Fixture.AddMission(Mission, { TEXT("Frag_Yield") });

	Fixture.Witness->SetStructuralMode(EStructuralMode::FirstCenturyOnly);
	TestTrue(TEXT("Mode B starts in the first century"),
		Fixture.Witness->GetCurrentEra() == EGameEra::Witness);

	// BeginFrameEra is a no-op: there is no frame.
	Fixture.Witness->BeginFrameEra(FrameEra, Jerusalem);
	TestTrue(TEXT("Still the witness era"), Fixture.Witness->GetCurrentEra() == EGameEra::Witness);

	// The scene has not come round yet, so there is nothing to walk into.
	Fixture.Timeline->SetDate(40, 0);
	TestFalse(TEXT("Not yet AD 65"), Fixture.Witness->IsMissionAvailable(Mission));

	Fixture.Timeline->SetDate(MissionYear, 0);
	Fixture.Map->SetPartyLocation(Rome);
	TestTrue(TEXT("Available once the campaign reaches it"), Fixture.Witness->IsMissionAvailable(Mission));

	TestTrue(TEXT("Entered"), Fixture.Witness->EnterMission(Mission));
	TestEqual(TEXT("The clock was not pushed: this is simply where he is"),
		Fixture.Timeline->GetCurrentYearAD(), MissionYear);

	Fixture.Witness->CompleteMission();

	TestTrue(TEXT("Still in the first century afterwards"),
		Fixture.Witness->GetCurrentEra() == EGameEra::Witness);
	TestEqual(TEXT("And the clock was not rolled back"),
		Fixture.Timeline->GetCurrentYearAD(), MissionYear);
	TestTrue(TEXT("The evidence stands"), Fixture.Codex->IsFragmentRecovered(TEXT("Frag_Yield")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWitnessReconstructionKnowledgeTest,
	"ChainOfWitnesses.Witness.FrameKnowledgeCarriesIntoTheReconstruction",
	CHAIN_TEST_FLAGS)

bool FWitnessReconstructionKnowledgeTest::RunTest(const FString& Parameters)
{
	using namespace WitnessTestHelpers;

	FWitnessFixture Fixture;

	// A source written two generations after the year the mission is set in.
	Fixture.AddFragment(TEXT("Frag_Papias"));
	Fixture.Codex->RecoverFragment(TEXT("Frag_Papias"));

	Fixture.AddFragment(TEXT("Frag_Yield"));
	Fixture.AddMission(Mission, { TEXT("Frag_Yield") });
	Fixture.BeginFrame();
	Fixture.Witness->EnterMission(Mission);

	// The mission is a reconstruction, so what the frame character knows is
	// available inside it even though the document does not exist yet in AD 65.
	TestEqual(TEXT("Standing in AD 65"), Fixture.Timeline->GetCurrentYearAD(), MissionYear);
	TestTrue(TEXT("He still knows what he read in 1229"),
		Fixture.Codex->IsFragmentRecovered(TEXT("Frag_Papias")));

	// Era-gated systems still evaluate at the mission's year, which is what keeps
	// the scene itself honest.
	FDialogueConditionSet LaterOnly;
	LaterOnly.EarliestYearAD = 110;

	EDialogueGate Gate = EDialogueGate::None;
	TestFalse(TEXT("But a line belonging to AD 110 does not fire in AD 65"),
		Fixture.Dialogue->EvaluateConditions(LaterOnly, TEXT("NPC_Peter"),
			EConversationSafety::Private, Gate));
	TestTrue(TEXT("And says why"), Gate == EDialogueGate::OutsideEra);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWitnessSaveRoundTripTest,
	"ChainOfWitnesses.Witness.ASaveTakenMidMissionComesBackMidMission",
	CHAIN_TEST_FLAGS)

bool FWitnessSaveRoundTripTest::RunTest(const FString& Parameters)
{
	using namespace WitnessTestHelpers;

	FWitnessFixture Fixture;
	Fixture.AddFragment(TEXT("Frag_Yield"));
	Fixture.AddMission(Mission, { TEXT("Frag_Yield") });
	Fixture.BeginFrame();
	Fixture.Witness->EnterMission(Mission);

	const FWitnessMissionSaveData Saved = Fixture.Witness->CaptureSaveData();

	Fixture.Witness->CompleteMission();
	TestFalse(TEXT("Mission finished"), Fixture.Witness->IsMissionActive());

	Fixture.Witness->RestoreFromSaveData(Saved);

	TestTrue(TEXT("Mid-mission again"), Fixture.Witness->IsMissionActive());
	TestTrue(TEXT("The same mission"), Fixture.Witness->GetActiveMissionID() == FName(Mission));
	TestTrue(TEXT("In the witness era"), Fixture.Witness->GetCurrentEra() == EGameEra::Witness);
	TestTrue(TEXT("With the witness era's tuning"),
		Fixture.Witness->GetActiveCombatTuning().bEscapeIsWinState);
	TestEqual(TEXT("Knowing which frame to go back to"),
		Fixture.Witness->GetMissionState().Frame.YearAD, FrameYear);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
