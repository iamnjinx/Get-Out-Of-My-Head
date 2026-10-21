#include "SHeadFrontEnd.h"
#include "HeadGameInstance.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	TSharedRef<SButton> MakeButton(const FString& Label, FOnClicked OnClicked)
	{
		return SNew(SButton)
			.HAlign(HAlign_Center)
			.ContentPadding(FMargin(12.0f, 8.0f))
			.OnClicked(OnClicked)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Label))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 16))
			];
	}
}

void SHeadFrontEnd::Construct(const FArguments& InArgs)
{
	GameInstance = InArgs._GameInstance;

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
		.BorderBackgroundColor(FLinearColor(0.02f, 0.02f, 0.03f, 0.85f))
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SBox)
			.WidthOverride(420.0f)
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 0.0f, 0.0f, 24.0f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("Get Out Of My Head")))
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 30))
					.Justification(ETextJustify::Center)
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 0.0f, 0.0f, 16.0f)
				[
					SNew(STextBlock)
					.Text(this, &SHeadFrontEnd::GetMessageText)
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 14))
					.ColorAndOpacity(FLinearColor(1.0f, 0.8f, 0.4f))
					.Justification(ETextJustify::Center)
					.AutoWrapText(true)
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 0.0f, 0.0f, 24.0f)
				[
					MakeButton(TEXT("방 만들기"), FOnClicked::CreateSP(this, &SHeadFrontEnd::OnHostClicked))
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 0.0f, 0.0f, 8.0f)
				[
					SAssignNew(AddressBox, SEditableTextBox)
					.Text(FText::FromString(TEXT("127.0.0.1")))
					.HintText(FText::FromString(TEXT("호스트 주소")))
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 16))
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					MakeButton(TEXT("참가하기"), FOnClicked::CreateSP(this, &SHeadFrontEnd::OnJoinClicked))
				]
			]
		]
	];
}

FReply SHeadFrontEnd::OnHostClicked()
{
	if (UHeadGameInstance* GI = GameInstance.Get())
	{
		GI->HostRoom();
	}

	return FReply::Handled();
}

FReply SHeadFrontEnd::OnJoinClicked()
{
	UHeadGameInstance* GI = GameInstance.Get();
	const FString Address = AddressBox->GetText().ToString().TrimStartAndEnd();

	if (GI && !Address.IsEmpty())
	{
		GI->JoinRoom(Address);
	}

	return FReply::Handled();
}

FText SHeadFrontEnd::GetMessageText() const
{
	const UHeadGameInstance* GI = GameInstance.Get();
	return GI ? FText::FromString(GI->GetFrontEndMessage()) : FText::GetEmpty();
}
