#include "Game/ChainPlayerController.h"

#include "Codex/CodexSubsystem.h"
#include "Combat/CombatEncounterSubsystem.h"
#include "Combat/ChainCombatTypes.h"
#include "Debate/DebateSubsystem.h"
#include "Engine/GameInstance.h"
#include "Game/ChainGameFlowSubsystem.h"
#include "InputCoreTypes.h"
#include "Witness/WitnessMissionSubsystem.h"

namespace ChainControllerPrivate
{
	/** The slice Task 9 authored. Named once so the shortcut cannot drift from the content. */
	const TCHAR* const SliceMission = TEXT("WM_PeterDictatesMark");
}

AChainPlayerController::AChainPlayerController()
{
	bShowMouseCursor = false;
}

UChainGameFlowSubsystem* AChainPlayerController::Flow() const
{
	UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UChainGameFlowSubsystem>() : nullptr;
}

void AChainPlayerController::Echo(const FString& Line)
{
	UE_LOG(LogChainFlow, Log, TEXT("%s"), *Line);
	ClientMessage(Line);
}

void AChainPlayerController::EchoLines(const TArray<FString>& Lines)
{
	for (const FString& Line : Lines)
	{
		Echo(Line);
	}
}

void AChainPlayerController::BeginPlay()
{
	Super::BeginPlay();
	SetInputMode(FInputModeGameOnly());
}

void AChainPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (!InputComponent)
	{
		return;
	}

	InputComponent->BindKey(EKeys::One, IE_Pressed, this, &AChainPlayerController::OnOption1);
	InputComponent->BindKey(EKeys::Two, IE_Pressed, this, &AChainPlayerController::OnOption2);
	InputComponent->BindKey(EKeys::Three, IE_Pressed, this, &AChainPlayerController::OnOption3);
	InputComponent->BindKey(EKeys::Four, IE_Pressed, this, &AChainPlayerController::OnOption4);
	InputComponent->BindKey(EKeys::Five, IE_Pressed, this, &AChainPlayerController::OnOption5);
	InputComponent->BindKey(EKeys::Six, IE_Pressed, this, &AChainPlayerController::OnOption6);
	InputComponent->BindKey(EKeys::Seven, IE_Pressed, this, &AChainPlayerController::OnOption7);
	InputComponent->BindKey(EKeys::Eight, IE_Pressed, this, &AChainPlayerController::OnOption8);
	InputComponent->BindKey(EKeys::Nine, IE_Pressed, this, &AChainPlayerController::OnOption9);

	InputComponent->BindKey(EKeys::C, IE_Pressed, this, &AChainPlayerController::OnToggleCodex);
	InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &AChainPlayerController::OnBack);
	InputComponent->BindKey(EKeys::Tab, IE_Pressed, this, &AChainPlayerController::OnStatus);
	InputComponent->BindKey(EKeys::H, IE_Pressed, this, &AChainPlayerController::OnHelp);
}

void AChainPlayerController::SelectByDisplayNumber(int32 DisplayNumber)
{
	UChainGameFlowSubsystem* F = Flow();
	if (!F)
	{
		return;
	}

	// The screen is one-based because the keyboard is. The subsystems are zero-based
	// because arrays are. This is the one place the two meet.
	F->Say(DisplayNumber - 1);
	Echo(F->GetLastMessage());
}

void AChainPlayerController::OnOption1() { SelectByDisplayNumber(1); }
void AChainPlayerController::OnOption2() { SelectByDisplayNumber(2); }
void AChainPlayerController::OnOption3() { SelectByDisplayNumber(3); }
void AChainPlayerController::OnOption4() { SelectByDisplayNumber(4); }
void AChainPlayerController::OnOption5() { SelectByDisplayNumber(5); }
void AChainPlayerController::OnOption6() { SelectByDisplayNumber(6); }
void AChainPlayerController::OnOption7() { SelectByDisplayNumber(7); }
void AChainPlayerController::OnOption8() { SelectByDisplayNumber(8); }
void AChainPlayerController::OnOption9() { SelectByDisplayNumber(9); }

void AChainPlayerController::OnToggleCodex()
{
	if (UChainGameFlowSubsystem* F = Flow())
	{
		F->ToggleCodex();
	}
}

void AChainPlayerController::OnBack()
{
	if (UChainGameFlowSubsystem* F = Flow())
	{
		if (F->Back())
		{
			Echo(F->GetLastMessage());
		}
	}
}

void AChainPlayerController::OnStatus() { ChainStatus(); }
void AChainPlayerController::OnHelp() { ChainHelp(); }

// --- Console: campaign ------------------------------------------------------

void AChainPlayerController::ChainHelp()
{
	Echo(TEXT("--- Chain of Witnesses ---"));
	Echo(TEXT("Keys:  1-9 say a line   C codex   Esc back   Tab status   H help"));
	Echo(TEXT("ChainNew                      open a campaign in the frame era"));
	Echo(TEXT("ChainStatus                   where and when you are, and what you hold"));
	Echo(TEXT("ChainCodex                    the recovered testimony and what it scores"));
	Echo(TEXT("ChainMissions                 reconstructions open to you now"));
	Echo(TEXT("ChainOpen <MissionID>         enter a mission and open its scene"));
	Echo(TEXT("ChainSlice                    jump into Rome AD 65 (grants the source)"));
	Echo(TEXT("ChainComplete                 finish the mission and return to the frame"));
	Echo(TEXT("ChainTalk <Npc> <Node>        start a conversation"));
	Echo(TEXT("ChainSay <n>                  say the nth line on screen"));
	Echo(TEXT("ChainTravel <LocationID>      set a destination"));
	Echo(TEXT("ChainWait <days>              move the clock and the party"));
	Echo(TEXT("ChainRecover <FragmentID>     grant a fragment (testing)"));
	Echo(TEXT("ChainDebate <Opp> <Anchor>    open an argument"));
	Echo(TEXT("ChainAnswer                   answer with the whole chain"));
	Echo(TEXT("ChainAnswerWith <FragmentID>  answer with one fragment"));
	Echo(TEXT("ChainConcede                  give up the point"));
	Echo(TEXT("ChainFight <EncounterID>      start an encounter"));
	Echo(TEXT("ChainPosture <0-3>            undrawn, drawn, engaged, withdrawing"));
	Echo(TEXT("ChainStrike <index>           strike a combatant"));
	Echo(TEXT("ChainEscape                   try to get out"));
	Echo(TEXT("ChainTalkDown                 try to de-escalate"));
}

void AChainPlayerController::ChainStatus()
{
	if (UChainGameFlowSubsystem* F = Flow())
	{
		TArray<FString> Lines;
		F->GetStatusLines(Lines);
		EchoLines(Lines);
	}
}

void AChainPlayerController::ChainNew()
{
	if (UChainGameFlowSubsystem* F = Flow())
	{
		F->StartNewCampaign();
		Echo(F->GetLastMessage());
	}
}

void AChainPlayerController::ChainCodex()
{
	if (UChainGameFlowSubsystem* F = Flow())
	{
		TArray<FString> Lines;
		F->GetCodexLines(Lines);
		EchoLines(Lines);
	}
}

void AChainPlayerController::ChainMissions()
{
	UChainGameFlowSubsystem* F = Flow();
	UWitnessMissionSubsystem* Witness = F ? F->GetWitness() : nullptr;
	if (!Witness)
	{
		Echo(TEXT("No witness mission subsystem."));
		return;
	}

	TArray<FName> Available;
	Witness->GetAvailableMissionIDs(Available);

	Echo(FString::Printf(TEXT("--- Open to you now (%d) ---"), Available.Num()));
	if (Available.Num() == 0)
	{
		Echo(TEXT("None. Recover the sources that open them."));
	}
	for (const FName& MissionID : Available)
	{
		Echo(FString::Printf(TEXT("  %s"), *MissionID.ToString()));
	}
}

void AChainPlayerController::ChainOpen(const FString& MissionID)
{
	if (UChainGameFlowSubsystem* F = Flow())
	{
		F->OpenMission(FName(*MissionID));
		Echo(F->GetLastMessage());
	}
}

void AChainPlayerController::ChainSlice()
{
	if (UChainGameFlowSubsystem* F = Flow())
	{
		F->ForceOpenMission(FName(ChainControllerPrivate::SliceMission));
		Echo(F->GetLastMessage());
	}
}

void AChainPlayerController::ChainComplete()
{
	if (UChainGameFlowSubsystem* F = Flow())
	{
		F->CompleteMission();
		Echo(F->GetLastMessage());
	}
}

// --- Console: dialogue ------------------------------------------------------

void AChainPlayerController::ChainTalk(const FString& NpcID, const FString& RootNodeID)
{
	if (UChainGameFlowSubsystem* F = Flow())
	{
		F->StartConversation(FName(*NpcID), FName(*RootNodeID));
		Echo(F->GetLastMessage());
	}
}

void AChainPlayerController::ChainSay(int32 DisplayNumber)
{
	SelectByDisplayNumber(DisplayNumber);
}

// --- Console: travel --------------------------------------------------------

void AChainPlayerController::ChainTravel(const FString& LocationID)
{
	if (UChainGameFlowSubsystem* F = Flow())
	{
		F->TravelTo(FName(*LocationID));
		Echo(F->GetLastMessage());
	}
}

void AChainPlayerController::ChainWait(int32 Days)
{
	if (UChainGameFlowSubsystem* F = Flow())
	{
		F->AdvanceDays(Days);
		Echo(F->GetLastMessage());
	}
}

// --- Console: evidence ------------------------------------------------------

void AChainPlayerController::ChainRecover(const FString& FragmentID)
{
	UChainGameFlowSubsystem* F = Flow();
	UCodexSubsystem* Codex = F ? F->GetCodex() : nullptr;
	if (!Codex)
	{
		Echo(TEXT("No codex subsystem."));
		return;
	}

	const FName ID(*FragmentID);
	if (!Codex->IsFragmentRegistered(ID))
	{
		Echo(FString::Printf(TEXT("No such fragment: %s"), *FragmentID));
		return;
	}

	Echo(Codex->RecoverFragment(ID)
		? FString::Printf(TEXT("Recovered %s."), *FragmentID)
		: FString::Printf(TEXT("%s was already in hand."), *FragmentID));
}

// --- Console: debate --------------------------------------------------------

void AChainPlayerController::ChainDebate(const FString& OpponentID, const FString& AnchorEventID)
{
	if (UChainGameFlowSubsystem* F = Flow())
	{
		F->StartDebate(FName(*OpponentID), FName(*AnchorEventID));
		Echo(F->GetLastMessage());
	}
}

void AChainPlayerController::ChainAnswer()
{
	UChainGameFlowSubsystem* F = Flow();
	UDebateSubsystem* Debate = F ? F->GetDebate() : nullptr;
	if (!Debate || !Debate->IsDebateActive())
	{
		Echo(TEXT("No argument is running."));
		return;
	}

	Echo(Debate->RespondWithChain()
		? TEXT("You answer with the chain.")
		: TEXT("The chain will not carry that objection."));
}

void AChainPlayerController::ChainAnswerWith(const FString& FragmentID)
{
	UChainGameFlowSubsystem* F = Flow();
	UDebateSubsystem* Debate = F ? F->GetDebate() : nullptr;
	if (!Debate || !Debate->IsDebateActive())
	{
		Echo(TEXT("No argument is running."));
		return;
	}

	Echo(Debate->RespondWithFragment(FName(*FragmentID))
		? FString::Printf(TEXT("You answer with %s."), *FragmentID)
		: FString::Printf(TEXT("%s will not answer that."), *FragmentID));
}

void AChainPlayerController::ChainConcede()
{
	UChainGameFlowSubsystem* F = Flow();
	UDebateSubsystem* Debate = F ? F->GetDebate() : nullptr;
	if (!Debate || !Debate->IsDebateActive())
	{
		Echo(TEXT("No argument is running."));
		return;
	}

	Echo(Debate->ConcedePoint() ? TEXT("You concede the point.") : TEXT("You cannot concede that."));
}

// --- Console: combat --------------------------------------------------------

void AChainPlayerController::ChainFight(const FString& EncounterTypeID)
{
	if (UChainGameFlowSubsystem* F = Flow())
	{
		F->StartCombat(FName(*EncounterTypeID));
		Echo(F->GetLastMessage());
	}
}

void AChainPlayerController::ChainPosture(int32 Posture)
{
	UChainGameFlowSubsystem* F = Flow();
	UCombatEncounterSubsystem* Combat = F ? F->GetCombat() : nullptr;
	if (!Combat || !Combat->IsEncounterActive())
	{
		Echo(TEXT("You are not in a fight."));
		return;
	}

	const int32 Clamped = FMath::Clamp(Posture, 0, static_cast<int32>(ECombatPosture::MAX) - 1);
	const ECombatPosture NewPosture = static_cast<ECombatPosture>(Clamped);

	Echo(Combat->SetPosture(NewPosture)
		? FString::Printf(TEXT("Posture: %s"),
			*StaticEnum<ECombatPosture>()->GetNameStringByValue(Clamped))
		: TEXT("You cannot take that posture now."));
}

void AChainPlayerController::ChainStrike(int32 CombatantIndex)
{
	UChainGameFlowSubsystem* F = Flow();
	UCombatEncounterSubsystem* Combat = F ? F->GetCombat() : nullptr;
	if (!Combat || !Combat->IsEncounterActive())
	{
		Echo(TEXT("You are not in a fight."));
		return;
	}

	Combat->ApplyDamageToCombatant(CombatantIndex, 0.34f);
	Echo(Combat->HasHostilesRemaining()
		? FString::Printf(TEXT("You strike %d. Hostiles remain."), CombatantIndex)
		: TEXT("You strike, and nothing is left standing."));
}

void AChainPlayerController::ChainEscape()
{
	UChainGameFlowSubsystem* F = Flow();
	UCombatEncounterSubsystem* Combat = F ? F->GetCombat() : nullptr;
	if (!Combat || !Combat->IsEncounterActive())
	{
		Echo(TEXT("You are not in a fight."));
		return;
	}

	const FCombatAttemptResult Result = Combat->AttemptEscape(0.5f);
	Echo(FString::Printf(TEXT("Escape: %s (%.2f of %.2f). %s"),
		Result.bSucceeded ? TEXT("away") : TEXT("held"),
		Result.Achieved, Result.Required, *Result.Reason.ToString()));
}

void AChainPlayerController::ChainTalkDown()
{
	UChainGameFlowSubsystem* F = Flow();
	UCombatEncounterSubsystem* Combat = F ? F->GetCombat() : nullptr;
	if (!Combat || !Combat->IsEncounterActive())
	{
		Echo(TEXT("You are not in a fight."));
		return;
	}

	// What you can prove is what you can say with force. Using the campaign's own
	// attestation strength here is the design pillar made mechanical: the argument
	// is the progression system, and it is worth something outside a debate.
	const UCodexSubsystem* Codex = F->GetCodex();
	const float Persuasion = Codex ? Codex->GetCampaignAttestationStrength() : 0.f;

	const FCombatAttemptResult Result = Combat->AttemptDeEscalation(Persuasion);
	Echo(FString::Printf(TEXT("De-escalation: %s (%.2f of %.2f). %s"),
		Result.bSucceeded ? TEXT("they stand down") : TEXT("they are not moved"),
		Result.Achieved, Result.Required, *Result.Reason.ToString()));
}
