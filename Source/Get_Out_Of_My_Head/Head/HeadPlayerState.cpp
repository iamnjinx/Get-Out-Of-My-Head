#include "HeadPlayerState.h"
#include "Net/UnrealNetwork.h"

void AHeadPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(AHeadPlayerState, MissionButtonIndex, COND_OwnerOnly);
	DOREPLIFETIME(AHeadPlayerState, RevealedMissionButtonIndex);
}

void AHeadPlayerState::SetMission(int32 ButtonIndex)
{
	MissionButtonIndex = ButtonIndex;
	RevealedMissionButtonIndex = INDEX_NONE;
}

void AHeadPlayerState::RevealMission()
{
	RevealedMissionButtonIndex = MissionButtonIndex;
}

void AHeadPlayerState::ClearMission()
{
	MissionButtonIndex = INDEX_NONE;
	RevealedMissionButtonIndex = INDEX_NONE;
}
