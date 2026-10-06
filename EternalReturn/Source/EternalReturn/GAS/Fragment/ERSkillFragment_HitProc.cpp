// Copyright Epic Games, Inc. All Rights Reserved.

#include "GAS/Fragment/ERSkillFragment_HitProc.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "EternalReturn.h"
#include "GAS/ERAttributeSet.h"
#include "GAS/ERGameplayAbility.h"
#include "GAS/ERGameplayTags.h"
#include "GAS/ERLifestealEffect.h"
#include "GAS/ERSkillData.h"
#include "GAS/ERSkillDamageEffect.h"

void UERSkillFragment_HitProc::OnHitDealt(FERSkillContext& Ctx, AActor* Target, const FGameplayTagContainer& HitTags) const
{
	UERGameplayAbility* A = Ctx.Ability;
	UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);
	if (!Ctx.bAuthority || !A || !Ctx.ASC || !TargetASC || !WhileTag.IsValid() || !Ctx.ASC->HasMatchingGameplayTag(WhileTag))
	{
		return;
	}
	const int32 Level = Ctx.Level;

	// ① 추가 피해 — 부가 피해 (적중 이벤트를 안 낸다 → 이 조각이 다시 불리지 않는다)
	const float Base = UERSkillData::LevelValue(BonusDamage, Level);
	const float AP = UERSkillData::LevelValue(BonusAPRatio, Level);
	if (Base > 0.f || AP > 0.f)
	{
		const FGameplayEffectSpecHandle SpecHandle = A->MakeOutgoingGameplayEffectSpec(
			A->GetCurrentAbilitySpecHandle(), A->GetCurrentActorInfo(), A->GetCurrentActivationInfo(), UERSkillDamageEffect::StaticClass(), Level);
		if (FGameplayEffectSpec* Spec = SpecHandle.Data.Get())
		{
			Spec->SetSetByCallerMagnitude(ERTags::Data_Damage_Base, Base);
			Spec->SetSetByCallerMagnitude(ERTags::Data_Damage_APRatio, AP);
			Spec->AddDynamicAssetTag(ERTags::Damage_Type_Skill);
			Spec->AddDynamicAssetTag(ERTags::Damage_Secondary);
			Ctx.ASC->ApplyGameplayEffectSpecToTarget(*Spec, TargetASC);
		}
	}

	// ② 회복 — 잃은 체력 비례 (가득 = 최소 · FullHealBelowHPRatio 이하 = 최대)
	const float MyAP = Ctx.ASC->GetNumericAttribute(UERAttributeSet::GetAttackPowerAttribute());
	const float HP = Ctx.ASC->GetNumericAttribute(UERAttributeSet::GetHPAttribute());
	const float MaxHP = Ctx.ASC->GetNumericAttribute(UERAttributeSet::GetMaxHPAttribute());
	const float HPRatio = MaxHP > 0.f ? HP / MaxHP : 1.f;
	const float T = FMath::Clamp((1.f - HPRatio) / FMath::Max(1.f - FullHealBelowHPRatio, KINDA_SMALL_NUMBER), 0.f, 1.f);
	const float HealLo = UERSkillData::LevelValue(HealMin, Level) + MyAP * UERSkillData::LevelValue(HealMinAPRatio, Level);
	const float HealHi = UERSkillData::LevelValue(HealMax, Level) + MyAP * UERSkillData::LevelValue(HealMaxAPRatio, Level);
	const float Heal = FMath::Lerp(HealLo, HealHi, T);
	if (Heal > 0.f)
	{
		FGameplayEffectSpec HealSpec(GetDefault<UERLifestealEffect>(), Ctx.ASC->MakeEffectContext(), 1.f);
		HealSpec.SetSetByCallerMagnitude(ERTags::SetByCaller_HealAmount, Heal);
		Ctx.ASC->ApplyGameplayEffectSpecToSelf(HealSpec);
	}
	UE_LOG(LogEternalReturn, Log, TEXT("[%s] %s -> %s 추가 피해 (기본 %.0f · 계수 %.2f) · 회복 %.1f (체력 %.0f%% → %.1f ~ %.1f 중 %.0f%%)"),
		*WhileTag.ToString(), *GetNameSafe(Ctx.Avatar), *GetNameSafe(Target), Base, AP, Heal, HPRatio * 100.f, HealLo, HealHi, T * 100.f);
}
