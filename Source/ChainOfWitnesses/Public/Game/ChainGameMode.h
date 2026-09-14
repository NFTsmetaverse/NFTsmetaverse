#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ChainGameMode.generated.h"

/**
 * The entry point: what running the project actually starts.
 *
 * It deliberately has almost no behaviour. Its job is to name the controller and
 * the HUD, and to say loudly in the log if the content never registered -- because
 * an empty registry and a working game look identical from a black screen, and the
 * log line is the only difference a developer can act on.
 *
 * There is no default pawn. The game this module implements is a campaign layer:
 * conversations, travel, evidence and argument. A walking character belongs to the
 * level work that happens in the editor, not here.
 */
UCLASS()
class CHAINOFWITNESSES_API AChainGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AChainGameMode();

protected:
	virtual void BeginPlay() override;
};
