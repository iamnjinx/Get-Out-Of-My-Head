#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MissionButton.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;

/**
 *  M1 dummy mission
 *  Each player is secretly assigned one button. Pressing it with the body completes that player's mission.
 */
UCLASS()
class AMissionButton : public AActor
{
	GENERATED_BODY()

	/** Pedestal */
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> Base;

	/** Colored cap that sinks when pressed */
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> Cap;

	/** Letter floating above the button */
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UTextRenderComponent> Label;

	/** Which button this is (A, B, C...) */
	UPROPERTY(ReplicatedUsing=OnRep_ButtonIndex)
	int32 ButtonIndex = INDEX_NONE;

	/** Releases the cap after a press */
	FTimerHandle ReleaseTimer;

public:

	AMissionButton();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Keeps the label facing the local camera */
	virtual void Tick(float DeltaSeconds) override;

	/** Server: sets which button this is */
	void SetButtonIndex(int32 NewIndex);

	int32 GetButtonIndex() const { return ButtonIndex; }

	/** Plays the press on every client */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPress();

	/** Returns the display letter of a button index */
	static FString GetLabel(int32 Index);

	/** Returns the color of a button index */
	static FLinearColor GetColor(int32 Index);

protected:

	UFUNCTION()
	void OnRep_ButtonIndex();

	/** Applies label and color for the current index */
	void ApplyLook();

	void ReleaseCap();
};
