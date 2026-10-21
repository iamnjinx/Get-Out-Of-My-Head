#include "HeadGameMode.h"
#include "HeadGameState.h"
#include "HeadPlayerState.h"
#include "HeadPlayerController.h"
#include "MissionButton.h"
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

	// players only ever control the shared body
	DefaultPawnClass = nullptr;

	MissionButtonClass = AMissionButton::StaticClass();

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

	SpawnBody();
	SpawnMissionButtons();

	GetHeadGameState()->Phase = EHeadPhase::Lobby;
}

void AHeadGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bFrontEnd || !IsRoundRunning())
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
	}

	Super::Logout(Exiting);
}

void AHeadGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	// no RestartPlayer: nobody gets a pawn of their own, the controller watches the body
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

void AHeadGameMode::HandleInteract(AHeadPlayerController* PC)
{
	AHeadGameState* GS = GetHeadGameState();

	if (bFrontEnd || GS->Phase != EHeadPhase::Playing || !Body || Body->GetController() != PC)
	{
		return;
	}

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
		PC->ClientNotice(TEXT("가까이에 누를 버튼이 없습니다."));
		return;
	}

	Closest->MulticastPress();

	// pressing someone's mission button completes their mission, whoever is in control
	for (AHeadPlayerState* PS : GS->TurnOrder)
	{
		if (PS && PS->GetMissionButtonIndex() == Closest->GetButtonIndex())
		{
			EndRound(EHeadEndReason::MissionComplete, PS);
			return;
		}
	}

	PC->ClientNotice(FString::Printf(TEXT("버튼 %s: 아무 일도 일어나지 않았다."), *AMissionButton::GetLabel(Closest->GetButtonIndex())));
}

AHeadGameState* AHeadGameMode::GetHeadGameState() const
{
	return GetGameState<AHeadGameState>();
}

double AHeadGameMode::Now() const
{
	return GetHeadGameState()->GetServerWorldTimeSeconds();
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

	for (int32 Index = 0; Index < Players.Num(); ++Index)
	{
		Players[Index]->SetMission(ButtonIndices[Index]);
	}

	ResetBody();

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
	if (!Body)
	{
		return;
	}

	if (AController* Controller = Body->GetController())
	{
		Controller->UnPossess();
	}

	Body->StopJumping();
	Body->GetCharacterMovement()->StopMovementImmediately();
}

bool AHeadGameMode::IsRoundRunning() const
{
	const AHeadGameState* GS = GetHeadGameState();
	return GS && (GS->Phase == EHeadPhase::Playing || GS->Phase == EHeadPhase::Handover);
}
