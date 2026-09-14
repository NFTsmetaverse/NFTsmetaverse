#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "ChainAutomationFlags.h"

#include "Codex/CodexSubsystem.h"
#include "UObject/StrongObjectPtr.h"

namespace CodexTestHelpers
{
	/**
	 * Scoring is a pure function of the chain plus the registry, so the subsystem can
	 * be exercised standalone -- no GameInstance, no world. Tests use
	 * CreateChainWithAnchorYear rather than CreateChain for the same reason.
	 */
	UCodexSubsystem* MakeCodex()
	{
		return NewObject<UCodexSubsystem>(GetTransientPackage());
	}

	FTestimonyFragmentDefinition MakeFragment(FName ID, EWitnessTier Tier, int32 AttestationDateAD)
	{
		FTestimonyFragmentDefinition Definition;
		Definition.FragmentID = ID;
		Definition.Tier = Tier;
		Definition.AttestationDateAD = AttestationDateAD;
		return Definition;
	}

	/** Registers and immediately recovers, for tests that are not about recovery gating. */
	void AddAndRecover(UCodexSubsystem& Codex, const FTestimonyFragmentDefinition& Definition)
	{
		Codex.RegisterFragmentDefinition(Definition);
		Codex.RecoverFragment(Definition.FragmentID);
	}

	constexpr float Tolerance = 0.01f;
	constexpr int32 TestAnchorYear = 30;

	// Left as a literal rather than a global FName: FName globals are constructed
	// during static init, before the name table is guaranteed ready. Call sites
	// convert at use.
	const TCHAR* const TestAnchor = TEXT("Event_Test");
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCodexEmptyChainTest,
	"ChainOfWitnesses.Codex.EmptyChainScoresZeroAndNamesFirstLink",
	CHAIN_TEST_FLAGS)

bool FCodexEmptyChainTest::RunTest(const FString& Parameters)
{
	using namespace CodexTestHelpers;
	TStrongObjectPtr<UCodexSubsystem> Codex(MakeCodex());

	TestTrue(TEXT("Chain opens"), Codex->CreateChainWithAnchorYear(TestAnchor, TestAnchorYear));

	const FChainScoreBreakdown Breakdown = Codex->ScoreChain(TestAnchor);

	TestTrue(TEXT("Chain exists"), Breakdown.bChainExists);
	TestFalse(TEXT("Empty chain is not complete"), Breakdown.bIsComplete);
	TestEqual(TEXT("Empty chain scores zero"), Breakdown.AttestationStrength, 0.f, Tolerance);
	TestEqual(TEXT("All six links reported"), Breakdown.LinkScores.Num(), NumTransmissionLinks);
	TestTrue(TEXT("Weakest link ties resolve to the first link"),
		Breakdown.WeakestLink == ETransmissionLink::Event);

	// Scoring a chain that was never opened must be safe, not a crash or a false positive.
	const FChainScoreBreakdown Missing = Codex->ScoreChain(FName(TEXT("Event_DoesNotExist")));
	TestFalse(TEXT("Missing chain reports absence"), Missing.bChainExists);
	TestEqual(TEXT("Missing chain scores zero"), Missing.AttestationStrength, 0.f, Tolerance);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCodexProximityTest,
	"ChainOfWitnesses.Codex.ProximityDecaysWithDistanceFromTheEvent",
	CHAIN_TEST_FLAGS)

bool FCodexProximityTest::RunTest(const FString& Parameters)
{
	using namespace CodexTestHelpers;

	// Attestation inside the living-memory window: full credit for its link, so the
	// chain scores one filled link out of six.
	{
		TStrongObjectPtr<UCodexSubsystem> Codex(MakeCodex());
		AddAndRecover(*Codex, MakeFragment(TEXT("Frag_Close"), EWitnessTier::Eyewitness, TestAnchorYear + 10));
		Codex->CreateChainWithAnchorYear(TestAnchor, TestAnchorYear);
		Codex->SlotFragment(TestAnchor, ETransmissionLink::Eyewitness, TEXT("Frag_Close"));

		const FChainScoreBreakdown Breakdown = Codex->ScoreChain(TestAnchor);
		TestEqual(TEXT("Close attestation fills its link"), Breakdown.LinkScores[1].LinkScore, 1.f, Tolerance);
		TestEqual(TEXT("One of six links"), Breakdown.AttestationStrength, 100.f / NumTransmissionLinks, Tolerance);
		TestTrue(TEXT("An unfilled link is now the weakest"),
			Breakdown.WeakestLink == ETransmissionLink::Event);
	}

	// Past the falloff window the fragment contributes nothing.
	{
		TStrongObjectPtr<UCodexSubsystem> Codex(MakeCodex());
		AddAndRecover(*Codex, MakeFragment(TEXT("Frag_Late"), EWitnessTier::Eyewitness, TestAnchorYear + 200));
		Codex->CreateChainWithAnchorYear(TestAnchor, TestAnchorYear);
		Codex->SlotFragment(TestAnchor, ETransmissionLink::Eyewitness, TEXT("Frag_Late"));

		const FChainScoreBreakdown Breakdown = Codex->ScoreChain(TestAnchor);
		TestEqual(TEXT("Distant attestation scores nothing"), Breakdown.AttestationStrength, 0.f, Tolerance);
		TestTrue(TEXT("Link still counts as filled"), Breakdown.LinkScores[1].bIsFilled);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCodexTierFitTest,
	"ChainOfWitnesses.Codex.MismatchedTierIsPenalisedNotBlocked",
	CHAIN_TEST_FLAGS)

bool FCodexTierFitTest::RunTest(const FString& Parameters)
{
	using namespace CodexTestHelpers;
	TStrongObjectPtr<UCodexSubsystem> Codex(MakeCodex());

	AddAndRecover(*Codex, MakeFragment(TEXT("Frag_Witness"), EWitnessTier::Eyewitness, TestAnchorYear + 10));
	Codex->CreateChainWithAnchorYear(TestAnchor, TestAnchorYear);

	// An eyewitness is not a manuscript. The slot is allowed -- experimenting is how
	// the player learns the shape of a chain -- but it scores at half.
	TestTrue(TEXT("Mismatched slot is permitted"),
		Codex->SlotFragment(TestAnchor, ETransmissionLink::Manuscript, TEXT("Frag_Witness")));

	const FChainScoreBreakdown Breakdown = Codex->ScoreChain(TestAnchor);
	TestEqual(TEXT("Mismatched tier scores at half"),
		Breakdown.LinkScores[static_cast<int32>(ETransmissionLink::Manuscript)].LinkScore, 0.5f, Tolerance);

	// One fragment may occupy only one link of a chain.
	TestFalse(TEXT("Same fragment cannot be double-counted"),
		Codex->SlotFragment(TestAnchor, ETransmissionLink::Eyewitness, TEXT("Frag_Witness")));

	// Unrecovered fragments cannot be slotted at all.
	Codex->RegisterFragmentDefinition(MakeFragment(TEXT("Frag_Unfound"), EWitnessTier::Patristic, 110));
	TestFalse(TEXT("Unrecovered fragment cannot be slotted"),
		Codex->SlotFragment(TestAnchor, ETransmissionLink::PatristicAttestation, TEXT("Frag_Unfound")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCodexHostileAndCorroborationTest,
	"ChainOfWitnesses.Codex.HostileWitnessAndCorroborationRaiseStrength",
	CHAIN_TEST_FLAGS)

bool FCodexHostileAndCorroborationTest::RunTest(const FString& Parameters)
{
	using namespace CodexTestHelpers;
	TStrongObjectPtr<UCodexSubsystem> Codex(MakeCodex());

	FTestimonyFragmentDefinition Hostile = MakeFragment(TEXT("Frag_Hostile"), EWitnessTier::Hostile, TestAnchorYear + 20);
	FTestimonyFragmentDefinition Witness = MakeFragment(TEXT("Frag_Witness"), EWitnessTier::Eyewitness, TestAnchorYear + 10);
	Witness.CorroboratedBy.Add(TEXT("Frag_Hostile"));

	AddAndRecover(*Codex, Hostile);
	AddAndRecover(*Codex, Witness);

	Codex->CreateChainWithAnchorYear(TestAnchor, TestAnchorYear);
	Codex->SlotFragment(TestAnchor, ETransmissionLink::Event, TEXT("Frag_Hostile"));
	Codex->SlotFragment(TestAnchor, ETransmissionLink::Eyewitness, TEXT("Frag_Witness"));

	const FChainScoreBreakdown Breakdown = Codex->ScoreChain(TestAnchor);

	TestTrue(TEXT("Hostile confirmation detected"), Breakdown.bHasHostileConfirmation);
	TestEqual(TEXT("One corroborating pair"), Breakdown.IndependentCorroborationCount, 1);

	// Two filled links of six, plus the hostile bonus, plus one corroborating pair.
	const float Expected = (2.f / NumTransmissionLinks) * 100.f + 8.f + 3.f;
	TestEqual(TEXT("Bonuses applied"), Breakdown.AttestationStrength, Expected, Tolerance);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCodexChallengeTest,
	"ChainOfWitnesses.Codex.UnansweredChallengeCostsUntilRebutted",
	CHAIN_TEST_FLAGS)

bool FCodexChallengeTest::RunTest(const FString& Parameters)
{
	using namespace CodexTestHelpers;
	TStrongObjectPtr<UCodexSubsystem> Codex(MakeCodex());

	FTestimonyFragmentDefinition Attested = MakeFragment(TEXT("Frag_Attested"), EWitnessTier::Eyewitness, TestAnchorYear + 10);
	Attested.ChallengedBy.Add(TEXT("Frag_Objection"));

	FTestimonyFragmentDefinition Rebuttal = MakeFragment(TEXT("Frag_Rebuttal"), EWitnessTier::Patristic, TestAnchorYear + 10);
	Rebuttal.RebutsFragmentIDs.Add(TEXT("Frag_Objection"));

	AddAndRecover(*Codex, Attested);
	AddAndRecover(*Codex, Rebuttal);
	Codex->RegisterFragmentDefinition(MakeFragment(TEXT("Frag_Objection"), EWitnessTier::Manuscript, 300));

	Codex->CreateChainWithAnchorYear(TestAnchor, TestAnchorYear);
	Codex->SlotFragment(TestAnchor, ETransmissionLink::Eyewitness, TEXT("Frag_Attested"));

	// The objection stands whether or not the player has found it -- an opponent will
	// raise it regardless.
	const FChainScoreBreakdown Before = Codex->ScoreChain(TestAnchor);
	TestEqual(TEXT("Challenge is outstanding"), Before.UnansweredChallenges.Num(), 1);
	TestEqual(TEXT("Penalty applied"), Before.AttestationStrength,
		(1.f / NumTransmissionLinks) * 100.f - 10.f, Tolerance);

	// Slotting real rebutting evidence clears it.
	Codex->SlotFragment(TestAnchor, ETransmissionLink::PatristicAttestation, TEXT("Frag_Rebuttal"));

	const FChainScoreBreakdown After = Codex->ScoreChain(TestAnchor);
	TestEqual(TEXT("Challenge answered"), After.UnansweredChallenges.Num(), 0);
	TestEqual(TEXT("Penalty lifted"), After.AttestationStrength,
		(2.f / NumTransmissionLinks) * 100.f, Tolerance);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCodexSaveRoundTripTest,
	"ChainOfWitnesses.Codex.SaveDataRoundTrips",
	CHAIN_TEST_FLAGS)

bool FCodexSaveRoundTripTest::RunTest(const FString& Parameters)
{
	using namespace CodexTestHelpers;
	TStrongObjectPtr<UCodexSubsystem> Codex(MakeCodex());

	AddAndRecover(*Codex, MakeFragment(TEXT("Frag_A"), EWitnessTier::Eyewitness, TestAnchorYear + 10));
	AddAndRecover(*Codex, MakeFragment(TEXT("Frag_B"), EWitnessTier::Patristic, TestAnchorYear + 80));

	Codex->CreateChainWithAnchorYear(TestAnchor, TestAnchorYear);
	Codex->SlotFragment(TestAnchor, ETransmissionLink::Eyewitness, TEXT("Frag_A"));

	const FCodexSaveData Saved = Codex->CaptureSaveData();
	const float SavedStrength = Codex->ScoreChain(TestAnchor).AttestationStrength;

	// Diverge from the saved state, then restore over it.
	Codex->UnslotFragment(TestAnchor, TEXT("Frag_A"));
	Codex->SlotFragment(TestAnchor, ETransmissionLink::PatristicAttestation, TEXT("Frag_B"));
	Codex->RestoreFromSaveData(Saved);

	FTransmissionChain Restored;
	TestTrue(TEXT("Chain restored"), Codex->GetChain(TestAnchor, Restored));
	TestEqual(TEXT("Anchor year restored"), Restored.AnchorYearAD, TestAnchorYear);
	TestEqual(TEXT("All slots present"), Restored.Slots.Num(), NumTransmissionLinks);
	TestTrue(TEXT("Slotted fragment restored"), Restored.ContainsFragment(TEXT("Frag_A")));
	TestFalse(TEXT("Post-save slot discarded"), Restored.ContainsFragment(TEXT("Frag_B")));
	TestTrue(TEXT("Recovery restored"), Codex->IsFragmentRecovered(TEXT("Frag_B")));
	TestEqual(TEXT("Score round-trips"), Codex->ScoreChain(TestAnchor).AttestationStrength,
		SavedStrength, Tolerance);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
