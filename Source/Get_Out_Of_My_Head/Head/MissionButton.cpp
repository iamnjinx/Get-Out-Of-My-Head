#include "MissionButton.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	/** Cap height when released and pressed */
	constexpr float CapUpZ = 47.5f;
	constexpr float CapDownZ = 40.0f;
}

AMissionButton::AMissionButton()
{
	PrimaryActorTick.bCanEverTick = true;

	bReplicates = true;
	bAlwaysRelevant = true;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	Base = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Base"));
	Base->SetupAttachment(RootComponent);
	Base->SetStaticMesh(CubeMesh.Object);
	Base->SetRelativeLocation(FVector(0.0f, 0.0f, 20.0f));
	Base->SetRelativeScale3D(FVector(0.6f, 0.6f, 0.4f));

	Cap = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Cap"));
	Cap->SetupAttachment(RootComponent);
	Cap->SetStaticMesh(CylinderMesh.Object);
	Cap->SetRelativeScale3D(FVector(0.4f, 0.4f, 0.15f));
	Cap->SetRelativeLocation(FVector(0.0f, 0.0f, CapUpZ));
	Cap->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	Label->SetupAttachment(RootComponent);
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetVerticalAlignment(EVRTA_TextCenter);
	Label->SetWorldSize(60.0f);
	Label->SetRelativeLocation(FVector(0.0f, 0.0f, 130.0f));
}

void AMissionButton::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AMissionButton, ButtonIndex);
}

void AMissionButton::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// face the local camera so the letter is always readable
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		if (PC->PlayerCameraManager)
		{
			const FVector ToCamera = PC->PlayerCameraManager->GetCameraLocation() - Label->GetComponentLocation();
			Label->SetWorldRotation(FRotator(0.0f, ToCamera.Rotation().Yaw, 0.0f));
		}
	}
}

void AMissionButton::SetButtonIndex(int32 NewIndex)
{
	ButtonIndex = NewIndex;
	ApplyLook();
}

void AMissionButton::MulticastPress_Implementation()
{
	Cap->SetRelativeLocation(FVector(0.0f, 0.0f, CapDownZ));

	GetWorldTimerManager().SetTimer(ReleaseTimer, this, &AMissionButton::ReleaseCap, 0.2f, false);
}

FString AMissionButton::GetLabel(int32 Index)
{
	if (Index < 0)
	{
		return TEXT("?");
	}

	return FString::Chr(static_cast<TCHAR>(TEXT('A') + Index));
}

FLinearColor AMissionButton::GetColor(int32 Index)
{
	static const FLinearColor Colors[] = {
		FLinearColor(0.8f, 0.1f, 0.1f),
		FLinearColor(0.1f, 0.3f, 0.9f),
		FLinearColor(0.1f, 0.7f, 0.2f),
		FLinearColor(0.9f, 0.75f, 0.1f),
		FLinearColor(0.6f, 0.2f, 0.8f),
		FLinearColor(0.9f, 0.45f, 0.1f)
	};

	return Index >= 0 ? Colors[Index % UE_ARRAY_COUNT(Colors)] : FLinearColor::Gray;
}

void AMissionButton::OnRep_ButtonIndex()
{
	ApplyLook();
}

void AMissionButton::ApplyLook()
{
	Label->SetText(FText::FromString(GetLabel(ButtonIndex)));
	Label->SetTextRenderColor(GetColor(ButtonIndex).ToFColor(true));

	// BasicShapeMaterial exposes a "Color" parameter
	if (UMaterialInstanceDynamic* Material = Cap->CreateAndSetMaterialInstanceDynamic(0))
	{
		Material->SetVectorParameterValue(TEXT("Color"), GetColor(ButtonIndex));
	}
}

void AMissionButton::ReleaseCap()
{
	Cap->SetRelativeLocation(FVector(0.0f, 0.0f, CapUpZ));
}
