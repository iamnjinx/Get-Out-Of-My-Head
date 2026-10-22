#pragma once

#include "CoreMinimal.h"

class ACharacter;
class APlayerController;
class AHeadObject;
class UWorld;

/**
 *  Pointing rules shared by the controller (highlight, guide line) and the server (validation)
 *  - reach is measured from the body to the object's edge
 *  - walls between the body and the object block pointing, objects (large furniture included) do not
 *  - attached, held, flying and consumed objects cannot be pointed at
 */
namespace HeadInteraction
{
	/** True when the object's edge is within Range of the body */
	bool IsInReach(const ACharacter* Body, const AHeadObject* Object, float Range);

	/** True when no wall stands between the body and the object */
	bool HasLineOfSight(const ACharacter* Body, const AHeadObject* Object);

	/** Reach + line of sight + the object accepts being pointed at */
	bool IsTargetable(const ACharacter* Body, const AHeadObject* Object, float Range);

	/** Object under the screen-center crosshair, or null */
	AHeadObject* FindAimTarget(const APlayerController* PC, const ACharacter* Body, float Range);

	/** Point on the floor below Location, ignoring every object */
	FVector FindFloor(const UWorld* World, const FVector& Location, const AActor* Ignore = nullptr);
}
