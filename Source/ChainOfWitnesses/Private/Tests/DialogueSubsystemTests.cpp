#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "ChainAutomationFlags.h"
#include "ChainTestOuter.h"

#include "Campaign/CampaignTimelineSubsystem.h"
#include "Codex/CodexSubsystem.h"
#include "Dialogue/DialogueSubsystem.h"
#include "UObject/StrongObjectPtr.h"

namespace DialogueTestHelpers
{
	const TCHAR* const Npc = TEXT("NPC_Peter");
	const TCHAR* const Voucher = TEXT("NPC_Barnabas");
	const TCHAR* const RootNode = TEXT("Node_Root");
	const TCHAR* const NextNode = TEXT("Node_Next");

	/**
	 * The dialogue subsystem reads the Codex and the timeline through the owning
	 * GameInstance, which a standalone test has none of -- so all three are built
	 * loose and wired by hand.
	 */
	struct FDialogueFixture
	{
		TStrongObjectPtr<UDialogueSubsystem> Dialogue;
		TStrongObjectPtr<UCodexSubsystem> Codex;
		TStrongObjectPtr<UCampaignTimelineSubsystem> Timeline;

		FDialogueFixture()
			: Dialogue(NewObject<UDialogueSubsystem>(ChainTestOuter()))
			, Codex(NewObject<UCodexSubsystem>(ChainTestOuter()))
			, Timeline(NewObject<UCampaignTimelineSubsystem>(ChainTestOuter()))
		{
			Dialogue->SetDependenciesForTesting(Codex.Get(), Timeline.Get());
			Timeline->AdvanceToYear(35);
		}

		/** A two-node tree whose single option carries the conditions under test. */
		void BuildTree(const FDialogueConditionSet& OptionConditions, bool bShowWhenLocked = true)
		{
			FDialogueOption Option;
			Option.PlayerLine = FText::FromString(TEXT("The gated line."));
			Option.Conditions = OptionConditions;
			Option.NextNodeID = NextNode;
			Option.bShowWhenLocked = bShowWhenLocked;

			FDialogueNodeRow Root;
			Root.NodeID = RootNode;
			Root.SpeakerID = Npc;
			Root.Options.Add(Option);

			FDialogueNodeRow Next;
			Next.NodeID = NextNode;
			Next.SpeakerID = Npc;

			Dialogue->RegisterNode(Root);
			Dialogue->RegisterNode(Next);
		}

		/** Index 0's gate, as the UI would see it. */
		EDialogueGate FirstOptionGate() const
		{
			FDialogueNodeView View;
			Dialogue->GetCurrentNodeView(View);
			return View.Options.Num() > 0 ? View.Options[0].Gate : EDialogueGate::MAX;
		}

		bool FirstOptionAvailable() const
		{
			FDialogueNodeView View;
			Dialogue->GetCurrentNodeView(View);
			return View.Options.Num() > 0 && View.Options[0].bIsAvailable;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDialogueSafetyGateTest,
	"ChainOfWitnesses.Dialogue.SafetyGatesWhatHeWillSayInTheOpen",
	CHAIN_TEST_FLAGS)

bool FDialogueSafetyGateTest::RunTest(const FString& Parameters)
{
	using namespace DialogueTestHelpers;

	FDialogueConditionSet Conditions;
	Conditions.MinimumSafety = EConversationSafety::Private;

	FDialogueFixture Fixture;
	Fixture.BuildTree(Conditions);

	TestTrue(TEXT("Conversation starts in public"),
		Fixture.Dialogue->StartConversation(Npc, RootNode, EConversationSafety::Public));
	TestFalse(TEXT("Line is shut in the open"), Fixture.FirstOptionAvailable());
	TestTrue(TEXT("Gate names the reason"), Fixture.FirstOptionGate() == EDialogueGate::Unsafe);

	// The view is a presentation, not the authority: selecting by index must fail too.
	TestFalse(TEXT("Locked option cannot be selected"), Fixture.Dialogue->SelectOption(0));

	// Guarded is still not private enough.
	Fixture.Dialogue->StartConversation(Npc, RootNode, EConversationSafety::Guarded);
	TestFalse(TEXT("Guarded is insufficient"), Fixture.FirstOptionAvailable());

	Fixture.Dialogue->StartConversation(Npc, RootNode, EConversationSafety::Private);
	TestTrue(TEXT("Behind a shut door he will say it"), Fixture.FirstOptionAvailable());
	TestTrue(TEXT("Selection advances"), Fixture.Dialogue->SelectOption(0));

	FDialogueNodeView View;
	Fixture.Dialogue->GetCurrentNodeView(View);
	TestTrue(TEXT("Moved to the next node"), View.NodeID == FName(NextNode));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDialogueVouchGateTest,
	"ChainOfWitnesses.Dialogue.VouchingOpensWhatTrustAloneCannot",
	CHAIN_TEST_FLAGS)

bool FDialogueVouchGateTest::RunTest(const FString& Parameters)
{
	using namespace DialogueTestHelpers;

	FDialogueConditionSet Conditions;
	Conditions.AcceptedVoucherIDs.Add(Voucher);

	FDialogueFixture Fixture;
	Fixture.BuildTree(Conditions);
	Fixture.Dialogue->StartConversation(Npc, RootNode, EConversationSafety::Public);

	TestFalse(TEXT("No one has answered for the player"), Fixture.FirstOptionAvailable());
	TestTrue(TEXT("Gate names the reason"), Fixture.FirstOptionGate() == EDialogueGate::NoVoucher);

	// A vouch to a different NPC does not carry.
	Fixture.Dialogue->RecordVouch(Voucher, TEXT("NPC_Someone_Else"));
	TestFalse(TEXT("Vouch to another man does not carry"), Fixture.FirstOptionAvailable());

	Fixture.Dialogue->RecordVouch(Voucher, Npc);
	TestTrue(TEXT("Vouch to this man opens the line"), Fixture.FirstOptionAvailable());

	// A general vouch works with anyone who accepts the voucher.
	FDialogueFixture General;
	General.BuildTree(Conditions);
	General.Dialogue->RecordVouch(Voucher, NAME_None);
	General.Dialogue->StartConversation(Npc, RootNode, EConversationSafety::Public);
	TestTrue(TEXT("A general vouch carries"), General.FirstOptionAvailable());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDialogueDisclosureTest,
	"ChainOfWitnesses.Dialogue.DisclosureRatchetsAndPersists",
	CHAIN_TEST_FLAGS)

bool FDialogueDisclosureTest::RunTest(const FString& Parameters)
{
	using namespace DialogueTestHelpers;

	FDialogueConditionSet Conditions;
	Conditions.MinimumDisclosure = EDisclosureLevel::Confidant;
	Conditions.MinimumTrust = 10.f;

	FDialogueFixture Fixture;
	Fixture.BuildTree(Conditions);
	Fixture.Dialogue->StartConversation(Npc, RootNode, EConversationSafety::Public);

	// Trust is checked before disclosure, so it is the reported gate first.
	TestTrue(TEXT("Trust gates first"), Fixture.FirstOptionGate() == EDialogueGate::InsufficientTrust);

	Fixture.Dialogue->ModifyTrust(Npc, 25.f);
	TestTrue(TEXT("Disclosure gates next"), Fixture.FirstOptionGate() == EDialogueGate::InsufficientDisclosure);

	Fixture.Dialogue->RaiseDisclosure(Npc, EDisclosureLevel::Confidant);
	TestTrue(TEXT("Line opens"), Fixture.FirstOptionAvailable());

	// Disclosure does not un-happen.
	Fixture.Dialogue->RaiseDisclosure(Npc, EDisclosureLevel::Stranger);
	TestTrue(TEXT("Disclosure never lowers"),
		Fixture.Dialogue->GetDisclosure(Npc) == EDisclosureLevel::Confidant);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDialogueCodexGateTest,
	"ChainOfWitnesses.Dialogue.CodexGatesWhatThePlayerCanCrediblyClaim",
	CHAIN_TEST_FLAGS)

bool FDialogueCodexGateTest::RunTest(const FString& Parameters)
{
	using namespace DialogueTestHelpers;

	const FName AnchorEvent(TEXT("Event_Test"));

	FDialogueConditionSet Conditions;
	Conditions.RequiredFragmentIDs.Add(TEXT("Frag_Known"));
	Conditions.CredibleChainAnchorEventID = AnchorEvent;
	Conditions.MinimumChainStrength = 15.f;

	FDialogueFixture Fixture;
	Fixture.BuildTree(Conditions);

	FTestimonyFragmentDefinition Known;
	Known.FragmentID = TEXT("Frag_Known");
	Known.Tier = EWitnessTier::Eyewitness;
	Known.AttestationDateAD = 35;
	Fixture.Codex->RegisterFragmentDefinition(Known);

	Fixture.Dialogue->StartConversation(Npc, RootNode, EConversationSafety::Public);

	// Holding nothing: he cannot show he knows it.
	TestTrue(TEXT("Unknown fragment gates"), Fixture.FirstOptionGate() == EDialogueGate::UnknownFragment);

	// Holding the fragment is the weak form of knowing. Without a chain that would
	// survive an argument, the claim still will not carry.
	Fixture.Codex->RecoverFragment(TEXT("Frag_Known"));
	TestTrue(TEXT("Holding a fragment is not yet defending it"),
		Fixture.FirstOptionGate() == EDialogueGate::WeakChain);

	// One well-sited fragment is worth 100/6 ~= 16.7, which clears the threshold.
	Fixture.Codex->CreateChainWithAnchorYear(AnchorEvent, 30);
	Fixture.Codex->SlotFragment(AnchorEvent, ETransmissionLink::Eyewitness, TEXT("Frag_Known"));
	TestTrue(TEXT("A defensible chain opens the line"), Fixture.FirstOptionAvailable());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDialogueEraGateTest,
	"ChainOfWitnesses.Dialogue.EraGatesAnachronism",
	CHAIN_TEST_FLAGS)

bool FDialogueEraGateTest::RunTest(const FString& Parameters)
{
	using namespace DialogueTestHelpers;

	FDialogueConditionSet Conditions;
	Conditions.EarliestYearAD = 66;

	FDialogueFixture Fixture;
	Fixture.BuildTree(Conditions);
	Fixture.Dialogue->StartConversation(Npc, RootNode, EConversationSafety::Public);

	// No one discusses the Temple's destruction in AD 35.
	TestTrue(TEXT("Too early"), Fixture.FirstOptionGate() == EDialogueGate::OutsideEra);

	Fixture.Timeline->AdvanceToYear(70);
	TestTrue(TEXT("In era"), Fixture.FirstOptionAvailable());

	// An event that has not fired gates separately from the bare year.
	FDialogueConditionSet WorldState;
	WorldState.RequiredActiveEventIDs.Add(TEXT("Event_NeverFires"));

	EDialogueGate Gate = EDialogueGate::None;
	TestFalse(TEXT("Missing world state fails"),
		Fixture.Dialogue->EvaluateConditions(WorldState, Npc, EConversationSafety::Public, Gate));
	TestTrue(TEXT("Gate names world state"), Gate == EDialogueGate::MissingWorldState);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDialogueTraversalTest,
	"ChainOfWitnesses.Dialogue.EntryConditionsDivertToFallback",
	CHAIN_TEST_FLAGS)

bool FDialogueTraversalTest::RunTest(const FString& Parameters)
{
	using namespace DialogueTestHelpers;
	FDialogueFixture Fixture;

	// The private-only node diverts to a public brush-off rather than failing.
	FDialogueNodeRow House;
	House.NodeID = TEXT("Node_House");
	House.SpeakerID = Npc;
	House.EntryConditions.MinimumSafety = EConversationSafety::Private;
	House.FallbackNodeID = TEXT("Node_NotHere");
	House.EntryEffects.TrustDelta = 5.f;

	FDialogueNodeRow NotHere;
	NotHere.NodeID = TEXT("Node_NotHere");
	NotHere.SpeakerID = Npc;

	Fixture.Dialogue->RegisterNode(House);
	Fixture.Dialogue->RegisterNode(NotHere);

	Fixture.Dialogue->StartConversation(Npc, TEXT("Node_House"), EConversationSafety::Public);

	FDialogueNodeView View;
	Fixture.Dialogue->GetCurrentNodeView(View);
	TestTrue(TEXT("Diverted to the fallback"), View.NodeID == FName(TEXT("Node_NotHere")));
	TestTrue(TEXT("A node with no options is terminal"), View.bIsTerminal);
	TestEqual(TEXT("Skipped node's entry effects did not fire"), Fixture.Dialogue->GetTrust(Npc), 0.f, 0.01f);

	Fixture.Dialogue->StartConversation(Npc, TEXT("Node_House"), EConversationSafety::Private);
	Fixture.Dialogue->GetCurrentNodeView(View);
	TestTrue(TEXT("Entered the real node"), View.NodeID == FName(TEXT("Node_House")));
	TestEqual(TEXT("Entry effects fired"), Fixture.Dialogue->GetTrust(Npc), 5.f, 0.01f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDialogueHiddenOptionTest,
	"ChainOfWitnesses.Dialogue.HiddenOptionsAreOmittedNotGreyed",
	CHAIN_TEST_FLAGS)

bool FDialogueHiddenOptionTest::RunTest(const FString& Parameters)
{
	using namespace DialogueTestHelpers;

	FDialogueConditionSet Conditions;
	Conditions.MinimumSafety = EConversationSafety::Private;

	FDialogueFixture Fixture;
	Fixture.BuildTree(Conditions, /*bShowWhenLocked=*/false);
	Fixture.Dialogue->StartConversation(Npc, RootNode, EConversationSafety::Public);

	FDialogueNodeView View;
	Fixture.Dialogue->GetCurrentNodeView(View);
	TestEqual(TEXT("Hidden option is absent from the view"), View.Options.Num(), 0);
	TestTrue(TEXT("Node with nothing selectable is terminal"), View.bIsTerminal);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDialogueSaveRoundTripTest,
	"ChainOfWitnesses.Dialogue.StandingSurvivesSaveAndReload",
	CHAIN_TEST_FLAGS)

bool FDialogueSaveRoundTripTest::RunTest(const FString& Parameters)
{
	using namespace DialogueTestHelpers;
	FDialogueFixture Fixture;

	Fixture.Dialogue->ModifyTrust(Npc, 30.f);
	Fixture.Dialogue->RaiseDisclosure(Npc, EDisclosureLevel::Confidant);
	Fixture.Dialogue->RecordVouch(Voucher, Npc);
	Fixture.Dialogue->RecordVouch(TEXT("NPC_Public_Voucher"), NAME_None);

	const FDialogueSaveData Saved = Fixture.Dialogue->CaptureSaveData();

	Fixture.Dialogue->ModifyTrust(Npc, -100.f);
	Fixture.Dialogue->RestoreFromSaveData(Saved);

	TestEqual(TEXT("Trust restored"), Fixture.Dialogue->GetTrust(Npc), 30.f, 0.01f);
	TestTrue(TEXT("Disclosure restored"),
		Fixture.Dialogue->GetDisclosure(Npc) == EDisclosureLevel::Confidant);
	TestTrue(TEXT("Targeted vouch restored"), Fixture.Dialogue->HasVouch(Voucher, Npc));
	TestTrue(TEXT("General vouch restored"),
		Fixture.Dialogue->HasVouch(TEXT("NPC_Public_Voucher"), TEXT("NPC_Anyone")));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
