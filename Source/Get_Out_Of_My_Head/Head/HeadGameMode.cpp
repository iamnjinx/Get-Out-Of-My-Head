#include "HeadGameMode.h"
#include "HeadGameState.h"
#include "HeadPlayerState.h"
#include "HeadPlayerController.h"
#include "MissionButton.h"
#include "HeadObject.h"
#include "HeadDataAssets.h"
#include "HeadRules.h"
#include "HeadBodyComponent.h"
#include "HeadInteraction.h"
#include "HeadMindRoom.h"
#include "HeadAvatar.h"
#include "Get_Out_Of_My_HeadCharacter.h"
#include "Get_Out_Of_My_Head.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerStart.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "UObject/ConstructorHelpers.h"

AHeadGameMode::AHeadGameMode()
{
	PrimaryActorTick.bCanEverTick = true;

	GameStateClass = AHeadGameState::StaticClass();
	PlayerStateClass = AHeadPlayerState::StaticClass();
	PlayerControllerClass = AHeadPlayerController::StaticClass();

	// players control the shared body or their mind room avatar, spawned by hand
	DefaultPawnClass = nullptr;
	AvatarClass = AHeadAvatar::StaticClass();

	MissionButtonClass = AMissionButton::StaticClass();

	RuleSetAsset = TSoftObjectPtr<UHeadRuleSet>(FSoftObjectPath(TEXT("/Game/Head/Data/DA_RuleSet.DA_RuleSet")));

	static ConstructorHelpers::FClassFinder<AGet_Out_Of_My_HeadCharacter> BodyClassFinder(TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"));
	BodyClass = BodyClassFinder.Class;
}

void AHeadGameMode::StartPlay()
{
	Super::StartPlay();

	// a standalone world is the front end: no room, no round
	bFrontEnd = GetNetMode() == NM_Standalone;
	if (bFrontEnd)
	{
		return;
	}

	RuleSet = RuleSetAsset.LoadSynchronous();
	if (!RuleSet)
	{
		UE_LOG(LogGet_Out_Of_My_Head, Warning, TEXT("AHeadGameMode could not load the rule set %s. No objects, variables or missions."), *RuleSetAsset.ToString());
	}

	SpawnMindRoom();
	SpawnBody();
	SpawnMissionButtons();
	SpawnObjects();
	ResetWorld();

	// the host logged in before StartPlay
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		SpawnAvatar(It->Get());
	}

	if (UHeadBodyComponent* BodyComponent = Body ? Body->FindComponentByClass<UHeadBodyComponent>() : nullptr)
	{
		BodyComponent->OnFell.AddUObject(this, &AHeadGameMode::HandleBodyFell);
	}

	GetHeadGameState()->Phase = EHeadPhase::Lobby;
}

void AHeadGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bFrontEnd)
	{
		return;
	}

	UpdateHold(DeltaSeconds);

	if (!IsRoundRunning())
	{
		return;
	}

	AHeadGameState* GS = GetHeadGameState();

	// end conditions are checked every frame, mission completion is event driven
	if (GS->TurnOrder.Num() <= 1)
	{
		EndRound(EHeadEndReason::LastPlayer, nullptr);
	}
	else if (Now() >= GS->RoundEndTime)
	{
		EndRound(EHeadEndReason::TimeUp, nullptr);
	}
	else if (Now() >= GS->PhaseEndTime)
	{
		if (GS->Phase == EHeadPhase::Playing)
		{
			BeginHandover();
		}
		else
		{
			BeginTurn(NextTurnIndex);
		}
	}
}

void AHeadGameMode::PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
	Super::PreLogin(Options, Address, UniqueId, ErrorMessage);

	if (!ErrorMessage.IsEmpty())
	{
		return;
	}

	if (IsRoundRunning())
	{
		ErrorMessage = TEXT("판이 진행 중이라 참가할 수 없습니다. 판이 끝난 뒤 다시 시도해 주세요.");
	}
	else if (GameState && GameState->PlayerArray.Num() >= MaxPlayers)
	{
		ErrorMessage = TEXT("방이 가득 찼습니다.");
	}
}

void AHeadGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	if (!bFrontEnd && NewPlayer->PlayerState)
	{
		GetHeadGameState()->MulticastNotice(FString::Printf(TEXT("%s님이 들어왔습니다."), *NewPlayer->PlayerState->GetPlayerName()));
	}
}

void AHeadGameMode::Logout(AController* Exiting)
{
	if (!bFrontEnd)
	{
		// release the body first, a leaving player controller destroys its pawn
		if (Body && Body->GetController() == Exiting)
		{
			ReleaseBody();
		}

		if (AHeadPlayerState* PS = Exiting->GetPlayerState<AHeadPlayerState>())
		{
			GetHeadGameState()->MulticastNotice(FString::Printf(TEXT("%s님이 나갔습니다."), *PS->GetPlayerName()));

			if (IsRoundRunning())
			{
				RemoveFromTurnOrder(PS);
			}
		}

		if (APlayerController* PC = Cast<APlayerController>(Exiting))
		{
			TObjectPtr<AHeadAvatar> Avatar;
			if (Avatars.RemoveAndCopyValue(PC, Avatar) && Avatar)
			{
				Avatar->Destroy();
			}
		}
	}

	Super::Logout(Exiting);
}

void AHeadGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	// no RestartPlayer: players arrive in the mind room as their avatar
	if (MindRoom)
	{
		SpawnAvatar(NewPlayer);
	}
}

void AHeadGameMode::HandleHostAction(AHeadPlayerController* PC)
{
	// only the listen server's own player is the host
	if (bFrontEnd || !PC || !PC->IsLocalController())
	{
		return;
	}

	AHeadGameState* GS = GetHeadGameState();

	if (GS->Phase == EHeadPhase::Lobby)
	{
		if (GS->PlayerArray.Num() >= MinPlayers)
		{
			StartRound();
		}
		else
		{
			PC->ClientNotice(FString::Printf(TEXT("판을 시작하려면 %d명 이상 필요합니다."), MinPlayers));
		}
	}
	else if (GS->Phase == EHeadPhase::Results)
	{
		ReturnToLobby();
	}
}

void AHeadGameMode::HandleInteractPressed(AHeadPlayerController* PC, AHeadObject* AimTarget)
{
	if (!CanAct(PC) || HoldTarget)
	{
		return;
	}

	AHeadObject* Held = GetBodyComponent()->GetHeldObject();
	AHeadObject* Target = ValidateTarget(AimTarget);

	if (Held && Target)
	{
		// hand + target that fit together: attach (either way round)
		if (const FHeadAttachSlot* Slot = Target->GetObjectData()->FindAttachSlot(Held->GetObjectData()))
		{
			if (Target->HasAttachment(Held->GetObjectData()))
			{
				PC->ClientNotice(TEXT("이미 장착되어 있다"));
			}
			else
			{
				BeginAttach(PC, Target, Held, *Slot);
			}
			return;
		}

		if (const FHeadAttachSlot* Slot = Target->GetObjectData()->bCarryable ? Held->GetObjectData()->FindAttachSlot(Target->GetObjectData()) : nullptr)
		{
			if (Held->HasAttachment(Target->GetObjectData()))
			{
				PC->ClientNotice(TEXT("이미 장착되어 있다"));
			}
			else
			{
				BeginAttach(PC, Held, Target, *Slot);
			}
			return;
		}

		// not attachable: use the pointed object (a doll in hand + the bed)
		BeginUse(PC, Target);
	}
	else if (Held)
	{
		BeginUse(PC, Held);
	}
	else if (Target)
	{
		BeginUse(PC, Target);
	}
	else if (!TryPressMissionButton(PC))
	{
		PC->ClientNotice(TEXT("가리킨 대상이 없습니다."));
	}
}

void AHeadGameMode::HandleInteractReleased(AHeadPlayerController* PC)
{
	if (HoldTarget && Body && Body->GetController() == PC)
	{
		CancelHold();
	}
}

void AHeadGameMode::HandlePickUp(AHeadPlayerController* PC, AHeadObject* AimTarget)
{
	AHeadObject* Target = CanAct(PC) ? ValidateTarget(AimTarget) : nullptr;
	if (!Target || !Target->GetObjectData()->bCarryable)
	{
		return;
	}

	CancelHold();

	UHeadBodyComponent* BodyComponent = GetBodyComponent();

	// swap: what was in hand goes where the new object was
	if (AHeadObject* Held = BodyComponent->GetHeldObject())
	{
		Held->DropAt(Target->GetActorLocation());
	}

	Target->PickUp(Body);
	BodyComponent->SetHeldObject(Target);
}

void AHeadGameMode::HandleSecondaryPressed(AHeadPlayerController* PC)
{
	if (CanAct(PC) && GetBodyComponent()->GetHeldObject())
	{
		bSecondaryPressed = true;
		SecondaryPressTime = Now();
	}
}

void AHeadGameMode::HandleSecondaryReleased(AHeadPlayerController* PC)
{
	if (!bSecondaryPressed)
	{
		return;
	}

	bSecondaryPressed = false;

	if (!CanAct(PC) || !GetBodyComponent()->GetHeldObject())
	{
		return;
	}

	CancelHold();

	if (Now() - SecondaryPressTime >= GetBodyComponent()->ThrowHoldSeconds)
	{
		ThrowHeld(PC);
	}
	else
	{
		DropHeld();
	}
}

void AHeadGameMode::HandleTakeOut(AHeadPlayerController* PC, AHeadObject* AimTarget)
{
	if (!CanAct(PC))
	{
		return;
	}

	UHeadBodyComponent* BodyComponent = GetBodyComponent();
	AHeadObject* Held = BodyComponent->GetHeldObject();
	AHeadObject* Target = ValidateTarget(AimTarget);

	// the pointed host first, then what is in hand
	AHeadObject* Source = nullptr;
	if (Target && Target->GetAttachments().Num() > 0)
	{
		Source = Target;
	}
	else if (Held && Held->GetAttachments().Num() > 0)
	{
		Source = Held;
	}

	if (!Source)
	{
		return;
	}

	CancelHold();

	AHeadObject* Item = Source->GetAttachments().Last();
	Item->DetachFromHost();

	// empty hand takes it, otherwise it goes on the floor in front of the body
	if (!Held)
	{
		Item->PickUp(Body);
		BodyComponent->SetHeldObject(Item);
	}
	else
	{
		Item->DropAt(GetDropLocation());
	}
}

bool AHeadGameMode::TryPressMissionButton(AHeadPlayerController* PC)
{
	AHeadGameState* GS = GetHeadGameState();

	// find the closest button in reach
	AMissionButton* Closest = nullptr;
	float ClosestDistance = InteractRange;

	for (AMissionButton* Button : MissionButtons)
	{
		const float Distance = FVector::Dist2D(Body->GetActorLocation(), Button->GetActorLocation());
		if (Distance <= ClosestDistance)
		{
			Closest = Button;
			ClosestDistance = Distance;
		}
	}

	if (!Closest)
	{
		return false;
	}

	Closest->MulticastPress();

	// pressing someone's mission button completes their mission, whoever is in control
	for (AHeadPlayerState* PS : GS->TurnOrder)
	{
		if (PS && PS->GetMissionButtonIndex() == Closest->GetButtonIndex())
		{
			EndRound(EHeadEndReason::MissionComplete, PS);
			return true;
		}
	}

	PC->ClientNotice(FString::Printf(TEXT("버튼 %s: 아무 일도 일어나지 않았다."), *AMissionButton::GetLabel(Closest->GetButtonIndex())));

	return true;
}

AHeadGameState* AHeadGameMode::GetHeadGameState() const
{
	return GetGameState<AHeadGameState>();
}

double AHeadGameMode::Now() const
{
	return GetHeadGameState()->GetServerWorldTimeSeconds();
}

void AHeadGameMode::SpawnMindRoom()
{
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	MindRoom = GetWorld()->SpawnActor<AHeadMindRoom>(AHeadMindRoom::StaticClass(), FTransform(MindRoomLocation), SpawnParams);
}

void AHeadGameMode::SpawnAvatar(APlayerController* PC)
{
	if (!PC || !MindRoom || !AvatarClass || Avatars.Contains(PC))
	{
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	const int32 ColorIndex = NextAvatarColor++;
	AHeadAvatar* Avatar = GetWorld()->SpawnActor<AHeadAvatar>(AvatarClass, MindRoom->GetAvatarSpawnTransform(ColorIndex), SpawnParams);
	if (!Avatar)
	{
		return;
	}

	Avatar->InitAvatar(PC->PlayerState, ColorIndex);
	Avatars.Add(PC, Avatar);

	PC->Possess(Avatar);
	PC->ClientSetRotation(Avatar->GetActorRotation());
}

void AHeadGameMode::ReturnToAvatar(AController* Controller)
{
	APlayerController* PC = Cast<APlayerController>(Controller);
	AHeadAvatar* Avatar = PC ? Avatars.FindRef(PC) : nullptr;
	if (!Avatar)
	{
		return;
	}

	// back where the avatar was left
	Avatar->SetAway(false);
	PC->Possess(Avatar);
	PC->ClientSetRotation(Avatar->GetActorRotation());
}

void AHeadGameMode::SpawnBody()
{
	BodyStartTransform = FTransform(FVector(0.0f, 0.0f, 200.0f));

	for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
	{
		BodyStartTransform = It->GetActorTransform();
		break;
	}

	if (!BodyClass)
	{
		UE_LOG(LogGet_Out_Of_My_Head, Error, TEXT("AHeadGameMode has no BodyClass, the round cannot start."));
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	Body = GetWorld()->SpawnActor<AGet_Out_Of_My_HeadCharacter>(BodyClass, BodyStartTransform, SpawnParams);
	Body->bAlwaysRelevant = true;

	// keep falling and braking while nobody controls the body (handover)
	Body->GetCharacterMovement()->bRunPhysicsWithNoController = true;

	GetHeadGameState()->Body = Body;
}

void AHeadGameMode::SpawnMissionButtons()
{
	// every player needs a different button
	const int32 NumButtons = FMath::Max(NumMissionButtons, MaxPlayers);
	const FVector Center = BodyStartTransform.GetLocation();

	for (int32 Index = 0; Index < NumButtons; ++Index)
	{
		const FVector Direction = FRotator(0.0f, 360.0f * Index / NumButtons, 0.0f).Vector();
		FVector Location = Center + Direction * MissionButtonRingRadius;

		// drop the button onto the floor
		FHitResult Hit;
		if (GetWorld()->LineTraceSingleByChannel(Hit, Location + FVector(0.0f, 0.0f, 300.0f), Location - FVector(0.0f, 0.0f, 1000.0f), ECC_Visibility))
		{
			Location = Hit.ImpactPoint;
		}

		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		AMissionButton* Button = GetWorld()->SpawnActor<AMissionButton>(MissionButtonClass, Location, (-Direction).Rotation(), SpawnParams);
		Button->SetButtonIndex(Index);
		MissionButtons.Add(Button);
	}
}

void AHeadGameMode::StartRound()
{
	AHeadGameState* GS = GetHeadGameState();

	// random control order, public to everyone
	TArray<TObjectPtr<AHeadPlayerState>> Players;
	for (APlayerState* PlayerState : GS->PlayerArray)
	{
		AHeadPlayerState* PS = Cast<AHeadPlayerState>(PlayerState);
		if (PS && !PS->IsOnlyASpectator() && PS->GetOwner())
		{
			Players.Add(PS);
		}
	}

	for (int32 Index = Players.Num() - 1; Index > 0; --Index)
	{
		Players.Swap(Index, FMath::RandRange(0, Index));
	}

	GS->TurnOrder = Players;

	// secret missions, each player gets a different button
	TArray<int32> ButtonIndices;
	for (int32 Index = 0; Index < MissionButtons.Num(); ++Index)
	{
		ButtonIndices.Add(Index);
	}

	for (int32 Index = ButtonIndices.Num() - 1; Index > 0; --Index)
	{
		ButtonIndices.Swap(Index, FMath::RandRange(0, Index));
	}

	// mission pool, handed out again when there are more players than missions
	TArray<UHeadMissionData*> MissionPool;
	if (RuleSet)
	{
		for (UHeadMissionData* Mission : RuleSet->Missions)
		{
			if (Mission)
			{
				MissionPool.Add(Mission);
			}
		}
	}

	for (int32 Index = MissionPool.Num() - 1; Index > 0; --Index)
	{
		MissionPool.Swap(Index, FMath::RandRange(0, Index));
	}

	for (int32 Index = 0; Index < Players.Num(); ++Index)
	{
		UHeadMissionData* Mission = MissionPool.Num() > 0 ? MissionPool[Index % MissionPool.Num()] : nullptr;
		Players[Index]->SetMission(Mission, ButtonIndices[Index]);
	}

	ResetWorld();

	GS->EndReason = EHeadEndReason::None;
	GS->Winner = nullptr;
	GS->RoundEndTime = Now() + RoundSeconds;

	GS->MulticastNotice(TEXT("판이 시작되었습니다."));

	BeginTurn(0);
}

void AHeadGameMode::BeginTurn(int32 TurnIndex)
{
	AHeadGameState* GS = GetHeadGameState();

	CurrentTurnIndex = TurnIndex % GS->TurnOrder.Num();
	NextTurnIndex = INDEX_NONE;

	AHeadPlayerState* PS = GS->TurnOrder[CurrentTurnIndex];
	APlayerController* PC = Cast<APlayerController>(PS->GetOwner());

	ReleaseBody();

	GS->Phase = EHeadPhase::Playing;
	GS->CurrentController = PS;
	GS->NextController = nullptr;
	GS->PhaseEndTime = Now() + TurnSeconds;

	if (PC && Body)
	{
		// keep looking where the previous controller was looking
		const FRotator ViewRotation = Body->GetReplicatedViewRotation();

		PC->Possess(Body);
		PC->SetControlRotation(ViewRotation);
		PC->ClientSetRotation(ViewRotation);

		// the controller's avatar leaves the mind room
		if (AHeadAvatar* Avatar = Avatars.FindRef(PC))
		{
			Avatar->SetAway(true);
		}
	}
}

void AHeadGameMode::BeginHandover()
{
	AHeadGameState* GS = GetHeadGameState();

	// nobody controls the body during the handover
	ReleaseBody();

	NextTurnIndex = (CurrentTurnIndex + 1) % GS->TurnOrder.Num();

	GS->Phase = EHeadPhase::Handover;
	GS->NextController = GS->TurnOrder[NextTurnIndex];
	GS->PhaseEndTime = Now() + HandoverSeconds;
}

void AHeadGameMode::EndRound(EHeadEndReason Reason, AHeadPlayerState* InWinner)
{
	AHeadGameState* GS = GetHeadGameState();

	ReleaseBody();

	// reveal everyone's mission, including players who are not in the order anymore
	for (APlayerState* PlayerState : GS->PlayerArray)
	{
		if (AHeadPlayerState* PS = Cast<AHeadPlayerState>(PlayerState))
		{
			PS->RevealMission();
		}
	}

	GS->Phase = EHeadPhase::Results;
	GS->EndReason = Reason;
	GS->Winner = InWinner;
	GS->CurrentController = nullptr;
	GS->NextController = nullptr;

	CurrentTurnIndex = INDEX_NONE;
	NextTurnIndex = INDEX_NONE;
}

void AHeadGameMode::ReturnToLobby()
{
	AHeadGameState* GS = GetHeadGameState();

	for (APlayerState* PlayerState : GS->PlayerArray)
	{
		if (AHeadPlayerState* PS = Cast<AHeadPlayerState>(PlayerState))
		{
			PS->ClearMission();
		}
	}

	GS->Phase = EHeadPhase::Lobby;
	GS->TurnOrder.Empty();
	GS->EndReason = EHeadEndReason::None;
	GS->Winner = nullptr;

	ResetBody();
}

void AHeadGameMode::RemoveFromTurnOrder(AHeadPlayerState* Leaving)
{
	AHeadGameState* GS = GetHeadGameState();

	const int32 Removed = GS->TurnOrder.Find(Leaving);
	if (Removed == INDEX_NONE)
	{
		return;
	}

	const bool bWasControlling = GS->Phase == EHeadPhase::Playing && Removed == CurrentTurnIndex;

	GS->TurnOrder.RemoveAt(Removed);

	if (GS->TurnOrder.Num() <= 1)
	{
		EndRound(EHeadEndReason::LastPlayer, nullptr);
		return;
	}

	if (GS->Phase == EHeadPhase::Playing)
	{
		if (bWasControlling)
		{
			// the next player in line takes over right away
			BeginTurn(Removed);
		}
		else if (Removed < CurrentTurnIndex)
		{
			--CurrentTurnIndex;
		}
	}
	else
	{
		if (Removed == CurrentTurnIndex)
		{
			GS->CurrentController = nullptr;
		}

		if (Removed < CurrentTurnIndex)
		{
			--CurrentTurnIndex;
		}

		if (Removed < NextTurnIndex)
		{
			--NextTurnIndex;
		}

		NextTurnIndex %= GS->TurnOrder.Num();
		GS->NextController = GS->TurnOrder[NextTurnIndex];
	}
}

void AHeadGameMode::ResetBody()
{
	if (!Body)
	{
		return;
	}

	ReleaseBody();

	Body->TeleportTo(BodyStartTransform.GetLocation(), BodyStartTransform.Rotator());
	Body->SetReplicatedViewRotation(BodyStartTransform.Rotator());
}

void AHeadGameMode::ReleaseBody()
{
	// a turn ending breaks the hold
	CancelHold();
	bSecondaryPressed = false;

	if (!Body)
	{
		return;
	}

	if (AController* Controller = Body->GetController())
	{
		Controller->UnPossess();
		ReturnToAvatar(Controller);
	}

	Body->StopJumping();
	Body->GetCharacterMovement()->StopMovementImmediately();
}

bool AHeadGameMode::IsRoundRunning() const
{
	const AHeadGameState* GS = GetHeadGameState();
	return GS && (GS->Phase == EHeadPhase::Playing || GS->Phase == EHeadPhase::Handover);
}

void AHeadGameMode::SpawnObjects()
{
	if (!RuleSet)
	{
		return;
	}

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(Body);

	for (const FHeadObjectPlacement& Placement : RuleSet->Placements)
	{
		if (!Placement.Object)
		{
			continue;
		}

		FVector Location = BodyStartTransform.GetLocation() + Placement.Offset;

		// drop the object onto the floor
		FHitResult Hit;
		if (GetWorld()->LineTraceSingleByChannel(Hit, Location + FVector(0.0f, 0.0f, 300.0f), Location - FVector(0.0f, 0.0f, 1000.0f), ECC_Visibility, QueryParams))
		{
			Location = Hit.ImpactPoint;
		}

		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		AHeadObject* Object = GetWorld()->SpawnActor<AHeadObject>(AHeadObject::StaticClass(), Location, FRotator::ZeroRotator, SpawnParams);
		Object->InitObject(Placement.Object);
		Objects.Add(Object);
	}
}

void AHeadGameMode::ResetWorld()
{
	ResetBody();

	if (UHeadBodyComponent* BodyComponent = Body ? Body->FindComponentByClass<UHeadBodyComponent>() : nullptr)
	{
		BodyComponent->ResetBody();
		BodyComponent->SetHeldObject(nullptr);
	}

	for (AHeadObject* Object : Objects)
	{
		Object->ResetToInitial();
	}

	GetHeadGameState()->ResetVariables(RuleSet ? RuleSet->WorldVariables : TArray<TObjectPtr<UHeadWorldVariableData>>());
}

UHeadBodyComponent* AHeadGameMode::GetBodyComponent() const
{
	return Body ? Body->FindComponentByClass<UHeadBodyComponent>() : nullptr;
}

bool AHeadGameMode::CanAct(const AHeadPlayerController* PC) const
{
	const UHeadBodyComponent* BodyComponent = GetBodyComponent();

	return !bFrontEnd
		&& GetHeadGameState()->Phase == EHeadPhase::Playing
		&& PC && Body && Body->GetController() == PC
		&& BodyComponent && !BodyComponent->IsFallen();
}

AHeadObject* AHeadGameMode::ValidateTarget(AHeadObject* AimTarget) const
{
	const UHeadBodyComponent* BodyComponent = GetBodyComponent();
	return BodyComponent && HeadInteraction::IsTargetable(Body, AimTarget, BodyComponent->InteractRange) ? AimTarget : nullptr;
}

bool AHeadGameMode::IsHeldOrReachable(const AHeadObject* Object) const
{
	const UHeadBodyComponent* BodyComponent = GetBodyComponent();
	if (!Object || !BodyComponent)
	{
		return false;
	}

	if (Object == BodyComponent->GetHeldObject())
	{
		return true;
	}

	// a small margin so a hold does not break on the edge of the reach
	return Object->CanBeTargeted() && HeadInteraction::IsInReach(Body, Object, BodyComponent->InteractRange + 30.0f);
}

FHeadRuleContext AHeadGameMode::MakeContext(AHeadObject* Self, AHeadObject* Item) const
{
	FHeadRuleContext Context;
	Context.World = GetWorld();
	Context.GameState = GetHeadGameState();
	Context.Body = Body;
	Context.Self = Self;
	Context.Item = Item;
	return Context;
}

void AHeadGameMode::BeginUse(AHeadPlayerController* PC, AHeadObject* Object)
{
	if (!Object || !Object->CanBeUsed())
	{
		return;
	}

	// conditions are checked when starting, and again when a hold finishes
	FText FailText;
	const FHeadRuleContext Context = MakeContext(Object);
	if (!HeadRules::CheckConditions(Context, Object->GetObjectData()->Conditions, FailText))
	{
		// only the controller sees why
		PC->ClientNotice(FailText.ToString());
		return;
	}

	if (Object->GetObjectData()->IsHoldMode())
	{
		// the target is locked until the hold ends
		HoldTarget = Object;
		HoldItem = nullptr;
	}
	else
	{
		Object->CompleteInteraction(Context);
	}
}

void AHeadGameMode::BeginAttach(AHeadPlayerController* PC, AHeadObject* Host, AHeadObject* Item, const FHeadAttachSlot& Slot)
{
	FText FailText;
	if (!HeadRules::CheckConditions(MakeContext(Host, Item), Slot.Conditions, FailText))
	{
		PC->ClientNotice(FailText.ToString());
		return;
	}

	if (Slot.Mode == EHeadInteractMode::Instant)
	{
		FinishAttach(Host, Item, Slot);
	}
	else
	{
		HoldTarget = Host;
		HoldItem = Item;
	}
}

void AHeadGameMode::FinishAttach(AHeadObject* Host, AHeadObject* Item, const FHeadAttachSlot& Slot)
{
	UHeadBodyComponent* BodyComponent = GetBodyComponent();
	if (BodyComponent->GetHeldObject() == Item)
	{
		BodyComponent->SetHeldObject(nullptr);
	}

	Item->SetProgress(0.0f);
	Item->AttachToHost(Host);

	HeadRules::ApplyEffects(MakeContext(Host, Item), Slot.Effects);
}

FVector AHeadGameMode::GetDropLocation() const
{
	const FVector Front = Body->GetActorLocation() + Body->GetActorForwardVector() * 60.0f;
	return HeadInteraction::FindFloor(GetWorld(), Front, Body);
}

void AHeadGameMode::DropHeld()
{
	UHeadBodyComponent* BodyComponent = GetBodyComponent();
	AHeadObject* Held = BodyComponent ? BodyComponent->GetHeldObject() : nullptr;
	if (!Held)
	{
		return;
	}

	BodyComponent->SetHeldObject(nullptr);
	Held->DropAt(GetDropLocation());
}

void AHeadGameMode::ThrowHeld(const AHeadPlayerController* PC)
{
	UHeadBodyComponent* BodyComponent = GetBodyComponent();
	AHeadObject* Held = BodyComponent->GetHeldObject();

	// where the controller is looking, flattened
	const FVector Direction = FRotator(0.0f, PC->GetControlRotation().Yaw, 0.0f).Vector();
	const FVector Start = Held->GetActorLocation();
	const FVector End = HeadInteraction::FindFloor(GetWorld(), Start + Direction * BodyComponent->ThrowDistance, Body);

	// walls and large furniture stop the flight in front of them; loose objects and open doors do not
	FCollisionQueryParams Params(SCENE_QUERY_STAT(HeadThrow));
	Params.AddIgnoredActor(Body);

	TArray<AActor*> Carried;
	Body->GetAttachedActors(Carried, true, true);
	Params.AddIgnoredActors(Carried);

	for (AHeadObject* Object : Objects)
	{
		if (Object->GetObjectData()->bCarryable || Object->IsOpenDoor() || !(Object->GetObjectData()->bLargeFurniture || Object->GetObjectData()->bDoor))
		{
			Params.AddIgnoredActor(Object);
		}
	}

	constexpr int32 Samples = 16;
	TArray<FVector> Path;
	Path.Add(Start);

	for (int32 Index = 1; Index <= Samples; ++Index)
	{
		const float Alpha = static_cast<float>(Index) / Samples;
		const FVector Point = FMath::Lerp(Start, End, Alpha) + FVector(0.0f, 0.0f, 4.0f * BodyComponent->ThrowArcHeight * Alpha * (1.0f - Alpha));

		FHitResult Hit;
		if (GetWorld()->LineTraceSingleByChannel(Hit, Path.Last(), Point, ECC_Visibility, Params))
		{
			Path.Add(Hit.Location - (Point - Path.Last()).GetSafeNormal() * 20.0f);
			break;
		}

		Path.Add(Point);
	}

	const float Duration = FMath::Max(0.1f, BodyComponent->ThrowSeconds * (Path.Num() - 1) / Samples);

	BodyComponent->SetHeldObject(nullptr);
	Held->StartThrow(Path, Duration, Held->GetObjectData()->bBreakOnThrow);
}

void AHeadGameMode::UpdateHold(float DeltaSeconds)
{
	if (!HoldTarget)
	{
		return;
	}

	AHeadPlayerController* PC = Body ? Cast<AHeadPlayerController>(Body->GetController()) : nullptr;

	// breaks: turn over, nobody in control, fell, out of reach, object gone
	bool bValid = CanAct(PC) && IsHeldOrReachable(HoldTarget);
	bValid = bValid && (HoldItem ? IsHeldOrReachable(HoldItem) : HoldTarget->CanBeUsed());

	if (!bValid)
	{
		CancelHold();
		return;
	}

	// attach holds keep their progress on the item, use holds on the object
	const FHeadAttachSlot* Slot = HoldItem ? HoldTarget->GetObjectData()->FindAttachSlot(HoldItem->GetObjectData()) : nullptr;
	AHeadObject* ProgressObject = HoldItem ? HoldItem.Get() : HoldTarget.Get();
	const float HoldSeconds = FMath::Max(Slot ? Slot->HoldSeconds : HoldTarget->GetObjectData()->HoldSeconds, 0.01f);

	ProgressObject->SetProgress(ProgressObject->GetProgress() + DeltaSeconds / HoldSeconds);

	if (ProgressObject->GetProgress() < 1.0f)
	{
		return;
	}

	// conditions are checked again when the hold finishes (shampoo gone mid-shower fails)
	FText FailText;
	const FHeadRuleContext Context = MakeContext(HoldTarget, HoldItem);
	const TArray<FHeadCondition>& Conditions = Slot ? Slot->Conditions : HoldTarget->GetObjectData()->Conditions;

	if (!HeadRules::CheckConditions(Context, Conditions, FailText))
	{
		PC->ClientNotice(FailText.ToString());
		CancelHold();
		return;
	}

	AHeadObject* Target = HoldTarget;
	AHeadObject* Item = HoldItem;
	HoldTarget = nullptr;
	HoldItem = nullptr;

	if (Item && Slot)
	{
		FinishAttach(Target, Item, *Slot);
	}
	else
	{
		Target->CompleteInteraction(Context);
	}
}

void AHeadGameMode::CancelHold()
{
	if (!HoldTarget)
	{
		return;
	}

	// keep mode leaves the progress for the next player
	if (HoldItem)
	{
		const FHeadAttachSlot* Slot = HoldTarget->GetObjectData()->FindAttachSlot(HoldItem->GetObjectData());
		if (!Slot || Slot->Mode == EHeadInteractMode::HoldReset)
		{
			HoldItem->SetProgress(0.0f);
		}
	}
	else if (HoldTarget->GetObjectData()->Mode == EHeadInteractMode::HoldReset)
	{
		HoldTarget->SetProgress(0.0f);
	}

	HoldTarget = nullptr;
	HoldItem = nullptr;
}

void AHeadGameMode::HandleBodyFell()
{
	// falling breaks the hold and drops what is in hand
	CancelHold();
	DropHeld();

	GetHeadGameState()->MulticastNotice(TEXT("몸이 넘어졌다!"));
}
