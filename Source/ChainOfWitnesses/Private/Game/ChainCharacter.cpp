#include "Game/ChainCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "Game/ChainGameFlowSubsystem.h"
#include "Game/ChainNpc.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"

AChainCharacter::AChainCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.f);

	// The camera turns with the mouse; the man turns to face where he is walking.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.f, 540.f, 0.f);
	// A man on foot in armour, not a sprinter.
	GetCharacterMovement()->MaxWalkSpeed = 400.f;

	Boom = CreateDefaultSubobject<USpringArmComponent>(TEXT("Boom"));
	Boom->SetupAttachment(RootComponent);
	Boom->TargetArmLength = 400.f;
	Boom->bUsePawnControlRotation = true;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(Boom, USpringArmComponent::SocketName);
	Camera->bUsePawnControlRotation = false;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(
		TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(
		TEXT("/Engine/BasicShapes/Sphere.Sphere"));

	PlaceholderTorso = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlaceholderTorso"));
	PlaceholderTorso->SetupAttachment(RootComponent);
	PlaceholderTorso->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PlaceholderTorso->SetRelativeLocation(FVector(0.f, 0.f, -20.f));
	PlaceholderTorso->SetRelativeScale3D(FVector(0.45f, 0.45f, 0.75f));
	if (Cylinder.Succeeded())
	{
		PlaceholderTorso->SetStaticMesh(Cylinder.Object);
	}

	PlaceholderHead = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlaceholderHead"));
	PlaceholderHead->SetupAttachment(RootComponent);
	PlaceholderHead->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PlaceholderHead->SetRelativeLocation(FVector(0.f, 0.f, 70.f));
	PlaceholderHead->SetRelativeScale3D(FVector(0.35f, 0.35f, 0.35f));
	if (Sphere.Succeeded())
	{
		PlaceholderHead->SetStaticMesh(Sphere.Object);
	}
}

void AChainCharacter::BeginPlay()
{
	Super::BeginPlay();

	// A real character has been assigned, so the stand-in gets out of the way.
	const bool bHasRealMesh = GetMesh() && GetMesh()->GetSkeletalMeshAsset() != nullptr;
	if (bHasRealMesh)
	{
		PlaceholderTorso->SetVisibility(false);
		PlaceholderHead->SetVisibility(false);
	}
}

void AChainCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// Axis and action names come from Config/DefaultInput.ini. Enhanced Input would
	// need InputAction assets, which do not exist until somebody makes them in the
	// editor -- and this has to work on a fresh clone.
	PlayerInputComponent->BindAxis(TEXT("MoveForward"), this, &AChainCharacter::MoveForward);
	PlayerInputComponent->BindAxis(TEXT("MoveRight"), this, &AChainCharacter::MoveRight);
	PlayerInputComponent->BindAxis(TEXT("Turn"), this, &APawn::AddControllerYawInput);
	PlayerInputComponent->BindAxis(TEXT("LookUp"), this, &APawn::AddControllerPitchInput);
	PlayerInputComponent->BindAction(TEXT("Jump"), IE_Pressed, this, &ACharacter::Jump);
	PlayerInputComponent->BindAction(TEXT("Jump"), IE_Released, this, &ACharacter::StopJumping);
	PlayerInputComponent->BindAction(TEXT("Interact"), IE_Pressed, this, &AChainCharacter::Interact);
}

void AChainCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Interactable = FindInteractable();
}

AChainNpc* AChainCharacter::FindInteractable() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	AChainNpc* Nearest = nullptr;
	float NearestSquared = TNumericLimits<float>::Max();

	// A handful of placed NPCs per level; iterating them is cheaper than the
	// collision setup an overlap volume would need, and has nothing to configure.
	for (TActorIterator<AChainNpc> It(World); It; ++It)
	{
		AChainNpc* Npc = *It;
		const float DistanceSquared = FVector::DistSquared(GetActorLocation(), Npc->GetActorLocation());
		if (DistanceSquared <= FMath::Square(Npc->InteractionRange) && DistanceSquared < NearestSquared)
		{
			Nearest = Npc;
			NearestSquared = DistanceSquared;
		}
	}

	return Nearest;
}

void AChainCharacter::MoveForward(float Value)
{
	if (Value == 0.f || !Controller)
	{
		return;
	}

	// Movement is relative to where the camera is looking, not where the man faces.
	const FRotator Yaw(0.f, Controller->GetControlRotation().Yaw, 0.f);
	AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::X), Value);
}

void AChainCharacter::MoveRight(float Value)
{
	if (Value == 0.f || !Controller)
	{
		return;
	}

	const FRotator Yaw(0.f, Controller->GetControlRotation().Yaw, 0.f);
	AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y), Value);
}

void AChainCharacter::Interact()
{
	if (!Interactable)
	{
		return;
	}

	UGameInstance* GI = GetGameInstance();
	UChainGameFlowSubsystem* Flow = GI ? GI->GetSubsystem<UChainGameFlowSubsystem>() : nullptr;
	if (!Flow)
	{
		return;
	}

	Flow->StartConversation(Interactable->NpcID, Interactable->RootNodeID,
		Interactable->Safety);
}
