#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateBrush.h"
#include "HeadScreenWidget.generated.h"

class UTexture;

/** Mind room screen: draws the body's head capture. Built in Slate so it needs no assets */
UCLASS()
class UHeadScreenWidget : public UUserWidget
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	FSlateBrush ScreenBrush;

public:

	void SetScreenTexture(UTexture* Texture);

protected:

	virtual TSharedRef<SWidget> RebuildWidget() override;
};
