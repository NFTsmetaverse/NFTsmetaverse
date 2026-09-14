#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "ChainAutomationFlags.h"

#include "Campaign/CampaignTimelineSubsystem.h"
#include "Codex/CodexSubsystem.h"
#include "Dialogue/DialogueSubsystem.h"
#include "Map/CampaignMapSubsystem.h"
#include "Reputation/ReputationSubsystem.h"
#include "UObject/StrongObjectPtr.h"
#include "Witness/WitnessMissionSubsystem.h"

/**
 * Task 10.9's vertical slice, end to end: Rome c. AD 65, Peter dictating and Mark
 * writing.
 *
 * This is the integration test for the whole module. It builds the slice's real
 * content in code, walks it the way a player would, and checks that the seven
 * subsystems compose: the frame era hands off to the mission, the mission moves the
 * clock, the dialogue gates resolve at the mission's year, the scene yields its
 * three fragments, the frame comes back unchanged, and the evidence assembles into
 * a chain that scores.
 */
namespace SliceTestHelpers
{
	constexpr int32 FrameYear = 1229;
	constexpr int32 SliceYear = 65;

	const TCHAR* const Mission = TEXT("WM_PeterDictatesMark");
	const TCHAR* const RootNode = TEXT("Node_Rome_Room");
	const TCHAR* const Peter = TEXT("NPC_Peter");
	const TCHAR* const Rome = TEXT("Loc_Rome");
	const TCHAR* const Jerusalem = TEXT("Loc_Jerusalem");
	const TCHAR* const MarkAnchor = TEXT("Event_MarkWrittenInRome");
	const TCHAR* const Fire = TEXT("Event_GreatFireOfRome_NeronianPersecution");

	const TCHAR* const FragUnlock = TEXT("Frag_MarkAsPetersInterpreter");
	const TCHAR* const FragPreaching = TEXT("Frag_PeterPreachingInRome");
	const TCHAR* const FragNotInOrder = TEXT("Frag_MarkNotInOrder");
	const TCHAR* const FragGospel = TEXT("Frag_GospelOfMarkComposed");
	const TCHAR* const ChallengePapias = TEXT("Frag_EusebiusOnPapiasJudgement");
	const TCHAR* const ChallengeEnding = TEXT("Frag_LongerEndingAbsentInEarliestMSS");

	struct FSliceFixture
	{
		TStrongObjectPtr<UWitnessMissionSubsystem> Witness;
		TStrongObjectPtr<UCodexSubsystem> Codex;
		TStrongObjectPtr<UCampaignTimelineSubsystem> Timeline;
		TStrongObjectPtr<UReputationSubsystem> Reputation;
		TStrongObjectPtr<UCampaignMapSubsystem> Map;
		TStrongObjectPtr<UDialogueSubsystem> Dialogue;

		FSliceFixture()
			: Witness(NewObject<UWitnessMissionSubsystem>(GetTransientPackage()))
			, Codex(NewObject<UCodexSubsystem>(GetTransientPackage()))
			, Timeline(NewObject<UCampaignTimelineSubsystem>(GetTransientPackage()))
			, Reputation(NewObject<UReputationSubsystem>(GetTransientPackage()))
			, Map(NewObject<UCampaignMapSubsystem>(GetTransientPackage()))
			, Dialogue(NewObject<UDialogueSubsystem>(GetTransientPackage()))
		{
			Witness->SetDependenciesForTesting(Codex.Get(), Timeline.Get(), Reputation.Get(),
				Map.Get(), Dialogue.Get());
			Dialogue->SetDependenciesForTesting(Codex.Get(), Timeline.Get(), Reputation.Get());
			Reputation->SetDependenciesForTesting(Timeline.Get(), Map.Get());
			Map->SetDependenciesForTesting(Timeline.Get(), Reputation.Get());
			Codex->SetTimelineForTesting(Timeline.Get());

			BuildTimeline();
			BuildFragments();
			BuildDialogue();
			BuildMission();
		}

		void AddEvent(const TCHAR* ID, int32 Earliest, int32 Latest)
		{
			FCampaignEventRow Row;
			Row.EventID = ID;
			Row.YearEarliestAD = Earliest;
			Row.YearLatestAD = Latest;
			Timeline->RegisterEvent(Row);
		}

		void BuildTimeline()
		{
			AddEvent(MarkAnchor, 55, 70);
			AddEvent(Fire, 64, 64);
		}

		void AddFragment(const TCHAR* ID, EWitnessTier Tier, int32 AttestedAD,
			const TArray<FName>& ChallengedBy = TArray<FName>(),
			const TArray<FName>& CorroboratedBy = TArray<FName>(), bool bContested = false)
		{
			FTestimonyFragmentDefinition Definition;
			Definition.FragmentID = ID;
			Definition.Tier = Tier;
			Definition.AttestationDateAD = AttestedAD;
			Definition.ChallengedBy = ChallengedBy;
			Definition.CorroboratedBy = CorroboratedBy;
			Definition.bIsContested = bContested;
			Codex->RegisterFragmentDefinition(Definition);
		}

		/** The slice's fragments, with the real tiers, dates and challenges from the corpus. */
		void BuildFragments()
		{
			AddFragment(ChallengePapias, EWitnessTier::Patristic, 320);
			AddFragment(ChallengeEnding, EWitnessTier::Manuscript, 325);

			AddFragment(FragUnlock, EWitnessTier::Companion, 110, { ChallengePapias });
			AddFragment(FragPreaching, EWitnessTier::Eyewitness, 60, {}, { FragUnlock }, /*bContested=*/true);
			AddFragment(FragNotInOrder, EWitnessTier::Patristic, 110, { ChallengePapias }, { FragUnlock });
			AddFragment(FragGospel, EWitnessTier::Companion, 55, { ChallengeEnding }, { FragUnlock });
		}

		void AddNode(const TCHAR* NodeID, const FDialogueConditionSet& EntryConditions,
			const FDialogueEffects& EntryEffects, const TArray<FDialogueOption>& Options)
		{
			FDialogueNodeRow Row;
			Row.NodeID = NodeID;
			Row.SpeakerID = Peter;
			Row.EntryConditions = EntryConditions;
			Row.EntryEffects = EntryEffects;
			Row.Options = Options;
			Dialogue->RegisterNode(Row);
		}

		static FDialogueOption MakeOption(const TCHAR* NextNodeID,
			const FDialogueConditionSet& Conditions = FDialogueConditionSet(),
			const FDialogueEffects& Effects = FDialogueEffects())
		{
			FDialogueOption Option;
			Option.NextNodeID = NextNodeID;
			Option.Conditions = Conditions;
			Option.Effects = Effects;
			return Option;
		}

		/** The shape of DT_Dialogue_RomeAD65, with the gates that matter to this test. */
		void BuildDialogue()
		{
			FDialogueConditionSet RoomEntry;
			RoomEntry.MinimumSafety = EConversationSafety::Private;
			RoomEntry.EarliestYearAD = 55;
			RoomEntry.LatestYearAD = 70;

			FDialogueEffects RoomEffects;
			RoomEffects.GrantsFragmentIDs.Add(FragPreaching);

			// Option 1 needs the persecution to have happened, which is the world-state
			// gate resolving at the mission's year rather than the frame's.
			FDialogueConditionSet NeedsFire;
			NeedsFire.RequiredActiveEventIDs.Add(Fire);

			FDialogueEffects Opened;
			Opened.TrustDelta = 15.f;
			Opened.RaiseDisclosureTo = EDisclosureLevel::Acquaintance;

			AddNode(RootNode, RoomEntry, RoomEffects, {
				MakeOption(TEXT("Node_Peter_Terms")),
				MakeOption(TEXT("Node_Peter_Terms"), NeedsFire, Opened),
				MakeOption(TEXT("")) });

			AddNode(TEXT("Node_Peter_Terms"), FDialogueConditionSet(), FDialogueEffects(), {
				MakeOption(TEXT("Node_Peter_Dictates")),
				MakeOption(TEXT("Node_Peter_NotInOrder")) });

			FDialogueEffects NotInOrderEffects;
			NotInOrderEffects.TrustDelta = 15.f;
			NotInOrderEffects.RaiseDisclosureTo = EDisclosureLevel::Confidant;
			NotInOrderEffects.GrantsFragmentIDs.Add(FragNotInOrder);

			AddNode(TEXT("Node_Peter_NotInOrder"), FDialogueConditionSet(), NotInOrderEffects, {
				MakeOption(TEXT("Node_Peter_Dictates")) });

			FDialogueConditionSet NeedsConfidant;
			NeedsConfidant.MinimumDisclosure = EDisclosureLevel::Confidant;

			AddNode(TEXT("Node_Peter_Dictates"), FDialogueConditionSet(), FDialogueEffects(), {
				MakeOption(TEXT("Node_Peter_OwnName"), NeedsConfidant),
				MakeOption(TEXT("Node_Peter_Begins")) });

			FDialogueEffects OwnNameEffects;
			OwnNameEffects.TrustDelta = 20.f;
			OwnNameEffects.RaiseDisclosureTo = EDisclosureLevel::Insider;

			AddNode(TEXT("Node_Peter_OwnName"), FDialogueConditionSet(), OwnNameEffects, {
				MakeOption(TEXT("Node_Peter_Dictates")) });

			FDialogueEffects BeginsEffects;
			BeginsEffects.GrantsFragmentIDs.Add(FragGospel);

			AddNode(TEXT("Node_Peter_Begins"), FDialogueConditionSet(), BeginsEffects, {
				MakeOption(TEXT("")) });
		}

		void BuildMission()
		{
			FEraDefinitionRow FrameEra;
			FrameEra.EraID = TEXT("Era_Frame");
			FrameEra.Era = EGameEra::Frame;
			FrameEra.CombatTuning.bFormationCommandEnabled = true;
			Witness->RegisterEra(FrameEra);

			FEraDefinitionRow FirstCentury;
			FirstCentury.EraID = TEXT("Era_FirstCentury");
			FirstCentury.Era = EGameEra::Witness;
			FirstCentury.CombatTuning.bEscapeIsWinState = true;
			FirstCentury.CombatTuning.IncomingDamageScale = 2.5f;
			Witness->RegisterEra(FirstCentury);

			FWitnessMissionRow Row;
			Row.MissionID = Mission;
			Row.AnchorEventID = MarkAnchor;
			Row.MissionYearAD = SliceYear;
			Row.MissionDayOfYear = 150;
			Row.MissionLocationID = Rome;
			Row.EraID = TEXT("Era_FirstCentury");
			Row.PlayableCharacterID = TEXT("Char_Mark");
			Row.OpeningDialogueNodeID = RootNode;
			Row.OpeningNpcID = Peter;
			Row.OpeningSafety = EConversationSafety::Private;
			Row.UnlockedByFragmentID = FragUnlock;
			Row.GrantsFragmentIDs = { FragPreaching, FragNotInOrder, FragGospel };
			Witness->RegisterMission(Row);
		}

		/** What a GameMode does once the mission's level is up. */
		bool StartOpeningConversation()
		{
			FWitnessMissionRow Row;
			if (!Witness->GetMission(Witness->GetActiveMissionID(), Row))
			{
				return false;
			}

			return Dialogue->StartConversation(Row.OpeningNpcID, Row.OpeningDialogueNodeID, Row.OpeningSafety);
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVerticalSliceTest,
	"ChainOfWitnesses.Slice.RomeAD65PeterDictatesAndMarkWrites",
	CHAIN_TEST_FLAGS)

bool FVerticalSliceTest::RunTest(const FString& Parameters)
{
	using namespace SliceTestHelpers;

	FSliceFixture Fixture;

	// --- The frame era ------------------------------------------------------
	Fixture.Witness->BeginFrameEra(TEXT("Era_Frame"), Jerusalem);
	Fixture.Dialogue->ModifyTrust(TEXT("NPC_Preceptor"), 30.f);

	TestEqual(TEXT("The campaign opens in 1229"), Fixture.Timeline->GetCurrentYearAD(), FrameYear);
	TestFalse(TEXT("The reconstruction is shut until the source turns up"),
		Fixture.Witness->IsMissionAvailable(Mission));

	// Section 2: recovering the source is what opens the mission. Papias' testimony
	// about Mark is the document the frame character finds.
	Fixture.Codex->RecoverFragment(FragUnlock);
	TestTrue(TEXT("Finding Papias opens Rome"), Fixture.Witness->IsMissionAvailable(Mission));

	// --- Into AD 65 ---------------------------------------------------------
	TestTrue(TEXT("Mission entered"), Fixture.Witness->EnterMission(Mission));
	TestEqual(TEXT("Standing in AD 65"), Fixture.Timeline->GetCurrentYearAD(), SliceYear);
	TestTrue(TEXT("In Rome"), Fixture.Map->GetPartyLocation() == FName(Rome));
	TestTrue(TEXT("Under first-century rules, where getting out alive counts"),
		Fixture.Witness->GetActiveCombatTuning().bEscapeIsWinState);

	TestTrue(TEXT("The scene opens"), Fixture.StartOpeningConversation());

	// Being in the room is how the player learns Peter preached here.
	TestTrue(TEXT("Entering the room yields the first fragment"),
		Fixture.Codex->IsFragmentRecovered(FragPreaching));

	// --- The conversation ---------------------------------------------------
	FDialogueNodeView View;
	Fixture.Dialogue->GetCurrentNodeView(View);
	TestEqual(TEXT("Three things to say"), View.Options.Num(), 3);

	// The persecution line is open because world state resolves at AD 65, where the
	// fire has happened -- not at 1229, and not at AD 30.
	TestTrue(TEXT("The fire has happened, so the hard line is available"),
		View.Options[1].bIsAvailable);
	TestTrue(TEXT("Said it"), Fixture.Dialogue->SelectOption(1));

	// Pressing him on sequence is what surfaces the admission.
	TestTrue(TEXT("Pressed him on order"), Fixture.Dialogue->SelectOption(1));
	TestTrue(TEXT("Which yields the second fragment"),
		Fixture.Codex->IsFragmentRecovered(FragNotInOrder));
	TestTrue(TEXT("And opens him up"),
		Fixture.Dialogue->GetDisclosure(Peter) == EDisclosureLevel::Confidant);

	Fixture.Dialogue->SelectOption(0);

	// The courtyard question needs the disclosure the last exchange earned.
	Fixture.Dialogue->GetCurrentNodeView(View);
	TestTrue(TEXT("He will now be asked about his own failure"), View.Options[0].bIsAvailable);
	Fixture.Dialogue->SelectOption(0);
	TestTrue(TEXT("Which takes him as far as he goes"),
		Fixture.Dialogue->GetDisclosure(Peter) == EDisclosureLevel::Insider);

	Fixture.Dialogue->SelectOption(0);
	Fixture.Dialogue->SelectOption(1);

	TestTrue(TEXT("And the work is begun: the third fragment"),
		Fixture.Codex->IsFragmentRecovered(FragGospel));

	Fixture.Dialogue->SelectOption(0);
	TestFalse(TEXT("The scene is over"), Fixture.Dialogue->IsConversationActive());

	// --- Back to the frame --------------------------------------------------
	TestTrue(TEXT("Mission completed"), Fixture.Witness->CompleteMission());

	TestEqual(TEXT("Back in 1229"), Fixture.Timeline->GetCurrentYearAD(), FrameYear);
	TestTrue(TEXT("Back in Jerusalem"), Fixture.Map->GetPartyLocation() == FName(Jerusalem));
	TestEqual(TEXT("What Peter came to think of him did not follow him out"),
		Fixture.Dialogue->GetTrust(Peter), 0.f, 0.01f);
	TestEqual(TEXT("But the frame era's own relationships are untouched"),
		Fixture.Dialogue->GetTrust(TEXT("NPC_Preceptor")), 30.f, 0.01f);

	// The three fragments are the exception, and the reason for going.
	TestTrue(TEXT("First fragment kept"), Fixture.Codex->IsFragmentRecovered(FragPreaching));
	TestTrue(TEXT("Second fragment kept"), Fixture.Codex->IsFragmentRecovered(FragNotInOrder));
	TestTrue(TEXT("Third fragment kept"), Fixture.Codex->IsFragmentRecovered(FragGospel));

	// --- The argument it buys -----------------------------------------------
	TestTrue(TEXT("A chain opens on the event just reconstructed"),
		Fixture.Codex->CreateChain(MarkAnchor));

	Fixture.Codex->SlotFragment(MarkAnchor, ETransmissionLink::Eyewitness, FragPreaching);
	Fixture.Codex->SlotFragment(MarkAnchor, ETransmissionLink::WrittenSource, FragGospel);
	Fixture.Codex->SlotFragment(MarkAnchor, ETransmissionLink::PatristicAttestation, FragNotInOrder);

	const FChainScoreBreakdown ThreeLinks = Fixture.Codex->ScoreChain(MarkAnchor);

	TestTrue(TEXT("It scores for something"), ThreeLinks.AttestationStrength > 0.f);
	TestFalse(TEXT("But it is nowhere near complete"), ThreeLinks.bIsComplete);
	TestTrue(TEXT("And the Event link is still the hole in it"),
		ThreeLinks.WeakestLink == ETransmissionLink::Event);

	// Both challenges the slice's own sources carry are outstanding, which is what a
	// debate opponent will reach for. One evening in Rome is a start, not a case.
	TestEqual(TEXT("Two objections stand unanswered"), ThreeLinks.UnansweredChallenges.Num(), 2);

	// Slotting the source that opened the mission ties the three together.
	Fixture.Codex->SlotFragment(MarkAnchor, ETransmissionLink::OralProclamation, FragUnlock);
	const FChainScoreBreakdown FourLinks = Fixture.Codex->ScoreChain(MarkAnchor);

	TestTrue(TEXT("Papias corroborates all three"), FourLinks.IndependentCorroborationCount == 3);
	TestTrue(TEXT("So the chain is stronger for it"),
		FourLinks.AttestationStrength > ThreeLinks.AttestationStrength);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
