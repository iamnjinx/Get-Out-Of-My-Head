#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "HeadTypes.h"
#include "HeadGameState.generated.h"

class AHeadPlayerState;
class AGet_Out_Of_My_HeadCharacter;
class UHeadWorldVariableData;
class USoundBase;

/** Current value of a world variable */
USTRUCT()
struct FHeadVariableValue
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UHeadWorldVariableData> Variable;

	UPROPERTY()
	int32 Value = 0;
};

/**
 *  Public round state, visible to everyone
 *  The server (AHeadGameMode) writes it, clients only read it
 */
UCLASS()
class AHeadGameState : public AGameStateBase
{
	GENERATED_BODY()

public:

	/** Current room phase */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Head")
	EHeadPhase Phase = EHeadPhase::None;

	/** Control order of this round, public to everyone */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Head")
	TArray<TObjectPtr<AHeadPlayerState>> TurnOrder;

	/** Player controlling the body (Playing), or the one who just did (Handover) */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Head")
	TObjectPtr<AHeadPlayerState> CurrentController;

	/** Player who takes the body when the handover ends */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Head")
	TObjectPtr<AHeadPlayerState> NextController;

	/** Server time at which the current turn or handover ends */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Head")
	double PhaseEndTime = 0.0;

	/** Server time at which the round ends (handovers included) */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Head")
	double RoundEndTime = 0.0;

	/** Why the last round ended */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Head")
	EHeadEndReason EndReason = EHeadEndReason::None;

	/** Winner of the last round, null on a draw */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Head")
	TObjectPtr<AHeadPlayerState> Winner;

	/** The single shared body */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Head")
	TObjectPtr<AGet_Out_Of_My_HeadCharacter> Body;

	/** House-wide values, public to everyone (방 온도) */
	UPROPERTY(Replicated)
	TArray<FHeadVariableValue> WorldVariables;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Returns the variable's value, or its initial value if it is not tracked */
	int32 GetVariable(const UHeadWorldVariableData* Variable) const;

	/** Server: changes a variable */
	void AddVariable(UHeadWorldVariableData* Variable, int32 Delta);

	/** Server: puts every variable back to its initial value */
	void ResetVariables(const TArray<TObjectPtr<UHeadWorldVariableData>>& Variables);

	/** Plays a world sound on every machine */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPlaySound(USoundBase* Sound, FVector Location);

	/** Shows a notice line on every client */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastNotice(const FString& Text);

	/** Returns seconds left until the given server time, never negative */
	double GetSecondsUntil(double ServerTime) const;
};
