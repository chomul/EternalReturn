// Copyright Epic Games, Inc. All Rights Reserved.

#include "GAS/ERSkillDamageEffect.h"
#include "GAS/ERDamageExecution.h"

UERSkillDamageEffect::UERSkillDamageEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FGameplayEffectExecutionDefinition ExecDef;
	ExecDef.CalculationClass = UERDamageExecution::StaticClass();
	Executions.Add(ExecDef);
}
