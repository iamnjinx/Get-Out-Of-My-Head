#include "SHeadHUD.h"
#include "HeadPlayerController.h"
#include "HeadGameState.h"
#include "HeadPlayerState.h"
#include "MissionButton.h"
#include "HeadDataAssets.h"
#include "HeadBodyComponent.h"
#include "Get_Out_Of_My_HeadCharacter.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	/** Formats seconds as m:ss, rounding up */
	FString FormatTime(double Seconds)
	{
		const int32 Total = FMath::CeilToInt(Seconds);
		return FString::Printf(TEXT("%d:%02d"), Total / 60, Total % 60);
	}

	FString NameOf(const APlayerState* PS)
	{
		return PS ? PS->GetPlayerName() : TEXT("-");
	}

	TSharedRef<STextBlock> MakeText(TAttribute<FText> Text, float Size, ETextJustify::Type Justify = ETextJustify::Left)
	{
		return SNew(STextBlock)
			.Text(Text)
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", Size))
			.Justification(Justify)
			.ColorAndOpacity(FLinearColor::White)
			.ShadowOffset(FVector2D(1.5f, 1.5f))
			.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.8f));
	}
}

void SHeadHUD::Construct(const FArguments& InArgs)
{
	OwningController = InArgs._OwningController;

	SetVisibility(EVisibility::HitTestInvisible);

	ChildSlot
	[
		SNew(SOverlay)

		+ SOverlay::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Top)
		.Padding(24.0f)
		[
			MakeText(TAttribute<FText>::CreateSP(this, &SHeadHUD::GetPublicText), 15.0f)
		]

		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Top)
		.Padding(24.0f)
		[
			MakeText(TAttribute<FText>::CreateSP(this, &SHeadHUD::GetStatusText), 18.0f, ETextJustify::Center)
		]

		+ SOverlay::Slot()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Top)
		.Padding(24.0f)
		[
			MakeText(TAttribute<FText>::CreateSP(this, &SHeadHUD::GetPrivateText), 16.0f, ETextJustify::Right)
		]

		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			MakeText(TAttribute<FText>::CreateSP(this, &SHeadHUD::GetCenterText), 26.0f, ETextJustify::Center)
		]

		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Bottom)
		.Padding(24.0f)
		[
			MakeText(TAttribute<FText>::CreateSP(this, &SHeadHUD::GetBottomText), 15.0f, ETextJustify::Center)
		]
	];
}

FText SHeadHUD::GetPublicText() const
{
	const AHeadPlayerController* PC = OwningController.Get();
	const AHeadGameState* GS = PC ? PC->GetHeadGameState() : nullptr;
	if (!GS)
	{
		return FText::GetEmpty();
	}

	const APlayerState* Me = PC->PlayerState;
	TArray<FString> Lines;

	if (GS->Phase == EHeadPhase::Lobby)
	{
		Lines.Add(FString::Printf(TEXT("대기실 (%d명)"), GS->PlayerArray.Num()));

		for (const APlayerState* PS : GS->PlayerArray)
		{
			Lines.Add(FString::Printf(TEXT("  %s%s"), *NameOf(PS), PS == Me ? TEXT(" (나)") : TEXT("")));
		}

		return FText::FromString(FString::Join(Lines, TEXT("\n")));
	}

	if (GS->Phase == EHeadPhase::Playing || GS->Phase == EHeadPhase::Handover)
	{
		Lines.Add(FString::Printf(TEXT("판 남은 시간  %s"), *FormatTime(GS->GetSecondsUntil(GS->RoundEndTime))));

		// world variables are public
		for (const FHeadVariableValue& Entry : GS->WorldVariables)
		{
			if (Entry.Variable)
			{
				Lines.Add(FString::Printf(TEXT("%s  %d%s"), *Entry.Variable->DisplayName.ToString(), Entry.Value, *Entry.Variable->Unit.ToString()));
			}
		}


		if (GS->Phase == EHeadPhase::Playing)
		{
			Lines.Add(FString::Printf(TEXT("턴 남은 시간  %s"), *FormatTime(GS->GetSecondsUntil(GS->PhaseEndTime))));
		}
		else
		{
			Lines.Add(TEXT("교대 중"));
		}

		Lines.Add(TEXT(""));
	}

	if (GS->TurnOrder.Num() > 0)
	{
		Lines.Add(TEXT("조종 순서"));

		const APlayerState* Active = GS->Phase == EHeadPhase::Handover ? GS->NextController.Get() : GS->CurrentController.Get();

		for (int32 Index = 0; Index < GS->TurnOrder.Num(); ++Index)
		{
			const APlayerState* PS = GS->TurnOrder[Index];
			Lines.Add(FString::Printf(TEXT("%s %d. %s%s"),
				PS == Active ? TEXT(">") : TEXT(" "),
				Index + 1,
				*NameOf(PS),
				PS == Me ? TEXT(" (나)") : TEXT("")));
		}
	}

	return FText::FromString(FString::Join(Lines, TEXT("\n")));
}

FText SHeadHUD::GetStatusText() const
{
	const AHeadPlayerController* PC = OwningController.Get();
	const AHeadGameState* GS = PC ? PC->GetHeadGameState() : nullptr;
	if (!GS || GS->Phase != EHeadPhase::Playing)
	{
		return FText::GetEmpty();
	}

	if (GS->CurrentController.Get() == PC->PlayerState.Get())
	{
		FString Status = TEXT("내가 조종 중");

		// body statuses only reach the controller
		if (const UHeadBodyComponent* BodyComponent = GS->Body ? GS->Body->FindComponentByClass<UHeadBodyComponent>() : nullptr)
		{
			TArray<FString> Names;
			for (const FHeadStatusInstance& Instance : BodyComponent->GetStatuses())
			{
				if (Instance.Status)
				{
					Names.Add(Instance.Status->bStackable
						? FString::Printf(TEXT("%s x%d"), *Instance.Status->DisplayName.ToString(), Instance.Stacks)
						: Instance.Status->DisplayName.ToString());
				}
			}

			if (Names.Num() > 0)
			{
				Status += TEXT("\n몸 상태: ") + FString::Join(Names, TEXT(", "));
			}

			if (BodyComponent->IsFallen())
			{
				Status += TEXT("\n넘어짐!");
			}
		}

		return FText::FromString(Status);
	}

	return FText::FromString(FString::Printf(TEXT("%s님이 조종 중"), *NameOf(GS->CurrentController)));
}

FText SHeadHUD::GetPrivateText() const
{
	const AHeadPlayerController* PC = OwningController.Get();
	const AHeadPlayerState* Me = PC ? PC->GetHeadPlayerState() : nullptr;
	if (!Me || Me->GetMissionButtonIndex() == INDEX_NONE)
	{
		return FText::GetEmpty();
	}

	// only replicated to this player
	const FString MissionName = Me->GetMission() ? Me->GetMission()->DisplayName.ToString() : TEXT("-");
	return FText::FromString(FString::Printf(TEXT("내 임무: %s\n(M1 테스트: 버튼 %s 누르기)"), *MissionName, *AMissionButton::GetLabel(Me->GetMissionButtonIndex())));
}

FText SHeadHUD::GetCenterText() const
{
	const AHeadPlayerController* PC = OwningController.Get();
	const AHeadGameState* GS = PC ? PC->GetHeadGameState() : nullptr;
	if (!GS)
	{
		return FText::GetEmpty();
	}

	switch (GS->Phase)
	{
	case EHeadPhase::Lobby:
		return FText::FromString(PC->IsHost()
			? TEXT("Enter: 판 시작 (2명 이상)")
			: TEXT("호스트가 판을 시작하기를 기다리는 중"));

	case EHeadPhase::Handover:
		return FText::FromString(FString::Printf(TEXT("교대 중\n다음 조종자: %s"), *NameOf(GS->NextController)));

	case EHeadPhase::Results:
	{
		TArray<FString> Lines;

		switch (GS->EndReason)
		{
		case EHeadEndReason::MissionComplete:
			Lines.Add(FString::Printf(TEXT("임무 완수! %s님 승리"), *NameOf(GS->Winner)));
			break;
		case EHeadEndReason::TimeUp:
			Lines.Add(TEXT("제한 시간 종료 - 무승부"));
			break;
		case EHeadEndReason::LastPlayer:
			Lines.Add(TEXT("1명만 남아 판이 끝났습니다 - 무승부"));
			break;
		default:
			break;
		}

		Lines.Add(TEXT(""));

		// everyone's mission is public now
		for (const APlayerState* PlayerState : GS->PlayerArray)
		{
			if (const AHeadPlayerState* PS = Cast<AHeadPlayerState>(PlayerState))
			{
				if (PS->GetRevealedMissionButtonIndex() != INDEX_NONE)
				{
					Lines.Add(FString::Printf(TEXT("%s: %s (버튼 %s)%s"),
						*NameOf(PS),
						PS->GetRevealedMission() ? *PS->GetRevealedMission()->DisplayName.ToString() : TEXT("-"),
						*AMissionButton::GetLabel(PS->GetRevealedMissionButtonIndex()),
						PS == GS->Winner ? TEXT("  (승리)") : TEXT("")));
				}
			}
		}

		Lines.Add(TEXT(""));
		Lines.Add(PC->IsHost() ? TEXT("Enter: 같은 방에서 다시 하기") : TEXT("호스트가 다시 하기를 고르기를 기다리는 중"));

		return FText::FromString(FString::Join(Lines, TEXT("\n")));
	}

	default:
		return FText::GetEmpty();
	}
}

FText SHeadHUD::GetBottomText() const
{
	const AHeadPlayerController* PC = OwningController.Get();
	if (!PC)
	{
		return FText::GetEmpty();
	}

	TArray<FString> Lines = PC->GetActiveNotices();
	Lines.Add(TEXT(""));
	Lines.Add(TEXT("E: 사용 (홀드형은 누르고 있기)   F10: 방 나가기"));

	return FText::FromString(FString::Join(Lines, TEXT("\n")));
}
