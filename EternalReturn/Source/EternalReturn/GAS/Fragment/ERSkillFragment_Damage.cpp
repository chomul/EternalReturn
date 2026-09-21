// Copyright Epic Games, Inc. All Rights Reserved.

#include "GAS/Fragment/ERSkillFragment_Damage.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "EternalReturn.h"
#include "GAS/ERAttributeSet.h"
#include "GAS/ERGameplayAbility.h"
#include "GAS/ERGameplayTags.h"
#include "GAS/ERLifestealEffect.h"
#include "GAS/ERSkillDamageEffect.h"

void UERSkillFragment_Damage::OnTargetsResolved(FERSkillContext& Ctx, const TArray<AActor*>& Targets) const
{
	Apply(Ctx, Targets);
}

void UERSkillFragment_Damage::Apply(FERSkillContext& Ctx, const TArray<AActor*>& Targets) const
{
	UERGameplayAbility* A = Ctx.Ability;
	if (!A || !Ctx.bAuthority || Targets.IsEmpty() || DamageType == ESkillDamageType::None)
	{
		return;
	}
	const int32 Level = Ctx.Level;
	const float Scale = Ctx.DamageScale;
	const FGameplayAbilitySpecHandle Handle = A->GetCurrentAbilitySpecHandle();
	const FGameplayAbilityActorInfo* Info = A->GetCurrentActorInfo();
	const FGameplayAbilityActivationInfo ActInfo = A->GetCurrentActivationInfo();

	const FGameplayEffectSpecHandle SpecHandle = A->MakeOutgoingGameplayEffectSpec(Handle, Info, ActInfo, UERSkillDamageEffect::StaticClass(), Level);
	FGameplayEffectSpec* Spec = SpecHandle.Data.Get();
	if (!Spec)
	{
		return;
	}

	// ⭐ 계수만 넘긴다. 곱하는 건 ERDamageExecution 이다 (Docs/4_Argument/5). Scale = 장판 감쇠.
	Spec->SetSetByCallerMagnitude(ERTags::Data_Damage_Base,          UERSkillData::LevelValue(BaseDamage, Level) * Scale);
	Spec->SetSetByCallerMagnitude(ERTags::Data_Damage_APRatio,       UERSkillData::LevelValue(APRatio, Level) * Scale);
	Spec->SetSetByCallerMagnitude(ERTags::Data_Damage_BonusAPRatio,  UERSkillData::LevelValue(BonusAPRatio, Level) * Scale);
	Spec->SetSetByCallerMagnitude(ERTags::Data_Damage_SkillAmpRatio, UERSkillData::LevelValue(SkillAmpRatio, Level) * Scale);
	Spec->SetSetByCallerMagnitude(ERTags::Data_Damage_MaxHPRatio,    UERSkillData::LevelValue(MaxHPRatio, Level) * Scale);
	Spec->SetSetByCallerMagnitude(ERTags::Data_Damage_TargetLostHPScaleMax, UERSkillData::LevelValue(TargetLostHPScaleMax, Level));   // Scale 무관

	// 채널 태그 — 애셋 태그와 같은 통로 (GameplayEffect.h:1119-1120). 없으면 Execution 이 경고한다.
	switch (DamageType)
	{
	case ESkillDamageType::BasicAttack: Spec->AddDynamicAssetTag(ERTags::Damage_Type_BasicAttack); break;
	case ESkillDamageType::Fixed:       Spec->AddDynamicAssetTag(ERTags::Damage_Type_True);        break;
	default:                            Spec->AddDynamicAssetTag(ERTags::Damage_Type_Skill);       break;
	}

	// ⭐ 형상 태그는 시전 쪽이 붙인다 — Execution 이 형상을 추측하지 않는다 (F03-05 흡혈 치유 감소).
	const UERSkillData* ShapeOwner = Ctx.ShapeOwner ? Ctx.ShapeOwner : Ctx.Skill;
	const bool bAoE = ShapeOwner && ShapeOwner->Shape.IsAoE();
	if (bAoE)
	{
		Spec->AddDynamicAssetTag(ERTags::Damage_Shape_AoE);
	}

	// 도끼 D "입힌 피해의 60% 회복" — 실제로 깎인 HP 를 재려면 적용 전후 HP 가 필요하다 (서버 · Instant 라 동기).
	const float HealRatio = UERSkillData::LevelValue(HealFromDamageRatio, Level);
	TArray<TPair<UAbilitySystemComponent*, float>> HPBefore;
	if (HealRatio > 0.f)
	{
		for (AActor* Target : Targets)
		{
			if (UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target))
			{
				HPBefore.Emplace(TargetASC, TargetASC->GetNumericAttribute(UERAttributeSet::GetHPAttribute()));
			}
		}
	}

	// ⭐ 판정 결과를 GAS 그릇에 담아 넘긴다. Source = 시전자, Target = 대상 (F03-01).
	const FGameplayAbilityTargetDataHandle TargetData = UAbilitySystemBlueprintLibrary::AbilityTargetDataFromActorArray(Targets, /*OneTargetPerHandle=*/false);
	A->ApplySpecToTargets(SpecHandle, TargetData);
	Ctx.bHitAnything = true;

	if (!HPBefore.IsEmpty())
	{
		float Dealt = 0.f;
		for (const TPair<UAbilitySystemComponent*, float>& P : HPBefore)
		{
			Dealt += FMath::Max(0.f, P.Value - P.Key->GetNumericAttribute(UERAttributeSet::GetHPAttribute()));
		}
		const float Heal = Dealt * HealRatio;
		if (Heal > 0.f && Ctx.ASC)
		{
			FGameplayEffectContextHandle HealContext = Ctx.ASC->MakeEffectContext();
			FGameplayEffectSpec HealSpec(GetDefault<UERLifestealEffect>(), HealContext, 1.f);
			HealSpec.SetSetByCallerMagnitude(ERTags::SetByCaller_HealAmount, Heal);
			Ctx.ASC->ApplyGameplayEffectSpecToSelf(HealSpec);
			UE_LOG(LogEternalReturn, Log, TEXT("[스킬] %s <- %s 입힌 피해 %.1f 의 %.0f%% 회복 = %.1f"),
				*GetNameSafe(A->GetOwningActorFromActorInfo()), *GetNameSafe(Ctx.Skill), Dealt, HealRatio * 100.f, Heal);
		}
	}

	UE_LOG(LogEternalReturn, Log, TEXT("[스킬] %s <- %s 피해 %d명 (기본 %.0f · %s%s%s)"),
		*GetNameSafe(A->GetOwningActorFromActorInfo()), *GetNameSafe(Ctx.Skill), Targets.Num(),
		UERSkillData::LevelValue(BaseDamage, Level) * Scale,
		DamageType == ESkillDamageType::Fixed ? TEXT("고정") : DamageType == ESkillDamageType::BasicAttack ? TEXT("평타") : TEXT("스킬"),
		bAoE ? TEXT(" · 광역") : TEXT(""), Ctx.bEnhancement ? TEXT(" · 강화") : TEXT(""));

	// 단검 D — 현재 체력 비례 **고정 피해**는 별도 스펙 (Damage.Type.True · 방어력 무시). E20: 실행식이 체력 비례를 탄다.
	const float FixedRatio = UERSkillData::LevelValue(FixedCurHPRatio, Level);
	if (FixedRatio > 0.f)
	{
		const FGameplayEffectSpecHandle FixedHandle = A->MakeOutgoingGameplayEffectSpec(Handle, Info, ActInfo, UERSkillDamageEffect::StaticClass(), Level);
		if (FGameplayEffectSpec* FixedSpec = FixedHandle.Data.Get())
		{
			FixedSpec->SetSetByCallerMagnitude(ERTags::Data_Damage_CurHPRatio, FixedRatio * Scale);
			FixedSpec->AddDynamicAssetTag(ERTags::Damage_Type_True);
			if (bAoE) { FixedSpec->AddDynamicAssetTag(ERTags::Damage_Shape_AoE); }
			A->ApplySpecToTargets(FixedHandle, TargetData);
			UE_LOG(LogEternalReturn, Log, TEXT("[스킬] %s <- %s 고정 피해 현재체력 %.0f%%"), *GetNameSafe(A->GetOwningActorFromActorInfo()), *GetNameSafe(Ctx.Skill), FixedRatio * 100.f);
		}
	}
}
