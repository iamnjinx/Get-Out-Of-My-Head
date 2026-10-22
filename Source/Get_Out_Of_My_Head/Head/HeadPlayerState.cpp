#include "HeadPlayerState.h"
#include "HeadDataAssets.h"
#include "Net/UnrealNetwork.h"

void AHeadPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(AHeadPlayerState, Mission, COND_OwnerOnly);
	DOREPLIFETIME(AHeadPlayerState, RevealedMission);
	DOREPLIFETIME_CONDITION(AHeadPlayerState, MissionStage, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AHeadPlayerState, MissionStageCount, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AHeadPlayerState, StageHeldSeconds, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AHeadPlayerState, StageRequiredSeconds, COND_OwnerOnly);
}

void AHeadPlayerState::SetMission(UHeadMissionData* InMission, int32 InAssignmentOrder)
{
	Mission = InMission;
	RevealedMission = nullptr;
	AssignmentOrder = InAssignmentOrder;

	const int32 StageCount = InMission && InMission->bStaged ? InMission->Stages.Num() : 0;
	SetStageProgress(0, StageCount, 0.0f, StageCount > 0 ? InMission->Stages[0].HoldSeconds : 0.0f);
}

void AHeadPlayerState::RevealMission()
{
	RevealedMission = Mission;
}

void AHeadPlayerState::ClearMission()
{
	Mission = nullptr;
	RevealedMission = nullptr;
	AssignmentOrder = INDEX_NONE;
	SetStageProgress(0, 0, 0.0f, 0.0f);
}

void AHeadPlayerState::SetStageProgress(int32 Stage, int32 StageCount, float HeldSeconds, float RequiredSeconds)
{
	MissionStage = Stage;
	MissionStageCount = StageCount;
	StageHeldSeconds = HeldSeconds;
	StageRequiredSeconds = RequiredSeconds;
}
