#include "UI/CodexViewLibrary.h"

#include "Campaign/CampaignTimelineSubsystem.h"
#include "Codex/CodexSubsystem.h"

#define LOCTEXT_NAMESPACE "CodexUI"

FText UCodexViewLibrary::GetLinkName(ETransmissionLink Link)
{
	switch (Link)
	{
	case ETransmissionLink::Event:					return LOCTEXT("LinkEvent", "Event");
	case ETransmissionLink::Eyewitness:				return LOCTEXT("LinkEyewitness", "Eyewitness");
	case ETransmissionLink::OralProclamation:		return LOCTEXT("LinkOral", "Oral Proclamation");
	case ETransmissionLink::WrittenSource:			return LOCTEXT("LinkWritten", "Written Source");
	case ETransmissionLink::Manuscript:				return LOCTEXT("LinkManuscript", "Manuscript");
	case ETransmissionLink::PatristicAttestation:	return LOCTEXT("LinkPatristic", "Patristic Attestation");
	default:										return FText::GetEmpty();
	}
}

FText UCodexViewLibrary::GetLinkDescription(ETransmissionLink Link)
{
	switch (Link)
	{
	case ETransmissionLink::Event:
		return LOCTEXT("LinkEventDesc", "That it happened at all. Where sources with no stake in the claim belong.");
	case ETransmissionLink::Eyewitness:
		return LOCTEXT("LinkEyewitnessDesc", "Who was positioned to see it.");
	case ETransmissionLink::OralProclamation:
		return LOCTEXT("LinkOralDesc", "What was handed on, fixed and memorised, before anything was written.");
	case ETransmissionLink::WrittenSource:
		return LOCTEXT("LinkWrittenDesc", "The document itself.");
	case ETransmissionLink::Manuscript:
		return LOCTEXT("LinkManuscriptDesc", "The copies by which the document reaches us.");
	case ETransmissionLink::PatristicAttestation:
		return LOCTEXT("LinkPatristicDesc", "Who, outside the document, vouches for where it came from.");
	default:
		return FText::GetEmpty();
	}
}

FText UCodexViewLibrary::GetTierName(EWitnessTier Tier)
{
	switch (Tier)
	{
	case EWitnessTier::Eyewitness:		return LOCTEXT("TierEyewitness", "Eyewitness");
	case EWitnessTier::Companion:		return LOCTEXT("TierCompanion", "Companion");
	case EWitnessTier::Patristic:		return LOCTEXT("TierPatristic", "Patristic");
	case EWitnessTier::Manuscript:		return LOCTEXT("TierManuscript", "Manuscript");
	case EWitnessTier::Hostile:			return LOCTEXT("TierHostile", "Hostile Witness");
	case EWitnessTier::Archaeological:	return LOCTEXT("TierArchaeological", "Archaeological");
	default:							return FText::GetEmpty();
	}
}

FText UCodexViewLibrary::GetBandName(EAttestationBand Band)
{
	switch (Band)
	{
	case EAttestationBand::Empty:		return LOCTEXT("BandEmpty", "Empty");
	case EAttestationBand::Thin:		return LOCTEXT("BandThin", "Thin");
	case EAttestationBand::Serviceable:	return LOCTEXT("BandServiceable", "Serviceable");
	case EAttestationBand::Strong:		return LOCTEXT("BandStrong", "Strong");
	default:							return FText::GetEmpty();
	}
}

EAttestationBand UCodexViewLibrary::BandForLinkScore(float Score)
{
	if (Score <= 0.f)
	{
		return EAttestationBand::Empty;
	}
	if (Score < 0.35f)
	{
		return EAttestationBand::Thin;
	}
	if (Score < 0.7f)
	{
		return EAttestationBand::Serviceable;
	}

	return EAttestationBand::Strong;
}

EAttestationBand UCodexViewLibrary::BandForAttestationStrength(float Strength)
{
	return BandForLinkScore(Strength / 100.f);
}

FText UCodexViewLibrary::FormatYearAD(int32 YearAD)
{
	// Deliberately not FText::AsNumber: a year is not a quantity, and grouping would
	// render 1229 as "1,229".
	return FText::Format(LOCTEXT("YearAD", "AD {0}"), FText::FromString(FString::FromInt(YearAD)));
}

FText UCodexViewLibrary::FormatYearRangeAD(int32 EarliestAD, int32 LatestAD)
{
	if (EarliestAD == LatestAD)
	{
		return FormatYearAD(EarliestAD);
	}

	return FText::Format(LOCTEXT("YearRangeAD", "AD {0}-{1}"),
		FText::FromString(FString::FromInt(EarliestAD)),
		FText::FromString(FString::FromInt(LatestAD)));
}

FCodexFragmentView UCodexViewLibrary::MakeFragmentView(const UCodexSubsystem* Codex, FName FragmentID)
{
	FCodexFragmentView View;
	View.FragmentID = FragmentID;

	if (!Codex)
	{
		return View;
	}

	FTestimonyFragmentDefinition Definition;
	if (!Codex->GetFragmentDefinition(FragmentID, Definition))
	{
		// Slotted but unregistered: say so rather than draw a blank row.
		return View;
	}

	View.bIsKnownFragment = true;
	View.Title = Definition.Title;
	View.SourceReference = Definition.SourceReference;
	View.Tier = Definition.Tier;
	View.TierName = GetTierName(Definition.Tier);
	View.AttestationDateAD = Definition.AttestationDateAD;
	View.AttestationDateText = FormatYearAD(Definition.AttestationDateAD);
	View.ScholarlyNote = Definition.ScholarlyNote;
	View.bIsContested = Definition.bIsContested;
	View.bIsRecovered = Codex->IsFragmentRecovered(FragmentID);

	return View;
}

void UCodexViewLibrary::GetSlottableFragments(const UCodexSubsystem* Codex, FName AnchorEventID,
	ETransmissionLink Link, bool bOnlyTierAppropriate, TArray<FCodexFragmentView>& OutFragments)
{
	OutFragments.Reset();

	if (!Codex || Link == ETransmissionLink::MAX)
	{
		return;
	}

	// A fragment already in this chain cannot go in twice, whichever link it sits in.
	FTransmissionChain Chain;
	const bool bHasChain = Codex->GetChain(AnchorEventID, Chain);

	TArray<FName> Recovered;
	Codex->GetRecoveredFragmentIDs(Recovered);

	for (const FName FragmentID : Recovered)
	{
		if (bHasChain && Chain.ContainsFragment(FragmentID))
		{
			continue;
		}

		FTestimonyFragmentDefinition Definition;
		if (!Codex->GetFragmentDefinition(FragmentID, Definition))
		{
			continue;
		}

		if (bOnlyTierAppropriate && !UTestimonyFragment::IsTierAppropriateForLink(Definition.Tier, Link))
		{
			continue;
		}

		OutFragments.Add(MakeFragmentView(Codex, FragmentID));
	}

	// Closest attestation first: proximity is what the link is mostly scored on, so
	// the best candidate should be the one at the top.
	OutFragments.Sort([](const FCodexFragmentView& A, const FCodexFragmentView& B)
	{
		return A.AttestationDateAD < B.AttestationDateAD;
	});
}

void UCodexViewLibrary::GetChainChallenges(const UCodexSubsystem* Codex, FName AnchorEventID,
	TArray<FCodexChallengeView>& OutChallenges)
{
	OutChallenges.Reset();

	if (!Codex)
	{
		return;
	}

	FTransmissionChain Chain;
	if (!Codex->GetChain(AnchorEventID, Chain))
	{
		return;
	}

	const FChainScoreBreakdown Breakdown = Codex->ScoreChain(AnchorEventID);

	for (const FTransmissionLinkSlot& Slot : Chain.Slots)
	{
		for (const FName SlottedID : Slot.FragmentIDs)
		{
			FTestimonyFragmentDefinition Slotted;
			if (!Codex->GetFragmentDefinition(SlottedID, Slotted))
			{
				continue;
			}

			for (const FName ChallengerID : Slotted.ChallengedBy)
			{
				FCodexChallengeView Challenge;
				Challenge.Challenger = MakeFragmentView(Codex, ChallengerID);
				Challenge.ChallengedFragmentID = SlottedID;
				Challenge.AgainstLink = Slot.Link;

				// The scoring pass is the authority on whether it still stands, so the
				// screen and the argument cannot disagree.
				Challenge.bIsAnswered = !Breakdown.UnansweredChallenges.Contains(ChallengerID);

				TArray<FName> Rebuttals;
				Codex->GetFragmentsThatRebut(ChallengerID, Rebuttals);
				Challenge.bHasNoKnownAnswer = Rebuttals.Num() == 0;

				for (const FName RebuttalID : Rebuttals)
				{
					const FCodexFragmentView RebuttalView = MakeFragmentView(Codex, RebuttalID);
					if (RebuttalView.bIsRecovered)
					{
						Challenge.AvailableAnswers.Add(RebuttalView);
					}
					else
					{
						Challenge.MissingAnswers.Add(RebuttalView);
					}
				}

				OutChallenges.Add(MoveTemp(Challenge));
			}
		}
	}
}

FCodexChainView UCodexViewLibrary::MakeChainView(const UCodexSubsystem* Codex,
	const UCampaignTimelineSubsystem* Timeline, FName AnchorEventID)
{
	FCodexChainView View;
	View.AnchorEventID = AnchorEventID;

	if (Timeline)
	{
		FCampaignEventRow AnchorRow;
		if (Timeline->GetEventRow(AnchorEventID, AnchorRow))
		{
			View.AnchorTitle = AnchorRow.EventTitle;
			View.AnchorDateText = FormatYearRangeAD(AnchorRow.YearEarliestAD, AnchorRow.YearLatestAD);
		}
	}

	if (!Codex)
	{
		return View;
	}

	const FChainScoreBreakdown Breakdown = Codex->ScoreChain(AnchorEventID);

	View.bChainExists = Breakdown.bChainExists;
	View.AttestationStrength = Breakdown.AttestationStrength;
	View.Band = BandForAttestationStrength(Breakdown.AttestationStrength);
	View.bIsComplete = Breakdown.bIsComplete;
	View.WeakestLink = Breakdown.WeakestLink;
	View.CorroborationCount = Breakdown.IndependentCorroborationCount;
	View.bHasHostileConfirmation = Breakdown.bHasHostileConfirmation;
	View.UnansweredChallengeCount = Breakdown.UnansweredChallenges.Num();
	View.UnknownFragmentIDs = Breakdown.UnknownFragmentIDs;

	if (!Breakdown.bChainExists)
	{
		return View;
	}

	FTransmissionChain Chain;
	Codex->GetChain(AnchorEventID, Chain);

	for (const FChainLinkScore& LinkScore : Breakdown.LinkScores)
	{
		FCodexLinkView LinkView;
		LinkView.Link = LinkScore.Link;
		LinkView.LinkName = GetLinkName(LinkScore.Link);
		LinkView.LinkDescription = GetLinkDescription(LinkScore.Link);
		LinkView.bIsFilled = LinkScore.bIsFilled;
		LinkView.Score = LinkScore.LinkScore;
		LinkView.Band = BandForLinkScore(LinkScore.LinkScore);
		LinkView.bIsWeakestLink = LinkScore.Link == Breakdown.WeakestLink;
		LinkView.UnansweredChallengeCount = LinkScore.UnansweredChallenges.Num();

		if (const FTransmissionLinkSlot* Slot = Chain.FindSlot(LinkScore.Link))
		{
			for (const FName FragmentID : Slot->FragmentIDs)
			{
				FCodexFragmentView FragmentView = MakeFragmentView(Codex, FragmentID);
				FragmentView.bIsSlotted = true;
				FragmentView.SlottedInLink = LinkScore.Link;

				LinkView.SlottedFragments.Add(MoveTemp(FragmentView));
			}
		}

		View.Links.Add(MoveTemp(LinkView));
	}

	GetChainChallenges(Codex, AnchorEventID, View.Challenges);

	return View;
}

#undef LOCTEXT_NAMESPACE
