#include "HeadZone.h"
#include "Components/BoxComponent.h"
#include "EngineUtils.h"

AHeadZone::AHeadZone()
{
	Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
	Box->SetBoxExtent(FVector(200.0f));
	Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RootComponent = Box;
}

bool AHeadZone::Contains(const FVector& Location) const
{
	const FVector Local = Box->GetComponentTransform().InverseTransformPosition(Location);
	const FVector Extent = Box->GetUnscaledBoxExtent();

	return FMath::Abs(Local.X) <= Extent.X && FMath::Abs(Local.Y) <= Extent.Y && FMath::Abs(Local.Z) <= Extent.Z;
}

bool AHeadZone::IsInZone(const UWorld* World, FName Name, const FVector& Location)
{
	for (TActorIterator<AHeadZone> It(World); It; ++It)
	{
		if (It->ZoneName == Name && It->Contains(Location))
		{
			return true;
		}
	}

	return false;
}
