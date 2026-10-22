#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HeadMindRoom.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;
class UWidgetComponent;
class UTextureRenderTarget2D;

/**
 *  The room inside the head where everyone who is not controlling the body waits (M3)
 *  Built from basic shapes so it needs no level edits. One wall holds the screen that shows
 *  what the body sees (rendered locally on every machine from the body's head capture).
 */
UCLASS()
class AHeadMindRoom : public AActor
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> Floor;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> Ceiling;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TArray<TObjectPtr<UStaticMeshComponent>> Walls;

	/** Every panel and its color, applied in BeginPlay */
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Panels;

	UPROPERTY()
	TArray<FLinearColor> PanelColors;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UPointLightComponent> LightA;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UPointLightComponent> LightB;

	/** The body's view, on the +X wall */
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UWidgetComponent> Screen;

	/** Local render target the body's head capture draws into */
	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> ScreenTarget;

public:

	AHeadMindRoom();

	/** Inner size of the room */
	static constexpr float RoomSizeXY = 1000.0f;
	static constexpr float RoomHeight = 400.0f;

	/** Screen resolution */
	static constexpr int32 ScreenWidth = 1280;
	static constexpr int32 ScreenHeight = 720;

	virtual void BeginPlay() override;

	/** Local render target for the head capture */
	UTextureRenderTarget2D* GetScreenTarget() const { return ScreenTarget; }

	/** Where the avatar of the Index-th player appears */
	FTransform GetAvatarSpawnTransform(int32 Index) const;

protected:

	UStaticMeshComponent* MakePanel(FName Name, const FVector& Location, const FVector& Size, const FLinearColor& Color);
};
