#include "UI/CodexWidgets.h"

#include "Campaign/CampaignTimelineSubsystem.h"
#include "Codex/CodexSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "UI/CodexViewLibrary.h"

// --- Shared base ------------------------------------------------------------

UCodexSubsystem* UCodexWidgetBase::GetCodex() const
{
	if (const UWorld* World = GetWorld())
	{
		if (UGameInstance* GameInstance = World->GetGameInstance())
		{
			return GameInstance->GetSubsystem<UCodexSubsystem>();
		}
	}

	return nullptr;
}

UCampaignTimelineSubsystem* UCodexWidgetBase::GetTimeline() const
{
	if (const UWorld* World = GetWorld())
	{
		if (UGameInstance* GameInstance = World->GetGameInstance())
		{
			return GameInstance->GetSubsystem<UCampaignTimelineSubsystem>();
		}
	}

	return nullptr;
}

void UCodexWidgetBase::NativeConstruct()
{
	Super::NativeConstruct();

	// Slotting a fragment from one screen should be visible on another without
	// anybody remembering to refresh it.
	if (UCodexSubsystem* Codex = GetCodex())
	{
		Codex->OnChainChanged.AddDynamic(this, &UCodexWidgetBase::HandleChainChanged);
		Codex->OnFragmentRecovered.AddDynamic(this, &UCodexWidgetBase::HandleFragmentRecovered);
	}

	Refresh();
}

void UCodexWidgetBase::NativeDestruct()
{
	if (UCodexSubsystem* Codex = GetCodex())
	{
		Codex->OnChainChanged.RemoveDynamic(this, &UCodexWidgetBase::HandleChainChanged);
		Codex->OnFragmentRecovered.RemoveDynamic(this, &UCodexWidgetBase::HandleFragmentRecovered);
	}

	Super::NativeDestruct();
}

void UCodexWidgetBase::Refresh()
{
	// Nothing at this level; each screen knows what it shows.
}

void UCodexWidgetBase::HandleChainChanged(FName AnchorEventID)
{
	Refresh();
}

void UCodexWidgetBase::HandleFragmentRecovered(FName FragmentID)
{
	Refresh();
}

// --- Chain-building screen --------------------------------------------------

void UCodexChainScreenBase::SetAnchorEvent(FName InAnchorEventID)
{
	AnchorEventID = InAnchorEventID;
	Refresh();
}

void UCodexChainScreenBase::Refresh()
{
	ChainView = UCodexViewLibrary::MakeChainView(GetCodex(), GetTimeline(), AnchorEventID);
	OnChainViewRefreshed(ChainView);
}

void UCodexChainScreenBase::GetSlottableFragments(ETransmissionLink Link, bool bOnlyTierAppropriate,
	TArray<FCodexFragmentView>& OutFragments) const
{
	UCodexViewLibrary::GetSlottableFragments(GetCodex(), AnchorEventID, Link,
		bOnlyTierAppropriate, OutFragments);
}

bool UCodexChainScreenBase::SlotFragment(ETransmissionLink Link, FName FragmentID)
{
	UCodexSubsystem* Codex = GetCodex();
	if (!Codex)
	{
		return false;
	}

	// The subsystem broadcasts OnChainChanged, which refreshes this screen, so there
	// is no second refresh here.
	return Codex->SlotFragment(AnchorEventID, Link, FragmentID);
}

bool UCodexChainScreenBase::UnslotFragment(FName FragmentID)
{
	UCodexSubsystem* Codex = GetCodex();
	if (!Codex)
	{
		return false;
	}

	return Codex->UnslotFragment(AnchorEventID, FragmentID);
}

// --- Fragment inspector -----------------------------------------------------

void UFragmentInspectorBase::SetFragment(FName InFragmentID)
{
	FragmentID = InFragmentID;
	Refresh();
}

void UFragmentInspectorBase::Refresh()
{
	Corroborations.Reset();
	Challenges.Reset();

	const UCodexSubsystem* Codex = GetCodex();
	FragmentView = UCodexViewLibrary::MakeFragmentView(Codex, FragmentID);

	FTestimonyFragmentDefinition Definition;
	if (Codex && Codex->GetFragmentDefinition(FragmentID, Definition))
	{
		for (const FName CorroboratingID : Definition.CorroboratedBy)
		{
			Corroborations.Add(UCodexViewLibrary::MakeFragmentView(Codex, CorroboratingID));
		}

		for (const FName ChallengingID : Definition.ChallengedBy)
		{
			Challenges.Add(UCodexViewLibrary::MakeFragmentView(Codex, ChallengingID));
		}
	}

	OnFragmentViewRefreshed(FragmentView);
}

// --- Challenge and response -------------------------------------------------

void UChallengeResponseViewBase::SetAnchorEvent(FName InAnchorEventID)
{
	AnchorEventID = InAnchorEventID;
	Refresh();
}

void UChallengeResponseViewBase::Refresh()
{
	UCodexViewLibrary::GetChainChallenges(GetCodex(), AnchorEventID, Challenges);

	UnansweredCount = 0;
	UnanswerableCount = 0;

	for (const FCodexChallengeView& Challenge : Challenges)
	{
		if (Challenge.bIsAnswered)
		{
			continue;
		}

		++UnansweredCount;

		// Counted separately: an objection nothing in the record answers is not a
		// gap in the player's collection, and telling him to go and look would be
		// a lie.
		if (Challenge.bHasNoKnownAnswer)
		{
			++UnanswerableCount;
		}
	}

	OnChallengesRefreshed();
}
