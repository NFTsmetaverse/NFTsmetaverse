#include "Debate/DebateSubsystem.h"

#include "Campaign/CampaignTimelineSubsystem.h"
#include "Codex/CodexSubsystem.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "Map/CampaignMapSubsystem.h"
#include "Reputation/ReputationSubsystem.h"

DEFINE_LOG_CATEGORY(LogDebate);

void UDebateSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UDebateSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

void UDebateSubsystem::SetDependenciesForTesting(UCodexSubsystem* InCodex, UCampaignTimelineSubsystem* InTimeline,
	UReputationSubsystem* InReputation, UCampaignMapSubsystem* InMap)
{
	CodexOverride = InCodex;
	TimelineOverride = InTimeline;
	ReputationOverride = InReputation;
	MapOverride = InMap;
}

UCodexSubsystem* UDebateSubsystem::GetCodex() const
{
	if (CodexOverride)
	{
		return CodexOverride;
	}

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		return GameInstance->GetSubsystem<UCodexSubsystem>();
	}

	return nullptr;
}

UCampaignTimelineSubsystem* UDebateSubsystem::GetTimeline() const
{
	if (TimelineOverride)
	{
		return TimelineOverride;
	}

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		return GameInstance->GetSubsystem<UCampaignTimelineSubsystem>();
	}

	return nullptr;
}

UReputationSubsystem* UDebateSubsystem::GetReputation() const
{
	if (ReputationOverride)
	{
		return ReputationOverride;
	}

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		return GameInstance->GetSubsystem<UReputationSubsystem>();
	}

	return nullptr;
}

UCampaignMapSubsystem* UDebateSubsystem::GetMap() const
{
	if (MapOverride)
	{
		return MapOverride;
	}

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		return GameInstance->GetSubsystem<UCampaignMapSubsystem>();
	}

	return nullptr;
}

int32 UDebateSubsystem::GetCurrentYearAD() const
{
	const UCampaignTimelineSubsystem* Timeline = GetTimeline();
	return Timeline ? Timeline->GetCurrentYearAD() : 0;
}

// --- Content ----------------------------------------------------------------

bool UDebateSubsystem::RegisterOpponent(const FDebateOpponentRow& Opponent)
{
	if (Opponent.OpponentID.IsNone() || Opponents.Contains(Opponent.OpponentID))
	{
		return false;
	}

	Opponents.Add(Opponent.OpponentID, Opponent);
	return true;
}

bool UDebateSubsystem::RegisterObjection(const FDebateObjectionRow& Objection)
{
	if (Objection.ObjectionID.IsNone() || Objections.Contains(Objection.ObjectionID))
	{
		return false;
	}

	Objections.Add(Objection.ObjectionID, Objection);
	return true;
}

int32 UDebateSubsystem::RegisterOpponentTable(const UDataTable* OpponentTable)
{
	if (!OpponentTable)
	{
		return 0;
	}

	TArray<FDebateOpponentRow*> Rows;
	OpponentTable->GetAllRows<FDebateOpponentRow>(TEXT("UDebateSubsystem::RegisterOpponentTable"), Rows);

	int32 AddedCount = 0;
	for (const FDebateOpponentRow* Row : Rows)
	{
		if (Row && RegisterOpponent(*Row))
		{
			++AddedCount;
		}
	}

	UE_LOG(LogDebate, Log, TEXT("Registered %d debate opponents."), AddedCount);
	return AddedCount;
}

int32 UDebateSubsystem::RegisterObjectionTable(const UDataTable* ObjectionTable)
{
	if (!ObjectionTable)
	{
		return 0;
	}

	TArray<FDebateObjectionRow*> Rows;
	ObjectionTable->GetAllRows<FDebateObjectionRow>(TEXT("UDebateSubsystem::RegisterObjectionTable"), Rows);

	int32 AddedCount = 0;
	for (const FDebateObjectionRow* Row : Rows)
	{
		if (Row && RegisterObjection(*Row))
		{
			++AddedCount;
		}
	}

	UE_LOG(LogDebate, Log, TEXT("Registered %d objections."), AddedCount);
	return AddedCount;
}

void UDebateSubsystem::ValidateDebateContent(TArray<FString>& OutProblems) const
{
	OutProblems.Reset();

	const UCodexSubsystem* Codex = GetCodex();

	for (const TPair<FName, FDebateOpponentRow>& Pair : Opponents)
	{
		if (Pair.Value.ObjectionIDs.Num() == 0)
		{
			OutProblems.Add(FString::Printf(TEXT("Opponent '%s' has nothing to say."), *Pair.Key.ToString()));
		}

		for (const FName ObjectionID : Pair.Value.ObjectionIDs)
		{
			if (!Objections.Contains(ObjectionID))
			{
				OutProblems.Add(FString::Printf(TEXT("Opponent '%s' raises unregistered objection '%s'."),
					*Pair.Key.ToString(), *ObjectionID.ToString()));
			}
		}
	}

	if (Codex)
	{
		for (const TPair<FName, FDebateObjectionRow>& Pair : Objections)
		{
			for (const FName FragmentID : Pair.Value.AnsweredByFragmentIDs)
			{
				if (!Codex->IsFragmentRegistered(FragmentID))
				{
					OutProblems.Add(FString::Printf(TEXT("Objection '%s' is answered by unregistered fragment '%s'."),
						*Pair.Key.ToString(), *FragmentID.ToString()));
				}
			}
		}
	}
}

// --- Selection and defence --------------------------------------------------

bool UDebateSubsystem::IsObjectionEligible(const FDebateObjectionRow& Objection) const
{
	const int32 CurrentYearAD = GetCurrentYearAD();

	if (Objection.EarliestYearAD != INDEX_NONE && CurrentYearAD < Objection.EarliestYearAD)
	{
		return false;
	}

	if (Objection.LatestYearAD != INDEX_NONE && CurrentYearAD > Objection.LatestYearAD)
	{
		return false;
	}

	return true;
}

float UDebateSubsystem::GetLinkDefence(ETransmissionLink Link, bool& bOutHasUnansweredChallenge) const
{
	bOutHasUnansweredChallenge = false;

	for (const FChainLinkScore& LinkScore : ChainAtStart.LinkScores)
	{
		if (LinkScore.Link != Link)
		{
			continue;
		}

		bOutHasUnansweredChallenge = LinkScore.UnansweredChallenges.Num() > 0;

		float Defence = LinkScore.LinkScore;
		if (bOutHasUnansweredChallenge)
		{
			// The opponent knows the objection is out there, and says so.
			Defence *= UnansweredChallengeDefencePenalty;
		}

		return Defence;
	}

	return 0.f;
}

FName UDebateSubsystem::SelectNextObjection() const
{
	const FDebateOpponentRow* Opponent = Opponents.Find(State.OpponentID);
	if (!Opponent)
	{
		return NAME_None;
	}

	FName BestObjectionID = NAME_None;
	float WeakestDefence = TNumericLimits<float>::Max();
	float BestWeight = -1.f;

	for (const FName ObjectionID : Opponent->ObjectionIDs)
	{
		if (State.UsedObjectionIDs.Contains(ObjectionID))
		{
			continue;
		}

		const FDebateObjectionRow* Objection = Objections.Find(ObjectionID);
		if (!Objection || !IsObjectionEligible(*Objection))
		{
			continue;
		}

		bool bHasUnansweredChallenge = false;
		const float Defence = GetLinkDefence(Objection->TargetsLink, bHasUnansweredChallenge);

		// Weakest link first; among equals, the objection that costs the most.
		// Ties beyond that resolve by ID so the order is stable across runs.
		const bool bBetter = Defence < WeakestDefence
			|| (FMath::IsNearlyEqual(Defence, WeakestDefence) && Objection->Weight > BestWeight)
			|| (FMath::IsNearlyEqual(Defence, WeakestDefence)
				&& FMath::IsNearlyEqual(Objection->Weight, BestWeight)
				&& ObjectionID.LexicalLess(BestObjectionID));

		if (bBetter)
		{
			WeakestDefence = Defence;
			BestWeight = Objection->Weight;
			BestObjectionID = ObjectionID;
		}
	}

	return BestObjectionID;
}

// --- Running a debate -------------------------------------------------------

bool UDebateSubsystem::StartDebate(FName OpponentID, FName AnchorEventID)
{
	if (State.bIsActive)
	{
		AbandonDebate();
	}

	const FDebateOpponentRow* Opponent = Opponents.Find(OpponentID);
	if (!Opponent)
	{
		UE_LOG(LogDebate, Warning, TEXT("No such opponent '%s'."), *OpponentID.ToString());
		return false;
	}

	const int32 CurrentYearAD = GetCurrentYearAD();
	if ((Opponent->EarliestYearAD != INDEX_NONE && CurrentYearAD < Opponent->EarliestYearAD)
		|| (Opponent->LatestYearAD != INDEX_NONE && CurrentYearAD > Opponent->LatestYearAD))
	{
		UE_LOG(LogDebate, Warning, TEXT("Opponent '%s' does not belong to AD %d."),
			*OpponentID.ToString(), CurrentYearAD);
		return false;
	}

	const UCodexSubsystem* Codex = GetCodex();
	if (!Codex)
	{
		UE_LOG(LogDebate, Warning, TEXT("No Codex; there is nothing to argue from."));
		return false;
	}

	State = FDebateState();
	State.OpponentID = OpponentID;
	State.AnchorEventID = AnchorEventID;
	State.MaxRounds = Opponent->MaxRounds;

	// The case is fixed at the outset. Slotting fragments mid-argument would be
	// assembling the chain while defending it.
	ChainAtStart = Codex->ScoreChain(AnchorEventID);

	State.CurrentObjectionID = SelectNextObjection();
	if (State.CurrentObjectionID.IsNone())
	{
		UE_LOG(LogDebate, Warning, TEXT("Opponent '%s' has nothing to raise in AD %d."),
			*OpponentID.ToString(), CurrentYearAD);
		return false;
	}

	State.bIsActive = true;
	Report = FDebateOutcomeReport();

	OnDebateStarted.Broadcast(OpponentID, AnchorEventID);
	OnObjectionRaised.Broadcast(State.CurrentObjectionID);

	return true;
}

bool UDebateSubsystem::GetCurrentObjection(FDebateObjectionView& OutView) const
{
	OutView = FDebateObjectionView();

	if (!State.bIsActive)
	{
		return false;
	}

	const FDebateObjectionRow* Objection = Objections.Find(State.CurrentObjectionID);
	if (!Objection)
	{
		return false;
	}

	OutView.ObjectionID = Objection->ObjectionID;
	OutView.Statement = Objection->Statement;
	OutView.TargetsLink = Objection->TargetsLink;
	OutView.RequiredLinkStrength = Objection->RequiredLinkStrength;

	bool bHasUnansweredChallenge = false;
	OutView.ChainDefence = GetLinkDefence(Objection->TargetsLink, bHasUnansweredChallenge);
	OutView.bLinkHasUnansweredChallenge = bHasUnansweredChallenge;

	if (const UCodexSubsystem* Codex = GetCodex())
	{
		for (const FName FragmentID : Objection->AnsweredByFragmentIDs)
		{
			if (Codex->IsFragmentRecovered(FragmentID) && !State.SpentFragmentIDs.Contains(FragmentID))
			{
				OutView.AvailableAnswerFragmentIDs.Add(FragmentID);
			}
		}
	}

	return true;
}

bool UDebateSubsystem::RespondWithChain()
{
	if (!State.bIsActive)
	{
		return false;
	}

	const FDebateObjectionRow* Objection = Objections.Find(State.CurrentObjectionID);
	if (!Objection)
	{
		return false;
	}

	bool bHasUnansweredChallenge = false;
	const float Defence = GetLinkDefence(Objection->TargetsLink, bHasUnansweredChallenge);

	return ResolveRound(EDebateResponseKind::LeanOnChain, NAME_None, Defence);
}

bool UDebateSubsystem::RespondWithFragment(FName FragmentID)
{
	if (!State.bIsActive)
	{
		return false;
	}

	const FDebateObjectionRow* Objection = Objections.Find(State.CurrentObjectionID);
	if (!Objection || !Objection->AnsweredByFragmentIDs.Contains(FragmentID))
	{
		UE_LOG(LogDebate, Warning, TEXT("'%s' does not answer '%s'."),
			*FragmentID.ToString(), *State.CurrentObjectionID.ToString());
		return false;
	}

	if (State.SpentFragmentIDs.Contains(FragmentID))
	{
		return false;
	}

	const UCodexSubsystem* Codex = GetCodex();
	if (!Codex || !Codex->IsFragmentRecovered(FragmentID))
	{
		UE_LOG(LogDebate, Warning, TEXT("Cannot cite unrecovered fragment '%s'."), *FragmentID.ToString());
		return false;
	}

	float Defence = DirectAnswerDefence;

	// Holding the source answers the objection; having placed it in the chain means
	// the answer was already part of the case rather than produced on the spot.
	FTransmissionChain Chain;
	if (Codex->GetChain(State.AnchorEventID, Chain) && Chain.ContainsFragment(FragmentID))
	{
		Defence += SlottedFragmentBonus;
	}

	State.SpentFragmentIDs.Add(FragmentID);

	return ResolveRound(EDebateResponseKind::PresentFragment, FragmentID, Defence);
}

bool UDebateSubsystem::ConcedePoint()
{
	if (!State.bIsActive)
	{
		return false;
	}

	return ResolveRound(EDebateResponseKind::Concede, NAME_None, 0.f);
}

bool UDebateSubsystem::ResolveRound(EDebateResponseKind ResponseKind, FName FragmentID, float Defence)
{
	const FDebateObjectionRow* Objection = Objections.Find(State.CurrentObjectionID);
	const FDebateOpponentRow* Opponent = Opponents.Find(State.OpponentID);
	if (!Objection || !Opponent)
	{
		return false;
	}

	const float Margin = Defence - Objection->RequiredLinkStrength;
	const bool bAnswered = Margin >= 0.f;

	// Ground is won at face value and lost at the opponent's aggression: a hostile
	// magistrate makes more of a gap than a patient elder does.
	const float ConvictionDelta = bAnswered
		? Margin * Objection->Weight
		: Margin * Objection->Weight * Opponent->Aggression;

	State.Conviction += ConvictionDelta;

	FDebateRoundRecord& Record = State.Rounds.AddDefaulted_GetRef();
	Record.ObjectionID = Objection->ObjectionID;
	Record.TargetedLink = Objection->TargetsLink;
	Record.ResponseKind = ResponseKind;
	Record.FragmentUsedID = FragmentID;
	Record.Defence = Defence;
	Record.ConvictionDelta = ConvictionDelta;
	Record.bAnswered = bAnswered;
	Record.OpponentReply = bAnswered ? Objection->ConcededResponse : Objection->PressedResponse;

	State.UsedObjectionIDs.Add(Objection->ObjectionID);
	++State.RoundIndex;

	// Either side can break early.
	if (State.Conviction >= Opponent->Composure)
	{
		EndDebate(EDebateOutcome::Won);
		return true;
	}

	if (State.Conviction <= -Opponent->Composure)
	{
		EndDebate(EDebateOutcome::Lost);
		return true;
	}

	if (State.RoundIndex >= State.MaxRounds)
	{
		EndDebate(State.Conviction > 0.f ? EDebateOutcome::Won : EDebateOutcome::Lost);
		return true;
	}

	State.CurrentObjectionID = SelectNextObjection();
	if (State.CurrentObjectionID.IsNone())
	{
		// He has run out of things to ask, which is itself a result.
		EndDebate(State.Conviction > 0.f ? EDebateOutcome::Won : EDebateOutcome::Drawn);
		return true;
	}

	OnObjectionRaised.Broadcast(State.CurrentObjectionID);
	return true;
}

void UDebateSubsystem::AbandonDebate()
{
	if (!State.bIsActive)
	{
		return;
	}

	EndDebate(EDebateOutcome::Abandoned);
}

void UDebateSubsystem::BuildOutcomeReport(EDebateOutcome Outcome)
{
	Report = FDebateOutcomeReport();
	Report.Outcome = Outcome;
	Report.OpponentID = State.OpponentID;
	Report.AnchorEventID = State.AnchorEventID;
	Report.FinalConviction = State.Conviction;
	Report.Rounds = State.Rounds;
	Report.WeakestLink = ChainAtStart.WeakestLink;

	const UCodexSubsystem* Codex = GetCodex();

	for (const FDebateRoundRecord& Round : State.Rounds)
	{
		if (Round.bAnswered)
		{
			continue;
		}

		Report.ObjectionsThatLanded.AddUnique(Round.ObjectionID);

		// The teaching half: what would have met this, that the player has not
		// found yet. This is the reading list the defeat hands him.
		const FDebateObjectionRow* Objection = Objections.Find(Round.ObjectionID);
		if (!Objection || !Codex)
		{
			continue;
		}

		for (const FName FragmentID : Objection->AnsweredByFragmentIDs)
		{
			if (!Codex->IsFragmentRecovered(FragmentID))
			{
				Report.WouldHaveAnsweredFragmentIDs.AddUnique(FragmentID);
			}
		}
	}
}

void UDebateSubsystem::EndDebate(EDebateOutcome Outcome)
{
	BuildOutcomeReport(Outcome);

	// An argument in a public place is something the player did, so it travels like
	// anything else he does.
	const FDebateOpponentRow* Opponent = Opponents.Find(State.OpponentID);
	if (Opponent && Outcome != EDebateOutcome::InProgress)
	{
		const FName DeedTypeID = (Outcome == EDebateOutcome::Won)
			? Opponent->VictoryDeedTypeID
			: Opponent->DefeatDeedTypeID;

		if (!DeedTypeID.IsNone())
		{
			if (UReputationSubsystem* Reputation = GetReputation())
			{
				const UCampaignMapSubsystem* Map = GetMap();
				const FName LocationID = Map ? Map->GetPartyLocation() : NAME_None;

				Reputation->RecordDeed(DeedTypeID, LocationID, TArray<FName>());
			}
		}
	}

	State.bIsActive = false;
	State.CurrentObjectionID = NAME_None;

	OnDebateEnded.Broadcast(Outcome);
}
