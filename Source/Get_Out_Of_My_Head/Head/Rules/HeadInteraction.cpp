#include "HeadInteraction.h"
#include "HeadObject.h"
#include "HeadGameState.h"
#include "Get_Out_Of_My_HeadCharacter.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "EngineUtils.h"

namespace
{
	/** Ignores every object, so only the house itself (walls, floors) is hit */
	void IgnoreAllObjects(const UWorld* World, FCollisionQueryParams& Params)
	{
		for (TActorIterator<AHeadObject> It(World); It; ++It)
		{
			Params.AddIgnoredActor(*It);
		}
	}
}

bool HeadInteraction::IsInReach(const ACharacter* Body, const AHeadObject* Object, float Range)
{
	if (!Body || !Object)
	{
		return false;
	}

	// measured to the object's edge, so a big bed is reachable from its side
	const FBox Bounds = Object->GetObjectBounds();
	if (!Bounds.IsValid)
	{
		return false;
	}

	FVector Point = Body->GetActorLocation();
	Point.Z = FMath::Clamp(Point.Z, Bounds.Min.Z, Bounds.Max.Z);

	return FMath::Sqrt(Bounds.ComputeSquaredDistanceToPoint(Point)) <= Range;
}

bool HeadInteraction::HasLineOfSight(const ACharacter* Body, const AHeadObject* Object)
{
	if (!Body || !Object)
	{
		return false;
	}

	const UWorld* World = Body->GetWorld();

	FCollisionQueryParams Params(SCENE_QUERY_STAT(HeadLineOfSight));
	Params.AddIgnoredActor(Body);
	IgnoreAllObjects(World, Params);

	FHitResult Hit;
	const FVector Start = Body->GetActorLocation() + FVector(0.0f, 0.0f, 50.0f);
	const FVector End = Object->GetObjectBounds().GetCenter();

	return !World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params);
}

bool HeadInteraction::IsTargetable(const ACharacter* Body, const AHeadObject* Object, float Range)
{
	return Object && Object->CanBeTargeted() && IsInReach(Body, Object, Range) && HasLineOfSight(Body, Object);
}

AHeadObject* HeadInteraction::FindAimTarget(const APlayerController* PC, const ACharacter* Body, float Range)
{
	if (!PC || !PC->PlayerCameraManager || !Body)
	{
		return nullptr;
	}

	const FVector Start = PC->PlayerCameraManager->GetCameraLocation();
	const FVector Direction = PC->PlayerCameraManager->GetCameraRotation().Vector();

	// long enough to pass the camera boom and still reach past the body
	const float Length = FVector::Dist(Start, Body->GetActorLocation()) + Range + 100.0f;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(HeadAim));
	Params.AddIgnoredActor(Body);

	// the held object and what is inside it are never the target
	TArray<AActor*> AttachedActors;
	Body->GetAttachedActors(AttachedActors, true, true);
	Params.AddIgnoredActors(AttachedActors);

	TArray<FHitResult> Hits;
	Body->GetWorld()->SweepMultiByChannel(Hits, Start, Start + Direction * Length, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(8.0f), Params);

	for (const FHitResult& Hit : Hits)
	{
		AHeadObject* Object = Cast<AHeadObject>(Hit.GetActor());
		if (!Object)
		{
			// the crosshair hit the house before any object
			if (Hit.bBlockingHit)
			{
				return nullptr;
			}
			continue;
		}

		if (Object->IsHeld())
		{
			continue;
		}

		return IsTargetable(Body, Object, Range) ? Object : nullptr;
	}

	return nullptr;
}

bool HeadInteraction::GetLocalViewerLocation(const UWorld* World, FVector& OutLocation)
{
	const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	if (!PC || !PC->IsLocalController() || !PC->PlayerCameraManager)
	{
		return false;
	}

	const AHeadGameState* GS = World->GetGameState<AHeadGameState>();
	if (GS && GS->Body && PC->GetPawn() != GS->Body)
	{
		OutLocation = GS->Body->GetHeadCapture()->GetComponentLocation();
	}
	else
	{
		OutLocation = PC->PlayerCameraManager->GetCameraLocation();
	}

	return true;
}

FVector HeadInteraction::FindFloor(const UWorld* World, const FVector& Location, const AActor* Ignore)
{
	FCollisionQueryParams Params(SCENE_QUERY_STAT(HeadFloor));
	IgnoreAllObjects(World, Params);
	if (Ignore)
	{
		Params.AddIgnoredActor(Ignore);
	}

	FHitResult Hit;
	if (World->LineTraceSingleByChannel(Hit, Location + FVector(0.0f, 0.0f, 50.0f), Location - FVector(0.0f, 0.0f, 2000.0f), ECC_Visibility, Params))
	{
		return Hit.ImpactPoint;
	}

	return Location;
}
