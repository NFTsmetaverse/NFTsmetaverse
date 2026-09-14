#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "ChainCharacter.generated.h"

class AChainNpc;
class UCameraComponent;
class USpringArmComponent;

/**
 * The body the player moves.
 *
 * Third person, because the frame narrative is about a man carrying documents
 * across a hostile map and you need to see him to believe that.
 *
 * It also does the work the dialogue system could never do for itself: finding
 * somebody to talk to. The rules take an NpcID; a player has a direction they are
 * facing and a person standing in front of them. This closes that gap.
 */
UCLASS()
class CHAINOFWITNESSES_API AChainCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AChainCharacter();

	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	/** Whoever is close enough to speak to, or null. Read by the HUD for its prompt. */
	UFUNCTION(BlueprintPure, Category = "Chain|Character")
	AChainNpc* GetInteractable() const { return Interactable; }

	UFUNCTION(BlueprintCallable, Category = "Chain|Character")
	void Interact();

protected:
	void MoveForward(float Value);
	void MoveRight(float Value);

	/** Nearest NPC within its own InteractionRange, or null. */
	AChainNpc* FindInteractable() const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chain|Character")
	TObjectPtr<USpringArmComponent> Boom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chain|Character")
	TObjectPtr<UCameraComponent> Camera;

private:
	UPROPERTY()
	TObjectPtr<AChainNpc> Interactable = nullptr;
};
