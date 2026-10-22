#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HeadNameTagWidget.generated.h"

class AHeadObject;

/**
 *  Object name tag shown above objects (name, attachments, variable, on/off, progress)
 *  Built in Slate so it needs no widget asset and shows Korean with the default font fallback.
 */
UCLASS()
class UHeadNameTagWidget : public UUserWidget
{
	GENERATED_BODY()

	TWeakObjectPtr<AHeadObject> OwningObject;

public:

	void SetOwningObject(AHeadObject* InObject) { OwningObject = InObject; }

protected:

	virtual TSharedRef<SWidget> RebuildWidget() override;
};
