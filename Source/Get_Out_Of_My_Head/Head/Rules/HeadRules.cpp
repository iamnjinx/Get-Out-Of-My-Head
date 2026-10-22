#include "HeadRules.h"
#include "HeadDataAssets.h"
#include "HeadBodyComponent.h"
#include "HeadObject.h"
#include "HeadZone.h"
#include "HeadGameState.h"
#include "HeadPlayerState.h"
#include "Get_Out_Of_My_HeadCharacter.h"
#include "EngineUtils.h"

UHeadBodyComponent* FHeadRuleContext::GetBodyComponent() const
{
	return Body ? Body->FindComponentByClass<UHeadBodyComponent>() : nullptr;
}

namespace
{
	/** Objects a condition looks at. Other matches every live object using the data */
	TArray<AHeadObject*> ResolveTargets(const FHeadRuleContext& Context, const FHeadCondition& Condition)
	{
		TArray<AHeadObject*> Targets;

		switch (Condition.Target)
		{
		case EHeadConditionTarget::Self:
			if (Context.Self)
			{
				Targets.Add(Context.Self);
			}
			break;

		case EHeadConditionTarget::Item:
			if (Context.Item)
			{
				Targets.Add(Context.Item);
			}
			break;

		case EHeadConditionTarget::Other:
			for (TActorIterator<AHeadObject> It(Context.World); It; ++It)
			{
				if (It->GetObjectData() == Condition.TargetObject && !It->IsConsumed())
				{
					Targets.Add(*It);
				}
			}
			break;
		}

		return Targets;
	}

	bool AnyTarget(const FHeadRuleContext& Context, const FHeadCondition& Condition, TFunctionRef<bool(const AHeadObject*)> Test)
	{
		for (const AHeadObject* Target : ResolveTargets(Context, Condition))
		{
			if (Test(Target))
			{
				return true;
			}
		}

		return false;
	}

	int32 GetVariable(const FHeadRuleContext& Context, const FHeadCondition& Condition)
	{
		return Context.GameState ? Context.GameState->GetVariable(Condition.Variable) : 0;
	}
}

bool HeadRules::CheckConditions(const FHeadRuleContext& Context, const TArray<FHeadCondition>& Conditions, FText& OutFailText)
{
	for (const FHeadCondition& Condition : Conditions)
	{
		if (!CheckCondition(Context, Condition))
		{
			OutFailText = Condition.FailText.IsEmpty() ? GetDefaultFailText() : Condition.FailText;
			return false;
		}
	}

	return true;
}

bool HeadRules::CheckCondition(const FHeadRuleContext& Context, const FHeadCondition& Condition)
{
	const UHeadBodyComponent* BodyComponent = Context.GetBodyComponent();

	switch (Condition.Type)
	{
	case EHeadConditionType::InteractedBefore:
		return AnyTarget(Context, Condition, [](const AHeadObject* Target) { return Target->HasBeenInteracted(); });

	case EHeadConditionType::InZone:
		return AnyTarget(Context, Condition, [&Context, &Condition](const AHeadObject* Target)
		{
			return AHeadZone::IsInZone(Context.World, Condition.Zone, Target->GetActorLocation());
		});

	case EHeadConditionType::HasStatus:
		return BodyComponent && BodyComponent->GetStacks(Condition.Status) > 0;

	case EHeadConditionType::LacksStatus:
		return !BodyComponent || BodyComponent->GetStacks(Condition.Status) == 0;

	case EHeadConditionType::StatusStacksAtLeast:
		return BodyComponent && BodyComponent->GetStacks(Condition.Status) >= Condition.Value;

	case EHeadConditionType::ToggleOn:
		return AnyTarget(Context, Condition, [](const AHeadObject* Target) { return Target->IsOn(); });

	case EHeadConditionType::ToggleOff:
		return AnyTarget(Context, Condition, [](const AHeadObject* Target) { return !Target->IsOn(); });

	case EHeadConditionType::Holding:
	{
		// what is in hand and everything attached to it counts as held
		const AHeadObject* Held = BodyComponent ? BodyComponent->GetHeldObject() : nullptr;
		return Held && Held->ContainsObjectData(Condition.Object);
	}

	case EHeadConditionType::MissionHolderPresent:
		if (Context.GameState)
		{
			for (const AHeadPlayerState* PS : Context.GameState->TurnOrder)
			{
				if (PS && PS->GetMission() == Condition.Mission)
				{
					return true;
				}
			}
		}
		return false;

	case EHeadConditionType::Attached:
		return AnyTarget(Context, Condition, [&Condition](const AHeadObject* Target) { return Target->HasAttachment(Condition.Object); });

	case EHeadConditionType::VariableEquals:
		return GetVariable(Context, Condition) == Condition.Value;

	case EHeadConditionType::VariableAtLeast:
		return GetVariable(Context, Condition) >= Condition.Value;

	case EHeadConditionType::VariableAtMost:
		return GetVariable(Context, Condition) <= Condition.Value;
	}

	return false;
}

void HeadRules::ApplyEffects(const FHeadRuleContext& Context, const TArray<FHeadEffect>& Effects)
{
	UHeadBodyComponent* BodyComponent = Context.GetBodyComponent();

	for (const FHeadEffect& Effect : Effects)
	{
		switch (Effect.Type)
		{
		case EHeadEffectType::AddStatus:
			if (BodyComponent)
			{
				BodyComponent->AddStatus(Effect.Status, Effect.Stacks);
			}
			break;

		case EHeadEffectType::RemoveStatus:
			if (BodyComponent)
			{
				BodyComponent->RemoveStatus(Effect.Status);
			}
			break;

		case EHeadEffectType::Sound:
			// world sound: plays where it happens, everyone's listener sits in the body's head
			if (Context.GameState)
			{
				if (Effect.Sound)
				{
					const FVector Location = Context.Self ? Context.Self->GetActorLocation() : (Context.Body ? Context.Body->GetActorLocation() : FVector::ZeroVector);
					Context.GameState->MulticastPlaySound(Effect.Sound, Location);
				}

				Context.GameState->MulticastNotice(FString::Printf(TEXT("소리: \"%s\""), *Effect.SoundText.ToString()));
			}
			break;

		case EHeadEffectType::AddVariable:
			if (Context.GameState)
			{
				Context.GameState->AddVariable(Effect.Variable, Effect.Delta);
			}
			break;
		}
	}
}

FText HeadRules::GetDefaultFailText()
{
	return FText::FromString(TEXT("조건 부족"));
}
