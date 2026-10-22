#include "HeadMindRoom.h"
#include "UI/HeadScreenWidget.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	/** Panel thickness */
	constexpr float MindRoomPanelThickness = 20.0f;
}

AHeadMindRoom::AHeadMindRoom()
{
	bReplicates = true;
	bAlwaysRelevant = true;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	const float Half = RoomSizeXY * 0.5f;
	const float Span = RoomSizeXY + MindRoomPanelThickness * 2.0f;

	Floor = MakePanel(TEXT("Floor"), FVector(0.0f, 0.0f, -MindRoomPanelThickness * 0.5f), FVector(Span, Span, MindRoomPanelThickness), FLinearColor(0.12f, 0.1f, 0.14f));
	Ceiling = MakePanel(TEXT("Ceiling"), FVector(0.0f, 0.0f, RoomHeight + MindRoomPanelThickness * 0.5f), FVector(Span, Span, MindRoomPanelThickness), FLinearColor(0.08f, 0.07f, 0.1f));

	const FLinearColor WallColor(0.25f, 0.2f, 0.28f);
	Walls.Add(MakePanel(TEXT("WallFront"), FVector(Half + MindRoomPanelThickness * 0.5f, 0.0f, RoomHeight * 0.5f), FVector(MindRoomPanelThickness, Span, RoomHeight), WallColor * 0.5f));
	Walls.Add(MakePanel(TEXT("WallBack"), FVector(-Half - MindRoomPanelThickness * 0.5f, 0.0f, RoomHeight * 0.5f), FVector(MindRoomPanelThickness, Span, RoomHeight), WallColor));
	Walls.Add(MakePanel(TEXT("WallLeft"), FVector(0.0f, -Half - MindRoomPanelThickness * 0.5f, RoomHeight * 0.5f), FVector(Span, MindRoomPanelThickness, RoomHeight), WallColor));
	Walls.Add(MakePanel(TEXT("WallRight"), FVector(0.0f, Half + MindRoomPanelThickness * 0.5f, RoomHeight * 0.5f), FVector(Span, MindRoomPanelThickness, RoomHeight), WallColor));

	LightA = CreateDefaultSubobject<UPointLightComponent>(TEXT("LightA"));
	LightA->SetupAttachment(RootComponent);
	LightA->SetRelativeLocation(FVector(-Half * 0.4f, 0.0f, RoomHeight - 60.0f));
	LightA->SetIntensity(8000.0f);
	LightA->SetAttenuationRadius(1500.0f);

	LightB = CreateDefaultSubobject<UPointLightComponent>(TEXT("LightB"));
	LightB->SetupAttachment(RootComponent);
	LightB->SetRelativeLocation(FVector(Half * 0.3f, 0.0f, RoomHeight - 60.0f));
	LightB->SetIntensity(4000.0f);
	LightB->SetAttenuationRadius(1200.0f);

	// 6m wide 16:9 screen on the front wall, facing into the room
	constexpr float ScreenMeters = 600.0f;
	Screen = CreateDefaultSubobject<UWidgetComponent>(TEXT("Screen"));
	Screen->SetupAttachment(RootComponent);
	Screen->SetWidgetSpace(EWidgetSpace::World);
	Screen->SetWidgetClass(UHeadScreenWidget::StaticClass());
	Screen->SetDrawSize(FVector2D(ScreenWidth, ScreenHeight));
	Screen->SetBlendMode(EWidgetBlendMode::Opaque);
	Screen->SetRelativeLocation(FVector(Half - 2.0f, 0.0f, RoomHeight * 0.5f + 20.0f));
	Screen->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
	Screen->SetRelativeScale3D(FVector(1.0f, ScreenMeters / ScreenWidth, ScreenMeters / ScreenWidth));
	Screen->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AHeadMindRoom::BeginPlay()
{
	Super::BeginPlay();

	// BasicShapeMaterial exposes a "Color" parameter
	for (int32 Index = 0; Index < Panels.Num() && Index < PanelColors.Num(); ++Index)
	{
		if (UMaterialInstanceDynamic* Material = Panels[Index]->CreateAndSetMaterialInstanceDynamic(0))
		{
			Material->SetVectorParameterValue(TEXT("Color"), PanelColors[Index]);
		}
	}

	// every machine renders its own screen
	if (GetNetMode() != NM_DedicatedServer)
	{
		ScreenTarget = NewObject<UTextureRenderTarget2D>(this, TEXT("HeadScreenTarget"));
		ScreenTarget->RenderTargetFormat = RTF_RGBA8_SRGB;
		ScreenTarget->ClearColor = FLinearColor::Black;
		ScreenTarget->InitAutoFormat(ScreenWidth, ScreenHeight);
		ScreenTarget->UpdateResourceImmediate(true);

		if (UHeadScreenWidget* Widget = Cast<UHeadScreenWidget>(Screen->GetUserWidgetObject()))
		{
			Widget->SetScreenTexture(ScreenTarget);
		}
	}
}

FTransform AHeadMindRoom::GetAvatarSpawnTransform(int32 Index) const
{
	// a loose ring facing the screen
	const FVector Offset = FRotator(0.0f, 90.0f * Index + 45.0f, 0.0f).Vector() * 220.0f;
	const FVector Location = GetActorLocation() + FVector(-150.0f, 0.0f, 100.0f) + Offset;

	return FTransform(FRotator::ZeroRotator, Location);
}

UStaticMeshComponent* AHeadMindRoom::MakePanel(FName Name, const FVector& Location, const FVector& Size, const FLinearColor& Color)
{
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));

	UStaticMeshComponent* Panel = CreateDefaultSubobject<UStaticMeshComponent>(Name);
	Panel->SetupAttachment(RootComponent);
	Panel->SetStaticMesh(CubeMesh.Object);
	Panel->SetRelativeLocation(Location);
	Panel->SetRelativeScale3D(Size / 100.0f);

	Panels.Add(Panel);
	PanelColors.Add(Color);

	return Panel;
}
