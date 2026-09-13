#include "Codex/CodexSubsystem.h"

#include "Campaign/CampaignTimelineSubsystem.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"

DEFINE_LOG_CATEGORY(LogCodex);

void UCodexSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UCodexSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

void UCodexSubsystem::SetTimelineForTesting(UCampaignTimelineSubsystem* InTimeline)
{
	TimelineOverride = InTimeline;
}

UCampaignTimelineSubsystem* UCodexSubsystem::GetTimeline() const
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

// --- Registry ---------------------------------------------------------------

bool UCodexSubsystem::RegisterFragmentDefinition(const FTestimonyFragmentDefinition& Definition)
{
	if (Definition.FragmentID.IsNone())
	{
		UE_LOG(LogCodex, Warning, TEXT("Refusing to register a testimony fragment with no FragmentID."));
		return false;
	}

	if (FragmentRegistry.Contains(Definition.FragmentID))
	{
		UE_LOG(LogCodex, Warning, TEXT("Testimony fragment '%s' is already registered; ignoring the duplicate."),
			*Definition.FragmentID.ToString());
		return false;
	}

	FragmentRegistry.Add(Definition.FragmentID, Definition);
	return true;
}

bool UCodexSubsystem::RegisterFragment(const UTestimonyFragment* Fragment)
{
	if (!Fragment)
	{
		return false;
	}

	return RegisterFragmentDefinition(Fragment->Definition);
}

int32 UCodexSubsystem::RegisterFragmentsFromDataTable(const UDataTable* FragmentTable)
{
	if (!FragmentTable)
	{
		return 0;
	}

	TArray<FTestimonyFragmentDefinition*> Rows;
	FragmentTable->GetAllRows<FTestimonyFragmentDefinition>(TEXT("UCodexSubsystem::RegisterFragmentsFromDataTable"), Rows);

	int32 RegisteredCount = 0;
	for (const FTestimonyFragmentDefinition* Row : Rows)
	{
		if (Row && RegisterFragmentDefinition(*Row))
		{
			++RegisteredCount;
		}
	}

	UE_LOG(LogCodex, Log, TEXT("Registered %d testimony fragments from '%s'."),
		RegisteredCount, *FragmentTable->GetName());

	return RegisteredCount;
}

bool UCodexSubsystem::GetFragmentDefinition(FName FragmentID, FTestimonyFragmentDefinition& OutDefinition) const
{
	if (const FTestimonyFragmentDefinition* Found = FragmentRegistry.Find(FragmentID))
	{
		OutDefinition = *Found;
		return true;
	}

	return false;
}

bool UCodexSubsystem::IsFragmentRegistered(FName FragmentID) const
{
	return FragmentRegistry.Contains(FragmentID);
}

void UCodexSubsystem::ValidateRegistry(TArray<FName>& OutDanglingReferences) const
{
	OutDanglingReferences.Reset();

	for (const TPair<FName, FTestimonyFragmentDefinition>& Pair : FragmentRegistry)
	{
		auto CheckReferences = [this, &OutDanglingReferences](const TArray<FName>& References)
		{
			for (const FName ReferencedID : References)
			{
				if (!ReferencedID.IsNone() && !FragmentRegistry.Contains(ReferencedID))
				{
					OutDanglingReferences.AddUnique(ReferencedID);
				}
			}
		};

		CheckReferences(Pair.Value.CorroboratedBy);
		CheckReferences(Pair.Value.ChallengedBy);
		CheckReferences(Pair.Value.RebutsFragmentIDs);
	}
}

// --- Recovery ---------------------------------------------------------------

bool UCodexSubsystem::RecoverFragment(FName FragmentID)
{
	if (!FragmentRegistry.Contains(FragmentID))
	{
		UE_LOG(LogCodex, Warning, TEXT("Cannot recover unregistered testimony fragment '%s'."),
			*FragmentID.ToString());
		return false;
	}

	bool bAlreadyRecovered = false;
	RecoveredFragmentIDs.Add(FragmentID, &bAlreadyRecovered);
	if (bAlreadyRecovered)
	{
		return false;
	}

	OnFragmentRecovered.Broadcast(FragmentID);
	return true;
}

bool UCodexSubsystem::IsFragmentRecovered(FName FragmentID) const
{
	return RecoveredFragmentIDs.Contains(FragmentID);
}

void UCodexSubsystem::GetRecoveredFragmentIDs(TArray<FName>& OutFragmentIDs) const
{
	OutFragmentIDs = RecoveredFragmentIDs.Array();
}

// --- Chain assembly ---------------------------------------------------------

FTransmissionChain* UCodexSubsystem::FindChain(FName AnchorEventID)
{
	return Chains.FindByPredicate([AnchorEventID](const FTransmissionChain& Chain)
	{
		return Chain.AnchorEventID == AnchorEventID;
	});
}

const FTransmissionChain* UCodexSubsystem::FindChain(FName AnchorEventID) const
{
	return Chains.FindByPredicate([AnchorEventID](const FTransmissionChain& Chain)
	{
		return Chain.AnchorEventID == AnchorEventID;
	});
}

bool UCodexSubsystem::CreateChain(FName AnchorEventID)
{
	int32 AnchorYearAD = INDEX_NONE;

	if (const UCampaignTimelineSubsystem* Timeline = GetTimeline())
	{
		FCampaignEventRow Row;
		if (Timeline->GetEventRow(AnchorEventID, Row))
		{
			AnchorYearAD = Row.YearEarliestAD;
		}
	}

	if (AnchorYearAD == INDEX_NONE)
	{
		UE_LOG(LogCodex, Warning,
			TEXT("Cannot open a chain for '%s': no such row in the campaign timeline, or the timeline table is unset."),
			*AnchorEventID.ToString());
		return false;
	}

	return CreateChainWithAnchorYear(AnchorEventID, AnchorYearAD);
}

bool UCodexSubsystem::CreateChainWithAnchorYear(FName AnchorEventID, int32 AnchorYearAD)
{
	if (AnchorEventID.IsNone() || FindChain(AnchorEventID) != nullptr)
	{
		return false;
	}

	FTransmissionChain& Chain = Chains.AddDefaulted_GetRef();
	Chain.AnchorEventID = AnchorEventID;
	Chain.AnchorYearAD = AnchorYearAD;
	Chain.InitialiseSlots();

	OnChainChanged.Broadcast(AnchorEventID);
	return true;
}

bool UCodexSubsystem::SlotFragment(FName AnchorEventID, ETransmissionLink Link, FName FragmentID)
{
	if (Link == ETransmissionLink::MAX)
	{
		return false;
	}

	FTransmissionChain* Chain = FindChain(AnchorEventID);
	if (!Chain)
	{
		UE_LOG(LogCodex, Warning, TEXT("No open chain for '%s'."), *AnchorEventID.ToString());
		return false;
	}

	if (!IsFragmentRecovered(FragmentID))
	{
		UE_LOG(LogCodex, Warning, TEXT("Fragment '%s' has not been recovered and cannot be slotted."),
			*FragmentID.ToString());
		return false;
	}

	// One link per chain: the same fragment must not be counted twice in one argument.
	if (Chain->ContainsFragment(FragmentID))
	{
		return false;
	}

	FTransmissionLinkSlot* Slot = Chain->FindSlot(Link);
	if (!Slot)
	{
		return false;
	}

	Slot->FragmentIDs.Add(FragmentID);
	OnChainChanged.Broadcast(AnchorEventID);
	return true;
}

bool UCodexSubsystem::UnslotFragment(FName AnchorEventID, FName FragmentID)
{
	FTransmissionChain* Chain = FindChain(AnchorEventID);
	if (!Chain)
	{
		return false;
	}

	bool bRemovedAny = false;
	for (FTransmissionLinkSlot& Slot : Chain->Slots)
	{
		bRemovedAny |= Slot.FragmentIDs.Remove(FragmentID) > 0;
	}

	if (bRemovedAny)
	{
		OnChainChanged.Broadcast(AnchorEventID);
	}

	return bRemovedAny;
}

bool UCodexSubsystem::GetChain(FName AnchorEventID, FTransmissionChain& OutChain) const
{
	if (const FTransmissionChain* Found = FindChain(AnchorEventID))
	{
		OutChain = *Found;
		return true;
	}

	return false;
}

void UCodexSubsystem::GetChainAnchorEventIDs(TArray<FName>& OutAnchorEventIDs) const
{
	OutAnchorEventIDs.Reset();
	OutAnchorEventIDs.Reserve(Chains.Num());

	for (const FTransmissionChain& Chain : Chains)
	{
		OutAnchorEventIDs.Add(Chain.AnchorEventID);
	}
}

// --- Scoring ----------------------------------------------------------------

float UCodexSubsystem::ComputeProximityScore(int32 AttestationDateAD, int32 AnchorYearAD) const
{
	if (AnchorYearAD == INDEX_NONE)
	{
		return 0.f;
	}

	// Testimony predating the event it attests is a data error, not extra credit.
	const int32 Gap = FMath::Max(0, AttestationDateAD - AnchorYearAD);
	if (Gap <= ScoringRules.LivingMemoryYears)
	{
		return 1.f;
	}

	const float FalloffYears = static_cast<float>(FMath::Max(1, ScoringRules.AttestationFalloffYears));
	const float Decayed = 1.f - (static_cast<float>(Gap - ScoringRules.LivingMemoryYears) / FalloffYears);

	return FMath::Clamp(Decayed, 0.f, 1.f);
}

bool UCodexSubsystem::IsChallengeAnswered(FName ChallengerID, const FTransmissionChain& Chain) const
{
	for (const FTransmissionLinkSlot& Slot : Chain.Slots)
	{
		for (const FName FragmentID : Slot.FragmentIDs)
		{
			const FTestimonyFragmentDefinition* Definition = FragmentRegistry.Find(FragmentID);
			if (Definition && Definition->RebutsFragmentIDs.Contains(ChallengerID))
			{
				return true;
			}
		}
	}

	return false;
}

int32 UCodexSubsystem::CountCorroboratingPairs(const FTransmissionChain& Chain) const
{
	TArray<FName> Slotted;
	for (const FTransmissionLinkSlot& Slot : Chain.Slots)
	{
		Slotted.Append(Slot.FragmentIDs);
	}

	int32 PairCount = 0;
	for (int32 i = 0; i < Slotted.Num(); ++i)
	{
		const FTestimonyFragmentDefinition* A = FragmentRegistry.Find(Slotted[i]);
		if (!A)
		{
			continue;
		}

		for (int32 j = i + 1; j < Slotted.Num(); ++j)
		{
			const FTestimonyFragmentDefinition* B = FragmentRegistry.Find(Slotted[j]);
			if (!B)
			{
				continue;
			}

			// Corroboration is symmetric even where only one side's data records it,
			// so a pair counts once regardless of which fragment names the other.
			if (A->CorroboratedBy.Contains(Slotted[j]) || B->CorroboratedBy.Contains(Slotted[i]))
			{
				++PairCount;
			}
		}
	}

	return PairCount;
}

FChainScoreBreakdown UCodexSubsystem::ScoreChain(FName AnchorEventID) const
{
	FChainScoreBreakdown Breakdown;
	Breakdown.AnchorEventID = AnchorEventID;

	const FTransmissionChain* Chain = FindChain(AnchorEventID);
	if (!Chain)
	{
		return Breakdown;
	}

	Breakdown.bChainExists = true;
	Breakdown.bIsComplete = true;

	float LinkScoreTotal = 0.f;
	float LowestLinkScore = TNumericLimits<float>::Max();

	for (int32 LinkIndex = 0; LinkIndex < NumTransmissionLinks; ++LinkIndex)
	{
		const ETransmissionLink Link = static_cast<ETransmissionLink>(LinkIndex);

		FChainLinkScore LinkScore;
		LinkScore.Link = Link;

		const FTransmissionLinkSlot* Slot = Chain->FindSlot(Link);
		int32 ValidFragmentCount = 0;

		if (Slot)
		{
			for (const FName FragmentID : Slot->FragmentIDs)
			{
				const FTestimonyFragmentDefinition* Definition = FragmentRegistry.Find(FragmentID);
				if (!Definition)
				{
					Breakdown.UnknownFragmentIDs.AddUnique(FragmentID);
					continue;
				}

				++ValidFragmentCount;

				float FragmentScore = ComputeProximityScore(Definition->AttestationDateAD, Chain->AnchorYearAD);

				if (!UTestimonyFragment::IsTierAppropriateForLink(Definition->Tier, Link))
				{
					FragmentScore *= ScoringRules.MismatchedTierMultiplier;
				}

				if (Definition->bIsContested)
				{
					FragmentScore *= ScoringRules.ContestedFragmentMultiplier;
				}

				LinkScore.BestFragmentScore = FMath::Max(LinkScore.BestFragmentScore, FragmentScore);

				if (Definition->Tier == EWitnessTier::Hostile)
				{
					Breakdown.bHasHostileConfirmation = true;
				}

				// A debate opponent raises a counterargument whether or not the player
				// has found it, so challenges count until something in this chain
				// rebuts them.
				for (const FName ChallengerID : Definition->ChallengedBy)
				{
					if (!IsChallengeAnswered(ChallengerID, *Chain))
					{
						LinkScore.UnansweredChallenges.AddUnique(ChallengerID);
						Breakdown.UnansweredChallenges.AddUnique(ChallengerID);
					}
				}
			}
		}

		LinkScore.FragmentCount = ValidFragmentCount;
		LinkScore.bIsFilled = ValidFragmentCount > 0;

		if (LinkScore.bIsFilled)
		{
			LinkScore.LinkScore = FMath::Clamp(
				LinkScore.BestFragmentScore + ScoringRules.IndependentFragmentBonus * static_cast<float>(ValidFragmentCount - 1),
				0.f,
				1.f);
		}
		else
		{
			Breakdown.bIsComplete = false;
		}

		// Strict less-than, so a tie reports the earliest link in transmission
		// order -- the one an opponent attacks first.
		if (LinkScore.LinkScore < LowestLinkScore)
		{
			LowestLinkScore = LinkScore.LinkScore;
			Breakdown.WeakestLink = Link;
		}

		LinkScoreTotal += LinkScore.LinkScore;
		Breakdown.LinkScores.Add(LinkScore);
	}

	Breakdown.IndependentCorroborationCount = CountCorroboratingPairs(*Chain);

	float Strength = (LinkScoreTotal / static_cast<float>(NumTransmissionLinks)) * 100.f;
	Strength += ScoringRules.CorroborationPairBonus * static_cast<float>(Breakdown.IndependentCorroborationCount);

	if (Breakdown.bHasHostileConfirmation)
	{
		Strength += ScoringRules.HostileConfirmationBonus;
	}

	Strength -= ScoringRules.UnansweredChallengePenalty * static_cast<float>(Breakdown.UnansweredChallenges.Num());

	Breakdown.AttestationStrength = FMath::Clamp(Strength, 0.f, 100.f);

	return Breakdown;
}

float UCodexSubsystem::GetCampaignAttestationStrength() const
{
	if (Chains.Num() == 0)
	{
		return 0.f;
	}

	float Total = 0.f;
	for (const FTransmissionChain& Chain : Chains)
	{
		Total += ScoreChain(Chain.AnchorEventID).AttestationStrength;
	}

	return Total / static_cast<float>(Chains.Num());
}

// --- Save/load --------------------------------------------------------------

FCodexSaveData UCodexSubsystem::CaptureSaveData() const
{
	FCodexSaveData SaveData;
	SaveData.RecoveredFragmentIDs = RecoveredFragmentIDs.Array();
	SaveData.Chains = Chains;

	return SaveData;
}

void UCodexSubsystem::RestoreFromSaveData(const FCodexSaveData& SaveData)
{
	RecoveredFragmentIDs.Reset();
	for (const FName FragmentID : SaveData.RecoveredFragmentIDs)
	{
		RecoveredFragmentIDs.Add(FragmentID);
	}

	Chains = SaveData.Chains;

	// A save written before a link was added to ETransmissionLink would be missing
	// its slot; fill any gap rather than rebuilding and discarding slotted work.
	for (FTransmissionChain& Chain : Chains)
	{
		for (int32 LinkIndex = 0; LinkIndex < NumTransmissionLinks; ++LinkIndex)
		{
			const ETransmissionLink Link = static_cast<ETransmissionLink>(LinkIndex);
			if (Chain.FindSlot(Link) == nullptr)
			{
				FTransmissionLinkSlot& Slot = Chain.Slots.AddDefaulted_GetRef();
				Slot.Link = Link;
			}
		}
	}
}
