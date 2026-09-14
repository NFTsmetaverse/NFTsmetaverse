#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "ChainAutomationFlags.h"

#include "Campaign/CampaignTimelineSubsystem.h"
#include "Map/CampaignMapSubsystem.h"
#include "Reputation/ReputationSubsystem.h"
#include "UObject/StrongObjectPtr.h"

namespace CampaignMapTestHelpers
{
	const TCHAR* const Jerusalem = TEXT("Loc_Jerusalem");
	const TCHAR* const Antioch = TEXT("Loc_Antioch");
	const TCHAR* const Rome = TEXT("Loc_Rome");
	const TCHAR* const Judaea = TEXT("Region_Judaea");

	/**
	 * Jerusalem to Antioch by road in ten days, Antioch to Rome by sea in twenty.
	 * The clock starts on day 100 -- high summer -- so that nothing waits on the
	 * sailing season unless a test means it to.
	 */
	struct FMapFixture
	{
		TStrongObjectPtr<UCampaignMapSubsystem> Map;
		TStrongObjectPtr<UCampaignTimelineSubsystem> Timeline;
		TStrongObjectPtr<UReputationSubsystem> Reputation;

		FMapFixture()
			: Map(NewObject<UCampaignMapSubsystem>(GetTransientPackage()))
			, Timeline(NewObject<UCampaignTimelineSubsystem>(GetTransientPackage()))
			, Reputation(NewObject<UReputationSubsystem>(GetTransientPackage()))
		{
			Map->SetDependenciesForTesting(Timeline.Get(), Reputation.Get());
			Reputation->SetDependenciesForTesting(Timeline.Get(), Map.Get());

			AddLocation(Jerusalem, Judaea, /*bIsPort=*/false);
			AddLocation(Antioch, TEXT("Region_Syria"), /*bIsPort=*/true);
			AddLocation(Rome, TEXT("Region_Italia"), /*bIsPort=*/true);

			AddRoute(Jerusalem, Antioch, 10, /*bSea=*/false);
			AddRoute(Antioch, Rome, 20, /*bSea=*/true);

			Timeline->AdvanceDays(100);
			Map->SetPartyLocation(Jerusalem);
		}

		void AddLocation(const TCHAR* ID, const TCHAR* RegionID, bool bIsPort)
		{
			FCampaignLocationRow Location;
			Location.LocationID = ID;
			Location.RegionID = RegionID;
			Location.bIsPort = bIsPort;
			Map->RegisterLocation(Location);
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

		/** Chance 1 always fires and chance 0 never does, so encounters are testable without a seeded roll. */
		void AddEncounter(const TCHAR* ID, float ChancePerDay, bool bLand, bool bSea)
		{
			FEncounterTypeRow EncounterType;
			EncounterType.EncounterID = ID;
			EncounterType.ChancePerDay = ChancePerDay;
			EncounterType.bOnLandRoutes = bLand;
			EncounterType.bOnSeaRoutes = bSea;
			Map->RegisterEncounterType(EncounterType);
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCampaignMapPathfindingTest,
	"ChainOfWitnesses.Map.QuickestRouteIsByArrivalNotDistance",
	CHAIN_TEST_FLAGS)

bool FCampaignMapPathfindingTest::RunTest(const FString& Parameters)
{
	using namespace CampaignMapTestHelpers;
	FMapFixture Fixture;

	TArray<FName> Path;
	int32 ArrivalDay = INDEX_NONE;

	TestTrue(TEXT("Route found"),
		Fixture.Map->FindPath(Jerusalem, Rome, 100, /*bAllowOutOfSeason=*/false, Path, ArrivalDay));
	TestEqual(TEXT("Two hops"), Path.Num(), 2);
	TestTrue(TEXT("Via Antioch"), Path[0] == FName(Antioch));
	TestTrue(TEXT("Ending at Rome"), Path[1] == FName(Rome));
	TestEqual(TEXT("Thirty days in summer"), ArrivalDay, 130);

	// Setting out in late October, the sea leg cannot start until spring, so the
	// same journey takes most of the winter.
	TestTrue(TEXT("Winter route found"),
		Fixture.Map->FindPath(Jerusalem, Rome, 300, /*bAllowOutOfSeason=*/false, Path, ArrivalDay));
	TestEqual(TEXT("The crossing waits for the season"), ArrivalDay, 453);

	// Unless someone insists on sailing.
	TestTrue(TEXT("Out-of-season route found"),
		Fixture.Map->FindPath(Jerusalem, Rome, 300, /*bAllowOutOfSeason=*/true, Path, ArrivalDay));
	TestEqual(TEXT("Sailing regardless keeps the summer timing"), ArrivalDay, 330);

	TestFalse(TEXT("Nowhere to an unconnected place"),
		Fixture.Map->FindPath(Jerusalem, TEXT("Loc_Nowhere"), 100, false, Path, ArrivalDay));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCampaignMapPartyMovementTest,
	"ChainOfWitnesses.Map.PartyMovesAndTimePassesWithIt",
	CHAIN_TEST_FLAGS)

bool FCampaignMapPartyMovementTest::RunTest(const FString& Parameters)
{
	using namespace CampaignMapTestHelpers;
	FMapFixture Fixture;

	TestTrue(TEXT("Course plotted"), Fixture.Map->SetDestination(Rome));
	TestTrue(TEXT("Party is travelling"), Fixture.Map->IsPartyTravelling());

	// Part way along the first leg.
	FTravelResult Result = Fixture.Map->AdvanceDays(4);
	TestTrue(TEXT("Still on the road"), Result.Outcome == ETravelOutcome::Travelling);
	TestEqual(TEXT("Four days spent"), Result.DaysElapsed, 4);
	TestEqual(TEXT("The campaign clock moved with it"), Fixture.Timeline->GetTotalElapsedDays(), 104);
	TestTrue(TEXT("Not yet arrived anywhere"), Fixture.Map->GetPartyLocation() == FName(Jerusalem));

	// Finishing the road leg puts the party in Antioch and straight onto the sea leg.
	Result = Fixture.Map->AdvanceDays(6);
	TestTrue(TEXT("Passing through does not stop the party"), Result.Outcome == ETravelOutcome::Travelling);
	TestTrue(TEXT("Reached Antioch"), Fixture.Map->GetPartyLocation() == FName(Antioch));
	TestTrue(TEXT("Still bound for Rome"), Fixture.Map->IsPartyTravelling());

	// More days than the crossing needs; the surplus comes back unspent.
	Result = Fixture.Map->AdvanceDays(25);
	TestTrue(TEXT("Arrived"), Result.Outcome == ETravelOutcome::Arrived);
	TestEqual(TEXT("Only the crossing was spent"), Result.DaysElapsed, 20);
	TestEqual(TEXT("Five days left over"), Result.DaysRemaining, 5);
	TestTrue(TEXT("The party is in Rome"), Fixture.Map->GetPartyLocation() == FName(Rome));
	TestFalse(TEXT("No longer travelling"), Fixture.Map->IsPartyTravelling());
	TestEqual(TEXT("Thirty days of campaign time"), Fixture.Timeline->GetTotalElapsedDays(), 130);

	// Standing still still burns days.
	Result = Fixture.Map->AdvanceDays(7);
	TestTrue(TEXT("Idle"), Result.Outcome == ETravelOutcome::Idle);
	TestEqual(TEXT("Waiting passes time"), Fixture.Timeline->GetTotalElapsedDays(), 137);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCampaignMapEncounterTest,
	"ChainOfWitnesses.Map.EncountersInterruptAndReportUnspentDays",
	CHAIN_TEST_FLAGS)

bool FCampaignMapEncounterTest::RunTest(const FString& Parameters)
{
	using namespace CampaignMapTestHelpers;
	FMapFixture Fixture;

	Fixture.AddEncounter(TEXT("Enc_Certain"), 1.f, /*bLand=*/true, /*bSea=*/false);
	Fixture.Map->SetDestination(Rome);

	const FTravelResult Result = Fixture.Map->AdvanceDays(8);

	TestTrue(TEXT("Interrupted"), Result.Outcome == ETravelOutcome::Encounter);
	TestTrue(TEXT("Named the encounter"), Result.EncounterID == FName(TEXT("Enc_Certain")));
	TestEqual(TEXT("Stopped after one day"), Result.DaysElapsed, 1);
	TestEqual(TEXT("Seven days still in hand"), Result.DaysRemaining, 7);
	TestTrue(TEXT("Still en route, not teleported"), Fixture.Map->IsPartyTravelling());

	// A land encounter cannot happen at sea.
	FMapFixture SeaFixture;
	SeaFixture.AddEncounter(TEXT("Enc_LandOnly"), 1.f, /*bLand=*/true, /*bSea=*/false);
	SeaFixture.Map->SetPartyLocation(Antioch);
	SeaFixture.Map->SetDestination(Rome);

	const FTravelResult AtSea = SeaFixture.Map->AdvanceDays(5);
	TestTrue(TEXT("Nothing on land troubles a ship"), AtSea.Outcome == ETravelOutcome::Travelling);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCampaignMapSailingSeasonTest,
	"ChainOfWitnesses.Map.PartyWaitsOutTheClosedSeasonUnlessItInsists",
	CHAIN_TEST_FLAGS)

bool FCampaignMapSailingSeasonTest::RunTest(const FString& Parameters)
{
	using namespace CampaignMapTestHelpers;

	// Risk is flat through the middle of the season and climbs at either end.
	{
		FMapFixture Fixture;
		TestEqual(TEXT("Midsummer is as safe as it gets"),
			Fixture.Map->GetSeaRiskMultiplier(180, false), 1.f, 0.01f);
		TestEqual(TEXT("Winter is the worst of it"),
			Fixture.Map->GetSeaRiskMultiplier(330, false), Fixture.Map->OutOfSeasonRiskMultiplier, 0.01f);
		TestTrue(TEXT("The shoulder is worse than midsummer but better than winter"),
			Fixture.Map->GetSeaRiskMultiplier(295, false) > 1.f
			&& Fixture.Map->GetSeaRiskMultiplier(295, false) < Fixture.Map->OutOfSeasonRiskMultiplier);
		TestTrue(TEXT("Season shut in December"), Fixture.Map->IsSailingSeasonClosed(340));
		TestFalse(TEXT("Season open in June"), Fixture.Map->IsSailingSeasonClosed(180));
	}

	// Reaching the port in the closed season, the party sits in harbour -- and
	// nothing happens to it there, however dangerous the crossing would be.
	{
		FMapFixture Fixture;
		Fixture.AddEncounter(TEXT("Enc_CertainAtSea"), 1.f, /*bLand=*/false, /*bSea=*/true);
		// Day 310: past the closing of the lanes on day 304.
		Fixture.Timeline->AdvanceDays(210);
		Fixture.Map->SetPartyLocation(Antioch);

		TestTrue(TEXT("Course plotted"), Fixture.Map->SetDestination(Rome));

		const FTravelResult Result = Fixture.Map->AdvanceDays(30);
		TestTrue(TEXT("Held in port"), Result.Outcome == ETravelOutcome::WaitingForSailingSeason);
		TestEqual(TEXT("The days still passed"), Result.DaysElapsed, 30);
		TestTrue(TEXT("No encounter while sitting in harbour"), Result.EncounterID.IsNone());
		TestTrue(TEXT("Not yet in Rome"), Fixture.Map->GetPartyLocation() == FName(Antioch));
	}

	// Insisting on sailing skips the wait and puts to sea at once.
	{
		FMapFixture Fixture;
		Fixture.Timeline->AdvanceDays(210);
		Fixture.Map->SetPartyLocation(Antioch);

		TestTrue(TEXT("Course plotted against advice"),
			Fixture.Map->SetDestination(Rome, /*bRiskOutOfSeasonSailing=*/true));

		const FTravelResult Result = Fixture.Map->AdvanceDays(20);
		TestTrue(TEXT("Made the crossing"), Result.Outcome == ETravelOutcome::Arrived);
		TestTrue(TEXT("In Rome"), Fixture.Map->GetPartyLocation() == FName(Rome));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCampaignMapSaveRoundTripTest,
	"ChainOfWitnesses.Map.PartyStateSurvivesReload",
	CHAIN_TEST_FLAGS)

bool FCampaignMapSaveRoundTripTest::RunTest(const FString& Parameters)
{
	using namespace CampaignMapTestHelpers;
	FMapFixture Fixture;

	Fixture.Map->SetDestination(Rome);
	Fixture.Map->AdvanceDays(14);

	const FCampaignMapSaveData Saved = Fixture.Map->CaptureSaveData();
	const FName LocationAtSave = Fixture.Map->GetPartyLocation();

	Fixture.Map->AdvanceDays(20);
	TestTrue(TEXT("Journey completed after the save"), Fixture.Map->GetPartyLocation() == FName(Rome));

	Fixture.Map->RestoreFromSaveData(Saved);

	TestTrue(TEXT("Back where the save was taken"), Fixture.Map->GetPartyLocation() == LocationAtSave);
	TestTrue(TEXT("Still travelling"), Fixture.Map->IsPartyTravelling());
	TestTrue(TEXT("Still bound for Rome"),
		Fixture.Map->GetPartyState().DestinationLocationID == FName(Rome));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
