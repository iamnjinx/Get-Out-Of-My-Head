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

	/** Index of the mission button assigned to this player, owner only */
	UPROPERTY(Replicated)
	int32 MissionButtonIndex = INDEX_NONE;

	/** Same as MissionButtonIndex, but visible to everyone once the round ends */
	UPROPERTY(Replicated)
	int32 RevealedMissionButtonIndex = INDEX_NONE;

	/** Secret mission, owner only */
	UPROPERTY(Replicated)
	TObjectPtr<UHeadMissionData> Mission;

	/** Same as Mission, visible to everyone once the round ends */
	UPROPERTY(Replicated)
	TObjectPtr<UHeadMissionData> RevealedMission;

public:

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Server: assigns the secret mission and its M1 dummy button */
	void SetMission(UHeadMissionData* InMission, int32 ButtonIndex);

	/** Server: makes the mission visible to everyone */
	void RevealMission();

	/** Server: clears mission data for the next round */
	void ClearMission();

	/** Returns the mission. Only valid on the server and the owning client */
	int32 GetMissionButtonIndex() const { return MissionButtonIndex; }

	/** Returns the mission revealed at the end of the round */
	int32 GetRevealedMissionButtonIndex() const { return RevealedMissionButtonIndex; }

	/** Returns the mission. Only valid on the server and the owning client */
	UHeadMissionData* GetMission() const { return Mission; }

	UHeadMissionData* GetRevealedMission() const { return RevealedMission; }
};
