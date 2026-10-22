#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HeadObject.generated.h"

class UHeadObjectData;
class UStaticMeshComponent;
class UWidgetComponent;
class ACharacter;
struct FHeadRuleContext;

/**
 *  An object in the house. Everything it does comes from its UHeadObjectData.
 *  Public state (on/off, hold progress, attachments, in hand) replicates to everyone and shows on its name tag.
 */
UCLASS()
class AHeadObject : public AActor
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> Mesh;

	/** Name, state and progress above the object */
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UWidgetComponent> NameTag;

	/** What this object is */
	UPROPERTY(EditAnywhere, ReplicatedUsing=OnRep_ObjectData, Category="Head")
	TObjectPtr<UHeadObjectData> ObjectData;

	/** Toggle state (doors: open) */
	UPROPERTY(ReplicatedUsing=OnRep_PublicState)
	bool bOn = false;

	/** Hold progress, 0 to 1 */
	UPROPERTY(Replicated)
	float Progress = 0.0f;

	/** Interacted with at least once this round */
	UPROPERTY(Replicated)
	bool bInteracted = false;

	/** Gone after use, and not coming back */
	UPROPERTY(ReplicatedUsing=OnRep_PublicState)
	bool bConsumed = false;

	/** In the body's hand */
	UPROPERTY(ReplicatedUsing=OnRep_PublicState)
	bool bHeld = false;

	/** Thrown and still in the air */
	UPROPERTY(ReplicatedUsing=OnRep_PublicState)
	bool bFlying = false;

	/** Host this object is attached to */
	UPROPERTY(ReplicatedUsing=OnRep_PublicState)
	TObjectPtr<AHeadObject> AttachedTo;

	/** Objects attached to this one, last attached last */
	UPROPERTY(Replicated)
	TArray<TObjectPtr<AHeadObject>> Attachments;

	/** Where the object was first placed, for respawning */
	FTransform InitialTransform;

	/** Server: InitObject has run */
	bool bInitialized = false;

	/** Local only: pointed at by this machine's controller */
	bool bHighlighted = false;

	/** Server: throw flight */
	TArray<FVector> FlightPath;
	float FlightDuration = 0.0f;
	float FlightTime = 0.0f;
	bool bBreakOnLanding = false;

public:

	AHeadObject();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** Server: sets the data and the first state */
	void InitObject(UHeadObjectData* InData);

	UHeadObjectData* GetObjectData() const { return ObjectData; }

	bool IsOn() const { return bOn; }
	bool IsConsumed() const { return bConsumed; }
	bool IsHeld() const { return bHeld; }
	bool HasBeenInteracted() const { return bInteracted; }
	float GetProgress() const { return Progress; }
	AHeadObject* GetAttachedTo() const { return AttachedTo; }
	const TArray<TObjectPtr<AHeadObject>>& GetAttachments() const { return Attachments; }

	/** Bounds of the visible object, valid even while it has no collision (in hand) */
	FBox GetObjectBounds() const;

	/** True when an object using Data is directly attached to this one */
	bool HasAttachment(const UHeadObjectData* Data) const;

	/** True when this object or anything attached inside it uses Data */
	bool ContainsObjectData(const UHeadObjectData* Data) const;

	/** True when the crosshair may point at it (not held, attached, flying or gone) */
	bool CanBeTargeted() const;

	/** True when E can run its effects (in hand or in the world) */
	bool CanBeUsed() const;

	/** True for doors that are open */
	bool IsOpenDoor() const;

	/** Server: sets hold progress */
	void SetProgress(float NewProgress);

	/**
	 *  Server: runs the effects of a completed interaction
	 *  Flips the toggle, marks it interacted, resets the hold and disappears if it is consumed on use.
	 */
	void CompleteInteraction(const FHeadRuleContext& Context);

	/** Server: disappears, or comes back at its first place if it respawns */
	void Consume();

	/** Server: back to the first place and first state, for a new round */
	void ResetToInitial();

	/** Server: puts the object in the body's hand */
	void PickUp(ACharacter* Body);

	/** Server: leaves the hand (or a host) and lies at Location */
	void DropAt(const FVector& Location);

	/** Server: attaches this object to a host */
	void AttachToHost(AHeadObject* Host);

	/** Server: takes this object out of its host, keeping it where it is */
	void DetachFromHost();

	/** Server: leaves the hand and flies along Path */
	void StartThrow(const TArray<FVector>& Path, float Duration, bool bBreak);

	/** Local: highlight for the controller pointing at it */
	void SetHighlighted(bool bInHighlighted);

	/** Name tag text, rebuilt every frame */
	FText GetNameTagText() const;

protected:

	UFUNCTION()
	void OnRep_ObjectData();

	UFUNCTION()
	void OnRep_PublicState();

	/** Applies mesh, color, collision and visibility from data and state */
	void ApplyLook();

	/** Lines attachments up on top of the host */
	void LayoutAttachments();

	/** Drops attachments to the floor where they are */
	void ReleaseAttachments();

	/** Leaves the hand, the host and the air without moving */
	void DetachEverything();

	/** Ends the flight: breaks or lands on the floor */
	void Land();

	/** Height of the object above its origin */
	float GetHeight() const;
};
