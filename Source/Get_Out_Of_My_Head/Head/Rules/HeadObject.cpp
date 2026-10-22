#include "HeadObject.h"
#include "HeadDataAssets.h"
#include "HeadRules.h"
#include "HeadInteraction.h"
#include "HeadGameState.h"
#include "UI/HeadNameTagWidget.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/Character.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	/** Hand bone of the template mannequin */
	const FName HandSocket(TEXT("hand_r"));

	/** Space between attachments on a host */
	constexpr float AttachmentSpacing = 25.0f;
}

AHeadObject::AHeadObject()
{
	// ticks only while flying
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicatingMovement(true);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> DefaultMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(RootComponent);
	Mesh->SetStaticMesh(DefaultMesh.Object);

	NameTag = CreateDefaultSubobject<UWidgetComponent>(TEXT("NameTag"));
	NameTag->SetupAttachment(RootComponent);
	NameTag->SetWidgetSpace(EWidgetSpace::Screen);
	NameTag->SetWidgetClass(UHeadNameTagWidget::StaticClass());
	NameTag->SetDrawAtDesiredSize(true);
	NameTag->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AHeadObject::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AHeadObject, ObjectData);
	DOREPLIFETIME(AHeadObject, bOn);
	DOREPLIFETIME(AHeadObject, Progress);
	DOREPLIFETIME(AHeadObject, bInteracted);
	DOREPLIFETIME(AHeadObject, bConsumed);
	DOREPLIFETIME(AHeadObject, bHeld);
	DOREPLIFETIME(AHeadObject, bFlying);
	DOREPLIFETIME(AHeadObject, AttachedTo);
	DOREPLIFETIME(AHeadObject, Attachments);
}

void AHeadObject::BeginPlay()
{
	Super::BeginPlay();

	if (UHeadNameTagWidget* Widget = Cast<UHeadNameTagWidget>(NameTag->GetUserWidgetObject()))
	{
		Widget->SetOwningObject(this);
	}

	// objects placed in the level get their first state here
	if (HasAuthority() && ObjectData && !bInitialized)
	{
		InitObject(ObjectData);
	}

	ApplyLook();
}

void AHeadObject::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bFlying || FlightPath.Num() < 2)
	{
		return;
	}

	FlightTime += DeltaSeconds;
	const float Alpha = FMath::Clamp(FlightTime / FMath::Max(FlightDuration, 0.01f), 0.0f, 1.0f);

	// move along the precomputed arc, which already stops in front of walls
	const float Position = Alpha * (FlightPath.Num() - 1);
	const int32 Segment = FMath::Min(FMath::FloorToInt(Position), FlightPath.Num() - 2);
	SetActorLocation(FMath::Lerp(FlightPath[Segment], FlightPath[Segment + 1], Position - Segment));

	if (Alpha >= 1.0f)
	{
		Land();
	}
}

void AHeadObject::InitObject(UHeadObjectData* InData)
{
	ObjectData = InData;
	InitialTransform = GetActorTransform();
	bInitialized = true;

	ResetToInitial();
}

FBox AHeadObject::GetObjectBounds() const
{
	return Mesh->Bounds.GetBox();
}

bool AHeadObject::HasAttachment(const UHeadObjectData* Data) const
{
	return Attachments.ContainsByPredicate([Data](const AHeadObject* Attachment) { return Attachment && Attachment->ObjectData == Data; });
}

bool AHeadObject::ContainsObjectData(const UHeadObjectData* Data) const
{
	if (ObjectData == Data)
	{
		return true;
	}

	for (const AHeadObject* Attachment : Attachments)
	{
		if (Attachment && Attachment->ContainsObjectData(Data))
		{
			return true;
		}
	}

	return false;
}

bool AHeadObject::CanBeTargeted() const
{
	return ObjectData && !bConsumed && !AttachedTo && !bHeld && !bFlying;
}

bool AHeadObject::CanBeUsed() const
{
	// attached objects are not interaction targets, objects without E only take pickup / attach
	return ObjectData && !bConsumed && !AttachedTo && !bFlying && !ObjectData->bNoInteract;
}

bool AHeadObject::IsOpenDoor() const
{
	return ObjectData && ObjectData->bDoor && bOn;
}

void AHeadObject::SetProgress(float NewProgress)
{
	Progress = FMath::Clamp(NewProgress, 0.0f, 1.0f);
}

void AHeadObject::CompleteInteraction(const FHeadRuleContext& Context)
{
	HeadRules::ApplyEffects(Context, ObjectData->Effects);

	if (ObjectData->bToggle)
	{
		bOn = !bOn;
		HeadRules::ApplyEffects(Context, bOn ? ObjectData->OnEffects : ObjectData->OffEffects);
		ApplyLook();
	}

	bInteracted = true;

	// hold objects can be used again after completing
	Progress = 0.0f;

	if (ObjectData->bConsumeOnUse)
	{
		Consume();
	}
}

void AHeadObject::Consume()
{
	ReleaseAttachments();
	DetachEverything();

	if (ObjectData && ObjectData->bRespawn)
	{
		ResetToInitial();
		return;
	}

	bConsumed = true;
	ApplyLook();
}

void AHeadObject::ResetToInitial()
{
	ReleaseAttachments();
	DetachEverything();

	SetActorTransform(InitialTransform, false, nullptr, ETeleportType::ResetPhysics);

	bOn = ObjectData && ObjectData->bInitiallyOn;
	Progress = 0.0f;
	bInteracted = false;
	bConsumed = false;

	ApplyLook();
}

void AHeadObject::PickUp(ACharacter* Body)
{
	DetachEverything();

	bHeld = true;

	const FAttachmentTransformRules Rules(EAttachmentRule::SnapToTarget, EAttachmentRule::SnapToTarget, EAttachmentRule::KeepWorld, false);
	AttachToComponent(Body->GetMesh(), Rules, HandSocket);

	ApplyLook();
}

void AHeadObject::DropAt(const FVector& Location)
{
	DetachEverything();

	SetActorLocationAndRotation(Location, FRotator(0.0f, GetActorRotation().Yaw, 0.0f), false, nullptr, ETeleportType::TeleportPhysics);

	ApplyLook();
}

void AHeadObject::AttachToHost(AHeadObject* Host)
{
	DetachEverything();

	AttachedTo = Host;
	Host->Attachments.Add(this);

	AttachToActor(Host, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	Host->LayoutAttachments();

	ApplyLook();
}

void AHeadObject::DetachFromHost()
{
	if (AHeadObject* Host = AttachedTo)
	{
		Host->Attachments.Remove(this);
		Host->LayoutAttachments();
	}

	AttachedTo = nullptr;
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

	ApplyLook();
}

void AHeadObject::StartThrow(const TArray<FVector>& Path, float Duration, bool bBreak)
{
	DetachEverything();

	FlightPath = Path;
	FlightDuration = Duration;
	FlightTime = 0.0f;
	bBreakOnLanding = bBreak;
	bFlying = true;

	SetActorTickEnabled(true);
	ApplyLook();
}

void AHeadObject::SetHighlighted(bool bInHighlighted)
{
	if (bHighlighted != bInHighlighted)
	{
		bHighlighted = bInHighlighted;
		ApplyLook();
	}
}

FText AHeadObject::GetNameTagText() const
{
	if (!ObjectData || bConsumed || AttachedTo)
	{
		return FText::GetEmpty();
	}

	FString Text = ObjectData->DisplayName.ToString();

	// attachments are public: 바구니(+수건, 알)
	TArray<FString> AttachmentNames;
	for (const AHeadObject* Attachment : Attachments)
	{
		if (Attachment && Attachment->ObjectData)
		{
			AttachmentNames.Add(Attachment->ObjectData->DisplayName.ToString());
		}
	}

	if (AttachmentNames.Num() > 0)
	{
		Text += FString::Printf(TEXT("(+%s)"), *FString::Join(AttachmentNames, TEXT(", ")));
	}

	if (ObjectData->LabelVariable)
	{
		const AHeadGameState* GS = GetWorld() ? GetWorld()->GetGameState<AHeadGameState>() : nullptr;
		Text += FString::Printf(TEXT(" %d%s"), GS ? GS->GetVariable(ObjectData->LabelVariable) : ObjectData->LabelVariable->InitialValue, *ObjectData->LabelVariable->Unit.ToString());
	}

	if (ObjectData->bDoor)
	{
		Text += bOn ? TEXT(" [열림]") : TEXT(" [닫힘]");
	}
	else if (ObjectData->bToggle)
	{
		Text += bOn ? TEXT(" [켜짐]") : TEXT(" [꺼짐]");
	}

	// progress bar, public to everyone
	if (Progress > 0.0f)
	{
		constexpr int32 BarLength = 10;
		const int32 Filled = FMath::Clamp(FMath::RoundToInt(Progress * BarLength), 0, BarLength);
		Text += TEXT("\n") + FString::ChrN(Filled, TEXT('|')) + FString::ChrN(BarLength - Filled, TEXT('.'));
	}

	return FText::FromString(Text);
}

void AHeadObject::OnRep_ObjectData()
{
	ApplyLook();
}

void AHeadObject::OnRep_PublicState()
{
	ApplyLook();
}

void AHeadObject::ApplyLook()
{
	if (!ObjectData)
	{
		return;
	}

	if (ObjectData->Mesh)
	{
		Mesh->SetStaticMesh(ObjectData->Mesh);
	}

	// basic shapes are 100 units with a centered pivot: stand the mesh on the actor origin
	Mesh->SetRelativeScale3D(ObjectData->MeshScale);
	Mesh->SetRelativeLocation(FVector(0.0f, 0.0f, 50.0f * ObjectData->MeshScale.Z));

	// open doors swing aside
	Mesh->SetRelativeRotation(FRotator(0.0f, IsOpenDoor() ? 90.0f : 0.0f, 0.0f));

	// BasicShapeMaterial exposes a "Color" parameter. Toggles show their state by brightness
	if (UMaterialInstanceDynamic* Material = Mesh->CreateAndSetMaterialInstanceDynamic(0))
	{
		FLinearColor Color = ObjectData->Color;

		if (ObjectData->bToggle && !ObjectData->bDoor && !bOn)
		{
			Color *= 0.25f;
		}

		// only the controller pointing at it sees the highlight
		if (bHighlighted)
		{
			Color = FMath::Lerp(Color, FLinearColor::White, 0.6f);
		}

		Material->SetVectorParameterValue(TEXT("Color"), Color);
	}

	// held, attached and flying objects do not collide, they move with their parent
	const bool bLoose = !bConsumed && !bHeld && !bFlying && !AttachedTo;
	Mesh->SetCollisionEnabled(bLoose ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);

	// large furniture and closed doors block the body, everything else lets it through
	const bool bBlocksBody = !ObjectData->bCarryable && (ObjectData->bLargeFurniture || ObjectData->bDoor) && !IsOpenDoor();
	Mesh->SetCollisionResponseToChannel(ECC_Pawn, bBlocksBody ? ECR_Block : ECR_Ignore);

	SetActorHiddenInGame(bConsumed);

	NameTag->SetRelativeLocation(FVector(0.0f, 0.0f, GetHeight() + 30.0f));
}

void AHeadObject::LayoutAttachments()
{
	// side by side on top of the host
	const float Top = GetHeight();
	const float Start = -0.5f * AttachmentSpacing * (Attachments.Num() - 1);

	for (int32 Index = 0; Index < Attachments.Num(); ++Index)
	{
		if (AHeadObject* Attachment = Attachments[Index])
		{
			Attachment->SetActorRelativeLocation(FVector(0.0f, Start + Index * AttachmentSpacing, Top));
			Attachment->SetActorRelativeRotation(FRotator::ZeroRotator);
		}
	}
}

void AHeadObject::ReleaseAttachments()
{
	// a vanishing host drops its attachments where it was
	TArray<TObjectPtr<AHeadObject>> Released = Attachments;
	Attachments.Empty();

	for (AHeadObject* Attachment : Released)
	{
		if (Attachment)
		{
			Attachment->AttachedTo = nullptr;
			Attachment->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
			Attachment->SetActorLocation(HeadInteraction::FindFloor(GetWorld(), Attachment->GetActorLocation(), this));
			Attachment->ApplyLook();
		}
	}
}

void AHeadObject::DetachEverything()
{
	if (AttachedTo)
	{
		DetachFromHost();
	}

	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

	bHeld = false;
	bFlying = false;
	SetActorTickEnabled(false);
}

void AHeadObject::Land()
{
	bFlying = false;
	SetActorTickEnabled(false);

	// break-on-throw objects vanish without effect (respawning ones come back)
	if (bBreakOnLanding)
	{
		Consume();
		return;
	}

	SetActorLocation(HeadInteraction::FindFloor(GetWorld(), GetActorLocation(), this));
	ApplyLook();
}

float AHeadObject::GetHeight() const
{
	return ObjectData ? 100.0f * ObjectData->MeshScale.Z : 0.0f;
}
