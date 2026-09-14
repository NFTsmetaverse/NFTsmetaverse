#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "ChainHUD.generated.h"

class UChainGameFlowSubsystem;

/**
 * Draws the game.
 *
 * This is a canvas HUD rather than UMG, for one reason: UMG needs widget Blueprints,
 * and a widget Blueprint only exists after someone has opened the editor and drawn
 * it. A canvas HUD compiles into the module and runs the moment the project does,
 * which makes the difference between a game that can be played and a game that can
 * be played once the art is in.
 *
 * It renders whatever screen the flow subsystem says is current, and it reads its
 * text from the same view structs the eventual UMG will read. Replacing it with
 * real widgets is a presentation change, not a rewrite: nothing here holds state.
 */
UCLASS()
class CHAINOFWITNESSES_API AChainHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

private:
	UChainGameFlowSubsystem* Flow() const;

	/** Draws one line at the running cursor and moves it down. */
	void Line(const FString& Text, const FLinearColor& Colour = FLinearColor::White);

	/** Breaks a long line on spaces so a paragraph of dialogue stays on screen. */
	void Wrapped(const FString& Text, const FLinearColor& Colour = FLinearColor::White);

	void Blank();

	void DrawCampaign();
	void DrawDialogue();
	void DrawDebate();
	void DrawCombat();
	void DrawCodex();

	float CursorY = 0.f;
	float LineHeight = 18.f;
	float Margin = 24.f;
};
