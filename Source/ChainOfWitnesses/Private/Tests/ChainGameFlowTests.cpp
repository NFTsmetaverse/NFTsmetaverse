#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "ChainAutomationFlags.h"

#include "Campaign/CampaignTimelineSubsystem.h"
#include "Codex/CodexSubsystem.h"
#include "Combat/CombatEncounterSubsystem.h"
#include "Debate/DebateSubsystem.h"
#include "Dialogue/DialogueSubsystem.h"
#include "Game/ChainGameFlowSubsystem.h"
#include "Map/CampaignMapSubsystem.h"
#include "Reputation/ReputationSubsystem.h"
#include "UObject/StrongObjectPtr.h"
#include "Witness/WitnessMissionSubsystem.h"

/**
 * The flow layer: the part that knows the order things happen in.
 *
 * These tests are about sequencing and refusal, not about rules -- the rules belong
 * to the nine subsystems and are tested with them. What matters here is that the
 * screen always agrees with what is actually running, that a mistyped or locked
 * choice changes nothing, and that backing out unwinds one layer at a time.
 */
namespace FlowTestHelpers
{
	const TCHAR* const FrameEra = TEXT("Era_Frame");
	const TCHAR* const WitnessEra = TEXT("Era_Witness");
	const TCHAR* const Mission = TEXT("WM_Test");
	const TCHAR* const Rome = TEXT("Loc_Rome");
	const TCHAR* const Jerusalem = TEXT("Loc_Jerusalem");
	const TCHAR* const Npc = TEXT("NPC_Peter");
	const TCHAR* const RootNode = TEXT("Node_Root");
	const TCHAR* const NextNode = TEXT("Node_Next");
	const TCHAR* const KeyFragment = TEXT("Frag_Key");
	const TCHAR* const GateFragment = TEXT("Frag_NeverHeld");
	const TCHAR* const Encounter = TEXT("Cbt_Test");

	struct FFlowFixture
	{
		TStrongObjectPtr<UCampaignTimelineSubsystem> Timeline;
		TStrongObjectPtr<UCodexSubsystem> Codex;
		TStrongObjectPtr<UReputationSubsystem> Reputation;
		TStrongObjectPtr<UCampaignMapSubsystem> Map;
		TStrongObjectPtr<UDialogueSubsystem> Dialogue;
		TStrongObjectPtr<UWitnessMissionSubsystem> Witness;
		TStrongObjectPtr<UDebateSubsystem> Debate;
		TStrongObjectPtr<UCombatEncounterSubsystem> Combat;
		TStrongObjectPtr<UChainGameFlowSubsystem> Flow;

		FFlowFixture()
			: Timeline(NewObject<UCampaignTimelineSubsystem>(GetTransientPackage()))
			, Codex(NewObject<UCodexSubsystem>(GetTransientPackage()))
			, Reputation(NewObject<UReputationSubsystem>(GetTransientPackage()))
			, Map(NewObject<UCampaignMapSubsystem>(GetTransientPackage()))
			, Dialogue(NewObject<UDialogueSubsystem>(GetTransientPackage()))
			, Witness(NewObject<UWitnessMissionSubsystem>(GetTransientPackage()))
			, Debate(NewObject<UDebateSubsystem>(GetTransientPackage()))
			, Combat(NewObject<UCombatEncounterSubsystem>(GetTransientPackage()))
			, Flow(NewObject<UChainGameFlowSubsystem>(GetTransientPackage()))
		{
			Codex->SetTimelineForTesting(Timeline.Get());
			Map->SetDependenciesForTesting(Timeline.Get(), Reputation.Get());
			Reputation->SetDependenciesForTesting(Timeline.Get(), Map.Get());
			Dialogue->SetDependenciesForTesting(Codex.Get(), Timeline.Get(), Reputation.Get());
			Witness->SetDependenciesForTesting(Codex.Get(), Timeline.Get(), Reputation.Get(),
				Map.Get(), Dialogue.Get());
			Debate->SetDependenciesForTesting(Codex.Get(), Timeline.Get(), Reputation.Get(), Map.Get());
			Combat->SetDependenciesForTesting(Reputation.Get(), Map.Get(), Witness.Get());
			Flow->SetDependenciesForTesting(Codex.Get(), Dialogue.Get(), Witness.Get(), Map.Get(),
				Timeline.Get(), Debate.Get(), Combat.Get(), Reputation.Get());

			BuildContent();
		}

		void AddLocation(const TCHAR* ID)
		{
			FCampaignLocationRow Row;
			Row.LocationID = ID;
			Map->RegisterLocation(Row);
		}

		void AddFragment(const TCHAR* ID)
		{
			FTestimonyFragmentDefinition Definition;
			Definition.FragmentID = ID;
			Definition.Tier = EWitnessTier::Companion;
			Definition.AttestationDateAD = 100;
			Codex->RegisterFragmentDefinition(Definition);
		}

		void BuildContent()
		{
			AddLocation(Rome);
			AddLocation(Jerusalem);
			AddFragment(KeyFragment);
			AddFragment(GateFragment);

			FEraDefinitionRow Frame;
			Frame.EraID = FrameEra;
			Frame.Era = EGameEra::Frame;
			Witness->RegisterEra(Frame);

			FEraDefinitionRow First;
			First.EraID = WitnessEra;
			First.Era = EGameEra::Witness;
			Witness->RegisterEra(First);

			// A terminal node, so the open option has somewhere to go.
			FDialogueNodeRow Tail;
			Tail.NodeID = NextNode;
			Tail.SpeakerID = Npc;
			Dialogue->RegisterNode(Tail);

			// One option open, one gated on a fragment the player will never hold.
			// bShowWhenLocked, because a refusal the player cannot see is not a test
			// of anything.
			FDialogueOption Open;
			Open.NextNodeID = NextNode;

			FDialogueOption Locked;
			Locked.NextNodeID = NextNode;
			Locked.bShowWhenLocked = true;
			Locked.Conditions.RequiredFragmentIDs.Add(GateFragment);

			FDialogueNodeRow Root;
			Root.NodeID = RootNode;
			Root.SpeakerID = Npc;
			Root.Options = { Open, Locked };
			Dialogue->RegisterNode(Root);

			FWitnessMissionRow MissionRow;
			MissionRow.MissionID = Mission;
			MissionRow.EraID = WitnessEra;
			MissionRow.MissionYearAD = 65;
			MissionRow.MissionLocationID = Rome;
			MissionRow.OpeningDialogueNodeID = RootNode;
			MissionRow.OpeningNpcID = Npc;
			MissionRow.OpeningSafety = EConversationSafety::Private;
			MissionRow.UnlockedByFragmentID = KeyFragment;
			Witness->RegisterMission(MissionRow);

			FCombatEncounterRow EncounterRow;
			EncounterRow.EncounterTypeID = Encounter;
			EncounterRow.CombatantCount = 1;
			EncounterRow.StartingResolve = 1.f;
			Combat->RegisterEncounterType(EncounterRow);

			Witness->BeginFrameEra(FrameEra, Jerusalem);
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FChainFlowScreenTest,
	"ChainOfWitnesses.Flow.ScreenFollowsWhateverIsActuallyRunning",
	CHAIN_TEST_FLAGS)

bool FChainFlowScreenTest::RunTest(const FString& Parameters)
{
	using namespace FlowTestHelpers;
	FFlowFixture Fixture;

	TestTrue(TEXT("Nothing running is the campaign screen"),
		Fixture.Flow->GetScreen() == EChainScreen::Campaign);

	TestTrue(TEXT("Conversation opens"), Fixture.Flow->StartConversation(Npc, RootNode));
	TestTrue(TEXT("Talking shows dialogue"),
		Fixture.Flow->GetScreen() == EChainScreen::Dialogue);

	// The codex is an overlay: it sits on top of whatever it was opened over, and
	// closing it must put the player back where they were, not at the map.
	Fixture.Flow->ToggleCodex();
	TestTrue(TEXT("The codex takes the screen"),
		Fixture.Flow->GetScreen() == EChainScreen::Codex);
	Fixture.Flow->ToggleCodex();
	TestTrue(TEXT("Closing it returns to the conversation"),
		Fixture.Flow->GetScreen() == EChainScreen::Dialogue);

	// A fight outranks a conversation: if both are somehow live, the fight is what
	// the player is dealing with.
	TestTrue(TEXT("Encounter begins"), Fixture.Flow->StartCombat(Encounter));
	TestTrue(TEXT("Combat outranks dialogue"),
		Fixture.Flow->GetScreen() == EChainScreen::Combat);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FChainFlowRefusesBadLinesTest,
	"ChainOfWitnesses.Flow.LockedAndOutOfRangeLinesChangeNothing",
	CHAIN_TEST_FLAGS)

bool FChainFlowRefusesBadLinesTest::RunTest(const FString& Parameters)
{
	using namespace FlowTestHelpers;
	FFlowFixture Fixture;

	TestFalse(TEXT("Nobody is talking, so nothing can be said"), Fixture.Flow->Say(0));

	Fixture.Flow->StartConversation(Npc, RootNode);

	TestFalse(TEXT("A number nobody offered is refused"), Fixture.Flow->Say(7));
	TestTrue(TEXT("And the conversation is untouched"), Fixture.Dialogue->IsConversationActive());

	FDialogueNodeView Before;
	Fixture.Dialogue->GetCurrentNodeView(Before);

	TestFalse(TEXT("The gated line is refused"), Fixture.Flow->Say(1));

	FDialogueNodeView After;
	Fixture.Dialogue->GetCurrentNodeView(After);
	TestTrue(TEXT("A refused line does not advance the scene"), Before.NodeID == After.NodeID);
	TestTrue(TEXT("And the refusal says why"),
		Fixture.Flow->GetLastMessage().Contains(TEXT("closed")));

	TestTrue(TEXT("The open line plays"), Fixture.Flow->Say(0));
	Fixture.Dialogue->GetCurrentNodeView(After);
	TestTrue(TEXT("And it moved"), After.NodeID == FName(NextNode));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FChainFlowBackTest,
	"ChainOfWitnesses.Flow.BackUnwindsOneLayerAtATime",
	CHAIN_TEST_FLAGS)

bool FChainFlowBackTest::RunTest(const FString& Parameters)
{
	using namespace FlowTestHelpers;
	FFlowFixture Fixture;

	TestFalse(TEXT("Nothing to back out of"), Fixture.Flow->Back());

	Fixture.Flow->StartConversation(Npc, RootNode);
	Fixture.Flow->ToggleCodex();

	TestTrue(TEXT("Back closes the codex first"), Fixture.Flow->Back());
	TestFalse(TEXT("The codex is shut"), Fixture.Flow->IsCodexOpen());
	TestTrue(TEXT("But the conversation survived it"), Fixture.Dialogue->IsConversationActive());

	TestTrue(TEXT("Back again leaves the conversation"), Fixture.Flow->Back());
	TestFalse(TEXT("Which is now over"), Fixture.Dialogue->IsConversationActive());

	// Leaving a fight is the combat system's decision -- escape, de-escalation or
	// defeat -- not a menu action, or every encounter is optional.
	Fixture.Flow->StartCombat(Encounter);
	TestFalse(TEXT("You cannot back out of a fight"), Fixture.Flow->Back());
	TestTrue(TEXT("So the fight is still on"), Fixture.Combat->IsEncounterActive());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FChainFlowMissionGateTest,
	"ChainOfWitnesses.Flow.ShutMissionRefusesUntilItsSourceIsHeld",
	CHAIN_TEST_FLAGS)

bool FChainFlowMissionGateTest::RunTest(const FString& Parameters)
{
	using namespace FlowTestHelpers;
	FFlowFixture Fixture;

	TestFalse(TEXT("The reconstruction is shut without its source"),
		Fixture.Flow->OpenMission(Mission));
	TestTrue(TEXT("And it names the source that would open it"),
		Fixture.Flow->GetLastMessage().Contains(KeyFragment));
	TestFalse(TEXT("Nothing was entered"), Fixture.Witness->IsMissionActive());

	// The shortcut grants the source and then takes the ordinary path, so it cannot
	// open anything the real game would not.
	TestTrue(TEXT("Granting the source opens it"), Fixture.Flow->ForceOpenMission(Mission));
	TestTrue(TEXT("The source is genuinely held"), Fixture.Codex->IsFragmentRecovered(KeyFragment));
	TestTrue(TEXT("The mission is running"), Fixture.Witness->IsMissionActive());
	TestTrue(TEXT("Standing in its year"), Fixture.Timeline->GetCurrentYearAD() == 65);
	TestTrue(TEXT("And its scene opened with it"), Fixture.Dialogue->IsConversationActive());

	TestTrue(TEXT("Completing returns to the frame"), Fixture.Flow->CompleteMission());
	TestFalse(TEXT("No mission is running"), Fixture.Witness->IsMissionActive());
	TestFalse(TEXT("And the scene was closed with it"), Fixture.Dialogue->IsConversationActive());

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
