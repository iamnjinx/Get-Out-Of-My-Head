#include "HeadBodyComponent.h"
#include "HeadDataAssets.h"
#include "HeadObject.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

UHeadBodyComponent::UHeadBodyComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
}

void UHeadBodyComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// body statuses are only visible to whoever controls the body
	DOREPLIFETIME_CONDITION(UHeadBodyComponent, Statuses, COND_OwnerOnly);
	DOREPLIFETIME(UHeadBodyComponent, FallenUntil);
	DOREPLIFETIME(UHeadBodyComponent, HeldObject);
}

void UHeadBodyComponent::BeginPlay()
{
	Super::BeginPlay();

	if (UCharacterMovementComponent* Movement = GetMovement())
	{
		BaseWalkSpeed = Movement->MaxWalkSpeed;
	}
}

void UHeadBodyComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (GetOwner()->HasAuthority())
	{
		UpdateExpiry();
		UpdateFalling(DeltaTime);
	}

	// the server and the controlling client both simulate movement, keep them in sync
	UpdateMovement();
}

void UHeadBodyComponent::AddStatus(UHeadStatusData* Status, int32 Stacks)
{
	if (!Status || Stacks <= 0)
	{
		return;
	}

	const double Expire = Status->Duration > 0.0f ? Now() + Status->Duration : 0.0;

	FHeadStatusInstance* Existing = Statuses.FindByPredicate([Status](const FHeadStatusInstance& Instance) { return Instance.Status == Status; });
	if (!Existing)
	{
		FHeadStatusInstance& Instance = Statuses.AddDefaulted_GetRef();
		Instance.Status = Status;
		Instance.Stacks = Status->bStackable ? Stacks : 1;
		Instance.ExpireTime = Expire;
		return;
	}

	if (Status->bStackable)
	{
		// new stacks do not restart the timer
		Existing->Stacks += Stacks;
	}
	else
	{
		Existing->ExpireTime = Expire;
	}
}

void UHeadBodyComponent::RemoveStatus(UHeadStatusData* Status)
{
	Statuses.RemoveAll([Status](const FHeadStatusInstance& Instance) { return Instance.Status == Status; });
}

void UHeadBodyComponent::ResetBody()
{
	Statuses.Empty();
	FallenUntil = 0.0;
	bWasMoving = false;
	MoveTimeSinceRoll = 0.0f;
}

int32 UHeadBodyComponent::GetStacks(const UHeadStatusData* Status) const
{
	const FHeadStatusInstance* Existing = Statuses.FindByPredicate([Status](const FHeadStatusInstance& Instance) { return Instance.Status == Status; });
	return Existing ? Existing->Stacks : 0;
}

bool UHeadBodyComponent::IsFallen() const
{
	return FallenUntil > Now();
}

bool UHeadBodyComponent::IsImmobile() const
{
	if (IsFallen())
	{
		return true;
	}

	for (const FHeadStatusInstance& Instance : Statuses)
	{
		const FHeadStatusTier* Tier = Instance.Status ? Instance.Status->FindTier(Instance.Stacks) : nullptr;
		if (Tier && Tier->bImmobile)
		{
			return true;
		}
	}

	return false;
}

float UHeadBodyComponent::GetSpeedMultiplier() const
{
	float Multiplier = 1.0f;

	for (const FHeadStatusInstance& Instance : Statuses)
	{
		if (const FHeadStatusTier* Tier = Instance.Status ? Instance.Status->FindTier(Instance.Stacks) : nullptr)
		{
			Multiplier *= Tier->SpeedMultiplier;
		}
	}

	return Multiplier;
}

void UHeadBodyComponent::SetHeldObject(AHeadObject* NewHeld)
{
	HeldObject = NewHeld;
}

double UHeadBodyComponent::Now() const
{
	const AGameStateBase* GameState = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	return GameState ? GameState->GetServerWorldTimeSeconds() : 0.0;
}

void UHeadBodyComponent::UpdateMovement()
{
	if (UCharacterMovementComponent* Movement = GetMovement())
	{
		Movement->MaxWalkSpeed = IsImmobile() ? 0.0f : BaseWalkSpeed * GetSpeedMultiplier();
	}
}

void UHeadBodyComponent::UpdateExpiry()
{
	const double Time = Now();

	for (int32 Index = Statuses.Num() - 1; Index >= 0; --Index)
	{
		FHeadStatusInstance& Instance = Statuses[Index];
		if (Instance.ExpireTime <= 0.0 || Instance.ExpireTime > Time)
		{
			continue;
		}

		if (Instance.Status && Instance.Status->bStackable && Instance.Status->StackExpiry == EHeadStackExpiry::OneByOne && Instance.Stacks > 1)
		{
			--Instance.Stacks;
			Instance.ExpireTime += Instance.Status->Duration;
		}
		else
		{
			Statuses.RemoveAt(Index);
		}
	}
}

void UHeadBodyComponent::UpdateFalling(float DeltaTime)
{
	const UCharacterMovementComponent* Movement = GetMovement();
	const bool bMoving = Movement && !IsFallen() && Movement->Velocity.Size2D() > 10.0f && Movement->IsMovingOnGround();

	if (!bMoving)
	{
		bWasMoving = false;
		return;
	}

	bool bRoll = false;

	if (!bWasMoving)
	{
		// first roll the moment the body starts moving
		bRoll = true;
		MoveTimeSinceRoll = 0.0f;
	}
	else
	{
		MoveTimeSinceRoll += DeltaTime;
		if (MoveTimeSinceRoll >= FallRollInterval)
		{
			bRoll = true;
			MoveTimeSinceRoll -= FallRollInterval;
		}
	}

	bWasMoving = true;

	if (bRoll && FMath::FRand() < GetFallChance())
	{
		Fall();
	}
}

float UHeadBodyComponent::GetFallChance() const
{
	float StayChance = 1.0f;

	for (const FHeadStatusInstance& Instance : Statuses)
	{
		if (const FHeadStatusTier* Tier = Instance.Status ? Instance.Status->FindTier(Instance.Stacks) : nullptr)
		{
			StayChance *= 1.0f - Tier->FallChance;
		}
	}

	return 1.0f - StayChance;
}

void UHeadBodyComponent::Fall()
{
	FallenUntil = Now() + FallSeconds;
	bWasMoving = false;

	if (UCharacterMovementComponent* Movement = GetMovement())
	{
		Movement->StopMovementImmediately();
	}

	// the interaction code drops the held object and breaks the hold
	OnFell.Broadcast();
}

UCharacterMovementComponent* UHeadBodyComponent::GetMovement() const
{
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	return Character ? Character->GetCharacterMovement() : nullptr;
}
