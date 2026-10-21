#include "HeadGameState.h"
#include "HeadPlayerState.h"
#include "HeadPlayerController.h"
#include "Get_Out_Of_My_HeadCharacter.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

void AHeadGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AHeadGameState, Phase);
	DOREPLIFETIME(AHeadGameState, TurnOrder);
	DOREPLIFETIME(AHeadGameState, CurrentController);
	DOREPLIFETIME(AHeadGameState, NextController);
	DOREPLIFETIME(AHeadGameState, PhaseEndTime);
	DOREPLIFETIME(AHeadGameState, RoundEndTime);
	DOREPLIFETIME(AHeadGameState, EndReason);
	DOREPLIFETIME(AHeadGameState, Winner);
	DOREPLIFETIME(AHeadGameState, Body);
}

void AHeadGameState::MulticastNotice_Implementation(const FString& Text)
{
	// hand the notice to the local player of this machine
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		AHeadPlayerController* PC = Cast<AHeadPlayerController>(It->Get());
		if (PC && PC->IsLocalController())
		{
			PC->AddNotice(Text);
		}
	}
}

double AHeadGameState::GetSecondsUntil(double ServerTime) const
{
	return FMath::Max(0.0, ServerTime - GetServerWorldTimeSeconds());
}
