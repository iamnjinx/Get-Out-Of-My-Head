#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HeadNameTagWidget.generated.h"

/**
 *  Name tag above objects and avatars (name, attachments, variable, on/off, progress)
 *  Built in Slate so it needs no widget asset and shows Korean with the default font fallback.
 *  Drawn in world space so the mind room screen capture shows it too.
 */
UCLASS()
class UHeadNameTagWidget : public UUserWidget
{
	GENERATED_BODY()

	TFunction<FText()> TextSource;

public:

	/** Function returning the text, called every frame */
	void SetTextSource(TFunction<FText()> InTextSource) { TextSource = MoveTemp(InTextSource); }

protected:

	virtual TSharedRef<SWidget> RebuildWidget() override;
};
