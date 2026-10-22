#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HeadBodyComponent.generated.h"

class UHeadStatusData;
class AHeadObject;
class UCharacterMovementComponent;

/** A status on the body */
USTRUCT()
struct FHeadStatusInstance
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UHeadStatusData> Status;

	UPROPERTY()
	int32 Stacks = 0;

	/** Server time of the next expiry, 0 when permanent */
	UPROPERTY()
	double ExpireTime = 0.0;
};

DECLARE_MULTICAST_DELEGATE(FOnHeadBodyFell);

/**
 *  State that belongs to the shared body, not to a player
 *  Statuses (only the controller sees them), the hand, and falling.
 *  Statuses survive turns: the next player receives the body as it was left.
 */
UCLASS(ClassGroup=(Head), meta=(BlueprintSpawnableComponent))
class UHeadBodyComponent : public UActorComponent
{
	GENERATED_BODY()

	/** Statuses, replicated to the controller only */
	UPROPERTY(Replicated)
	TArray<FHeadStatusInstance> Statuses;

	/** Server time until which the body lies on the floor */
	UPROPERTY(Replicated)
	double FallenUntil = 0.0;

	/** Object in hand (one at most) */
	UPROPERTY(Replicated)
	TObjectPtr<AHeadObject> HeldObject;

	/** Walk speed without statuses */
	float BaseWalkSpeed = 0.0f;

	/** Server: was moving last frame, for the fall roll when movement starts */
	bool bWasMoving = false;

	/** Server: seconds moved since the last fall roll */
	float MoveTimeSinceRoll = 0.0f;

public:

	UHeadBodyComponent();

	/** Seconds the body cannot move after falling */
	UPROPERTY(EditAnywhere, Category="Head|Body")
	float FallSeconds = 3.0f;

	/** Seconds between fall rolls while moving */
	UPROPERTY(EditAnywhere, Category="Head|Body")
	float FallRollInterval = 1.0f;

	/** Broadcast on the server when the body falls */
	FOnHeadBodyFell OnFell;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Server: adds stacks. Non-stackable statuses restart their duration instead */
	void AddStatus(UHeadStatusData* Status, int32 Stacks = 1);

	/** Server: removes the status entirely */
	void RemoveStatus(UHeadStatusData* Status);

	/** Server: removes every status and gets back on the feet, for a new round */
	void ResetBody();

	/** Returns current stacks, 0 when absent. Valid on the server and the controller */
	int32 GetStacks(const UHeadStatusData* Status) const;

	const TArray<FHeadStatusInstance>& GetStatuses() const { return Statuses; }

	bool IsFallen() const;

	/** True when statuses or a fall keep the body still */
	bool IsImmobile() const;

	/** Product of every status speed multiplier */
	float GetSpeedMultiplier() const;

	AHeadObject* GetHeldObject() const { return HeldObject; }

	/** Server: sets what is in hand (pick up / drop are handled by the interaction code) */
	void SetHeldObject(AHeadObject* NewHeld);

protected:

	double Now() const;

	/** Applies speed and immobility to the movement component */
	void UpdateMovement();

	/** Server: expires stacks whose time is up */
	void UpdateExpiry();

	/** Server: rolls falls while moving */
	void UpdateFalling(float DeltaTime);

	/** Combined chance of falling from every status */
	float GetFallChance() const;

	void Fall();

	UCharacterMovementComponent* GetMovement() const;
};
