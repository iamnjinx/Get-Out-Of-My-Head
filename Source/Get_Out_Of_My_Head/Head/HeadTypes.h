#pragma once

#include "CoreMinimal.h"
#include "HeadTypes.generated.h"

/** Phase of the room. Lobby and Results accept new players, Playing and Handover do not */
UENUM(BlueprintType)
enum class EHeadPhase : uint8
{
	/** Front end (not hosting or joined) */
	None,

	/** Waiting in the room between rounds */
	Lobby,

	/** Someone is controlling the body */
	Playing,

	/** Controller handover, everyone's input is locked */
	Handover,

	/** Round finished, missions revealed */
	Results
};

/** Why a round ended */
UENUM(BlueprintType)
enum class EHeadEndReason : uint8
{
	None,

	/** Someone's mission was completed */
	MissionComplete,

	/** The round time ran out */
	TimeUp,

	/** Only one player is left */
	LastPlayer
};
