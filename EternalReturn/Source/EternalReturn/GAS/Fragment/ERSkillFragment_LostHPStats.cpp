// Copyright Epic Games, Inc. All Rights Reserved.

#include "GAS/Fragment/ERSkillFragment_LostHPStats.h"

#include "AbilitySystemComponent.h"
#include "EternalReturn.h"
#include "GAS/ERAttributeSet.h"
#include "GAS/ERGameplayAbility.h"
#include "GAS/ERGameplayTags.h"
#include "GAS/ERLostHPStats.h"
#include "GAS/ERSkillData.h"

void UERSkillFragment_LostHPStats::OnPassiveStart(FERSkillContext& Ctx) const
{
	if (!Ctx.bAuthority || !Ctx.ASC)
	{
		return;
	}
	// 상시 것은 하나 — 레벨이 바뀌면 지우고 새 레벨로 (시간 제한 것은 태그가 없어 그대로)
	FGameplayTagContainer Q; Q.AddTag(ERTags::State_LostHPStats);
	Ctx.ASC->RemoveActiveEffects(FGameplayEffectQuery::MakeQuery_MatchAllOwningTags(Q));
	ApplyEffect(Ctx, 0.f);
}

void UERSkillFragment_LostHPStats::ApplyTimed(FERSkillContext& Ctx, float Duration) const
{
	if (Ctx.bAuthority && Ctx.ASC && Duration > 0.f)
	{
		ApplyEffect(Ctx, Duration);
	}
}

void UERSkillFragment_LostHPStats::ApplyEffect(FERSkillContext& Ctx, float Duration) const
{
	UERGameplayAbility* A = Ctx.Ability;
	if (!A)
	{
		return;
	}
	const bool bTimed = Duration > 0.f;
	const int32 Level = Ctx.Level;
	const FGameplayEffectSpecHandle SpecHandle = A->MakeOutgoingGameplayEffectSpec(
		A->GetCurrentAbilitySpecHandle(), A->GetCurrentActorInfo(), A->GetCurrentActivationInfo(),
		bTimed ? UERLostHPStatsTimedEffect::StaticClass() : UERLostHPStatsEffect::StaticClass(), Level);
	FGameplayEffectSpec* Spec = SpecHandle.Data.Get();
	if (!Spec)
	{
		return;
	}
	const float R0 = UERSkillData::LevelValue(RegenMin, Level);
	const float R1 = UERSkillData::LevelValue(RegenMax, Level);
	const float S0 = UERSkillData::LevelValue(SkillAmpMin, Level);
	const float S1 = UERSkillData::LevelValue(SkillAmpMax, Level);
	Spec->SetSetByCallerMagnitude(ERTags::SetByCaller_LostHP_RegenMin, R0);
	Spec->SetSetByCallerMagnitude(ERTags::SetByCaller_LostHP_RegenMax, R1);
	Spec->SetSetByCallerMagnitude(ERTags::SetByCaller_LostHP_SkillAmpMin, S0);
	Spec->SetSetByCallerMagnitude(ERTags::SetByCaller_LostHP_SkillAmpMax, S1);
	if (bTimed)
	{
		Spec->SetSetByCallerMagnitude(ERTags::SetByCaller_StateDuration, Duration);
	}
	else
	{
		Spec->DynamicGrantedTags.AddTag(ERTags::State_LostHPStats);
	}
	A->ApplySpecToSelf(SpecHandle);

	const float HP = Ctx.ASC->GetNumericAttribute(UERAttributeSet::GetHPAttribute());
	const float MaxHP = Ctx.ASC->GetNumericAttribute(UERAttributeSet::GetMaxHPAttribute());
	UE_LOG(LogEternalReturn, Log, TEXT("[잃은체력스탯] %s Lv.%d %s — 재생 %.0f~%.0f · 스증 %.0f~%.0f · 지금 체력 %.0f/%.0f → 재생 %.2f · 스증 %.1f (어트리뷰트 합)"),
		*GetNameSafe(Ctx.Avatar), Level, bTimed ? *FString::Printf(TEXT("+100%% %.0f초"), Duration) : TEXT("상시"),
		R0, R1, S0, S1, HP, MaxHP,
		Ctx.ASC->GetNumericAttribute(UERAttributeSet::GetHPRegenAttribute()), Ctx.ASC->GetNumericAttribute(UERAttributeSet::GetSkillAmpAttribute()));
}
