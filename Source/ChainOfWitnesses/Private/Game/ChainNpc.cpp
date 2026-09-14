#include "Game/ChainNpc.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

AChainNpc::AChainNpc()
{
	PrimaryActorTick.bCanEverTick = false;

	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	SetRootComponent(Body);

	// An engine primitive, so a placed NPC is visible in a project that owns no art.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(
		TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (Cylinder.Succeeded())
	{
		Body->SetStaticMesh(Cylinder.Object);
		// Roughly a person: 1m across at the base is a barrel, not a man.
		Body->SetRelativeScale3D(FVector(0.5f, 0.5f, 0.9f));
	}
}

FText AChainNpc::GetLabel() const
{
	if (!DisplayName.IsEmpty())
	{
		return DisplayName;
	}
	return FText::FromName(NpcID);
}
