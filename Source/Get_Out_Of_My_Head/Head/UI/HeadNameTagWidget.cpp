#include "HeadNameTagWidget.h"
#include "HeadObject.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Text/STextBlock.h"

TSharedRef<SWidget> UHeadNameTagWidget::RebuildWidget()
{
	TWeakObjectPtr<AHeadObject> WeakObject = OwningObject;

	return SNew(STextBlock)
		.Text_Lambda([WeakObject]()
		{
			const AHeadObject* Object = WeakObject.Get();
			return Object ? Object->GetNameTagText() : FText::GetEmpty();
		})
		.Font(FCoreStyle::GetDefaultFontStyle("Regular", 12))
		.Justification(ETextJustify::Center)
		.ColorAndOpacity(FLinearColor::White)
		.ShadowOffset(FVector2D(1.0f, 1.0f))
		.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.9f));
}
