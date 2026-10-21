#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class UHeadGameInstance;
class SEditableTextBox;

/**
 *  M1 front end: host a room or join one by address
 *  Also shows why the player came back here (host left, join refused...)
 */
class SHeadFrontEnd : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SHeadFrontEnd) {}
		SLATE_ARGUMENT(TWeakObjectPtr<UHeadGameInstance>, GameInstance)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual bool SupportsKeyboardFocus() const override { return true; }

private:

	FReply OnHostClicked();
	FReply OnJoinClicked();

	FText GetMessageText() const;

	TWeakObjectPtr<UHeadGameInstance> GameInstance;

	TSharedPtr<SEditableTextBox> AddressBox;
};
