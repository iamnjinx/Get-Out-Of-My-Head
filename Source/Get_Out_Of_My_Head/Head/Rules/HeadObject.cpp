#include "HeadObject.h"
#include "HeadDataAssets.h"
#include "HeadRules.h"
#include "HeadGameState.h"
#include "UI/HeadNameTagWidget.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

AHeadObject::AHeadObject()
{
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

void AHeadObject::InitObject(UHeadObjectData* InData)
{
	ObjectData = InData;
	InitialTransform = GetActorTransform();
	bInitialized = true;

	ResetToInitial();
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

bool AHeadObject::CanBeUsed() const
{
	// attached objects cannot be targeted, objects without E only take pickup / attach
	return ObjectData && !bConsumed && !AttachedTo && !ObjectData->bNoInteract;
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

	SetActorTransform(InitialTransform, false, nullptr, ETeleportType::ResetPhysics);

	bOn = ObjectData && ObjectData->bInitiallyOn;
	Progress = 0.0f;
	bInteracted = false;
	bConsumed = false;
	AttachedTo = nullptr;

	ApplyLook();
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

	if (ObjectData->bToggle)
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

	// BasicShapeMaterial exposes a "Color" parameter. Toggles show their state by brightness
	if (UMaterialInstanceDynamic* Material = Mesh->CreateAndSetMaterialInstanceDynamic(0))
	{
		const bool bDim = ObjectData->bToggle && !bOn;
		Material->SetVectorParameterValue(TEXT("Color"), bDim ? ObjectData->Color * 0.25f : ObjectData->Color);
	}

	// large furniture and closed doors block the body, everything else lets it through
	const bool bBlocksBody = !ObjectData->bCarryable && (ObjectData->bLargeFurniture || ObjectData->bDoor) && !(ObjectData->bDoor && bOn);
	Mesh->SetCollisionResponseToChannel(ECC_Pawn, bBlocksBody ? ECR_Block : ECR_Ignore);

	SetActorHiddenInGame(bConsumed);
	Mesh->SetCollisionEnabled(bConsumed ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryAndPhysics);

	NameTag->SetRelativeLocation(FVector(0.0f, 0.0f, 100.0f * ObjectData->MeshScale.Z + 30.0f));
}

void AHeadObject::ReleaseAttachments()
{
	// a vanishing host drops its attachments where it was
	for (AHeadObject* Attachment : Attachments)
	{
		if (Attachment)
		{
			Attachment->AttachedTo = nullptr;
			Attachment->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
			Attachment->ApplyLook();
		}
	}

	Attachments.Empty();
}
