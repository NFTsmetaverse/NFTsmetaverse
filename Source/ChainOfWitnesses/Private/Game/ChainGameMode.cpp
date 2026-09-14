#include "Game/ChainGameMode.h"

#include "Bootstrap/ChainBootstrapSubsystem.h"
#include "Engine/GameInstance.h"
#include "Game/ChainGameFlowSubsystem.h"
#include "Game/ChainHUD.h"
#include "Game/ChainPlayerController.h"
#include "GameFramework/SpectatorPawn.h"

AChainGameMode::AChainGameMode()
{
	PlayerControllerClass = AChainPlayerController::StaticClass();
	HUDClass = AChainHUD::StaticClass();

	// A spectator pawn rather than none: the player still needs a view, and this
	// avoids the possess-nothing warnings that make a clean log hard to read.
	DefaultPawnClass = ASpectatorPawn::StaticClass();
}

void AChainGameMode::BeginPlay()
{
	Super::BeginPlay();

	UGameInstance* GI = GetGameInstance();
	if (!GI)
	{
		return;
	}

	const UChainBootstrapSubsystem* Bootstrap = GI->GetSubsystem<UChainBootstrapSubsystem>();
	if (!Bootstrap || !Bootstrap->IsContentRegistered())
	{
		UE_LOG(LogChainFlow, Error,
			TEXT("No content is registered. The subsystems are empty registries and nothing will work. ")
			TEXT("Assign the data tables in Project Settings > Game > Chain of Witnesses Content, ")
			TEXT("or check that bRegisterContentOnStartup is on."));
		return;
	}

	if (UChainGameFlowSubsystem* Flow = GI->GetSubsystem<UChainGameFlowSubsystem>())
	{
		TArray<FString> Status;
		Flow->GetStatusLines(Status);
		for (const FString& Line : Status)
		{
			UE_LOG(LogChainFlow, Log, TEXT("%s"), *Line);
		}
		UE_LOG(LogChainFlow, Log,
			TEXT("Ready. Open the console (~) and type ChainHelp, or press H."));
	}
}
