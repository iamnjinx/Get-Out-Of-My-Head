#include "HeadGameState.h"
#include "HeadPlayerState.h"
#include "HeadPlayerController.h"
#include "Get_Out_Of_My_HeadCharacter.h"
#include "HeadDataAssets.h"
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
	DOREPLIFETIME(AHeadGameState, WorldVariables);
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

int32 AHeadGameState::GetVariable(const UHeadWorldVariableData* Variable) const
{
	const FHeadVariableValue* Found = WorldVariables.FindByPredicate([Variable](const FHeadVariableValue& Entry) { return Entry.Variable == Variable; });
	if (Found)
	{
		return Found->Value;
	}

	return Variable ? Variable->InitialValue : 0;
}

void AHeadGameState::AddVariable(UHeadWorldVariableData* Variable, int32 Delta)
{
	if (!Variable)
	{
		return;
	}

	FHeadVariableValue* Found = WorldVariables.FindByPredicate([Variable](const FHeadVariableValue& Entry) { return Entry.Variable == Variable; });
	if (!Found)
	{
		Found = &WorldVariables.AddDefaulted_GetRef();
		Found->Variable = Variable;
		Found->Value = Variable->InitialValue;
	}

	Found->Value += Delta;
}

void AHeadGameState::ResetVariables(const TArray<TObjectPtr<UHeadWorldVariableData>>& Variables)
{
	WorldVariables.Empty();

	for (UHeadWorldVariableData* Variable : Variables)
	{
		if (Variable)
		{
			FHeadVariableValue& Entry = WorldVariables.AddDefaulted_GetRef();
			Entry.Variable = Variable;
			Entry.Value = Variable->InitialValue;
		}
	}
}
