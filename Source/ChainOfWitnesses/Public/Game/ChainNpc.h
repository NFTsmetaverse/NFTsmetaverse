#pragma once

#include "CoreMinimal.h"
#include "Dialogue/ChainDialogueTypes.h"
#include "GameFramework/Actor.h"
#include "ChainNpc.generated.h"

class UStaticMeshComponent;

/**
 * Someone in the world worth talking to.
 *
 * The dialogue system has always worked by ID: a conversation is opened by naming
 * an NPC and a root node. That is right for the rules and useless for a player,
 * who has no way to name anyone. This actor is the missing half -- it puts an ID
 * somewhere you can walk up to.
 *
 * The mesh is a placeholder primitive on purpose. Swapping it for a real character
 * is a property change in the details panel, and nothing here depends on what it
 * looks like.
 */
UCLASS()
class CHAINOFWITNESSES_API AChainNpc : public AActor
{
	GENERATED_BODY()

public:
	AChainNpc();

	/** Matches the NpcID the dialogue content and reputation profiles use. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chain|NPC")
	FName NpcID;

	/** The conversation this person opens with. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chain|NPC")
	FName RootNodeID;

	/** What the player sees over them before they know the name. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chain|NPC")
	FText DisplayName;

	/** How close you have to be, in centimetres. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chain|NPC")
	float InteractionRange = 250.f;

	/**
	 * Public and private speech are different conversations in this design -- a man
	 * says less where he can be overheard -- so a placed NPC declares which he is.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chain|NPC")
	EConversationSafety Safety = EConversationSafety::Public;

	UFUNCTION(BlueprintPure, Category = "Chain|NPC")
	FText GetLabel() const;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chain|NPC")
	TObjectPtr<UStaticMeshComponent> Body;
};
