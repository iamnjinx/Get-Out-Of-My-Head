#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "HeadAvatar.generated.h"

class UStaticMeshComponent;
class USpringArmComponent;
class UCameraComponent;
class UWidgetComponent;
class UInputAction;
class APlayerState;
struct FInputActionValue;

/**
 *  A player's avatar in the mind room (M3)
 *  Position and nickname are public. It disappears while its owner is out controlling the body.
 */
UCLASS()
class AHeadAvatar : public ACharacter
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> Shape;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UCameraComponent> Camera;

	/** Nickname above the avatar */
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UWidgetComponent> NameTag;

	UPROPERTY(EditAnywhere, Category="Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditAnywhere, Category="Input")
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(EditAnywhere, Category="Input")
	TObjectPtr<UInputAction> MouseLookAction;

	UPROPERTY(EditAnywhere, Category="Input")
	TObjectPtr<UInputAction> JumpAction;

	/** Player this avatar belongs to, kept while the player is away */
	UPROPERTY(ReplicatedUsing=OnRep_Look)
	TObjectPtr<APlayerState> OwnerPlayerState;

	/** Picks the avatar color */
	UPROPERTY(ReplicatedUsing=OnRep_Look)
	int32 ColorIndex = 0;

	/** Owner is controlling the body: the avatar is gone from the room */
	UPROPERTY(ReplicatedUsing=OnRep_Look)
	bool bAway = false;

public:

	AHeadAvatar();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** Server: ties the avatar to its player */
	void InitAvatar(APlayerState* InOwner, int32 InColorIndex);

	/** Server: hides or shows the avatar */
	void SetAway(bool bInAway);

	bool IsAway() const { return bAway; }

	FText GetNameTagText() const;

protected:

	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);

	/** Nobody moves during the handover */
	bool IsInputLocked() const;

	UFUNCTION()
	void OnRep_Look();

	void ApplyLook();
};
