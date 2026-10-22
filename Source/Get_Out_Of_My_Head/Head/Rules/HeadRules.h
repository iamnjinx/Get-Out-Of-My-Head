#pragma once

#include "CoreMinimal.h"
#include "HeadRuleTypes.h"

class UWorld;
class AHeadObject;
class AHeadGameState;
class UHeadBodyComponent;
class AGet_Out_Of_My_HeadCharacter;

/** What a rule is evaluated against */
struct FHeadRuleContext
{
	UWorld* World = nullptr;
	AHeadGameState* GameState = nullptr;
	AGet_Out_Of_My_HeadCharacter* Body = nullptr;

	/** The object being used, or the host of an attach */
	AHeadObject* Self = nullptr;

	/** The item being attached, if any */
	AHeadObject* Item = nullptr;

	UHeadBodyComponent* GetBodyComponent() const;
};

/**
 *  Server-side evaluation of object conditions and effects
 *  Conditions are checked in order, the first failing one decides the message.
 */
namespace HeadRules
{
	/** Returns true when every condition passes. Otherwise OutFailText is the first failing condition's text */
	bool CheckConditions(const FHeadRuleContext& Context, const TArray<FHeadCondition>& Conditions, FText& OutFailText);

	bool CheckCondition(const FHeadRuleContext& Context, const FHeadCondition& Condition);

	void ApplyEffects(const FHeadRuleContext& Context, const TArray<FHeadEffect>& Effects);

	/** Message used when a condition has no text of its own */
	FText GetDefaultFailText();
}
