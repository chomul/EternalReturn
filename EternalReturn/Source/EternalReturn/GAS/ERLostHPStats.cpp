// Copyright Epic Games, Inc. All Rights Reserved.

#include "GAS/ERLostHPStats.h"

#include "GAS/ERAttributeSet.h"
#include "GAS/ERGameplayTags.h"

UERLostHPStatCalc::UERLostHPStatCalc()
{
	// 자기에게 거는 GE 라 Target = 시셀라. bSnapshot false — 바뀔 때마다 다시 계산
	HPDef = FGameplayEffectAttributeCaptureDefinition(UERAttributeSet::GetHPAttribute(), EGameplayEffectAttributeCaptureSource::Target, false);
	MaxHPDef = FGameplayEffectAttributeCaptureDefinition(UERAttributeSet::GetMaxHPAttribute(), EGameplayEffectAttributeCaptureSource::Target, false);
	RelevantAttributesToCapture.Add(HPDef);
	RelevantAttributesToCapture.Add(MaxHPDef);
}

float UERLostHPStatCalc::CalculateBaseMagnitude_Implementation(const FGameplayEffectSpec& Spec) const
{
	FAggregatorEvaluateParameters Params;
	Params.SourceTags = Spec.CapturedSourceTags.GetAggregatedTags();
	Params.TargetTags = Spec.CapturedTargetTags.GetAggregatedTags();
	float HP = 0.f;
	float MaxHP = 0.f;
	GetCapturedAttributeMagnitude(HPDef, Spec, Params, HP);
	GetCapturedAttributeMagnitude(MaxHPDef, Spec, Params, MaxHP);
	const float Lost = MaxHP > 0.f ? FMath::Clamp(1.f - HP / MaxHP, 0.f, 1.f) : 0.f;
	const float Min = Spec.GetSetByCallerMagnitude(MinTag, false, 0.f);
	const float Max = Spec.GetSetByCallerMagnitude(MaxTag, false, 0.f);
	return FMath::Lerp(Min, Max, Lost);
}

UERLostHPRegenCalc::UERLostHPRegenCalc()
{
	MinTag = ERTags::SetByCaller_LostHP_RegenMin;
	MaxTag = ERTags::SetByCaller_LostHP_RegenMax;
}

UERLostHPSkillAmpCalc::UERLostHPSkillAmpCalc()
{
	MinTag = ERTags::SetByCaller_LostHP_SkillAmpMin;
	MaxTag = ERTags::SetByCaller_LostHP_SkillAmpMax;
}

namespace
{
	FGameplayModifierInfo LostHPMod(const FGameplayAttribute& Attribute, TSubclassOf<UGameplayModMagnitudeCalculation> Calc)
	{
		FCustomCalculationBasedFloat Custom;
		Custom.CalculationClassMagnitude = Calc;
		FGameplayModifierInfo Mod;
		Mod.Attribute = Attribute;
		Mod.ModifierOp = EGameplayModOp::Additive;
		Mod.ModifierMagnitude = FGameplayEffectModifierMagnitude(Custom);
		return Mod;
	}
}

UERLostHPStatsEffect::UERLostHPStatsEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	Modifiers.Add(LostHPMod(UERAttributeSet::GetHPRegenAttribute(), UERLostHPRegenCalc::StaticClass()));
	Modifiers.Add(LostHPMod(UERAttributeSet::GetSkillAmpAttribute(), UERLostHPSkillAmpCalc::StaticClass()));
}

UERLostHPStatsTimedEffect::UERLostHPStatsTimedEffect()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	FSetByCallerFloat SetByCaller;
	SetByCaller.DataTag = ERTags::SetByCaller_StateDuration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(SetByCaller);
}
