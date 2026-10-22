#include "SHeadSurvey.h"
#include "HeadPlayerController.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	/** Questions from the design sheet, asked after every round */
	const TCHAR* SurveyQuestions[] = {
		TEXT("남의 임무를 추리하려고 했나요? 맞혔나요?"),
		TEXT("머릿속 방에 있을 때 무엇을 했나요?"),
		TEXT("가장 억울했던 순간은?"),
		TEXT("\"조건 부족\" 문구가 다음에 할 일을 알려 줬나요?")
	};

	TSharedRef<SButton> MakeSurveyButton(const FString& Label, FOnClicked OnClicked)
	{
		return SNew(SButton)
			.HAlign(HAlign_Center)
			.ContentPadding(FMargin(14.0f, 6.0f))
			.OnClicked(OnClicked)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Label))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 14))
			];
	}
}

void SHeadSurvey::Construct(const FArguments& InArgs)
{
	OwningController = InArgs._OwningController;

	TSharedRef<SVerticalBox> Questions = SNew(SVerticalBox);

	Questions->AddSlot()
	.AutoHeight()
	.Padding(0.0f, 0.0f, 0.0f, 12.0f)
	[
		SNew(STextBlock)
		.Text(FText::FromString(TEXT("판 끝 설문 (건너뛰어도 됩니다)")))
		.Font(FCoreStyle::GetDefaultFontStyle("Bold", 18))
	];

	for (const TCHAR* Question : SurveyQuestions)
	{
		TSharedPtr<SEditableTextBox> Box;

		Questions->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 4.0f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(Question))
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", 13))
		];

		Questions->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 6.0f)
		[
			SAssignNew(Box, SEditableTextBox)
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", 13))
		];

		AnswerBoxes.Add(Box);
	}

	Questions->AddSlot()
	.AutoHeight()
	.Padding(0.0f, 10.0f, 0.0f, 0.0f)
	[
		SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(0.0f, 0.0f, 8.0f, 0.0f)
		[
			MakeSurveyButton(TEXT("제출"), FOnClicked::CreateSP(this, &SHeadSurvey::OnSubmitClicked))
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			MakeSurveyButton(TEXT("건너뛰기"), FOnClicked::CreateSP(this, &SHeadSurvey::OnSkipClicked))
		]
	];

	ChildSlot
	.HAlign(HAlign_Right)
	.VAlign(VAlign_Center)
	.Padding(24.0f)
	[
		SNew(SBox)
		.WidthOverride(460.0f)
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(FLinearColor(0.02f, 0.02f, 0.03f, 0.85f))
			.Padding(16.0f)
			[
				Questions
			]
		]
	];
}

FReply SHeadSurvey::OnSubmitClicked()
{
	TArray<FString> Answers;
	for (const TSharedPtr<SEditableTextBox>& Box : AnswerBoxes)
	{
		Answers.Add(Box->GetText().ToString().TrimStartAndEnd());
	}

	if (AHeadPlayerController* PC = OwningController.Get())
	{
		PC->SubmitSurvey(Answers);
	}

	return FReply::Handled();
}

FReply SHeadSurvey::OnSkipClicked()
{
	if (AHeadPlayerController* PC = OwningController.Get())
	{
		PC->CloseSurvey();
	}

	return FReply::Handled();
}
