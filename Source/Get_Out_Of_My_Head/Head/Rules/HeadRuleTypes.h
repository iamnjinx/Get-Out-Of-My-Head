#pragma once

#include "CoreMinimal.h"
#include "HeadRuleTypes.generated.h"

class UHeadStatusData;
class UHeadObjectData;
class UHeadMissionData;
class UHeadWorldVariableData;

/** How an interaction (or an attach) runs */
UENUM(BlueprintType)
enum class EHeadInteractMode : uint8
{
	/** Effect right away when E is pressed */
	Instant,

	/** Hold for N seconds. Progress is kept when interrupted, the next player can continue */
	HoldKeep,

	/** Hold for N seconds. Progress resets when interrupted */
	HoldReset
};

/** Which object a condition looks at */
UENUM(BlueprintType)
enum class EHeadConditionTarget : uint8
{
	/** The object being used (or the host, for attach conditions) */
	Self,

	/** The item being attached (attach conditions only) */
	Item,

	/** Any object in the world using the given object data */
	Other
};

/** Requirement types from the design sheet */
UENUM(BlueprintType)
enum class EHeadConditionType : uint8
{
	/** Target has been interacted with at least once */
	InteractedBefore,

	/** Target is inside the zone named Zone */
	InZone,

	/** Body has Status */
	HasStatus,

	/** Body does not have Status */
	LacksStatus,

	/** Body has at least Value stacks of Status */
	StatusStacksAtLeast,

	/** Target toggle is on */
	ToggleOn,

	/** Target toggle is off */
	ToggleOff,

	/** Body holds Object (in hand, or attached to what is in hand) */
	Holding,

	/** Someone in the round has Mission */
	MissionHolderPresent,

	/** Object is attached to the target */
	Attached,

	/** World variable == Value */
	VariableEquals,

	/** World variable >= Value */
	VariableAtLeast,

	/** World variable <= Value */
	VariableAtMost
};

/** Effect types from the design sheet */
UENUM(BlueprintType)
enum class EHeadEffectType : uint8
{
	/** Give Stacks of Status to the body */
	AddStatus,

	/** Remove Status from the body */
	RemoveStatus,

	/** Play a sound heard at the body (M2: logged as a notice) */
	Sound,

	/** Add Delta to a world variable */
	AddVariable
};

/** One requirement. The first failing one shows its FailText to the controller */
USTRUCT(BlueprintType)
struct FHeadCondition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Condition")
	EHeadConditionType Type = EHeadConditionType::HasStatus;

	/** Object the condition looks at (InteractedBefore, InZone, ToggleOn/Off, Attached host) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Condition")
	EHeadConditionTarget Target = EHeadConditionTarget::Self;

	/** Object data for Target = Other, Holding and the attached item of Attached */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Condition")
	TObjectPtr<UHeadObjectData> Object;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Condition")
	TObjectPtr<UHeadStatusData> Status;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Condition")
	TObjectPtr<UHeadMissionData> Mission;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Condition")
	TObjectPtr<UHeadWorldVariableData> Variable;

	/** Zone name for InZone (AHeadZone::ZoneName) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Condition")
	FName Zone;

	/** Stack count or variable value to compare with */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Condition")
	int32 Value = 0;

	/** Shown to the controller when this is the first failing condition. Empty means "조건 부족" */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Condition")
	FText FailText;
};

/** One effect applied when an interaction completes */
USTRUCT(BlueprintType)
struct FHeadEffect
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Effect")
	EHeadEffectType Type = EHeadEffectType::AddStatus;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Effect")
	TObjectPtr<UHeadStatusData> Status;

	/** Stacks to add for AddStatus */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Effect")
	int32 Stacks = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Effect")
	TObjectPtr<UHeadWorldVariableData> Variable;

	/** Amount for AddVariable */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Effect")
	int32 Delta = 0;

	/** What the sound is, for Sound */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Effect")
	FText SoundText;
};

/** An item a host accepts, with its own mode, conditions and effects */
USTRUCT(BlueprintType)
struct FHeadAttachSlot
{
	GENERATED_BODY()

	/** Item that can be attached. Only one of each per host */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attach")
	TObjectPtr<UHeadObjectData> Item;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attach")
	EHeadInteractMode Mode = EHeadInteractMode::Instant;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attach", meta=(ClampMin="0"))
	float HoldSeconds = 0.0f;

	/** Checked in order. Target Self = host, Item = the item being attached */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attach")
	TArray<FHeadCondition> Conditions;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attach")
	TArray<FHeadEffect> Effects;
};

/** Stack-dependent effect of a status. The highest tier whose MinStacks is reached applies */
USTRUCT(BlueprintType)
struct FHeadStatusTier
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Status", meta=(ClampMin="1"))
	int32 MinStacks = 1;

	/** Multiplies the body's walk speed */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Status", meta=(ClampMin="0"))
	float SpeedMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Status")
	bool bImmobile = false;

	/** Chance (0-1) to fall, rolled when the body starts moving and every second while moving */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Status", meta=(ClampMin="0", ClampMax="1"))
	float FallChance = 0.0f;
};

/** How stacks expire */
UENUM(BlueprintType)
enum class EHeadStackExpiry : uint8
{
	/** One stack is removed every Duration seconds */
	OneByOne,

	/** All stacks are removed after Duration seconds */
	All
};
