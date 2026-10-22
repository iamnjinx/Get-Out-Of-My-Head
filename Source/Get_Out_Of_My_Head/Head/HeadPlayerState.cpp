#include "HeadPlayerState.h"
#include "HeadDataAssets.h"
#include "Net/UnrealNetwork.h"

void AHeadPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(AHeadPlayerState, MissionButtonIndex, COND_OwnerOnly);
	DOREPLIFETIME(AHeadPlayerState, RevealedMissionButtonIndex);
	DOREPLIFETIME_CONDITION(AHeadPlayerState, Mission, COND_OwnerOnly);
	DOREPLIFETIME(AHeadPlayerState, RevealedMission);
}

void AHeadPlayerState::SetMission(UHeadMissionData* InMission, int32 ButtonIndex)
{
	Mission = InMission;
	MissionButtonIndex = ButtonIndex;
	RevealedMission = nullptr;
	RevealedMissionButtonIndex = INDEX_NONE;
}

void AHeadPlayerState::RevealMission()
{
	RevealedMissionButtonIndex = MissionButtonIndex;
	RevealedMission = Mission;
}

void AHeadPlayerState::ClearMission()
{
	Mission = nullptr;
	RevealedMission = nullptr;
	MissionButtonIndex = INDEX_NONE;
	RevealedMissionButtonIndex = INDEX_NONE;
}
