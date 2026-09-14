#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "ChainAutomationFlags.h"

#include "Campaign/CampaignTimelineSubsystem.h"
#include "Map/CampaignMapSubsystem.h"
#include "Reputation/ReputationSubsystem.h"
#include "UObject/StrongObjectPtr.h"

namespace ReputationTestHelpers
{
	const TCHAR* const Jerusalem = TEXT("Loc_Jerusalem");
	const TCHAR* const Antioch = TEXT("Loc_Antioch");
	const TCHAR* const Rome = TEXT("Loc_Rome");

	const TCHAR* const Church = TEXT("Faction_Church");
	const TCHAR* const Temple = TEXT("Faction_Temple");

	/**
	 * A three-city line: Jerusalem to Antioch by road in ten days, Antioch to Rome
	 * by sea in twenty, plus two factions that dislike each other. Small enough that
	 * every arrival day below can be worked out by hand.
	 */
	struct FReputationFixture
	{
		TStrongObjectPtr<UReputationSubsystem> Reputation;
		TStrongObjectPtr<UCampaignMapSubsystem> Map;
		TStrongObjectPtr<UCampaignTimelineSubsystem> Timeline;

		// The road network lives on the map; reputation asks it how far word got.
		FReputationFixture()
			: Reputation(NewObject<UReputationSubsystem>(GetTransientPackage()))
			, Map(NewObject<UCampaignMapSubsystem>(GetTransientPackage()))
			, Timeline(NewObject<UCampaignTimelineSubsystem>(GetTransientPackage()))
		{
			Reputation->SetDependenciesForTesting(Timeline.Get(), Map.Get());
			Map->SetDependenciesForTesting(Timeline.Get(), Reputation.Get());
		}

		void AddRoute(const TCHAR* From, const TCHAR* To, int32 Days, bool bSea)
		{
			FTravelRouteRow Route;
			Route.FromLocationID = From;
			Route.ToLocationID = To;
			Route.TravelDays = Days;
			Route.bIsSeaRoute = bSea;
			Route.bIsBidirectional = true;
			Map->RegisterRoute(Route);
		}

		void BuildDefaultWorld()
		{
			FFactionRow ChurchRow;
			ChurchRow.FactionID = Church;

			// The Temple reads any gain of the Church's as half a loss of its own.
			FFactionAttitude TempleTowardChurch;
			TempleTowardChurch.TowardFactionID = Church;
			TempleTowardChurch.Multiplier = -0.5f;

			FFactionRow TempleRow;
			TempleRow.FactionID = Temple;
			TempleRow.AttitudesToward.Add(TempleTowardChurch);

			Reputation->RegisterFaction(ChurchRow);
			Reputation->RegisterFaction(TempleRow);

			AddRoute(Jerusalem, Antioch, 10, /*bSea=*/false);
			AddRoute(Antioch, Rome, 20, /*bSea=*/true);
		}

		void AddDeedType(const TCHAR* ID, int32 ReachDays, bool bTravels = true)
		{
			FDeedTypeRow Type;
			Type.DeedTypeID = ID;
			Type.NewsReachDays = ReachDays;
			Type.bTravelsByWordOfMouth = bTravels;
			Type.WitnessStandingDelta = 10.f;

			FFactionImpact Impact;
			Impact.FactionID = Church;
			Impact.Delta = 20.f;
			Type.FactionImpacts.Add(Impact);

			Reputation->RegisterDeedType(Type);
		}

		void AddNpc(const TCHAR* NpcID, const TCHAR* FactionID, const TCHAR* HomeLocationID)
		{
			FNpcProfile Profile;
			Profile.NpcID = NpcID;
			Profile.FactionID = FactionID;
			Profile.HomeLocationID = HomeLocationID;
			Reputation->RegisterNpcProfile(Profile);
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReputationFactionBleedTest,
	"ChainOfWitnesses.Reputation.StandingWithOneFactionMovesTheOthers",
	CHAIN_TEST_FLAGS)

bool FReputationFactionBleedTest::RunTest(const FString& Parameters)
{
	using namespace ReputationTestHelpers;

	FReputationFixture Fixture;
	Fixture.BuildDefaultWorld();
	Fixture.AddDeedType(TEXT("Deed_Good"), 999);

	const FName DeedID = Fixture.Reputation->RecordDeed(TEXT("Deed_Good"), Jerusalem, TArray<FName>());

	TestFalse(TEXT("Deed recorded"), DeedID.IsNone());
	TestEqual(TEXT("Direct impact applied"),
		Fixture.Reputation->GetFactionReputation(Church), 20.f, 0.01f);
	TestEqual(TEXT("Attitude bled into the rival faction"),
		Fixture.Reputation->GetFactionReputation(Temple), -10.f, 0.01f);

	// An unregistered deed type is refused rather than silently changing nothing.
	TestTrue(TEXT("Unknown deed type refused"),
		Fixture.Reputation->RecordDeed(TEXT("Deed_Nonexistent"), Jerusalem, TArray<FName>()).IsNone());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReputationPropagationTest,
	"ChainOfWitnesses.Reputation.NewsTravelsAtTheSpeedOfTheRoad",
	CHAIN_TEST_FLAGS)

bool FReputationPropagationTest::RunTest(const FString& Parameters)
{
	using namespace ReputationTestHelpers;

	FReputationFixture Fixture;
	Fixture.BuildDefaultWorld();
	Fixture.AddDeedType(TEXT("Deed_Loud"), 999);

	// Day 100 of the campaign's first year: well inside the sailing season.
	Fixture.Timeline->AdvanceDays(100);

	const FName DeedID = Fixture.Reputation->RecordDeed(TEXT("Deed_Loud"), Jerusalem, TArray<FName>());

	TestEqual(TEXT("Known where it happened at once"),
		Fixture.Reputation->GetNewsArrivalDay(DeedID, Jerusalem), 100);
	TestEqual(TEXT("Antioch is ten days by road"),
		Fixture.Reputation->GetNewsArrivalDay(DeedID, Antioch), 110);
	TestEqual(TEXT("Rome is twenty more by sea"),
		Fixture.Reputation->GetNewsArrivalDay(DeedID, Rome), 130);

	TestTrue(TEXT("Jerusalem has heard"), Fixture.Reputation->HasNewsReached(DeedID, Jerusalem));
	TestFalse(TEXT("Rome has not heard yet"), Fixture.Reputation->HasNewsReached(DeedID, Rome));

	Fixture.Timeline->AdvanceDays(30);
	TestTrue(TEXT("Rome has heard by day 130"), Fixture.Reputation->HasNewsReached(DeedID, Rome));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReputationSailingSeasonTest,
	"ChainOfWitnesses.Reputation.SeaNewsWaitsOutTheClosedSeason",
	CHAIN_TEST_FLAGS)

bool FReputationSailingSeasonTest::RunTest(const FString& Parameters)
{
	using namespace ReputationTestHelpers;

	FReputationFixture Fixture;
	Fixture.BuildDefaultWorld();
	Fixture.AddDeedType(TEXT("Deed_Loud"), 999);

	// Day 300 is late October. The road leg reaches Antioch on day 310, by which
	// point the sea lanes have shut.
	Fixture.Timeline->AdvanceDays(300);

	const FName DeedID = Fixture.Reputation->RecordDeed(TEXT("Deed_Loud"), Jerusalem, TArray<FName>());

	TestEqual(TEXT("The road runs in winter"),
		Fixture.Reputation->GetNewsArrivalDay(DeedID, Antioch), 310);

	// From day 310 it waits out the year (55 days) and then until day 68 of the
	// next, departing on day 433 and arriving twenty days later.
	TestEqual(TEXT("The sea leg waits for spring"),
		Fixture.Reputation->GetNewsArrivalDay(DeedID, Rome), 453);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReputationMemoryTest,
	"ChainOfWitnesses.Reputation.NpcsRememberOnlyWhatReachedThem",
	CHAIN_TEST_FLAGS)

bool FReputationMemoryTest::RunTest(const FString& Parameters)
{
	using namespace ReputationTestHelpers;

	FReputationFixture Fixture;
	Fixture.BuildDefaultWorld();

	// Reaches Antioch at ten days, but dies out before the thirty Rome would need.
	Fixture.AddDeedType(TEXT("Deed_Local"), 15);
	Fixture.AddDeedType(TEXT("Deed_Private"), 0, /*bTravels=*/false);

	Fixture.AddNpc(TEXT("NPC_InAntioch"), Church, Antioch);
	Fixture.AddNpc(TEXT("NPC_InRome"), Church, Rome);

	Fixture.Timeline->AdvanceDays(100);

	// The Roman saw it himself, though he lives where the account never arrives.
	TArray<FName> Witnesses;
	Witnesses.Add(TEXT("NPC_InRome"));
	const FName DeedID = Fixture.Reputation->RecordDeed(TEXT("Deed_Local"), Jerusalem, Witnesses);

	// Long enough that arrival, not elapsed time, is what decides who knows.
	Fixture.Timeline->AdvanceDays(365);

	TestTrue(TEXT("The man in Antioch heard about it"),
		Fixture.Reputation->DoesNpcKnowOfDeed(TEXT("NPC_InAntioch"), DeedID));
	TestTrue(TEXT("Seeing it beats hearing about it"),
		Fixture.Reputation->DoesNpcKnowOfDeed(TEXT("NPC_InRome"), DeedID));
	TestEqual(TEXT("The account never reached Rome on its own"),
		Fixture.Reputation->GetNewsArrivalDay(DeedID, Rome), (int32)INDEX_NONE);

	const FName PrivateID = Fixture.Reputation->RecordDeed(TEXT("Deed_Private"), Jerusalem, TArray<FName>());
	TestFalse(TEXT("A kept confidence does not travel"),
		Fixture.Reputation->DoesNpcKnowOfDeed(TEXT("NPC_InAntioch"), PrivateID));

	TArray<FReputationDeed> Known;
	Fixture.Reputation->GetDeedsKnownToNpc(TEXT("NPC_InAntioch"), Known);
	TestEqual(TEXT("He can bring up exactly one thing"), Known.Num(), 1);

	// Witnesses take the act personally; everyone else moves only with their faction.
	TestEqual(TEXT("Witness standing applied"),
		Fixture.Reputation->GetNpcPersonalStanding(TEXT("NPC_InRome")), 10.f, 0.01f);
	TestEqual(TEXT("Non-witness has no personal standing"),
		Fixture.Reputation->GetNpcPersonalStanding(TEXT("NPC_InAntioch")), 0.f, 0.01f);

	const float ChurchRep = Fixture.Reputation->GetFactionReputation(Church);
	TestEqual(TEXT("Effective standing folds in the faction's view"),
		Fixture.Reputation->GetEffectiveStanding(TEXT("NPC_InAntioch")),
		ChurchRep * Fixture.Reputation->FactionWeight, 0.01f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReputationSaveRoundTripTest,
	"ChainOfWitnesses.Reputation.DeedsAndStandingsSurviveReload",
	CHAIN_TEST_FLAGS)

bool FReputationSaveRoundTripTest::RunTest(const FString& Parameters)
{
	using namespace ReputationTestHelpers;

	FReputationFixture Fixture;
	Fixture.BuildDefaultWorld();
	Fixture.AddDeedType(TEXT("Deed_Good"), 999);

	Fixture.Timeline->AdvanceDays(100);

	const FName DeedID = Fixture.Reputation->RecordDeed(TEXT("Deed_Good"), Jerusalem, TArray<FName>());
	const FReputationSaveData Saved = Fixture.Reputation->CaptureSaveData();

	Fixture.Reputation->RecordDeed(TEXT("Deed_Good"), Jerusalem, TArray<FName>());
	TestEqual(TEXT("A second deed doubled the standing"),
		Fixture.Reputation->GetFactionReputation(Church), 40.f, 0.01f);

	Fixture.Reputation->RestoreFromSaveData(Saved);

	TestEqual(TEXT("Faction standing restored"),
		Fixture.Reputation->GetFactionReputation(Church), 20.f, 0.01f);
	TestEqual(TEXT("Arrival times restored"),
		Fixture.Reputation->GetNewsArrivalDay(DeedID, Rome), 130);

	// The deed counter is part of the save, so new IDs do not collide with restored ones.
	const FName AfterRestore = Fixture.Reputation->RecordDeed(TEXT("Deed_Good"), Jerusalem, TArray<FName>());
	TestTrue(TEXT("New deed does not reuse a restored ID"), AfterRestore != DeedID);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
