#include "HeadScreenWidget.h"
#include "HeadMindRoom.h"
#include "Engine/Texture.h"
#include "Widgets/Images/SImage.h"

void UHeadScreenWidget::SetScreenTexture(UTexture* Texture)
{
	ScreenBrush.SetResourceObject(Texture);
	ScreenBrush.ImageSize = FVector2D(AHeadMindRoom::ScreenWidth, AHeadMindRoom::ScreenHeight);
	ScreenBrush.DrawAs = ESlateBrushDrawType::Image;
}

TSharedRef<SWidget> UHeadScreenWidget::RebuildWidget()
{
	return SNew(SImage).Image(&ScreenBrush);
}
