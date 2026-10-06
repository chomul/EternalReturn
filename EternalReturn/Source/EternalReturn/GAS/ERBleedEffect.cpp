// Copyright Epic Games, Inc. All Rights Reserved.

#include "GAS/ERBleedEffect.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GAS/ERDamageExecution.h"
#include "GAS/ERGameplayTags.h"

UERBleedEffect::UERBleedEffect()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	FSetByCallerFloat Duration;
	Duration.DataTag = ERTags::SetByCaller_StateDuration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(Duration);

	Period = TickSeconds;
	bExecutePeriodicEffectOnApplication = false;   // 첫 틱은 1초 뒤 — 6초면 6번

	StackingType = EGameplayEffectStackingType::AggregateBySource;
	StackLimitCount = MaxStacks;
	StackDurationRefreshPolicy = EGameplayEffectStackingDurationPolicy::RefreshOnSuccessfulApplication;
	StackPeriodResetPolicy = EGameplayEffectStackingPeriodPolicy::NeverReset;
	StackExpirationPolicy = EGameplayEffectStackingExpirationPolicy::ClearEntireStack;

	FGameplayEffectExecutionDefinition ExecDef;
	ExecDef.CalculationClass = UERDamageExecution::StaticClass();
	Executions.Add(ExecDef);
}

int32 UERBleedEffect::GetStacks(const AActor* Target, UAbilitySystemComponent* Source)
{
	const UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);
	return TargetASC ? TargetASC->GetGameplayEffectCount(UERBleedEffect::StaticClass(), Source) : 0;
}
