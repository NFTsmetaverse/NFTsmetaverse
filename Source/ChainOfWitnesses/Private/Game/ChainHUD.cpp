#include "Game/ChainHUD.h"

#include "Combat/CombatEncounterSubsystem.h"
#include "Combat/ChainCombatTypes.h"
#include "Debate/DebateSubsystem.h"
#include "Debate/ChainDebateTypes.h"
#include "Dialogue/DialogueSubsystem.h"
#include "Dialogue/ChainDialogueTypes.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Game/ChainCharacter.h"
#include "Game/ChainGameFlowSubsystem.h"
#include "Game/ChainNpc.h"
#include "UI/CodexViewLibrary.h"

namespace ChainHUDPrivate
{
	const FLinearColor Dim(0.55f, 0.55f, 0.58f, 1.f);
	const FLinearColor Speaker(0.93f, 0.86f, 0.66f, 1.f);
	const FLinearColor Heading(0.72f, 0.82f, 0.95f, 1.f);
	const FLinearColor Locked(0.45f, 0.42f, 0.40f, 1.f);
	const FLinearColor Alarm(0.92f, 0.55f, 0.45f, 1.f);

	constexpr int32 WrapColumn = 108;

	template <typename TEnum>
	FString EnumName(TEnum Value)
	{
		if (const UEnum* Enum = StaticEnum<TEnum>())
		{
			return Enum->GetNameStringByValue(static_cast<int64>(Value));
		}
		return TEXT("?");
	}
}

UChainGameFlowSubsystem* AChainHUD::Flow() const
{
	if (const APlayerController* PC = GetOwningPlayerController())
	{
		if (UGameInstance* GI = PC->GetGameInstance())
		{
			return GI->GetSubsystem<UChainGameFlowSubsystem>();
		}
	}
	return nullptr;
}

void AChainHUD::Line(const FString& Text, const FLinearColor& Colour)
{
	UFont* Font = GEngine ? GEngine->GetMediumFont() : nullptr;
	DrawText(Text, Colour, Margin, CursorY, Font);
	CursorY += LineHeight;
}

void AChainHUD::Blank()
{
	CursorY += LineHeight * 0.6f;
}

void AChainHUD::Wrapped(const FString& Text, const FLinearColor& Colour)
{
	if (Text.Len() <= ChainHUDPrivate::WrapColumn)
	{
		Line(Text, Colour);
		return;
	}

	FString Remaining = Text;
	while (Remaining.Len() > ChainHUDPrivate::WrapColumn)
	{
		int32 Break = INDEX_NONE;
		for (int32 i = ChainHUDPrivate::WrapColumn; i > 0; --i)
		{
			if (Remaining[i] == TEXT(' '))
			{
				Break = i;
				break;
			}
		}

		// A single word longer than the column: cut it rather than loop forever.
		if (Break == INDEX_NONE)
		{
			Break = ChainHUDPrivate::WrapColumn;
		}

		Line(Remaining.Left(Break), Colour);
		Remaining = Remaining.RightChop(Break + 1);
	}

	if (!Remaining.IsEmpty())
	{
		Line(Remaining, Colour);
	}
}

void AChainHUD::DrawHUD()
{
	Super::DrawHUD();

	UChainGameFlowSubsystem* F = Flow();
	if (!F || !Canvas)
	{
		return;
	}

	UFont* Font = GEngine ? GEngine->GetMediumFont() : nullptr;
	const EChainScreen Screen = F->GetScreen();

	// The codex is the one screen that earns the whole viewport: it is a document
	// you stop and read, not something you do while watching the world.
	if (Screen == EChainScreen::Codex)
	{
		DrawRect(FLinearColor(0.03f, 0.03f, 0.05f, 0.94f), 0.f, 0.f, Canvas->SizeX, Canvas->SizeY);
		CursorY = Margin;
		DrawCodex();
		DrawText(TEXT("C to close"), ChainHUDPrivate::Dim,
			Margin, Canvas->SizeY - Margin - LineHeight, Font);
		return;
	}

	// Everything else is drawn over the game, not instead of it. A full-screen
	// backdrop here was right when there was nothing behind it and wrong the moment
	// there was: it hid the world it was supposed to annotate.
	TArray<FString> Status;
	F->GetStatusLines(Status);

	const float StatusHeight = LineHeight * (Status.Num() + 1);
	DrawRect(FLinearColor(0.02f, 0.02f, 0.04f, 0.55f), 0.f, 0.f, Canvas->SizeX, StatusHeight);

	CursorY = LineHeight * 0.4f;
	for (const FString& StatusLine : Status)
	{
		Line(StatusLine, ChainHUDPrivate::Heading);
	}

	// The body sits in a band across the bottom, the way a subtitle does.
	const float BodyTop = Canvas->SizeY * 0.58f;
	const float FooterY = Canvas->SizeY - Margin - LineHeight;

	if (Screen != EChainScreen::Campaign)
	{
		DrawRect(FLinearColor(0.02f, 0.02f, 0.04f, 0.78f),
			0.f, BodyTop, Canvas->SizeX, Canvas->SizeY - BodyTop);
	}

	CursorY = BodyTop + LineHeight * 0.6f;

	switch (Screen)
	{
	case EChainScreen::Combat:   DrawCombat();   break;
	case EChainScreen::Debate:   DrawDebate();   break;
	case EChainScreen::Dialogue: DrawDialogue(); break;
	default:                     DrawCampaign(); break;
	}

	const FString Message = F->GetLastMessage();
	if (!Message.IsEmpty())
	{
		DrawText(Message, ChainHUDPrivate::Speaker, Margin, FooterY - LineHeight, Font);
	}
	DrawText(TEXT("WASD move    E talk    1-9 speak    C codex    Esc back    H help    ~ console"),
		ChainHUDPrivate::Dim, Margin, FooterY, Font);
}

void AChainHUD::DrawCampaign()
{
	const AChainCharacter* Player = Cast<AChainCharacter>(GetOwningPawn());
	const AChainNpc* Near = Player ? Player->GetInteractable() : nullptr;

	if (!Near)
	{
		return;
	}

	// Only drawn when there is somebody to talk to, so an empty road stays empty.
	UFont* Font = GEngine ? GEngine->GetMediumFont() : nullptr;
	const FString Prompt = FString::Printf(TEXT("%s   --   E to speak"),
		*Near->GetLabel().ToString());
	DrawText(Prompt, ChainHUDPrivate::Speaker, Margin, Canvas->SizeY * 0.52f, Font);
}

void AChainHUD::DrawDialogue()
{
	UChainGameFlowSubsystem* F = Flow();
	UDialogueSubsystem* Dialogue = F ? F->GetDialogue() : nullptr;
	if (!Dialogue)
	{
		return;
	}

	FDialogueNodeView View;
	if (!Dialogue->GetCurrentNodeView(View))
	{
		return;
	}

	Line(FString::Printf(TEXT("%s  [%s]"),
		*View.SpeakerID.ToString(),
		*ChainHUDPrivate::EnumName(Dialogue->GetDisclosure(View.SpeakerID))),
		ChainHUDPrivate::Speaker);
	Blank();
	Wrapped(View.Line.ToString());
	Blank();

	for (const FDialogueOptionView& Option : View.Options)
	{
		const int32 DisplayNumber = Option.OptionIndex + 1;
		if (Option.bIsAvailable)
		{
			Wrapped(FString::Printf(TEXT("  [%d] %s"), DisplayNumber, *Option.PlayerLine.ToString()));
		}
		else
		{
			Wrapped(FString::Printf(TEXT("  [-] %s   (%s)"),
				*Option.PlayerLine.ToString(),
				*ChainHUDPrivate::EnumName(Option.Gate)),
				ChainHUDPrivate::Locked);
		}
	}

	if (View.bIsTerminal)
	{
		Blank();
		Line(TEXT("Nothing more to say. Esc to take your leave."), ChainHUDPrivate::Dim);
	}
}

void AChainHUD::DrawDebate()
{
	UChainGameFlowSubsystem* F = Flow();
	UDebateSubsystem* Debate = F ? F->GetDebate() : nullptr;
	if (!Debate)
	{
		return;
	}

	const FDebateState State = Debate->GetDebateState();
	Line(FString::Printf(TEXT("%s  |  round %d of %d  |  conviction %.2f"),
		*State.OpponentID.ToString(), State.RoundIndex + 1, State.MaxRounds, State.Conviction),
		ChainHUDPrivate::Speaker);
	Blank();

	FDebateObjectionView Objection;
	if (Debate->GetCurrentObjection(Objection))
	{
		Wrapped(Objection.Statement.ToString());
		Blank();
		Line(FString::Printf(TEXT("Attacks: %s   your defence %.2f against %.2f required"),
			*UCodexViewLibrary::GetLinkName(Objection.TargetsLink).ToString(),
			Objection.ChainDefence, Objection.RequiredLinkStrength),
			Objection.ChainDefence >= Objection.RequiredLinkStrength
				? ChainHUDPrivate::Dim
				: ChainHUDPrivate::Alarm);

		if (Objection.bLinkHasUnansweredChallenge)
		{
			Line(TEXT("That link carries a challenge you have not answered."), ChainHUDPrivate::Alarm);
		}
	}

	Blank();
	Line(TEXT("ChainAnswer   ChainAnswerWith <FragmentID>   ChainConcede   Esc break off"),
		ChainHUDPrivate::Dim);
}

void AChainHUD::DrawCombat()
{
	UChainGameFlowSubsystem* F = Flow();
	UCombatEncounterSubsystem* Combat = F ? F->GetCombat() : nullptr;
	if (!Combat)
	{
		return;
	}

	const FCombatEncounterState State = Combat->GetEncounterState();
	Line(FString::Printf(TEXT("%s"), *State.EncounterTypeID.ToString()), ChainHUDPrivate::Speaker);
	Blank();
	Line(FString::Printf(TEXT("Health %.0f%%   posture %s"),
		State.PlayerHealthNormalised * 100.f,
		*ChainHUDPrivate::EnumName(State.Posture)),
		State.PlayerHealthNormalised < 0.34f ? ChainHUDPrivate::Alarm : FLinearColor::White);

	Line(Combat->HasHostilesRemaining()
		? TEXT("Hostiles are still standing.")
		: TEXT("Nothing is left standing."), ChainHUDPrivate::Dim);

	if (State.FailedEscapeAttempts > 0 || State.FailedDeEscalationAttempts > 0)
	{
		Line(FString::Printf(TEXT("Failed: %d escapes, %d appeals"),
			State.FailedEscapeAttempts, State.FailedDeEscalationAttempts), ChainHUDPrivate::Dim);
	}

	Blank();
	Line(TEXT("ChainPosture <0-3>   ChainStrike <i>   ChainEscape   ChainTalkDown"),
		ChainHUDPrivate::Dim);
}

void AChainHUD::DrawCodex()
{
	UChainGameFlowSubsystem* F = Flow();
	if (!F)
	{
		return;
	}

	TArray<FString> Lines;
	F->GetCodexLines(Lines);
	for (const FString& CodexLine : Lines)
	{
		Wrapped(CodexLine, CodexLine.StartsWith(TEXT("---"))
			? ChainHUDPrivate::Heading
			: FLinearColor::White);
	}

	Blank();
	Line(TEXT("C to close."), ChainHUDPrivate::Dim);
}
