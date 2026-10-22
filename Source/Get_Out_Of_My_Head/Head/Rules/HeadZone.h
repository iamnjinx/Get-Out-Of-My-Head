#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HeadZone.generated.h"

class UBoxComponent;

/** A named area used by "물건이 구역에 있음" conditions (e.g. the 2층 방 zone) */
UCLASS()
class AHeadZone : public AActor
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UBoxComponent> Box;

public:

	AHeadZone();

	/** Name conditions refer to */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Zone")
	FName ZoneName;

	bool Contains(const FVector& Location) const;

	/** True when the location is inside any zone with the given name */
	static bool IsInZone(const UWorld* World, FName Name, const FVector& Location);
};
