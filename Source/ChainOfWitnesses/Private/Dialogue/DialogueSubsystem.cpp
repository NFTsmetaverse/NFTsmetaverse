#include "Dialogue/DialogueSubsystem.h"

#include "Campaign/CampaignTimelineSubsystem.h"
#include "Codex/CodexSubsystem.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"

DEFINE_LOG_CATEGORY(LogDialogue);

void UDialogueSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UDialogueSubsystem::Deinitialize()
{
	EndConversation();
	Super::Deinitialize();
}

void UDialogueSubsystem::SetDependenciesForTesting(UCodexSubsystem* InCodex, UCampaignTimelineSubsystem* InTimeline)
{
	CodexOverride = InCodex;
	TimelineOverride = InTimeline;
}

UCodexSubsystem* UDialogueSubsystem::GetCodex() const
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

UCampaignTimelineSubsystem* UDialogueSubsystem::GetTimeline() const
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

// --- Content ----------------------------------------------------------------

bool UDialogueSubsystem::RegisterNode(const FDialogueNodeRow& Node)
{
	if (Node.NodeID.IsNone())
	{
		UE_LOG(LogDialogue, Warning, TEXT("Refusing to register a dialogue node with no NodeID."));
		return false;
	}

	if (Nodes.Contains(Node.NodeID))
	{
		UE_LOG(LogDialogue, Warning, TEXT("Dialogue node '%s' is already registered; ignoring the duplicate."),
			*Node.NodeID.ToString());
		return false;
	}

	Nodes.Add(Node.NodeID, Node);
	return true;
}

int32 UDialogueSubsystem::RegisterDialogueTable(const UDataTable* DialogueTable)
{
	if (!DialogueTable)
	{
		return 0;
	}

	TArray<FDialogueNodeRow*> Rows;
	DialogueTable->GetAllRows<FDialogueNodeRow>(TEXT("UDialogueSubsystem::RegisterDialogueTable"), Rows);

	int32 AddedCount = 0;
	for (const FDialogueNodeRow* Row : Rows)
	{
		if (Row && RegisterNode(*Row))
		{
			++AddedCount;
		}
	}

	UE_LOG(LogDialogue, Log, TEXT("Registered %d dialogue nodes from '%s'."), AddedCount, *DialogueTable->GetName());
	return AddedCount;
}

bool UDialogueSubsystem::GetNode(FName NodeID, FDialogueNodeRow& OutNode) const
{
	if (const FDialogueNodeRow* Found = Nodes.Find(NodeID))
	{
		OutNode = *Found;
		return true;
	}

	return false;
}

void UDialogueSubsystem::ValidateDialogue(TArray<FString>& OutProblems) const
{
	OutProblems.Reset();

	const UCodexSubsystem* Codex = GetCodex();

	auto CheckGrantedFragments = [this, Codex, &OutProblems](const FDialogueEffects& Effects, const FString& Where)
	{
		if (!Codex)
		{
			return;
		}

		for (const FName FragmentID : Effects.GrantsFragmentIDs)
		{
			if (!Codex->IsFragmentRegistered(FragmentID))
			{
				OutProblems.Add(FString::Printf(TEXT("%s grants unregistered fragment '%s'."),
					*Where, *FragmentID.ToString()));
			}
		}
	};

	for (const TPair<FName, FDialogueNodeRow>& Pair : Nodes)
	{
		const FDialogueNodeRow& Node = Pair.Value;
		const FString NodeName = Node.NodeID.ToString();

		if (!Node.FallbackNodeID.IsNone() && !Nodes.Contains(Node.FallbackNodeID))
		{
			OutProblems.Add(FString::Printf(TEXT("Node '%s' falls back to missing node '%s'."),
				*NodeName, *Node.FallbackNodeID.ToString()));
		}

		CheckGrantedFragments(Node.EntryEffects, FString::Printf(TEXT("Node '%s' entry"), *NodeName));

		for (int32 Index = 0; Index < Node.Options.Num(); ++Index)
		{
			const FDialogueOption& Option = Node.Options[Index];

			if (!Option.NextNodeID.IsNone() && !Nodes.Contains(Option.NextNodeID))
			{
				OutProblems.Add(FString::Printf(TEXT("Node '%s' option %d points at missing node '%s'."),
					*NodeName, Index, *Option.NextNodeID.ToString()));
			}

			CheckGrantedFragments(Option.Effects,
				FString::Printf(TEXT("Node '%s' option %d"), *NodeName, Index));

			for (const FName FragmentID : Option.Conditions.RequiredFragmentIDs)
			{
				if (Codex && !Codex->IsFragmentRegistered(FragmentID))
				{
					OutProblems.Add(FString::Printf(TEXT("Node '%s' option %d requires unregistered fragment '%s'."),
						*NodeName, Index, *FragmentID.ToString()));
				}
			}
		}
	}
}

// --- Standing ---------------------------------------------------------------

const FNpcDialogueState* UDialogueSubsystem::FindNpcState(FName NpcID) const
{
	return NpcStates.FindByPredicate([NpcID](const FNpcDialogueState& State)
	{
		return State.NpcID == NpcID;
	});
}

FNpcDialogueState& UDialogueSubsystem::FindOrAddNpcState(FName NpcID)
{
	FNpcDialogueState* Existing = NpcStates.FindByPredicate([NpcID](const FNpcDialogueState& State)
	{
		return State.NpcID == NpcID;
	});

	if (Existing)
	{
		return *Existing;
	}

	FNpcDialogueState& Added = NpcStates.AddDefaulted_GetRef();
	Added.NpcID = NpcID;
	return Added;
}

float UDialogueSubsystem::GetTrust(FName NpcID) const
{
	const FNpcDialogueState* State = FindNpcState(NpcID);
	return State ? State->Trust : 0.f;
}

void UDialogueSubsystem::ModifyTrust(FName NpcID, float Delta)
{
	if (NpcID.IsNone())
	{
		return;
	}

	FNpcDialogueState& State = FindOrAddNpcState(NpcID);
	State.Trust += Delta;

	OnTrustChanged.Broadcast(NpcID, State.Trust);
}

EDisclosureLevel UDialogueSubsystem::GetDisclosure(FName NpcID) const
{
	const FNpcDialogueState* State = FindNpcState(NpcID);
	return State ? State->Disclosure : EDisclosureLevel::Stranger;
}

void UDialogueSubsystem::RaiseDisclosure(FName NpcID, EDisclosureLevel NewLevel)
{
	if (NpcID.IsNone() || NewLevel == EDisclosureLevel::MAX)
	{
		return;
	}

	FNpcDialogueState& State = FindOrAddNpcState(NpcID);
	if (NewLevel <= State.Disclosure)
	{
		return;
	}

	State.Disclosure = NewLevel;
	OnDisclosureChanged.Broadcast(NpcID, NewLevel);
}

void UDialogueSubsystem::RecordVouch(FName VoucherNpcID, FName TargetNpcID)
{
	if (VoucherNpcID.IsNone())
	{
		return;
	}

	// A vouch with no named target is a general one -- the voucher has spoken for
	// the player publicly, and it counts with anyone who accepts him.
	if (TargetNpcID.IsNone())
	{
		GeneralVouchers.Add(VoucherNpcID);
		return;
	}

	FNpcDialogueState& State = FindOrAddNpcState(TargetNpcID);
	State.VouchersToThisNpc.AddUnique(VoucherNpcID);
}

bool UDialogueSubsystem::HasVouch(FName VoucherNpcID, FName TargetNpcID) const
{
	if (GeneralVouchers.Contains(VoucherNpcID))
	{
		return true;
	}

	const FNpcDialogueState* State = FindNpcState(TargetNpcID);
	return State && State->VouchersToThisNpc.Contains(VoucherNpcID);
}

// --- Gating -----------------------------------------------------------------

bool UDialogueSubsystem::EvaluateConditions(const FDialogueConditionSet& Conditions, FName NpcID,
	EConversationSafety Safety, EDialogueGate& OutGate) const
{
	OutGate = EDialogueGate::None;

	// World facts first: these are the gates the player cannot act on at all.
	const bool bNeedsTimeline = Conditions.EarliestYearAD != INDEX_NONE
		|| Conditions.LatestYearAD != INDEX_NONE
		|| Conditions.RequiredActiveEventIDs.Num() > 0;

	if (bNeedsTimeline)
	{
		const UCampaignTimelineSubsystem* Timeline = GetTimeline();
		if (!Timeline)
		{
			// Fail closed. A line that stays shut because its gate could not be
			// checked is a bug someone will notice; one that opens is not.
			OutGate = EDialogueGate::OutsideEra;
			return false;
		}

		const int32 CurrentYearAD = Timeline->GetCurrentYearAD();

		if (Conditions.EarliestYearAD != INDEX_NONE && CurrentYearAD < Conditions.EarliestYearAD)
		{
			OutGate = EDialogueGate::OutsideEra;
			return false;
		}

		if (Conditions.LatestYearAD != INDEX_NONE && CurrentYearAD > Conditions.LatestYearAD)
		{
			OutGate = EDialogueGate::OutsideEra;
			return false;
		}

		for (const FName EventID : Conditions.RequiredActiveEventIDs)
		{
			if (!Timeline->IsEventActive(EventID))
			{
				OutGate = EDialogueGate::MissingWorldState;
				return false;
			}
		}
	}

	// Then the setting, then the player's standing: gates he can act on, cheapest first.
	if (Safety < Conditions.MinimumSafety)
	{
		OutGate = EDialogueGate::Unsafe;
		return false;
	}

	if (Conditions.AcceptedVoucherIDs.Num() > 0)
	{
		bool bVouchedFor = false;
		for (const FName VoucherID : Conditions.AcceptedVoucherIDs)
		{
			if (HasVouch(VoucherID, NpcID))
			{
				bVouchedFor = true;
				break;
			}
		}

		if (!bVouchedFor)
		{
			OutGate = EDialogueGate::NoVoucher;
			return false;
		}
	}

	if (GetTrust(NpcID) < Conditions.MinimumTrust)
	{
		OutGate = EDialogueGate::InsufficientTrust;
		return false;
	}

	if (GetDisclosure(NpcID) < Conditions.MinimumDisclosure)
	{
		OutGate = EDialogueGate::InsufficientDisclosure;
		return false;
	}

	// Finally what he can demonstrate.
	const bool bNeedsCodex = Conditions.RequiredFragmentIDs.Num() > 0
		|| !Conditions.CredibleChainAnchorEventID.IsNone();

	if (bNeedsCodex)
	{
		const UCodexSubsystem* Codex = GetCodex();
		if (!Codex)
		{
			OutGate = EDialogueGate::UnknownFragment;
			return false;
		}

		for (const FName FragmentID : Conditions.RequiredFragmentIDs)
		{
			if (!Codex->IsFragmentRecovered(FragmentID))
			{
				OutGate = EDialogueGate::UnknownFragment;
				return false;
			}
		}

		if (!Conditions.CredibleChainAnchorEventID.IsNone())
		{
			const FChainScoreBreakdown Breakdown = Codex->ScoreChain(Conditions.CredibleChainAnchorEventID);
			if (!Breakdown.bChainExists || Breakdown.AttestationStrength < Conditions.MinimumChainStrength)
			{
				OutGate = EDialogueGate::WeakChain;
				return false;
			}
		}
	}

	return true;
}

// --- Conversation flow ------------------------------------------------------

void UDialogueSubsystem::ApplyEffects(const FDialogueEffects& Effects, FName SpeakerNpcID)
{
	if (!FMath::IsNearlyZero(Effects.TrustDelta))
	{
		ModifyTrust(SpeakerNpcID, Effects.TrustDelta);
	}

	// Stranger is the "no change" value; RaiseDisclosure ignores downgrades anyway.
	if (Effects.RaiseDisclosureTo != EDisclosureLevel::Stranger)
	{
		RaiseDisclosure(SpeakerNpcID, Effects.RaiseDisclosureTo);
	}

	if (Effects.GrantsFragmentIDs.Num() > 0)
	{
		if (UCodexSubsystem* Codex = GetCodex())
		{
			for (const FName FragmentID : Effects.GrantsFragmentIDs)
			{
				Codex->RecoverFragment(FragmentID);
			}
		}
		else
		{
			UE_LOG(LogDialogue, Warning,
				TEXT("Dialogue would grant %d fragment(s) but no Codex subsystem is available."),
				Effects.GrantsFragmentIDs.Num());
		}
	}

	for (const FName TargetNpcID : Effects.VouchesForPlayerTo)
	{
		RecordVouch(SpeakerNpcID, TargetNpcID);
	}
}

bool UDialogueSubsystem::EnterNode(FName NodeID)
{
	TSet<FName> VisitedThisTraversal;
	FName NextNodeID = NodeID;

	while (!NextNodeID.IsNone())
	{
		if (VisitedThisTraversal.Contains(NextNodeID))
		{
			UE_LOG(LogDialogue, Error, TEXT("Fallback loop reached node '%s' twice; ending conversation."),
				*NextNodeID.ToString());
			EndConversation();
			return false;
		}
		VisitedThisTraversal.Add(NextNodeID);

		const FDialogueNodeRow* Node = Nodes.Find(NextNodeID);
		if (!Node)
		{
			UE_LOG(LogDialogue, Warning, TEXT("No dialogue node '%s'; ending conversation."),
				*NextNodeID.ToString());
			EndConversation();
			return false;
		}

		EDialogueGate EntryGate = EDialogueGate::None;
		if (EvaluateConditions(Node->EntryConditions, CurrentNpcID, CurrentSafety, EntryGate))
		{
			CurrentNodeID = NextNodeID;

			const FDialogueEffects EntryEffects = Node->EntryEffects;
			ApplyEffects(EntryEffects, CurrentNpcID);

			OnNodeEntered.Broadcast(CurrentNodeID);
			return true;
		}

		// Unmet entry conditions divert rather than fail, which is how one NPC
		// greets a returning player differently without a duplicated tree.
		NextNodeID = Node->FallbackNodeID;
	}

	EndConversation();
	return false;
}

bool UDialogueSubsystem::StartConversation(FName NpcID, FName RootNodeID, EConversationSafety Safety)
{
	if (bConversationActive)
	{
		EndConversation();
	}

	if (NpcID.IsNone() || !Nodes.Contains(RootNodeID))
	{
		UE_LOG(LogDialogue, Warning, TEXT("Cannot start a conversation with '%s' at node '%s'."),
			*NpcID.ToString(), *RootNodeID.ToString());
		return false;
	}

	bConversationActive = true;
	CurrentNpcID = NpcID;
	CurrentSafety = Safety;
	CurrentNodeID = NAME_None;

	OnConversationStarted.Broadcast(NpcID, RootNodeID);

	return EnterNode(RootNodeID);
}

bool UDialogueSubsystem::GetCurrentNodeView(FDialogueNodeView& OutView) const
{
	OutView = FDialogueNodeView();

	if (!bConversationActive)
	{
		return false;
	}

	const FDialogueNodeRow* Node = Nodes.Find(CurrentNodeID);
	if (!Node)
	{
		return false;
	}

	OutView.NodeID = Node->NodeID;
	OutView.SpeakerID = Node->SpeakerID;
	OutView.Line = Node->Line;
	OutView.bIsTerminal = true;

	for (int32 Index = 0; Index < Node->Options.Num(); ++Index)
	{
		const FDialogueOption& Option = Node->Options[Index];

		EDialogueGate Gate = EDialogueGate::None;
		const bool bIsAvailable = EvaluateConditions(Option.Conditions, CurrentNpcID, CurrentSafety, Gate);

		if (!bIsAvailable && !Option.bShowWhenLocked)
		{
			continue;
		}

		if (bIsAvailable)
		{
			OutView.bIsTerminal = false;
		}

		FDialogueOptionView& OptionView = OutView.Options.AddDefaulted_GetRef();
		OptionView.OptionIndex = Index;
		OptionView.PlayerLine = Option.PlayerLine;
		OptionView.bIsAvailable = bIsAvailable;
		OptionView.Gate = Gate;
	}

	return true;
}

bool UDialogueSubsystem::SelectOption(int32 OptionIndex)
{
	if (!bConversationActive)
	{
		return false;
	}

	const FDialogueNodeRow* Node = Nodes.Find(CurrentNodeID);
	if (!Node || !Node->Options.IsValidIndex(OptionIndex))
	{
		return false;
	}

	// Copied because traversal moves off this node while the effects are applied.
	const FDialogueOption Option = Node->Options[OptionIndex];

	// The view is a presentation, never the authority: re-check before acting.
	EDialogueGate Gate = EDialogueGate::None;
	if (!EvaluateConditions(Option.Conditions, CurrentNpcID, CurrentSafety, Gate))
	{
		return false;
	}

	ApplyEffects(Option.Effects, CurrentNpcID);

	if (Option.NextNodeID.IsNone())
	{
		EndConversation();
	}
	else
	{
		// A missing target node ends the conversation and logs; the option was still
		// validly selected and its effects stand.
		EnterNode(Option.NextNodeID);
	}

	return true;
}

void UDialogueSubsystem::EndConversation()
{
	if (!bConversationActive)
	{
		return;
	}

	const FName EndedWithNpcID = CurrentNpcID;

	bConversationActive = false;
	CurrentNodeID = NAME_None;
	CurrentNpcID = NAME_None;
	CurrentSafety = EConversationSafety::Public;

	OnConversationEnded.Broadcast(EndedWithNpcID);
}

// --- Save/load --------------------------------------------------------------

FDialogueSaveData UDialogueSubsystem::CaptureSaveData() const
{
	FDialogueSaveData SaveData;
	SaveData.NpcStates = NpcStates;
	SaveData.GeneralVouchers = GeneralVouchers.Array();

	return SaveData;
}

void UDialogueSubsystem::RestoreFromSaveData(const FDialogueSaveData& SaveData)
{
	EndConversation();

	NpcStates = SaveData.NpcStates;

	GeneralVouchers.Reset();
	for (const FName VoucherID : SaveData.GeneralVouchers)
	{
		GeneralVouchers.Add(VoucherID);
	}
}
