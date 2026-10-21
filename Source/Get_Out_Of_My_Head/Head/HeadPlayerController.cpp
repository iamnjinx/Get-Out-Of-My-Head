#include "HeadPlayerController.h"
#include "HeadGameMode.h"
#include "HeadGameState.h"
#include "HeadPlayerState.h"
#include "HeadGameInstance.h"
#include "Get_Out_Of_My_HeadCharacter.h"
#include "UI/SHeadHUD.h"
#include "UI/SHeadFrontEnd.h"
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

	RoomMappingContext = NewObject<UInputMappingContext>(this, TEXT("IMC_HeadRoom"));
	RoomMappingContext->MapKey(InteractAction, EKeys::E);
	RoomMappingContext->MapKey(HostAction, EKeys::Enter);
	RoomMappingContext->MapKey(LeaveAction, EKeys::F10);

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		Subsystem->AddMappingContext(RoomMappingContext, 1);
	}

	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent))
	{
		EnhancedInputComponent->BindAction(InteractAction, ETriggerEvent::Started, this, &AHeadPlayerController::OnInteract);
		EnhancedInputComponent->BindAction(HostAction, ETriggerEvent::Started, this, &AHeadPlayerController::OnHostAction);
		EnhancedInputComponent->BindAction(LeaveAction, ETriggerEvent::Started, this, &AHeadPlayerController::OnLeave);
	}
}

void AHeadPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	// everyone who is not in control watches the body
	const AHeadGameState* GS = GetHeadGameState();
	if (GS && GS->Body && GetPawn() != GS->Body && GetViewTarget() != GS->Body)
	{
		SetViewTarget(GS->Body);
	}

	const double Now = FPlatformTime::Seconds();
	Notices.RemoveAll([Now](const FHeadNotice& Notice) { return Notice.ExpireTime <= Now; });
}

void AHeadPlayerController::ServerInteract_Implementation()
{
	if (AHeadGameMode* GameMode = GetWorld()->GetAuthGameMode<AHeadGameMode>())
	{
		GameMode->HandleInteract(this);
	}
}

void AHeadPlayerController::ServerHostAction_Implementation()
{
	if (AHeadGameMode* GameMode = GetWorld()->GetAuthGameMode<AHeadGameMode>())
	{
		GameMode->HandleHostAction(this);
	}
}

void AHeadPlayerController::OnInteract()
{
	if (!IsFrontEnd())
	{
		ServerInteract();
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
