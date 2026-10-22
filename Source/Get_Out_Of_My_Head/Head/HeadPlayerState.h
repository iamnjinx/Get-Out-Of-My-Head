#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "HeadPlayerState.generated.h"

class UHeadMissionData;

/**
 *  Per-player round data
 *  The mission is secret: it only replicates to its owner until the results reveal it
 */
UCLASS()
class AHeadPlayerState : public APlayerState
{
	GENERATED_BODY()

	/** Secret mission, owner only */
	UPROPERTY(Replicated)
	TObjectPtr<UHeadMissionData> Mission;

	/** Same as Mission, visible to everyone once the round ends */
	UPROPERTY(Replicated)
	TObjectPtr<UHeadMissionData> RevealedMission;

	/** Staged missions: current stage (0-based) and stage count, owner only. 0 stages for condition missions */
	UPROPERTY(Replicated)
	int32 MissionStage = 0;

	UPROPERTY(Replicated)
	int32 MissionStageCount = 0;

	/** Seconds held / needed for the current stage, owner only */
	UPROPERTY(Replicated)
	float StageHeldSeconds = 0.0f;

	UPROPERTY(Replicated)
	float StageRequiredSeconds = 0.0f;

	/** Server: order in which missions were handed out, breaks priority ties */
	int32 AssignmentOrder = INDEX_NONE;

public:

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Server: assigns the secret mission */
	void SetMission(UHeadMissionData* InMission, int32 InAssignmentOrder);

	/** Server: makes the mission visible to everyone */
	void RevealMission();

	/** Server: clears mission data for the next round */
	void ClearMission();

	/** Server: progress of a staged mission */
	void SetStageProgress(int32 Stage, int32 StageCount, float HeldSeconds, float RequiredSeconds);

	/** Returns the mission. Only valid on the server and the owning client */
	UHeadMissionData* GetMission() const { return Mission; }

	UHeadMissionData* GetRevealedMission() const { return RevealedMission; }

	int32 GetAssignmentOrder() const { return AssignmentOrder; }
	int32 GetMissionStage() const { return MissionStage; }
	int32 GetMissionStageCount() const { return MissionStageCount; }
	float GetStageHeldSeconds() const { return StageHeldSeconds; }
	float GetStageRequiredSeconds() const { return StageRequiredSeconds; }
};
