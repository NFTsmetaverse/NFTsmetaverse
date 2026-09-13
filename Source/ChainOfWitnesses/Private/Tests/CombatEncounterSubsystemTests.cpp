#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Campaign/CampaignTimelineSubsystem.h"
#include "Codex/CodexSubsystem.h"
#include "Combat/CombatEncounterSubsystem.h"
#include "Dialogue/DialogueSubsystem.h"
#include "Map/CampaignMapSubsystem.h"
#include "Reputation/ReputationSubsystem.h"
#include "UObject/StrongObjectPtr.h"
#include "Witness/WitnessMissionSubsystem.h"

namespace CombatTestHelpers
{
	const TCHAR* const FrameEraID = TEXT("Era_TestFrame");
	const TCHAR* const WitnessEraID = TEXT("Era_TestWitness");
	const TCHAR* const FactionID = TEXT("Faction_TestTemple");
	const TCHAR* const LocationID = TEXT("Loc_TestCity");
	const TCHAR* const OvercomeDeedID = TEXT("Deed_TestShedBlood");
	const TCHAR* const DeEscalatedDeedID = TEXT("Deed_TestTalkedDown");

	/**
	 * The whole chain of subsystems the encounter layer touches, wired by hand.
	 *
	 * Two eras are registered because half of what this system does is read the
	 * era's judgement rather than make its own: the witness era counts running away
	 * as a win and the frame era does not, and neither of those is a fact about
	 * combat.
	 */
	struct FCombatFixture
	{
		TStrongObjectPtr<UCombatEncounterSubsystem> Combat;
		TStrongObjectPtr<UWitnessMissionSubsystem> Witness;
		TStrongObjectPtr<UReputationSubsystem> Reputation;
		TStrongObjectPtr<UCampaignMapSubsystem> Map;
		TStrongObjectPtr<UCampaignTimelineSubsystem> Timeline;
		TStrongObjectPtr<UCodexSubsystem> Codex;
		TStrongObjectPtr<UDialogueSubsystem> Dialogue;

		FCombatFixture()
			: Combat(NewObject<UCombatEncounterSubsystem>(GetTransientPackage()))
			, Witness(NewObject<UWitnessMissionSubsystem>(GetTransientPackage()))
			, Reputation(NewObject<UReputationSubsystem>(GetTransientPackage()))
			, Map(NewObject<UCampaignMapSubsystem>(GetTransientPackage()))
			, Timeline(NewObject<UCampaignTimelineSubsystem>(GetTransientPackage()))
			, Codex(NewObject<UCodexSubsystem>(GetTransientPackage()))
			, Dialogue(NewObject<UDialogueSubsystem>(GetTransientPackage()))
		{
			Combat->SetDependenciesForTesting(Reputation.Get(), Map.Get(), Witness.Get());
			Witness->SetDependenciesForTesting(Codex.Get(), Timeline.Get(), Reputation.Get(), Map.Get(), Dialogue.Get());
			Reputation->SetDependenciesForTesting(Timeline.Get(), Map.Get());
			Map->SetDependenciesForTesting(Timeline.Get(), Reputation.Get());

			AddEra(FrameEraID, EGameEra::Frame, MakeFrameTuning());
			AddEra(WitnessEraID, EGameEra::Witness, MakeWitnessTuning());

			FCampaignLocationRow Location;
			Location.LocationID = LocationID;
			Map->RegisterLocation(Location);
			Map->SetPartyLocation(LocationID);

			FFactionRow Faction;
			Faction.FactionID = FactionID;
			Reputation->RegisterFaction(Faction);
		}

		static FEraCombatTuning MakeFrameTuning()
		{
			FEraCombatTuning Tuning;
			Tuning.IncomingDamageScale = 1.f;
			Tuning.OutgoingDamageScale = 1.f;
			Tuning.bEscapeIsWinState = false;
			Tuning.bDeEscalationIsWinState = false;
			Tuning.TypicalEnemyCount = 12;
			return Tuning;
		}

		static FEraCombatTuning MakeWitnessTuning()
		{
			FEraCombatTuning Tuning;
			Tuning.IncomingDamageScale = 2.5f;
			Tuning.OutgoingDamageScale = 1.f;
			Tuning.bEscapeIsWinState = true;
			Tuning.bDeEscalationIsWinState = true;
			Tuning.TypicalEnemyCount = 3;
			return Tuning;
		}

		void AddEra(const TCHAR* EraID, EGameEra Era, const FEraCombatTuning& Tuning)
		{
			FEraDefinitionRow Row;
			Row.EraID = EraID;
			Row.Era = Era;
			Row.CombatTuning = Tuning;
			Witness->RegisterEra(Row);
		}

		/** Puts the fixture in the frame era, which is where a campaign starts. */
		void UseFrameEra()
		{
			UseEra(FrameEraID);
		}

		/**
		 * Selects an era's combat tuning. BeginFrameEra also moves the clock, which
		 * nothing here cares about; what matters is which FEraCombatTuning is live.
		 */
		void UseEra(const TCHAR* EraID)
		{
			Witness->BeginFrameEra(EraID, LocationID);
		}

		/** Both carry a faction impact, so that "was this recorded" is observable. */
		void AddDeedTypes()
		{
			FFactionImpact Blame;
			Blame.FactionID = FactionID;
			Blame.Delta = -15.f;

			FDeedTypeRow Overcome;
			Overcome.DeedTypeID = OvercomeDeedID;
			Overcome.WitnessStandingDelta = -10.f;
			Overcome.FactionImpacts.Add(Blame);
			Reputation->RegisterDeedType(Overcome);

			FFactionImpact Credit;
			Credit.FactionID = FactionID;
			Credit.Delta = 10.f;

			FDeedTypeRow DeEscalated;
			DeEscalated.DeedTypeID = DeEscalatedDeedID;
			DeEscalated.WitnessStandingDelta = 10.f;
			DeEscalated.FactionImpacts.Add(Credit);
			Reputation->RegisterDeedType(DeEscalated);
		}

		FCombatEncounterRow MakeRow(const TCHAR* EncounterTypeID, ECrowdKind CrowdKind, int32 Count)
		{
			FCombatEncounterRow Row;
			Row.EncounterTypeID = EncounterTypeID;
			Row.Summary = FText::FromString(TEXT("A test encounter."));
			Row.CrowdKind = CrowdKind;
			Row.FactionID = FactionID;
			Row.CombatantCount = Count;
			Row.StartingResolve = 1.f;
			Row.DeEscalationDifficulty = 0.5f;
			Row.EscapeDifficulty = 0.5f;
			Row.OvercomeDeedTypeID = OvercomeDeedID;
			Row.DeEscalatedDeedTypeID = DeEscalatedDeedID;
			return Row;
		}
	};
}

using namespace CombatTestHelpers;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatEraCapsCombatantCountTest,
	"ChainOfWitnesses.Combat.EraCapsCombatantCountUnlessTheSizeIsThePoint",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatEraCapsCombatantCountTest::RunTest(const FString& Parameters)
{
	FCombatFixture Fixture;
	Fixture.UseFrameEra();

	FCombatEncounterRow Patrol = Fixture.MakeRow(TEXT("Cbt_TestPatrol"), ECrowdKind::Patrol, 8);
	Fixture.Combat->RegisterEncounterType(Patrol);

	FCombatEncounterRow Riot = Fixture.MakeRow(TEXT("Cbt_TestRiot"), ECrowdKind::Mob, 9);
	Riot.bIgnoreEraCombatantCap = true;
	Fixture.Combat->RegisterEncounterType(Riot);

	// The frame era wants twelve, so an eight-man patrol arrives intact.
	Fixture.Combat->BeginEncounter(TEXT("Cbt_TestPatrol"));
	TestEqual(TEXT("Frame era does not shrink an eight-man patrol"),
		Fixture.Combat->GetEncounterState().Combatants.Num(), 8);
	Fixture.Combat->EndEncounter(ECombatOutcome::Escaped);

	// A witness-era mission wants three. The patrol shrinks; the riot does not,
	// because a riot with three men in it is not a riot.
	FCombatFixture WitnessFixture;
	WitnessFixture.UseEra(WitnessEraID);
	WitnessFixture.Combat->RegisterEncounterType(WitnessFixture.MakeRow(TEXT("Cbt_TestPatrol"), ECrowdKind::Patrol, 8));

	FCombatEncounterRow BigRiot = WitnessFixture.MakeRow(TEXT("Cbt_TestRiot"), ECrowdKind::Mob, 9);
	BigRiot.bIgnoreEraCombatantCap = true;
	WitnessFixture.Combat->RegisterEncounterType(BigRiot);

	WitnessFixture.Combat->BeginEncounter(TEXT("Cbt_TestPatrol"));
	TestEqual(TEXT("Witness era caps the patrol at TypicalEnemyCount"),
		WitnessFixture.Combat->GetEncounterState().Combatants.Num(), 3);
	WitnessFixture.Combat->EndEncounter(ECombatOutcome::Escaped);

	WitnessFixture.Combat->BeginEncounter(TEXT("Cbt_TestRiot"));
	TestEqual(TEXT("A set piece keeps its authored size"),
		WitnessFixture.Combat->GetEncounterState().Combatants.Num(), 9);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatMenBreakBeforeTheyDieTest,
	"ChainOfWitnesses.Combat.MenBreakBeforeTheyDie",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatMenBreakBeforeTheyDieTest::RunTest(const FString& Parameters)
{
	FCombatFixture Fixture;
	Fixture.UseFrameEra();

	// Bandits take casualties worst of anyone: they are here for profit and a dead
	// companion changes the arithmetic at once.
	Fixture.Combat->RegisterEncounterType(Fixture.MakeRow(TEXT("Cbt_TestBandits"), ECrowdKind::Bandits, 3));
	Fixture.Combat->BeginEncounter(TEXT("Cbt_TestBandits"));

	// One of three killed. Shock is 0.35 * 1.5 = 0.525 on each of the other two,
	// leaving them at 0.475 -- above the 0.25 break threshold, but only just.
	Fixture.Combat->ApplyDamageToCombatant(0, 1.f);

	const FCombatEncounterState AfterFirst = Fixture.Combat->GetEncounterState();
	TestTrue(TEXT("The man hit is down"),
		AfterFirst.Combatants[0].Disposition == ECombatantDisposition::Down);
	TestTrue(TEXT("The others are shaken but still pressing"),
		AfterFirst.Combatants[1].Disposition == ECombatantDisposition::Hostile);
	TestTrue(TEXT("Shock took most of a man's resolve"),
		AfterFirst.Combatants[1].Resolve < 0.5f && AfterFirst.Combatants[1].Resolve > 0.25f);

	// A second body finishes it without a third being touched. This is the point of
	// the whole design: the encounter ends with one man alive and unhurt.
	Fixture.Combat->ApplyDamageToCombatant(1, 1.f);

	const FCombatOutcomeReport Report = Fixture.Combat->GetOutcomeReport();
	TestTrue(TEXT("The encounter is over"), Report.Outcome == ECombatOutcome::Overcome);
	TestEqual(TEXT("Two men were put down"), Report.CombatantsDown, 2);
	TestEqual(TEXT("The third broke rather than died"), Report.CombatantsBroken, 1);
	TestFalse(TEXT("Blood was drawn"), Report.bEndedWithoutBloodshed);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatCasualtyShockVariesByCrowdTest,
	"ChainOfWitnesses.Combat.CasualtyShockVariesByWhoTheyAre",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatCasualtyShockVariesByCrowdTest::RunTest(const FString& Parameters)
{
	FCombatFixture Fixture;
	Fixture.UseFrameEra();

	// Men who came to carry out a sentence expected resistance and have orders.
	// Killing two of them moves the remaining four almost not at all.
	Fixture.Combat->RegisterEncounterType(
		Fixture.MakeRow(TEXT("Cbt_TestExecution"), ECrowdKind::ExecutionParty, 6));
	Fixture.Combat->BeginEncounter(TEXT("Cbt_TestExecution"));

	Fixture.Combat->ApplyDamageToCombatant(0, 1.f);
	Fixture.Combat->ApplyDamageToCombatant(1, 1.f);

	const FCombatEncounterState State = Fixture.Combat->GetEncounterState();
	TestTrue(TEXT("An execution party is still coming after two of them are down"),
		Fixture.Combat->HasHostilesRemaining());
	TestTrue(TEXT("Two dead barely moved them"), State.Combatants[5].Resolve > 0.75f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatAMobBurnsItselfOutTest,
	"ChainOfWitnesses.Combat.AMobBurnsItselfOutAndNothingElseDoes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatAMobBurnsItselfOutTest::RunTest(const FString& Parameters)
{
	{
		FCombatFixture Fixture;
		Fixture.UseFrameEra();
		Fixture.Combat->RegisterEncounterType(Fixture.MakeRow(TEXT("Cbt_TestMob"), ECrowdKind::Mob, 5));
		Fixture.Combat->BeginEncounter(TEXT("Cbt_TestMob"));

		// Resolve 1.0, break at 0.25, decay 0.004/s: about three minutes. Long enough
		// that standing there is not a strategy, short enough that it is a real
		// property of a crowd rather than a decorative one.
		Fixture.Combat->AdvanceEncounter(120.f);
		TestTrue(TEXT("Two minutes in, a crowd is still a crowd"), Fixture.Combat->HasHostilesRemaining());

		Fixture.Combat->AdvanceEncounter(120.f);
		TestFalse(TEXT("Four minutes in, it has gone home"), Fixture.Combat->HasHostilesRemaining());

		const FCombatOutcomeReport Report = Fixture.Combat->GetOutcomeReport();
		TestTrue(TEXT("Nobody was hurt"), Report.bEndedWithoutBloodshed);
	}

	{
		FCombatFixture Fixture;
		Fixture.UseFrameEra();
		Fixture.Combat->RegisterEncounterType(Fixture.MakeRow(TEXT("Cbt_TestGuards"), ECrowdKind::Guard, 3));
		Fixture.Combat->BeginEncounter(TEXT("Cbt_TestGuards"));

		// Posted men will wait all day, which is why running works on a mob and
		// patience works on nobody.
		Fixture.Combat->AdvanceEncounter(600.f);
		TestTrue(TEXT("Guards are exactly where they were"), Fixture.Combat->HasHostilesRemaining());
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatStandingDoesTheTalkingTest,
	"ChainOfWitnesses.Combat.StandingCountsForAsMuchAsRhetoric",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatStandingDoesTheTalkingTest::RunTest(const FString& Parameters)
{
	FCombatFixture Fixture;
	Fixture.UseFrameEra();
	Fixture.AddDeedTypes();

	FCombatEncounterRow Row = Fixture.MakeRow(TEXT("Cbt_TestPatrol"), ECrowdKind::Patrol, 3);
	Row.DeEscalationDifficulty = 0.7f;
	Fixture.Combat->RegisterEncounterType(Row);

	// 0.6 of rhetoric against 0.7 of difficulty, with no name worth anything.
	Fixture.Combat->BeginEncounter(TEXT("Cbt_TestPatrol"));
	const FCombatAttemptResult Cold = Fixture.Combat->AttemptDeEscalation(0.6f);
	TestFalse(TEXT("Rhetoric alone falls short"), Cold.bSucceeded);
	TestTrue(TEXT("The requirement is reported, not hidden"), FMath::IsNearlyEqual(Cold.Required, 0.7f));
	TestTrue(TEXT("And so is what he brought"), FMath::IsNearlyEqual(Cold.Achieved, 0.6f));

	// The same words from a man the Temple thinks well of. +20 of standing at a
	// weight of 0.01 is worth 0.2, which is exactly the difference.
	FDeedTypeRow Favour;
	Favour.DeedTypeID = TEXT("Deed_TestFavour");
	FFactionImpact Impact;
	Impact.FactionID = FactionID;
	Impact.Delta = 20.f;
	Favour.FactionImpacts.Add(Impact);

	FCombatFixture Warm;
	Warm.UseFrameEra();
	Warm.AddDeedTypes();
	Warm.Combat->RegisterEncounterType(Row);
	Warm.Reputation->RegisterDeedType(Favour);
	Warm.Reputation->RecordDeed(TEXT("Deed_TestFavour"), LocationID, TArray<FName>());

	Warm.Combat->BeginEncounter(TEXT("Cbt_TestPatrol"));
	const FCombatAttemptResult Known = Warm.Combat->AttemptDeEscalation(0.6f);
	TestTrue(TEXT("The same words from a man they know land"), Known.bSucceeded);
	TestTrue(TEXT("Standing is what made the difference"), Known.Achieved > Cold.Achieved);
	TestTrue(TEXT("And it ends without anyone being touched"),
		Warm.Combat->GetOutcomeReport().Outcome == ECombatOutcome::DeEscalated);
	TestTrue(TEXT("Nobody was hurt"), Warm.Combat->GetOutcomeReport().bEndedWithoutBloodshed);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatDrawnSteelIsAnArgumentAgainstYouTest,
	"ChainOfWitnesses.Combat.DrawingHardensThemAndBloodEndsTheTalking",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatDrawnSteelIsAnArgumentAgainstYouTest::RunTest(const FString& Parameters)
{
	FCombatFixture Fixture;
	Fixture.UseFrameEra();

	FCombatEncounterRow Row = Fixture.MakeRow(TEXT("Cbt_TestPatrol"), ECrowdKind::Patrol, 3);
	Row.DeEscalationDifficulty = 0.5f;
	Fixture.Combat->RegisterEncounterType(Row);
	Fixture.Combat->BeginEncounter(TEXT("Cbt_TestPatrol"));

	Fixture.Combat->SetPosture(ECombatPosture::Drawn);
	const FCombatAttemptResult Drawn = Fixture.Combat->AttemptDeEscalation(0.6f);
	TestFalse(TEXT("A drawn blade costs him the attempt"), Drawn.bSucceeded);
	TestTrue(TEXT("The penalty is exactly the drawn penalty"),
		FMath::IsNearlyEqual(Drawn.Required, 0.5f + Fixture.Combat->DrawnPersuasionPenalty));

	// A failure hardens them, so the second attempt is harder than the first.
	const FCombatAttemptResult Second = Fixture.Combat->AttemptDeEscalation(0.6f);
	TestTrue(TEXT("Men who heard it once are harder to move"), Second.Required > Drawn.Required);

	// A crowd can be addressed before it moves.
	FCombatFixture MobFixture;
	MobFixture.UseFrameEra();
	MobFixture.Combat->RegisterEncounterType(MobFixture.MakeRow(TEXT("Cbt_TestMob"), ECrowdKind::Mob, 5));
	MobFixture.Combat->BeginEncounter(TEXT("Cbt_TestMob"));

	const FCombatAttemptResult BeforeBlood = MobFixture.Combat->AttemptDeEscalation(5.f);
	TestTrue(TEXT("A crowd can be addressed before it moves"), BeforeBlood.bSucceeded);

	// And it can still be addressed by a man it has just beaten. Acts 21-22: Paul is
	// seized and struck in the Temple court, and speaks to the crowd from the steps
	// afterwards. A rule that forbade that would be a tidier rule and a false one.
	MobFixture.Combat->BeginEncounter(TEXT("Cbt_TestMob"));
	MobFixture.Combat->ApplyDamageToPlayer(0.2f);
	const FCombatAttemptResult AfterBeating = MobFixture.Combat->AttemptDeEscalation(5.f);
	TestTrue(TEXT("A crowd will hear a man it has just manhandled"), AfterBeating.bSucceeded);

	// It will not hear one standing over a man he has put down.
	MobFixture.Combat->BeginEncounter(TEXT("Cbt_TestMob"));
	MobFixture.Combat->ApplyDamageToCombatant(0, 0.2f);
	const FCombatAttemptResult AfterBlood = MobFixture.Combat->AttemptDeEscalation(100.f);
	TestFalse(TEXT("But not one who has hurt one of its own"), AfterBlood.bSucceeded);
	TestFalse(TEXT("It says why, rather than just failing"), AfterBlood.Reason.IsEmpty());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatNearMissPeelsOneOffTest,
	"ChainOfWitnesses.Combat.ANearMissTakesTheLeastCommittedManOutOfIt",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatNearMissPeelsOneOffTest::RunTest(const FString& Parameters)
{
	FCombatFixture Fixture;
	Fixture.UseFrameEra();

	FCombatEncounterRow Row = Fixture.MakeRow(TEXT("Cbt_TestMob"), ECrowdKind::Mob, 4);
	Row.DeEscalationDifficulty = 1.f;
	Fixture.Combat->RegisterEncounterType(Row);
	Fixture.Combat->BeginEncounter(TEXT("Cbt_TestMob"));

	// 0.8 against 1.0 is a failure, but it clears the 0.75 partial fraction, so one
	// man at the back remembers he has somewhere to be.
	const FCombatAttemptResult Near = Fixture.Combat->AttemptDeEscalation(0.8f);
	TestFalse(TEXT("It still failed"), Near.bSucceeded);

	int32 WaryCount = 0;
	for (const FCombatantState& Combatant : Fixture.Combat->GetEncounterState().Combatants)
	{
		if (Combatant.Disposition == ECombatantDisposition::Wary)
		{
			++WaryCount;
		}
	}
	TestEqual(TEXT("One of them stepped back"), WaryCount, 1);

	// Which matters, because every man still coming makes running harder.
	const FCombatAttemptResult Run = Fixture.Combat->AttemptEscape(0.f);
	TestTrue(TEXT("Escape is now priced for three, not four"),
		FMath::IsNearlyEqual(Run.Required,
			Row.EscapeDifficulty + 3 * Fixture.Combat->EscapeDifficultyPerHostile - 0.2f));

	// A miss that is not close does nothing but harden them.
	FCombatFixture Far;
	Far.UseFrameEra();
	Far.Combat->RegisterEncounterType(Row);
	Far.Combat->BeginEncounter(TEXT("Cbt_TestMob"));
	Far.Combat->AttemptDeEscalation(0.2f);

	int32 StillHostile = 0;
	for (const FCombatantState& Combatant : Far.Combat->GetEncounterState().Combatants)
	{
		if (Combatant.Disposition == ECombatantDisposition::Hostile)
		{
			++StillHostile;
		}
	}
	TestEqual(TEXT("Nobody moves for a bad speech"), StillHostile, 4);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatEscapeIsPricedByWhoTheyAreTest,
	"ChainOfWitnesses.Combat.EscapeIsPricedByWhoTheyAreAndWhatHeIsDoing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatEscapeIsPricedByWhoTheyAreTest::RunTest(const FString& Parameters)
{
	FCombatFixture Fixture;
	Fixture.UseFrameEra();

	Fixture.Combat->RegisterEncounterType(Fixture.MakeRow(TEXT("Cbt_TestMob"), ECrowdKind::Mob, 3));
	Fixture.Combat->RegisterEncounterType(Fixture.MakeRow(TEXT("Cbt_TestPatrol"), ECrowdKind::Patrol, 3));
	Fixture.Combat->RegisterEncounterType(Fixture.MakeRow(TEXT("Cbt_TestGuards"), ECrowdKind::Guard, 3));

	// A crowd has no perimeter; a patrol's whole function is to follow.
	Fixture.Combat->BeginEncounter(TEXT("Cbt_TestMob"));
	const float MobRequired = Fixture.Combat->AttemptEscape(0.f).Required;
	Fixture.Combat->EndEncounter(ECombatOutcome::Escaped);

	Fixture.Combat->BeginEncounter(TEXT("Cbt_TestPatrol"));
	const float PatrolRequired = Fixture.Combat->AttemptEscape(0.f).Required;

	// A failed break for it leaves him moving backwards.
	TestTrue(TEXT("A failed escape puts him on the back foot"),
		Fixture.Combat->GetEncounterState().Posture == ECombatPosture::Withdrawing);
	TestEqual(TEXT("And it is counted"),
		Fixture.Combat->GetEncounterState().FailedEscapeAttempts, 1);
	Fixture.Combat->EndEncounter(ECombatOutcome::Escaped);

	Fixture.Combat->BeginEncounter(TEXT("Cbt_TestGuards"));
	const float GuardRequired = Fixture.Combat->AttemptEscape(0.f).Required;

	TestTrue(TEXT("Slipping a crowd is easier than losing a patrol"), MobRequired < PatrolRequired);
	TestTrue(TEXT("And walking away from a post is easiest of all"), GuardRequired < MobRequired);

	// Being committed costs him the difference again.
	Fixture.Combat->ApplyDamageToCombatant(0, 0.1f);
	const float EngagedRequired = Fixture.Combat->AttemptEscape(0.f).Required;
	TestTrue(TEXT("Disengaging from a committed man is harder"), EngagedRequired > GuardRequired);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatTheEraDecidesWhatCountsTest,
	"ChainOfWitnesses.Combat.TheEraDecidesWhetherGettingOutCounted",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatTheEraDecidesWhatCountsTest::RunTest(const FString& Parameters)
{
	// Section 5's claim, made mechanical: escape and de-escalation are first-class
	// win states in a Witness Mission. In the frame era the same endings are a rout.
	{
		FCombatFixture Witness;
		Witness.UseEra(WitnessEraID);
		Witness.Combat->RegisterEncounterType(Witness.MakeRow(TEXT("Cbt_TestPatrol"), ECrowdKind::Patrol, 3));

		Witness.Combat->BeginEncounter(TEXT("Cbt_TestPatrol"));
		Witness.Combat->AttemptEscape(10.f);
		TestTrue(TEXT("Escaping a Witness Mission is winning it"),
			Witness.Combat->GetOutcomeReport().bCountedAsWin);

		Witness.Combat->BeginEncounter(TEXT("Cbt_TestPatrol"));
		Witness.Combat->AttemptDeEscalation(10.f);
		TestTrue(TEXT("So is talking your way out"),
			Witness.Combat->GetOutcomeReport().bCountedAsWin);
	}

	{
		FCombatFixture Frame;
		Frame.UseFrameEra();
		Frame.Combat->RegisterEncounterType(Frame.MakeRow(TEXT("Cbt_TestPatrol"), ECrowdKind::Patrol, 3));

		Frame.Combat->BeginEncounter(TEXT("Cbt_TestPatrol"));
		Frame.Combat->AttemptEscape(10.f);
		TestFalse(TEXT("Running from a frame-era fight is not"),
			Frame.Combat->GetOutcomeReport().bCountedAsWin);

		// Overcoming them counts everywhere, which is the only outcome that does.
		Frame.Combat->BeginEncounter(TEXT("Cbt_TestPatrol"));
		for (int32 Index = 0; Index < 3; ++Index)
		{
			Frame.Combat->ApplyDamageToCombatant(Index, 1.f);
		}
		TestTrue(TEXT("Overcoming them counts in either era"),
			Frame.Combat->GetOutcomeReport().bCountedAsWin);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatLethalityComesFromTheEraTest,
	"ChainOfWitnesses.Combat.LethalityComesFromTheEraNotTheEncounter",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatLethalityComesFromTheEraTest::RunTest(const FString& Parameters)
{
	// The same blow, twice. A first-century man in a tunic is not a Templar in mail,
	// and that difference is data rather than a second combat system.
	FCombatFixture Frame;
	Frame.UseFrameEra();
	Frame.Combat->RegisterEncounterType(Frame.MakeRow(TEXT("Cbt_TestBandits"), ECrowdKind::Bandits, 3));
	Frame.Combat->BeginEncounter(TEXT("Cbt_TestBandits"));
	Frame.Combat->ApplyDamageToPlayer(0.2f);

	FCombatFixture Witness;
	Witness.UseEra(WitnessEraID);

	FCombatEncounterRow NoQuarter = Witness.MakeRow(TEXT("Cbt_TestBandits"), ECrowdKind::Bandits, 3);
	NoQuarter.bTakesPrisoners = false;
	Witness.Combat->RegisterEncounterType(NoQuarter);
	Witness.Combat->BeginEncounter(TEXT("Cbt_TestBandits"));
	Witness.Combat->ApplyDamageToPlayer(0.2f);

	TestTrue(TEXT("The same blow costs a witness-era man far more"),
		Witness.Combat->GetEncounterState().PlayerHealthNormalised
			< Frame.Combat->GetEncounterState().PlayerHealthNormalised);

	// Bandits want goods, not deaths -- but they will not carry a body home either,
	// so a man who loses to them loses for good.
	Witness.Combat->ApplyDamageToPlayer(1.f);
	TestTrue(TEXT("Men who take no prisoners finish it"),
		Witness.Combat->GetOutcomeReport().Outcome == ECombatOutcome::Defeated);

	// Where being taken is on the table it is the ending, and it is not the end of
	// anything: Paul wrote from custody.
	FCombatFixture Taken;
	Taken.UseFrameEra();
	FCombatEncounterRow Arrest = Taken.MakeRow(TEXT("Cbt_TestPatrol"), ECrowdKind::Patrol, 3);
	Arrest.bTakesPrisoners = true;
	Taken.Combat->RegisterEncounterType(Arrest);
	Taken.Combat->BeginEncounter(TEXT("Cbt_TestPatrol"));
	Taken.Combat->ApplyDamageToPlayer(5.f);
	TestTrue(TEXT("A patrol takes him alive"),
		Taken.Combat->GetOutcomeReport().Outcome == ECombatOutcome::Taken);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatWhatHeDidTravelsTest,
	"ChainOfWitnesses.Combat.WhatHeDidHereTravelsTheRoads",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatWhatHeDidTravelsTest::RunTest(const FString& Parameters)
{
	// Killing men is a deed, and deeds travel. This is the join between the combat
	// layer and the reputation system, and it is the reason fighting is expensive.
	{
		FCombatFixture Fixture;
		Fixture.UseFrameEra();
		Fixture.AddDeedTypes();
		Fixture.Combat->RegisterEncounterType(
			Fixture.MakeRow(TEXT("Cbt_TestBandits"), ECrowdKind::Bandits, 2));

		const float Before = Fixture.Reputation->GetFactionReputation(FactionID);

		Fixture.Combat->BeginEncounter(TEXT("Cbt_TestBandits"));
		Fixture.Combat->ApplyDamageToCombatant(0, 1.f);
		Fixture.Combat->ApplyDamageToCombatant(1, 1.f);

		TestTrue(TEXT("The encounter ended"),
			Fixture.Combat->GetOutcomeReport().Outcome == ECombatOutcome::Overcome);
		TestTrue(TEXT("And it cost him with the faction whose men they were"),
			Fixture.Reputation->GetFactionReputation(FactionID) < Before);
	}

	// Talking them down is also a deed, and a better one.
	{
		FCombatFixture Fixture;
		Fixture.UseFrameEra();
		Fixture.AddDeedTypes();
		Fixture.Combat->RegisterEncounterType(
			Fixture.MakeRow(TEXT("Cbt_TestPatrol"), ECrowdKind::Patrol, 3));

		const float Before = Fixture.Reputation->GetFactionReputation(FactionID);

		Fixture.Combat->BeginEncounter(TEXT("Cbt_TestPatrol"));
		Fixture.Combat->AttemptDeEscalation(10.f);

		TestTrue(TEXT("Talking them down is worth something"),
			Fixture.Reputation->GetFactionReputation(FactionID) > Before);
	}

	// A crowd that simply lost interest is also "overcome", and the player did none
	// of it, so no deed is recorded for it.
	{
		FCombatFixture Fixture;
		Fixture.UseFrameEra();
		Fixture.AddDeedTypes();
		Fixture.Combat->RegisterEncounterType(
			Fixture.MakeRow(TEXT("Cbt_TestMob"), ECrowdKind::Mob, 3));

		const float Before = Fixture.Reputation->GetFactionReputation(FactionID);

		Fixture.Combat->BeginEncounter(TEXT("Cbt_TestMob"));
		Fixture.Combat->AdvanceEncounter(600.f);

		TestTrue(TEXT("It dispersed"),
			Fixture.Combat->GetOutcomeReport().Outcome == ECombatOutcome::Overcome);
		TestTrue(TEXT("And nobody was hurt"),
			Fixture.Combat->GetOutcomeReport().bEndedWithoutBloodshed);
		TestTrue(TEXT("A crowd going home is not something he did"),
			FMath::IsNearlyEqual(Fixture.Reputation->GetFactionReputation(FactionID), Before));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatContentValidatesTest,
	"ChainOfWitnesses.Combat.ContentNamesOnlyThingsThatExist",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatContentValidatesTest::RunTest(const FString& Parameters)
{
	FCombatFixture Fixture;
	Fixture.UseFrameEra();
	Fixture.AddDeedTypes();

	Fixture.Combat->RegisterEncounterType(Fixture.MakeRow(TEXT("Cbt_TestGood"), ECrowdKind::Patrol, 3));

	TArray<FString> Problems;
	Fixture.Combat->ValidateCombatContent(Problems);
	TestEqual(TEXT("Well-formed content is quiet"), Problems.Num(), 0);

	FCombatEncounterRow Bad = Fixture.MakeRow(TEXT("Cbt_TestBad"), ECrowdKind::Patrol, 3);
	Bad.FactionID = TEXT("Faction_DoesNotExist");
	Bad.OvercomeDeedTypeID = TEXT("Deed_DoesNotExist");
	Fixture.Combat->RegisterEncounterType(Bad);

	Fixture.Combat->ValidateCombatContent(Problems);
	TestEqual(TEXT("An unknown faction and an unknown deed are both reported"), Problems.Num(), 2);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
