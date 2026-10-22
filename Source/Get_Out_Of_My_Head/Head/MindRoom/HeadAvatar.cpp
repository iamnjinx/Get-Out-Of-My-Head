#include "HeadAvatar.h"
#include "HeadGameState.h"
#include "UI/HeadNameTagWidget.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "EnhancedInputComponent.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/SpringArmComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

AHeadAvatar::AHeadAvatar()
{
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(35.0f, 80.0f);

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 600.0f, 0.0f);
	GetCharacterMovement()->MaxWalkSpeed = 350.0f;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));

	Shape = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Shape"));
	Shape->SetupAttachment(RootComponent);
	Shape->SetStaticMesh(CylinderMesh.Object);
	Shape->SetRelativeScale3D(FVector(0.6f, 0.6f, 1.6f));
	Shape->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 300.0f;
	CameraBoom->SocketOffset = FVector(0.0f, 0.0f, 60.0f);
	CameraBoom->bUsePawnControlRotation = true;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);

	NameTag = CreateDefaultSubobject<UWidgetComponent>(TEXT("NameTag"));
	NameTag->SetupAttachment(RootComponent);
	NameTag->SetWidgetSpace(EWidgetSpace::World);
	NameTag->SetWidgetClass(UHeadNameTagWidget::StaticClass());
	NameTag->SetDrawSize(FVector2D(400.0f, 80.0f));
	NameTag->SetRelativeLocation(FVector(0.0f, 0.0f, 120.0f));
	NameTag->SetRelativeScale3D(FVector(0.25f));
	NameTag->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// same actions as the body, from the template input assets
	static ConstructorHelpers::FObjectFinder<UInputAction> MoveFinder(TEXT("/Game/Input/Actions/IA_Move"));
	static ConstructorHelpers::FObjectFinder<UInputAction> LookFinder(TEXT("/Game/Input/Actions/IA_Look"));
	static ConstructorHelpers::FObjectFinder<UInputAction> MouseLookFinder(TEXT("/Game/Input/Actions/IA_MouseLook"));
	static ConstructorHelpers::FObjectFinder<UInputAction> JumpFinder(TEXT("/Game/Input/Actions/IA_Jump"));
	MoveAction = MoveFinder.Object;
	LookAction = LookFinder.Object;
	MouseLookAction = MouseLookFinder.Object;
	JumpAction = JumpFinder.Object;
}

void AHeadAvatar::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AHeadAvatar, OwnerPlayerState);
	DOREPLIFETIME(AHeadAvatar, ColorIndex);
	DOREPLIFETIME(AHeadAvatar, bAway);
}

void AHeadAvatar::BeginPlay()
{
	Super::BeginPlay();

	if (UHeadNameTagWidget* Widget = Cast<UHeadNameTagWidget>(NameTag->GetUserWidgetObject()))
	{
		TWeakObjectPtr<AHeadAvatar> WeakThis(this);
		Widget->SetTextSource([WeakThis]() { return WeakThis.IsValid() ? WeakThis->GetNameTagText() : FText::GetEmpty(); });
	}

	ApplyLook();
}

void AHeadAvatar::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// the nickname faces this machine's camera
	if (const APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		if (PC->PlayerCameraManager)
		{
			const FVector ToCamera = PC->PlayerCameraManager->GetCameraLocation() - NameTag->GetComponentLocation();
			NameTag->SetWorldRotation(FRotator(0.0f, ToCamera.Rotation().Yaw, 0.0f));
		}
	}
}

void AHeadAvatar::InitAvatar(APlayerState* InOwner, int32 InColorIndex)
{
	OwnerPlayerState = InOwner;
	ColorIndex = InColorIndex;
	ApplyLook();
}

void AHeadAvatar::SetAway(bool bInAway)
{
	bAway = bInAway;
	ApplyLook();
}

FText AHeadAvatar::GetNameTagText() const
{
	return OwnerPlayerState ? FText::FromString(OwnerPlayerState->GetPlayerName()) : FText::GetEmpty();
}

void AHeadAvatar::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AHeadAvatar::Move);
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AHeadAvatar::Look);
		EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &AHeadAvatar::Look);
	}
}

void AHeadAvatar::Move(const FInputActionValue& Value)
{
	if (!Controller || IsInputLocked())
	{
		return;
	}

	const FVector2D Input = Value.Get<FVector2D>();
	const FRotator YawRotation(0.0f, Controller->GetControlRotation().Yaw, 0.0f);

	AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X), Input.Y);
	AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y), Input.X);
}

void AHeadAvatar::Look(const FInputActionValue& Value)
{
	const FVector2D Input = Value.Get<FVector2D>();

	AddControllerYawInput(Input.X);
	AddControllerPitchInput(Input.Y);
}

bool AHeadAvatar::IsInputLocked() const
{
	const AHeadGameState* GS = GetWorld()->GetGameState<AHeadGameState>();
	return GS && GS->Phase == EHeadPhase::Handover;
}

void AHeadAvatar::OnRep_Look()
{
	ApplyLook();
}

void AHeadAvatar::ApplyLook()
{
	static const FLinearColor Colors[] = {
		FLinearColor(0.9f, 0.3f, 0.3f),
		FLinearColor(0.3f, 0.5f, 0.95f),
		FLinearColor(0.3f, 0.85f, 0.4f),
		FLinearColor(0.95f, 0.8f, 0.25f)
	};

	if (UMaterialInstanceDynamic* Material = Shape->CreateAndSetMaterialInstanceDynamic(0))
	{
		Material->SetVectorParameterValue(TEXT("Color"), Colors[FMath::Abs(ColorIndex) % UE_ARRAY_COUNT(Colors)]);
	}

	// gone from the room while its owner controls the body
	SetActorHiddenInGame(bAway);
	SetActorEnableCollision(!bAway);
}
