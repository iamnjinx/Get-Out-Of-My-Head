#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class AHeadPlayerController;
class SEditableTextBox;

/**
 *  Post-round survey from the design sheet (M4)
 *  Shown on the results screen, answers go to the host's playtest log. Can be skipped.
 */
class SHeadSurvey : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SHeadSurvey) {}
		SLATE_ARGUMENT(TWeakObjectPtr<AHeadPlayerController>, OwningController)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual bool SupportsKeyboardFocus() const override { return true; }

private:

	FReply OnSubmitClicked();
	FReply OnSkipClicked();

	TWeakObjectPtr<AHeadPlayerController> OwningController;

	TArray<TSharedPtr<SEditableTextBox>> AnswerBoxes;
};
