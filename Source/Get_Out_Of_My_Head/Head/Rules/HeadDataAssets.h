#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "HeadRuleTypes.h"
#include "HeadDataAssets.generated.h"

class UStaticMesh;

/** A body status (젖음, 포만감...). Statuses stay on the body across turns */
UCLASS(BlueprintType)
class UHeadStatusData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Status")
	FText DisplayName;

	/** Seconds until it expires (per stack for OneByOne). 0 means permanent */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Status", meta=(ClampMin="0"))
	float Duration = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Status")
	bool bStackable = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Status", meta=(EditCondition="bStackable"))
	EHeadStackExpiry StackExpiry = EHeadStackExpiry::OneByOne;

	/** Effects by stack count. Empty means the status is only used by conditions */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Status")
	TArray<FHeadStatusTier> Tiers;

	/** Returns the tier active at the given stack count, or null */
	const FHeadStatusTier* FindTier(int32 Stacks) const;
};

/** A house-wide value everyone can see (방 온도) */
UCLASS(BlueprintType)
class UHeadWorldVariableData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Variable")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Variable")
	int32 InitialValue = 0;

	/** Shown after the value, e.g. "도" */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Variable")
	FText Unit;
};

/**
 *  A secret mission
 *  Condition missions complete the moment every condition is true.
 *  Staged missions complete their stages in order, holding each stage's conditions for its time:
 *  a broken prerequisite restarts from stage 1, a broken stage condition only restarts that stage's time.
 */
UCLASS(BlueprintType)
class UHeadMissionData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mission")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mission", meta=(MultiLine="true"))
	FText Description;

	/** Lower wins when several missions complete at once */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mission")
	int32 Priority = 0;

	/** Condition mission: all must be true at the same time */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mission", meta=(EditCondition="!bStaged"))
	TArray<FHeadCondition> Conditions;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mission")
	bool bStaged = false;

	/** Staged mission: must stay true the whole time, otherwise back to stage 1 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mission", meta=(EditCondition="bStaged"))
	TArray<FHeadCondition> Prerequisites;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mission", meta=(EditCondition="bStaged"))
	TArray<FHeadMissionStage> Stages;
};

/**
 *  An object in the house, defined only by data
 *  Requirements + effects, plus properties (carry, consume, toggle...) and what it accepts as attachments.
 */
UCLASS(BlueprintType)
class UHeadObjectData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Object")
	FText DisplayName;

	/** Placeholder look until M7 art */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Object|Look")
	TObjectPtr<UStaticMesh> Mesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Object|Look")
	FVector MeshScale = FVector(0.3f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Object|Look")
	FLinearColor Color = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Object|Interact")
	EHeadInteractMode Mode = EHeadInteractMode::Instant;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Object|Interact", meta=(ClampMin="0"))
	float HoldSeconds = 0.0f;

	/** Checked in order when starting and when finishing a hold */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Object|Interact")
	TArray<FHeadCondition> Conditions;

	/** Applied every time the interaction completes */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Object|Interact")
	TArray<FHeadEffect> Effects;

	/** Can be picked up */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Object|Properties")
	bool bCarryable = false;

	/** Disappears when its effect triggers (eating) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Object|Properties")
	bool bConsumeOnUse = false;

	/** Comes back at its first place in its first state after disappearing */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Object|Properties")
	bool bRespawn = false;

	/** Disappears without effect when thrown (not when thrown while attached) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Object|Properties")
	bool bBreakOnThrow = false;

	/** No E interaction, only pick up and attach */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Object|Properties")
	bool bNoInteract = false;

	/** Has an on/off state that flips every time it works */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Object|Properties")
	bool bToggle = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Object|Properties", meta=(EditCondition="bToggle"))
	bool bInitiallyOn = false;

	/** Applied when the toggle turns on */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Object|Properties", meta=(EditCondition="bToggle"))
	TArray<FHeadEffect> OnEffects;

	/** Applied when the toggle turns off */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Object|Properties", meta=(EditCondition="bToggle"))
	TArray<FHeadEffect> OffEffects;

	/** Blocks the body and thrown objects. Ignored for carryable objects */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Object|Properties")
	bool bLargeFurniture = false;

	/** Large furniture + toggle: passable while open (on) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Object|Properties")
	bool bDoor = false;

	/** Shows this variable's value next to the name (온도 조절기) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Object|Properties")
	TObjectPtr<UHeadWorldVariableData> LabelVariable;

	/** Items this object accepts as attachments */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Object|Attach")
	TArray<FHeadAttachSlot> AttachSlots;

	/** Returns the slot accepting the given item, or null */
	const FHeadAttachSlot* FindAttachSlot(const UHeadObjectData* Item) const;

	bool IsHoldMode() const { return Mode != EHeadInteractMode::Instant; }
};

/** Where an object starts, used until the M4 house map places objects itself */
USTRUCT(BlueprintType)
struct FHeadObjectPlacement
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Placement")
	TObjectPtr<UHeadObjectData> Object;

	/** Offset from the body start */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Placement")
	FVector Offset = FVector::ZeroVector;
};

/** Everything a round is made of: variables, missions and (for now) the test layout */
UCLASS(BlueprintType)
class UHeadRuleSet : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rules")
	TArray<TObjectPtr<UHeadWorldVariableData>> WorldVariables;

	/** Mission pool. Duplicates are given out when there are more players than missions */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rules")
	TArray<TObjectPtr<UHeadMissionData>> Missions;

	/** Objects spawned around the body start */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rules")
	TArray<FHeadObjectPlacement> Placements;
};
