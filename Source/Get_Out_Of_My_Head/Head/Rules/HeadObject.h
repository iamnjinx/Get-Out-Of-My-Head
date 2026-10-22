#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HeadObject.generated.h"

class UHeadObjectData;
class UStaticMeshComponent;
class UWidgetComponent;
struct FHeadRuleContext;

/**
 *  An object in the house. Everything it does comes from its UHeadObjectData.
 *  Public state (on/off, hold progress, attachments) replicates to everyone and shows on its name tag.
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

	/** Toggle state */
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

	/** Host this object is attached to */
	UPROPERTY(Replicated)
	TObjectPtr<AHeadObject> AttachedTo;

	/** Objects attached to this one, last attached last */
	UPROPERTY(Replicated)
	TArray<TObjectPtr<AHeadObject>> Attachments;

	/** Where the object was first placed, for respawning */
	FTransform InitialTransform;

	/** Server: InitObject has run */
	bool bInitialized = false;

public:

	AHeadObject();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;

	/** Server: sets the data and the first state */
	void InitObject(UHeadObjectData* InData);

	UHeadObjectData* GetObjectData() const { return ObjectData; }

	bool IsOn() const { return bOn; }
	bool IsConsumed() const { return bConsumed; }
	bool HasBeenInteracted() const { return bInteracted; }
	float GetProgress() const { return Progress; }
	AHeadObject* GetAttachedTo() const { return AttachedTo; }
	const TArray<TObjectPtr<AHeadObject>>& GetAttachments() const { return Attachments; }

	/** True when an object using Data is directly attached to this one */
	bool HasAttachment(const UHeadObjectData* Data) const;

	/** True when this object or anything attached inside it uses Data */
	bool ContainsObjectData(const UHeadObjectData* Data) const;

	/** True when it can be the target of E right now */
	bool CanBeUsed() const;

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

	/** Name tag text, rebuilt every frame */
	FText GetNameTagText() const;

protected:

	UFUNCTION()
	void OnRep_ObjectData();

	UFUNCTION()
	void OnRep_PublicState();

	/** Applies mesh, color, collision and visibility from data and state */
	void ApplyLook();

	/** Drops attachments where they are */
	void ReleaseAttachments();
};
