#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "ChainAutomationFlags.h"
#include "ChainTestOuter.h"

#include "Campaign/CampaignTimelineSubsystem.h"
#include "Codex/CodexSubsystem.h"
#include "UI/CodexViewLibrary.h"
#include "UObject/StrongObjectPtr.h"

namespace CodexViewTestHelpers
{
	const TCHAR* const Anchor = TEXT("Event_Test");
	constexpr int32 AnchorYear = 30;

	/**
	 * Two slotted fragments, each carrying an objection: one that something in the
	 * corpus answers, and one that nothing does. That pair is what the challenge
	 * view has to be able to tell apart.
	 */
	struct FViewFixture
	{
		TStrongObjectPtr<UCodexSubsystem> Codex;
		TStrongObjectPtr<UCampaignTimelineSubsystem> Timeline;

		FViewFixture()
			: Codex(NewObject<UCodexSubsystem>(ChainTestOuter()))
			, Timeline(NewObject<UCampaignTimelineSubsystem>(ChainTestOuter()))
		{
			Codex->SetTimelineForTesting(Timeline.Get());

			FCampaignEventRow Event;
			Event.EventID = Anchor;
			Event.EventTitle = FText::FromString(TEXT("A Test Event"));
			Event.YearEarliestAD = AnchorYear;
			Event.YearLatestAD = 33;
			Timeline->RegisterEvent(Event);

			Add(TEXT("Frag_Challenger"), EWitnessTier::Manuscript, 300);
			Add(TEXT("Frag_Unanswerable"), EWitnessTier::Manuscript, 325);

			// Answers the first objection, and the player has not found it.
			FTestimonyFragmentDefinition Rebuttal = Make(TEXT("Frag_Rebuttal"), EWitnessTier::Patristic, 180);
			Rebuttal.RebutsFragmentIDs.Add(TEXT("Frag_Challenger"));
			Codex->RegisterFragmentDefinition(Rebuttal);

			FTestimonyFragmentDefinition Witness = Make(TEXT("Frag_Witness"), EWitnessTier::Eyewitness, 35);
			Witness.ChallengedBy.Add(TEXT("Frag_Challenger"));
			Witness.Title = FText::FromString(TEXT("A witness"));
			Witness.SourceReference = FText::FromString(TEXT("Some Source 1.2.3"));
			Codex->RegisterFragmentDefinition(Witness);
			Codex->RecoverFragment(TEXT("Frag_Witness"));

			FTestimonyFragmentDefinition Document = Make(TEXT("Frag_Doc"), EWitnessTier::Companion, 55);
			Document.ChallengedBy.Add(TEXT("Frag_Unanswerable"));
			Codex->RegisterFragmentDefinition(Document);
			Codex->RecoverFragment(TEXT("Frag_Doc"));

			// Recovered but left out of the chain, so it shows up as slottable.
			Add(TEXT("Frag_Spare"), EWitnessTier::Eyewitness, 40);
			Codex->RecoverFragment(TEXT("Frag_Spare"));

			Codex->CreateChain(Anchor);
			Codex->SlotFragment(Anchor, ETransmissionLink::Eyewitness, TEXT("Frag_Witness"));
			Codex->SlotFragment(Anchor, ETransmissionLink::WrittenSource, TEXT("Frag_Doc"));
		}

		static FTestimonyFragmentDefinition Make(const TCHAR* ID, EWitnessTier Tier, int32 AttestedAD)
		{
			FTestimonyFragmentDefinition Definition;
			Definition.FragmentID = ID;
			Definition.Tier = Tier;
			Definition.AttestationDateAD = AttestedAD;
			return Definition;
		}

		void Add(const TCHAR* ID, EWitnessTier Tier, int32 AttestedAD)
		{
			Codex->RegisterFragmentDefinition(Make(ID, Tier, AttestedAD));
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCodexViewFormattingTest,
	"ChainOfWitnesses.CodexUI.ScoresAndYearsReadAsWordsNotNumbers",
	CHAIN_TEST_FLAGS)

bool FCodexViewFormattingTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("Nothing is empty"),
		UCodexViewLibrary::BandForLinkScore(0.f) == EAttestationBand::Empty);
	TestTrue(TEXT("A little is thin"),
		UCodexViewLibrary::BandForLinkScore(0.2f) == EAttestationBand::Thin);
	TestTrue(TEXT("Half is serviceable"),
		UCodexViewLibrary::BandForLinkScore(0.5f) == EAttestationBand::Serviceable);
	TestTrue(TEXT("Most of it holds"),
		UCodexViewLibrary::BandForLinkScore(0.9f) == EAttestationBand::Strong);

	// The chain meter is the same bands on a 0..100 scale.
	TestTrue(TEXT("Chain strength bands match"),
		UCodexViewLibrary::BandForAttestationStrength(50.f) == EAttestationBand::Serviceable);

	// A year is not a quantity: 1229 must not come out as "1,229".
	TestEqual(TEXT("A frame-era year reads plainly"),
		UCodexViewLibrary::FormatYearAD(1229).ToString(), FString(TEXT("AD 1229")));
	TestEqual(TEXT("A range reads as a range"),
		UCodexViewLibrary::FormatYearRangeAD(55, 70).ToString(), FString(TEXT("AD 55-70")));
	TestEqual(TEXT("A range of one year does not"),
		UCodexViewLibrary::FormatYearRangeAD(65, 65).ToString(), FString(TEXT("AD 65")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCodexViewFragmentTest,
	"ChainOfWitnesses.CodexUI.TheInspectorShowsTheCitation",
	CHAIN_TEST_FLAGS)

bool FCodexViewFragmentTest::RunTest(const FString& Parameters)
{
	using namespace CodexViewTestHelpers;
	FViewFixture Fixture;

	const FCodexFragmentView View = UCodexViewLibrary::MakeFragmentView(
		Fixture.Codex.Get(), TEXT("Frag_Witness"));

	TestTrue(TEXT("Known"), View.bIsKnownFragment);
	TestTrue(TEXT("Recovered"), View.bIsRecovered);
	TestEqual(TEXT("The citation is carried through verbatim"),
		View.SourceReference.ToString(), FString(TEXT("Some Source 1.2.3")));
	TestEqual(TEXT("Its date is legible"), View.AttestationDateText.ToString(), FString(TEXT("AD 35")));
	TestFalse(TEXT("Tier name is not blank"), View.TierName.IsEmpty());

	// An unregistered ID is reported as unknown rather than drawn as an empty row.
	const FCodexFragmentView Missing = UCodexViewLibrary::MakeFragmentView(
		Fixture.Codex.Get(), TEXT("Frag_DoesNotExist"));
	TestFalse(TEXT("Unknown fragments say so"), Missing.bIsKnownFragment);

	// A null Codex must not crash a screen that is still being set up.
	const FCodexFragmentView NoCodex = UCodexViewLibrary::MakeFragmentView(nullptr, TEXT("Frag_Witness"));
	TestFalse(TEXT("Safe without a Codex"), NoCodex.bIsKnownFragment);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCodexViewChainTest,
	"ChainOfWitnesses.CodexUI.TheChainScreenShowsAllSixLinksAndTheWeakest",
	CHAIN_TEST_FLAGS)

bool FCodexViewChainTest::RunTest(const FString& Parameters)
{
	using namespace CodexViewTestHelpers;
	FViewFixture Fixture;

	const FCodexChainView View = UCodexViewLibrary::MakeChainView(
		Fixture.Codex.Get(), Fixture.Timeline.Get(), Anchor);

	TestTrue(TEXT("The chain is open"), View.bChainExists);
	TestEqual(TEXT("Six links, always"), View.Links.Num(), NumTransmissionLinks);
	TestEqual(TEXT("The anchor is named"), View.AnchorTitle.ToString(), FString(TEXT("A Test Event")));
	TestEqual(TEXT("And dated"), View.AnchorDateText.ToString(), FString(TEXT("AD 30-33")));
	TestFalse(TEXT("Two links of six is not complete"), View.bIsComplete);

	// Exactly one link is marked as the one an opponent goes for.
	int32 WeakestMarked = 0;
	int32 FilledLinks = 0;
	for (const FCodexLinkView& Link : View.Links)
	{
		if (Link.bIsWeakestLink)
		{
			++WeakestMarked;
		}
		if (Link.bIsFilled)
		{
			++FilledLinks;
		}
		TestFalse(TEXT("Every link is named"), Link.LinkName.IsEmpty());
		TestFalse(TEXT("And explained"), Link.LinkDescription.IsEmpty());
	}

	TestEqual(TEXT("One weakest link"), WeakestMarked, 1);
	TestEqual(TEXT("Two links filled"), FilledLinks, 2);

	const FCodexLinkView& Eyewitness = View.Links[static_cast<int32>(ETransmissionLink::Eyewitness)];
	TestEqual(TEXT("The witness is in the eyewitness link"), Eyewitness.SlottedFragments.Num(), 1);
	TestTrue(TEXT("And is marked as slotted there"), Eyewitness.SlottedFragments[0].bIsSlotted);
	TestTrue(TEXT("Knowing which link it sits in"),
		Eyewitness.SlottedFragments[0].SlottedInLink == ETransmissionLink::Eyewitness);

	// An empty link is the weakest, and empties tie to the first in transmission order.
	TestTrue(TEXT("The Event link is the hole"), View.WeakestLink == ETransmissionLink::Event);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCodexViewSlottableTest,
	"ChainOfWitnesses.CodexUI.CandidatesForALinkExcludeWhatIsAlreadyInTheChain",
	CHAIN_TEST_FLAGS)

bool FCodexViewSlottableTest::RunTest(const FString& Parameters)
{
	using namespace CodexViewTestHelpers;
	FViewFixture Fixture;

	TArray<FCodexFragmentView> Candidates;
	UCodexViewLibrary::GetSlottableFragments(Fixture.Codex.Get(), Anchor,
		ETransmissionLink::Eyewitness, /*bOnlyTierAppropriate=*/true, Candidates);

	// Frag_Witness and Frag_Doc are already in the chain; Frag_Spare is not.
	TestEqual(TEXT("Only the spare is available"), Candidates.Num(), 1);
	TestTrue(TEXT("And it is the spare"), Candidates[0].FragmentID == FName(TEXT("Frag_Spare")));

	// Unrecovered fragments are never candidates, whatever their tier.
	UCodexViewLibrary::GetSlottableFragments(Fixture.Codex.Get(), Anchor,
		ETransmissionLink::PatristicAttestation, /*bOnlyTierAppropriate=*/true, Candidates);
	TestEqual(TEXT("The unrecovered rebuttal is not offered"), Candidates.Num(), 0);

	// Dropping the tier filter widens the list, because slotting a mismatch is
	// allowed -- it just scores badly.
	UCodexViewLibrary::GetSlottableFragments(Fixture.Codex.Get(), Anchor,
		ETransmissionLink::PatristicAttestation, /*bOnlyTierAppropriate=*/false, Candidates);
	TestEqual(TEXT("Without the filter, the spare is offered anywhere"), Candidates.Num(), 1);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCodexViewChallengeTest,
	"ChainOfWitnesses.CodexUI.AnUnansweredObjectionIsNotTheSameAsAnUnanswerableOne",
	CHAIN_TEST_FLAGS)

bool FCodexViewChallengeTest::RunTest(const FString& Parameters)
{
	using namespace CodexViewTestHelpers;
	FViewFixture Fixture;

	TArray<FCodexChallengeView> Challenges;
	UCodexViewLibrary::GetChainChallenges(Fixture.Codex.Get(), Anchor, Challenges);

	TestEqual(TEXT("Both objections are listed"), Challenges.Num(), 2);

	const FCodexChallengeView* Answerable = Challenges.FindByPredicate(
		[](const FCodexChallengeView& C){ return C.Challenger.FragmentID == FName(TEXT("Frag_Challenger")); });
	const FCodexChallengeView* Unanswerable = Challenges.FindByPredicate(
		[](const FCodexChallengeView& C){ return C.Challenger.FragmentID == FName(TEXT("Frag_Unanswerable")); });

	TestNotNull(TEXT("The answerable one is there"), Answerable);
	TestNotNull(TEXT("So is the unanswerable one"), Unanswerable);

	if (!Answerable || !Unanswerable)
	{
		return false;
	}

	// An answer exists; the player simply has not found it. That is a mission hook.
	TestFalse(TEXT("Standing"), Answerable->bIsAnswered);
	TestFalse(TEXT("But something does answer it"), Answerable->bHasNoKnownAnswer);
	TestEqual(TEXT("Which he does not hold"), Answerable->AvailableAnswers.Num(), 0);
	TestEqual(TEXT("And so appears on the reading list"), Answerable->MissingAnswers.Num(), 1);
	TestTrue(TEXT("It knows which link it hits"),
		Answerable->AgainstLink == ETransmissionLink::Eyewitness);

	// Nothing in the record answers this one. Telling the player to go and look
	// would be a lie.
	TestFalse(TEXT("Also standing"), Unanswerable->bIsAnswered);
	TestTrue(TEXT("And nothing answers it"), Unanswerable->bHasNoKnownAnswer);
	TestEqual(TEXT("No reading list for it"), Unanswerable->MissingAnswers.Num(), 0);

	// Finding and slotting the rebuttal settles the first and leaves the second.
	Fixture.Codex->RecoverFragment(TEXT("Frag_Rebuttal"));
	Fixture.Codex->SlotFragment(Anchor, ETransmissionLink::PatristicAttestation, TEXT("Frag_Rebuttal"));

	UCodexViewLibrary::GetChainChallenges(Fixture.Codex.Get(), Anchor, Challenges);

	Answerable = Challenges.FindByPredicate(
		[](const FCodexChallengeView& C){ return C.Challenger.FragmentID == FName(TEXT("Frag_Challenger")); });
	Unanswerable = Challenges.FindByPredicate(
		[](const FCodexChallengeView& C){ return C.Challenger.FragmentID == FName(TEXT("Frag_Unanswerable")); });

	TestNotNull(TEXT("Still listed"), Answerable);
	TestNotNull(TEXT("Both still listed"), Unanswerable);

	if (!Answerable || !Unanswerable)
	{
		return false;
	}

	TestTrue(TEXT("Now answered"), Answerable->bIsAnswered);
	TestEqual(TEXT("By something he now holds"), Answerable->AvailableAnswers.Num(), 1);
	TestFalse(TEXT("The other one has not moved"), Unanswerable->bIsAnswered);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
