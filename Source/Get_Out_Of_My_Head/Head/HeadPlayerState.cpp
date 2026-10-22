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
	DOREPLIFETIME_CONDITION(AHeadPlayerState, MissionStage, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AHeadPlayerState, MissionStageCount, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AHeadPlayerState, StageHeldSeconds, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AHeadPlayerState, StageRequiredSeconds, COND_OwnerOnly);
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

void AHeadPlayerState::SetStageProgress(int32 Stage, int32 StageCount, float HeldSeconds, float RequiredSeconds)
{
	MissionStage = Stage;
	MissionStageCount = StageCount;
	StageHeldSeconds = HeldSeconds;
	StageRequiredSeconds = RequiredSeconds;
}

void AHeadPlayerState::ClearMission()
{
	SetStageProgress(0, 0, 0.0f, 0.0f);

	Mission = nullptr;
	RevealedMission = nullptr;
	MissionButtonIndex = INDEX_NONE;
	RevealedMissionButtonIndex = INDEX_NONE;
}
