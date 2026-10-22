#include "HeadNameTagWidget.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Text/STextBlock.h"

TSharedRef<SWidget> UHeadNameTagWidget::RebuildWidget()
{
	TWeakObjectPtr<UHeadNameTagWidget> WeakThis(this);

	return SNew(STextBlock)
		.Text_Lambda([WeakThis]()
		{
			return WeakThis.IsValid() && WeakThis->TextSource ? WeakThis->TextSource() : FText::GetEmpty();
		})
		.Font(FCoreStyle::GetDefaultFontStyle("Bold", 28))
		.Justification(ETextJustify::Center)
		.ColorAndOpacity(FLinearColor::White)
		.ShadowOffset(FVector2D(2.0f, 2.0f))
		.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.9f));
}
