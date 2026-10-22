#include "HeadPlayerController.h"
#include "HeadGameMode.h"
#include "HeadGameState.h"
#include "HeadPlayerState.h"
#include "HeadGameInstance.h"
#include "Get_Out_Of_My_HeadCharacter.h"
#include "HeadObject.h"
#include "HeadDataAssets.h"
#include "HeadBodyComponent.h"
#include "HeadInteraction.h"
#include "HeadMindRoom.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "EngineUtils.h"
#include "UI/SHeadHUD.h"
#include "UI/SHeadFrontEnd.h"
#include "UI/SHeadSurvey.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "UObject/ConstructorHelpers.h"

AHeadPlayerController::AHeadPlayerController()
{
	// same mappings the template's BP_ThirdPersonPlayerController uses
	static ConstructorHelpers::FObjectFinder<UInputMappingContext> DefaultContext(TEXT("/Game/Input/IMC_Default"));
	static ConstructorHelpers::FObjectFinder<UInputMappingContext> MouseLookContext(TEXT("/Game/Input/IMC_MouseLook"));

	if (DefaultContext.Succeeded())
	{
		DefaultMappingContexts.Add(DefaultContext.Object);
	}

	if (MouseLookContext.Succeeded())
	{
		MobileExcludedMappingContexts.Add(MouseLookContext.Object);
	}

	// placeholder until M7 sounds
	static ConstructorHelpers::FObjectFinder<USoundBase> UISoundFinder(TEXT("/Engine/EngineSounds/1kSineTonePing"));
	UISound = UISoundFinder.Object;
}

void AHeadPlayerController::AddNotice(const FString& Text)
{
	Notices.Add({ Text, FPlatformTime::Seconds() + NoticeSeconds });
}

TArray<FString> AHeadPlayerController::GetActiveNotices() const
{
	const double Now = FPlatformTime::Seconds();

	TArray<FString> Active;
	for (const FHeadNotice& Notice : Notices)
	{
		if (Notice.ExpireTime > Now)
		{
			Active.Add(Notice.Text);
		}
	}

	return Active;
}

void AHeadPlayerController::ClientNotice_Implementation(const FString& Text)
{
	AddNotice(Text);

	// UI sounds are 2D and only reach this player
	if (UISound)
	{
		UGameplayStatics::PlaySound2D(this, UISound, UISoundVolume);
	}
}

bool AHeadPlayerController::IsHost() const
{
	return IsLocalController() && GetNetMode() == NM_ListenServer;
}

bool AHeadPlayerController::IsFrontEnd() const
{
	return GetNetMode() == NM_Standalone;
}

AHeadGameState* AHeadPlayerController::GetHeadGameState() const
{
	return GetWorld() ? GetWorld()->GetGameState<AHeadGameState>() : nullptr;
}

AHeadPlayerState* AHeadPlayerController::GetHeadPlayerState() const
{
	return GetPlayerState<AHeadPlayerState>();
}

void AHeadPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (!IsLocalPlayerController())
	{
		return;
	}

	UGameViewportClient* ViewportClient = GetLocalPlayer() ? GetLocalPlayer()->ViewportClient : nullptr;
	if (!ViewportClient)
	{
		return;
	}

	if (IsFrontEnd())
	{
		ScreenWidget = SNew(SHeadFrontEnd).GameInstance(GetGameInstance<UHeadGameInstance>());
		ViewportClient->AddViewportWidgetContent(ScreenWidget.ToSharedRef(), 10);

		FInputModeUIOnly InputMode;
		InputMode.SetWidgetToFocus(ScreenWidget);
		SetInputMode(InputMode);
		SetShowMouseCursor(true);
	}
	else
	{
		ScreenWidget = SNew(SHeadHUD).OwningController(this);
		ViewportClient->AddViewportWidgetContent(ScreenWidget.ToSharedRef(), 10);

		SetInputMode(FInputModeGameOnly());
		SetShowMouseCursor(false);
	}
}

void AHeadPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CloseSurvey();

	if (ScreenWidget.IsValid())
	{
		if (UGameViewportClient* ViewportClient = GetLocalPlayer() ? GetLocalPlayer()->ViewportClient : nullptr)
		{
			ViewportClient->RemoveViewportWidgetContent(ScreenWidget.ToSharedRef());
		}

		ScreenWidget.Reset();
	}

	Super::EndPlay(EndPlayReason);
}

void AHeadPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (!IsLocalPlayerController())
	{
		return;
	}

	// room actions are created in code so M1 needs no extra input assets
	InteractAction = NewObject<UInputAction>(this, TEXT("IA_HeadInteract"));
	HostAction = NewObject<UInputAction>(this, TEXT("IA_HeadHostAction"));
	LeaveAction = NewObject<UInputAction>(this, TEXT("IA_HeadLeave"));
	PickUpAction = NewObject<UInputAction>(this, TEXT("IA_HeadPickUp"));
	DropAction = NewObject<UInputAction>(this, TEXT("IA_HeadDrop"));
	TakeOutAction = NewObject<UInputAction>(this, TEXT("IA_HeadTakeOut"));

	RoomMappingContext = NewObject<UInputMappingContext>(this, TEXT("IMC_HeadRoom"));
	RoomMappingContext->MapKey(InteractAction, EKeys::E);
	RoomMappingContext->MapKey(HostAction, EKeys::Enter);
	RoomMappingContext->MapKey(LeaveAction, EKeys::F10);
	RoomMappingContext->MapKey(PickUpAction, EKeys::LeftMouseButton);
	RoomMappingContext->MapKey(DropAction, EKeys::RightMouseButton);
	RoomMappingContext->MapKey(TakeOutAction, EKeys::Q);

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		Subsystem->AddMappingContext(RoomMappingContext, 1);
	}

	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent))
	{
		EnhancedInputComponent->BindAction(InteractAction, ETriggerEvent::Started, this, &AHeadPlayerController::OnInteractPressed);
		EnhancedInputComponent->BindAction(InteractAction, ETriggerEvent::Completed, this, &AHeadPlayerController::OnInteractReleased);
		EnhancedInputComponent->BindAction(HostAction, ETriggerEvent::Started, this, &AHeadPlayerController::OnHostAction);
		EnhancedInputComponent->BindAction(LeaveAction, ETriggerEvent::Started, this, &AHeadPlayerController::OnLeave);
		EnhancedInputComponent->BindAction(PickUpAction, ETriggerEvent::Started, this, &AHeadPlayerController::OnPickUp);
		EnhancedInputComponent->BindAction(DropAction, ETriggerEvent::Started, this, &AHeadPlayerController::OnDropPressed);
		EnhancedInputComponent->BindAction(DropAction, ETriggerEvent::Completed, this, &AHeadPlayerController::OnDropReleased);
		EnhancedInputComponent->BindAction(TakeOutAction, ETriggerEvent::Started, this, &AHeadPlayerController::OnTakeOut);
	}
}

void AHeadPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	if (IsLocalController() && !IsFrontEnd())
	{
		UpdateAim();
		UpdateMindRoomView();
		UpdateSurvey();
	}

	const double Now = FPlatformTime::Seconds();
	Notices.RemoveAll([Now](const FHeadNotice& Notice) { return Notice.ExpireTime <= Now; });
}

void AHeadPlayerController::ServerInteractPressed_Implementation(AHeadObject* Target)
{
	if (AHeadGameMode* GameMode = GetWorld()->GetAuthGameMode<AHeadGameMode>())
	{
		GameMode->HandleInteractPressed(this, Target);
	}
}

void AHeadPlayerController::ServerInteractReleased_Implementation()
{
	if (AHeadGameMode* GameMode = GetWorld()->GetAuthGameMode<AHeadGameMode>())
	{
		GameMode->HandleInteractReleased(this);
	}
}

void AHeadPlayerController::ServerHostAction_Implementation()
{
	if (AHeadGameMode* GameMode = GetWorld()->GetAuthGameMode<AHeadGameMode>())
	{
		GameMode->HandleHostAction(this);
	}
}

void AHeadPlayerController::OnInteractPressed()
{
	if (!IsFrontEnd())
	{
		ServerInteractPressed(AimTarget.Get());
	}
}

void AHeadPlayerController::OnInteractReleased()
{
	if (!IsFrontEnd())
	{
		ServerInteractReleased();
	}
}

void AHeadPlayerController::OnHostAction()
{
	if (IsHost())
	{
		ServerHostAction();
	}
}

void AHeadPlayerController::OnLeave()
{
	if (!IsFrontEnd())
	{
		if (UHeadGameInstance* GameInstance = GetGameInstance<UHeadGameInstance>())
		{
			GameInstance->LeaveRoom();
		}
	}
}

bool AHeadPlayerController::IsControllingBody() const
{
	const AHeadGameState* GS = GetHeadGameState();
	return GS && GS->Body && GetPawn() == GS->Body;
}

UHeadBodyComponent* AHeadPlayerController::GetBodyComponent() const
{
	const AHeadGameState* GS = GetHeadGameState();
	return GS && GS->Body ? GS->Body->FindComponentByClass<UHeadBodyComponent>() : nullptr;
}

namespace
{
	FString NameOf(const AHeadObject* Object)
	{
		return Object && Object->GetObjectData() ? Object->GetObjectData()->DisplayName.ToString() : FString();
	}

	/** "수건 사용", "침실 불 스위치 켜기", "지하실 문 열기"... empty when E does nothing */
	FString UseText(const AHeadObject* Object)
	{
		if (!Object || !Object->CanBeUsed())
		{
			return FString();
		}

		const UHeadObjectData* Data = Object->GetObjectData();

		FString Verb = TEXT("사용");
		if (Data->bDoor)
		{
			Verb = Object->IsOn() ? TEXT("닫기") : TEXT("열기");
		}
		else if (Data->bToggle)
		{
			Verb = Object->IsOn() ? TEXT("끄기") : TEXT("켜기");
		}

		FString Text = FString::Printf(TEXT("E: %s %s"), *NameOf(Object), *Verb);
		if (Data->IsHoldMode())
		{
			Text += TEXT(" (누르고 있기)");
		}

		return Text;
	}

	/** E line for something held + something pointed, following the design sheet's table */
	FString AttachOrUseText(const AHeadObject* Held, const AHeadObject* Target)
	{
		if (Target->GetObjectData()->FindAttachSlot(Held->GetObjectData()))
		{
			return Target->HasAttachment(Held->GetObjectData())
				? TEXT("E: 이미 장착되어 있다")
				: FString::Printf(TEXT("E: %s 장착"), *NameOf(Held));
		}

		if (Target->GetObjectData()->bCarryable && Held->GetObjectData()->FindAttachSlot(Target->GetObjectData()))
		{
			return Held->HasAttachment(Target->GetObjectData())
				? TEXT("E: 이미 장착되어 있다")
				: FString::Printf(TEXT("E: %s 장착"), *NameOf(Target));
		}

		return UseText(Target);
	}
}

FText AHeadPlayerController::GetGuideText() const
{
	if (!IsControllingBody())
	{
		return FText::GetEmpty();
	}

	const UHeadBodyComponent* BodyComponent = GetBodyComponent();
	const AHeadObject* Held = BodyComponent ? BodyComponent->GetHeldObject() : nullptr;
	const AHeadObject* Target = AimTarget.Get();

	TArray<FString> Parts;

	const FString EText = Held && Target ? AttachOrUseText(Held, Target) : UseText(Target ? Target : Held);
	if (!EText.IsEmpty())
	{
		Parts.Add(EText);
	}

	if (Target && Target->GetObjectData()->bCarryable)
	{
		Parts.Add(Held
			? FString::Printf(TEXT("좌클릭: %s(으)로 바꿔 들기"), *NameOf(Target))
			: FString::Printf(TEXT("좌클릭: %s 집기"), *NameOf(Target)));
	}

	if (Held)
	{
		Parts.Add(FString::Printf(TEXT("우클릭: %s 내려놓기 (1초 이상: 던지기)"), *NameOf(Held)));
	}

	// Q takes from the pointed host first, then from what is in hand
	const AHeadObject* Source = Target && Target->GetAttachments().Num() > 0 ? Target : (Held && Held->GetAttachments().Num() > 0 ? Held : nullptr);
	if (Source)
	{
		Parts.Add(FString::Printf(TEXT("Q: %s 꺼내기"), *NameOf(Source->GetAttachments().Last())));
	}

	return FText::FromString(FString::Join(Parts, TEXT("    ")));
}

void AHeadPlayerController::UpdateAim()
{
	AHeadObject* NewTarget = nullptr;

	if (IsControllingBody())
	{
		const UHeadBodyComponent* BodyComponent = GetBodyComponent();
		NewTarget = HeadInteraction::FindAimTarget(this, Cast<ACharacter>(GetPawn()), BodyComponent ? BodyComponent->InteractRange : 150.0f);
	}

	// the highlight is local: only the controller sees it
	if (AimTarget.Get() != NewTarget)
	{
		if (AHeadObject* Previous = AimTarget.Get())
		{
			Previous->SetHighlighted(false);
		}

		if (NewTarget)
		{
			NewTarget->SetHighlighted(true);
		}

		AimTarget = NewTarget;
	}
}

void AHeadPlayerController::ServerPickUp_Implementation(AHeadObject* Target)
{
	if (AHeadGameMode* GameMode = GetWorld()->GetAuthGameMode<AHeadGameMode>())
	{
		GameMode->HandlePickUp(this, Target);
	}
}

void AHeadPlayerController::ServerDropPressed_Implementation()
{
	if (AHeadGameMode* GameMode = GetWorld()->GetAuthGameMode<AHeadGameMode>())
	{
		GameMode->HandleSecondaryPressed(this);
	}
}

void AHeadPlayerController::ServerDropReleased_Implementation()
{
	if (AHeadGameMode* GameMode = GetWorld()->GetAuthGameMode<AHeadGameMode>())
	{
		GameMode->HandleSecondaryReleased(this);
	}
}

void AHeadPlayerController::ServerTakeOut_Implementation(AHeadObject* Target)
{
	if (AHeadGameMode* GameMode = GetWorld()->GetAuthGameMode<AHeadGameMode>())
	{
		GameMode->HandleTakeOut(this, Target);
	}
}

void AHeadPlayerController::OnPickUp()
{
	if (IsControllingBody())
	{
		ServerPickUp(AimTarget.Get());
	}
}

void AHeadPlayerController::OnDropPressed()
{
	if (IsControllingBody())
	{
		ServerDropPressed();
	}
}

void AHeadPlayerController::OnDropReleased()
{
	if (IsControllingBody())
	{
		ServerDropReleased();
	}
}

void AHeadPlayerController::OnTakeOut()
{
	if (IsControllingBody())
	{
		ServerTakeOut(AimTarget.Get());
	}
}

void AHeadPlayerController::UpdateMindRoomView()
{
	const AHeadGameState* GS = GetHeadGameState();
	if (!GS || !GS->Body)
	{
		return;
	}

	USceneCaptureComponent2D* Capture = GS->Body->GetHeadCapture();

	// world sounds are heard from the body's head, wherever this player is
	if (!bListenerOnBody)
	{
		SetAudioListenerOverride(Capture, FVector::ZeroVector, FRotator::ZeroRotator);
		bListenerOnBody = true;
	}

	if (!MindRoom.IsValid())
	{
		for (TActorIterator<AHeadMindRoom> It(GetWorld()); It; ++It)
		{
			MindRoom = *It;
			break;
		}
	}

	// only machines with someone in the mind room render the screen
	const bool bInMindRoom = !IsControllingBody();
	Capture->bCaptureEveryFrame = bInMindRoom && MindRoom.IsValid();
	Capture->TextureTarget = MindRoom.IsValid() ? MindRoom->GetScreenTarget() : nullptr;
}

void AHeadPlayerController::UpdateSurvey()
{
	const AHeadGameState* GS = GetHeadGameState();
	const bool bResults = GS && GS->Phase == EHeadPhase::Results;

	if (!bResults)
	{
		// next results screen asks again
		CloseSurvey();
		bSurveyClosed = false;
		return;
	}

	if (SurveyWidget.IsValid() || bSurveyClosed)
	{
		return;
	}

	UGameViewportClient* ViewportClient = GetLocalPlayer() ? GetLocalPlayer()->ViewportClient : nullptr;
	if (!ViewportClient)
	{
		return;
	}

	SurveyWidget = SNew(SHeadSurvey).OwningController(this);
	ViewportClient->AddViewportWidgetContent(SurveyWidget.ToSharedRef(), 20);

	// typing needs the cursor, the game still gets the host's Enter once the boxes lose focus
	FInputModeGameAndUI InputMode;
	InputMode.SetHideCursorDuringCapture(false);
	SetInputMode(InputMode);
	SetShowMouseCursor(true);
}

void AHeadPlayerController::SubmitSurvey(const TArray<FString>& Answers)
{
	ServerSubmitSurvey(Answers);
	bSurveyClosed = true;
	CloseSurvey();
}

void AHeadPlayerController::CloseSurvey()
{
	if (!SurveyWidget.IsValid())
	{
		return;
	}

	if (UGameViewportClient* ViewportClient = GetLocalPlayer() ? GetLocalPlayer()->ViewportClient : nullptr)
	{
		ViewportClient->RemoveViewportWidgetContent(SurveyWidget.ToSharedRef());
	}

	SurveyWidget.Reset();
	bSurveyClosed = true;

	SetInputMode(FInputModeGameOnly());
	SetShowMouseCursor(false);
}

void AHeadPlayerController::ServerSubmitSurvey_Implementation(const TArray<FString>& Answers)
{
	if (AHeadGameMode* GameMode = GetWorld()->GetAuthGameMode<AHeadGameMode>())
	{
		GameMode->HandleSurvey(this, Answers);
	}
}
