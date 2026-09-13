#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Campaign/CampaignTimelineSubsystem.h"
#include "Codex/CodexSubsystem.h"
#include "Debate/DebateSubsystem.h"
#include "Map/CampaignMapSubsystem.h"
#include "Reputation/ReputationSubsystem.h"
#include "UObject/StrongObjectPtr.h"

namespace DebateTestHelpers
{
	const TCHAR* const Anchor = TEXT("Event_Test");
	constexpr int32 AnchorYear = 30;

	/**
	 * A chain with one strong link and five empty ones, so that "which link does he
	 * go for" has an unambiguous answer in every test below.
	 */
	struct FDebateFixture
	{
		TStrongObjectPtr<UDebateSubsystem> Debate;
		TStrongObjectPtr<UCodexSubsystem> Codex;
		TStrongObjectPtr<UCampaignTimelineSubsystem> Timeline;
		TStrongObjectPtr<UReputationSubsystem> Reputation;
		TStrongObjectPtr<UCampaignMapSubsystem> Map;

		FDebateFixture()
			: Debate(NewObject<UDebateSubsystem>(GetTransientPackage()))
			, Codex(NewObject<UCodexSubsystem>(GetTransientPackage()))
			, Timeline(NewObject<UCampaignTimelineSubsystem>(GetTransientPackage()))
			, Reputation(NewObject<UReputationSubsystem>(GetTransientPackage()))
			, Map(NewObject<UCampaignMapSubsystem>(GetTransientPackage()))
		{
			Debate->SetDependenciesForTesting(Codex.Get(), Timeline.Get(), Reputation.Get(), Map.Get());
			Reputation->SetDependenciesForTesting(Timeline.Get(), Map.Get());
			Map->SetDependenciesForTesting(Timeline.Get(), Reputation.Get());

			Timeline->AdvanceToYear(AnchorYear);
			Codex->CreateChainWithAnchorYear(Anchor, AnchorYear);
		}

		/** Registers and recovers a fragment attested close enough to score full proximity. */
		void AddFragment(const TCHAR* ID, EWitnessTier Tier, const TArray<FName>& ChallengedBy = TArray<FName>())
		{
			FTestimonyFragmentDefinition Definition;
			Definition.FragmentID = ID;
			Definition.Tier = Tier;
			Definition.AttestationDateAD = AnchorYear + 5;
			Definition.ChallengedBy = ChallengedBy;

			Codex->RegisterFragmentDefinition(Definition);
			Codex->RecoverFragment(ID);
		}

		void SlotInto(ETransmissionLink Link, const TCHAR* FragmentID)
		{
			Codex->SlotFragment(Anchor, Link, FragmentID);
		}

		void AddObjection(const TCHAR* ID, ETransmissionLink TargetsLink, float Required, float Weight,
			const TArray<FName>& AnsweredBy = TArray<FName>())
		{
			FDebateObjectionRow Objection;
			Objection.ObjectionID = ID;
			Objection.TargetsLink = TargetsLink;
			Objection.RequiredLinkStrength = Required;
			Objection.Weight = Weight;
			Objection.AnsweredByFragmentIDs = AnsweredBy;

			Debate->RegisterObjection(Objection);
		}

		void AddOpponent(const TCHAR* ID, const TArray<FName>& ObjectionIDs, float Composure = 100.f,
			float Aggression = 1.f, int32 MaxRounds = 5)
		{
			FDebateOpponentRow Opponent;
			Opponent.OpponentID = ID;
			Opponent.ObjectionIDs = ObjectionIDs;
			Opponent.Composure = Composure;
			Opponent.Aggression = Aggression;
			Opponent.MaxRounds = MaxRounds;

			Debate->RegisterOpponent(Opponent);
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDebateProbesWeakestLinkTest,
	"ChainOfWitnesses.Debate.OpponentLeadsWithTheWeakestLink",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDebateProbesWeakestLinkTest::RunTest(const FString& Parameters)
{
	using namespace DebateTestHelpers;

	FDebateFixture Fixture;
	Fixture.AddFragment(TEXT("Frag_Witness"), EWitnessTier::Eyewitness);
	Fixture.SlotInto(ETransmissionLink::Eyewitness, TEXT("Frag_Witness"));

	// One objection aimed at the link that is filled, one at a link that is empty.
	Fixture.AddObjection(TEXT("Obj_AtStrongLink"), ETransmissionLink::Eyewitness, 0.5f, 10.f);
	Fixture.AddObjection(TEXT("Obj_AtEmptyLink"), ETransmissionLink::Manuscript, 0.5f, 10.f);
	Fixture.AddOpponent(TEXT("Opp_Test"), { TEXT("Obj_AtStrongLink"), TEXT("Obj_AtEmptyLink") });

	TestTrue(TEXT("Debate opens"), Fixture.Debate->StartDebate(TEXT("Opp_Test"), Anchor));

	FDebateObjectionView View;
	TestTrue(TEXT("An objection is on the table"), Fixture.Debate->GetCurrentObjection(View));
	TestTrue(TEXT("He goes for the empty link first"), View.ObjectionID == FName(TEXT("Obj_AtEmptyLink")));
	TestEqual(TEXT("Which is worth nothing to defend"), View.ChainDefence, 0.f, 0.01f);

	// Answering it moves him on to the one he thinks is next weakest.
	Fixture.Debate->ConcedePoint();
	TestTrue(TEXT("Second objection raised"), Fixture.Debate->GetCurrentObjection(View));
	TestTrue(TEXT("Now the filled link"), View.ObjectionID == FName(TEXT("Obj_AtStrongLink")));
	TestEqual(TEXT("Worth its full link score"), View.ChainDefence, 1.f, 0.01f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDebateChainIsTheAmmunitionTest,
	"ChainOfWitnesses.Debate.ChainStrengthDecidesWhetherAnAnswerHolds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDebateChainIsTheAmmunitionTest::RunTest(const FString& Parameters)
{
	using namespace DebateTestHelpers;

	// A filled link carries the objection.
	{
		FDebateFixture Fixture;
		Fixture.AddFragment(TEXT("Frag_Witness"), EWitnessTier::Eyewitness);
		Fixture.SlotInto(ETransmissionLink::Eyewitness, TEXT("Frag_Witness"));
		Fixture.AddObjection(TEXT("Obj_Test"), ETransmissionLink::Eyewitness, 0.7f, 10.f);
		Fixture.AddOpponent(TEXT("Opp_Test"), { TEXT("Obj_Test") });

		Fixture.Debate->StartDebate(TEXT("Opp_Test"), Anchor);
		TestTrue(TEXT("Responded"), Fixture.Debate->RespondWithChain());

		const FDebateState State = Fixture.Debate->GetDebateState();
		TestEqual(TEXT("One round recorded"), State.Rounds.Num(), 1);
		TestTrue(TEXT("The objection was answered"), State.Rounds[0].bAnswered);
		// Defence 1.0 against a requirement of 0.7, at weight 10.
		TestEqual(TEXT("Ground gained on the margin"), State.Rounds[0].ConvictionDelta, 3.f, 0.01f);
	}

	// The same objection against an empty link does not.
	{
		FDebateFixture Fixture;
		Fixture.AddObjection(TEXT("Obj_Test"), ETransmissionLink::Eyewitness, 0.7f, 10.f);
		Fixture.AddOpponent(TEXT("Opp_Test"), { TEXT("Obj_Test") }, 100.f, /*Aggression=*/2.f);

		Fixture.Debate->StartDebate(TEXT("Opp_Test"), Anchor);
		Fixture.Debate->RespondWithChain();

		const FDebateState State = Fixture.Debate->GetDebateState();
		TestFalse(TEXT("The objection stood"), State.Rounds[0].bAnswered);
		// Margin -0.7, weight 10, and an aggressive opponent doubles what it costs.
		TestEqual(TEXT("Ground lost at the opponent's aggression"),
			State.Rounds[0].ConvictionDelta, -14.f, 0.01f);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDebateDirectCitationTest,
	"ChainOfWitnesses.Debate.CitingTheSourceBeatsGesturingAtTheChain",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDebateDirectCitationTest::RunTest(const FString& Parameters)
{
	using namespace DebateTestHelpers;

	FDebateFixture Fixture;
	Fixture.AddFragment(TEXT("Frag_Answer"), EWitnessTier::Hostile);
	Fixture.AddFragment(TEXT("Frag_Unheld"), EWitnessTier::Hostile);

	// Registered but never recovered: the player has heard of it and cannot cite it.
	FTestimonyFragmentDefinition Unrecovered;
	Unrecovered.FragmentID = TEXT("Frag_Unrecovered");
	Unrecovered.Tier = EWitnessTier::Hostile;
	Unrecovered.AttestationDateAD = AnchorYear + 5;
	Fixture.Codex->RegisterFragmentDefinition(Unrecovered);

	Fixture.AddObjection(TEXT("Obj_Test"), ETransmissionLink::Event, 1.f, 10.f,
		{ TEXT("Frag_Answer"), TEXT("Frag_Unrecovered") });
	Fixture.AddOpponent(TEXT("Opp_Test"), { TEXT("Obj_Test") });

	Fixture.Debate->StartDebate(TEXT("Opp_Test"), Anchor);

	FDebateObjectionView View;
	Fixture.Debate->GetCurrentObjection(View);
	TestEqual(TEXT("Only what he actually holds is offered"), View.AvailableAnswerFragmentIDs.Num(), 1);
	TestTrue(TEXT("And it is the recovered one"),
		View.AvailableAnswerFragmentIDs[0] == FName(TEXT("Frag_Answer")));

	// Leaning on an empty Event link could not have met a requirement of 1.0;
	// naming the source does.
	TestFalse(TEXT("Cannot cite something that does not answer this"),
		Fixture.Debate->RespondWithFragment(TEXT("Frag_Unheld")));
	TestFalse(TEXT("Cannot cite something never recovered"),
		Fixture.Debate->RespondWithFragment(TEXT("Frag_Unrecovered")));
	TestTrue(TEXT("Cited the source"), Fixture.Debate->RespondWithFragment(TEXT("Frag_Answer")));

	const FDebateState State = Fixture.Debate->GetDebateState();
	TestTrue(TEXT("Which answered it"), State.Rounds[0].bAnswered);
	TestEqual(TEXT("Worth the direct-answer value"),
		State.Rounds[0].Defence, Fixture.Debate->DirectAnswerDefence, 0.01f);
	TestTrue(TEXT("The citation is spent"), State.SpentFragmentIDs.Contains(FName(TEXT("Frag_Answer"))));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDebateUnansweredChallengeTest,
	"ChainOfWitnesses.Debate.AnUnansweredChallengeHalvesTheLink",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDebateUnansweredChallengeTest::RunTest(const FString& Parameters)
{
	using namespace DebateTestHelpers;

	FDebateFixture Fixture;

	// The objection the fragment carries with it, which nothing in the chain rebuts.
	FTestimonyFragmentDefinition Challenger;
	Challenger.FragmentID = TEXT("Frag_Challenger");
	Challenger.Tier = EWitnessTier::Manuscript;
	Challenger.AttestationDateAD = 300;
	Fixture.Codex->RegisterFragmentDefinition(Challenger);

	Fixture.AddFragment(TEXT("Frag_Witness"), EWitnessTier::Eyewitness, { TEXT("Frag_Challenger") });
	Fixture.SlotInto(ETransmissionLink::Eyewitness, TEXT("Frag_Witness"));

	// A full link score would have carried this. Halved, it does not.
	Fixture.AddObjection(TEXT("Obj_Test"), ETransmissionLink::Eyewitness, 0.7f, 10.f);
	Fixture.AddOpponent(TEXT("Opp_Test"), { TEXT("Obj_Test") });

	Fixture.Debate->StartDebate(TEXT("Opp_Test"), Anchor);

	FDebateObjectionView View;
	Fixture.Debate->GetCurrentObjection(View);
	TestTrue(TEXT("The view flags the standing objection"), View.bLinkHasUnansweredChallenge);
	TestEqual(TEXT("Defence is halved"), View.ChainDefence, 0.5f, 0.01f);

	Fixture.Debate->RespondWithChain();
	TestFalse(TEXT("So the answer no longer holds"),
		Fixture.Debate->GetDebateState().Rounds[0].bAnswered);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDebateLegibleDefeatTest,
	"ChainOfWitnesses.Debate.DefeatNamesTheLinkAndTheReadingList",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDebateLegibleDefeatTest::RunTest(const FString& Parameters)
{
	using namespace DebateTestHelpers;

	FDebateFixture Fixture;

	// Something that would have answered, which the player has never found.
	FTestimonyFragmentDefinition Missing;
	Missing.FragmentID = TEXT("Frag_NeverFound");
	Missing.Tier = EWitnessTier::Hostile;
	Missing.AttestationDateAD = AnchorYear + 5;
	Fixture.Codex->RegisterFragmentDefinition(Missing);

	Fixture.AddObjection(TEXT("Obj_Test"), ETransmissionLink::Event, 0.7f, 30.f, { TEXT("Frag_NeverFound") });
	Fixture.AddOpponent(TEXT("Opp_Test"), { TEXT("Obj_Test") }, /*Composure=*/15.f);

	Fixture.Debate->StartDebate(TEXT("Opp_Test"), Anchor);
	Fixture.Debate->RespondWithChain();

	TestFalse(TEXT("The debate is over"), Fixture.Debate->IsDebateActive());

	const FDebateOutcomeReport Report = Fixture.Debate->GetOutcomeReport();
	TestTrue(TEXT("Lost"), Report.Outcome == EDebateOutcome::Lost);
	TestEqual(TEXT("The objection that landed is named"), Report.ObjectionsThatLanded.Num(), 1);
	TestEqual(TEXT("So is what would have met it"), Report.WouldHaveAnsweredFragmentIDs.Num(), 1);
	TestTrue(TEXT("By name"),
		Report.WouldHaveAnsweredFragmentIDs[0] == FName(TEXT("Frag_NeverFound")));
	TestTrue(TEXT("And the link that failed"), Report.WeakestLink == ETransmissionLink::Event);
	TestEqual(TEXT("The transcript is kept"), Report.Rounds.Num(), 1);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDebateEarlyWinAndEraTest,
	"ChainOfWitnesses.Debate.ComposureBreaksEarlyAndEraGatesOpponents",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDebateEarlyWinAndEraTest::RunTest(const FString& Parameters)
{
	using namespace DebateTestHelpers;

	// A thin-skinned opponent concedes before his rounds are up.
	{
		FDebateFixture Fixture;
		Fixture.AddFragment(TEXT("Frag_Witness"), EWitnessTier::Eyewitness);
		Fixture.SlotInto(ETransmissionLink::Eyewitness, TEXT("Frag_Witness"));

		Fixture.AddObjection(TEXT("Obj_A"), ETransmissionLink::Eyewitness, 0.5f, 20.f);
		Fixture.AddObjection(TEXT("Obj_B"), ETransmissionLink::Eyewitness, 0.5f, 20.f);
		Fixture.AddOpponent(TEXT("Opp_Brittle"), { TEXT("Obj_A"), TEXT("Obj_B") }, /*Composure=*/5.f);

		Fixture.Debate->StartDebate(TEXT("Opp_Brittle"), Anchor);
		Fixture.Debate->RespondWithChain();

		TestFalse(TEXT("Over in one round"), Fixture.Debate->IsDebateActive());
		TestTrue(TEXT("Won"), Fixture.Debate->GetOutcomeReport().Outcome == EDebateOutcome::Won);
		TestEqual(TEXT("He never got to his second objection"),
			Fixture.Debate->GetOutcomeReport().Rounds.Num(), 1);
	}

	// A Marcionite has nothing to say in AD 30.
	{
		FDebateFixture Fixture;
		Fixture.AddObjection(TEXT("Obj_Late"), ETransmissionLink::Manuscript, 0.5f, 10.f);

		FDebateOpponentRow Marcionite;
		Marcionite.OpponentID = TEXT("Opp_Marcionite");
		Marcionite.ObjectionIDs.Add(TEXT("Obj_Late"));
		Marcionite.EarliestYearAD = 140;
		Fixture.Debate->RegisterOpponent(Marcionite);

		TestFalse(TEXT("He does not exist yet"),
			Fixture.Debate->StartDebate(TEXT("Opp_Marcionite"), Anchor));

		Fixture.Timeline->AdvanceToYear(150);
		TestTrue(TEXT("By AD 150 he does"),
			Fixture.Debate->StartDebate(TEXT("Opp_Marcionite"), Anchor));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDebateRecordsDeedTest,
	"ChainOfWitnesses.Debate.ArguingInPublicIsSomethingThePlayerDid",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDebateRecordsDeedTest::RunTest(const FString& Parameters)
{
	using namespace DebateTestHelpers;

	FDebateFixture Fixture;

	FFactionRow Church;
	Church.FactionID = TEXT("Faction_Church");
	Fixture.Reputation->RegisterFaction(Church);

	FFactionImpact Impact;
	Impact.FactionID = TEXT("Faction_Church");
	Impact.Delta = 15.f;

	FDeedTypeRow WonDeed;
	WonDeed.DeedTypeID = TEXT("Deed_WonPublicDisputation");
	WonDeed.FactionImpacts.Add(Impact);
	WonDeed.bTravelsByWordOfMouth = false;
	Fixture.Reputation->RegisterDeedType(WonDeed);

	Fixture.AddFragment(TEXT("Frag_Witness"), EWitnessTier::Eyewitness);
	Fixture.SlotInto(ETransmissionLink::Eyewitness, TEXT("Frag_Witness"));
	Fixture.AddObjection(TEXT("Obj_Test"), ETransmissionLink::Eyewitness, 0.5f, 20.f);

	FDebateOpponentRow Opponent;
	Opponent.OpponentID = TEXT("Opp_Public");
	Opponent.ObjectionIDs.Add(TEXT("Obj_Test"));
	Opponent.Composure = 5.f;
	Opponent.VictoryDeedTypeID = TEXT("Deed_WonPublicDisputation");
	Fixture.Debate->RegisterOpponent(Opponent);

	Fixture.Debate->StartDebate(TEXT("Opp_Public"), Anchor);
	Fixture.Debate->RespondWithChain();

	TestTrue(TEXT("Won"), Fixture.Debate->GetOutcomeReport().Outcome == EDebateOutcome::Won);
	TestEqual(TEXT("And it went on the player's record"),
		Fixture.Reputation->GetFactionReputation(TEXT("Faction_Church")), 15.f, 0.01f);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
