// Copyright Epic Games, Inc. All Rights Reserved.

#include "GAS/Fragment/ERSkillFragment_SelfBuff.h"

#include "AbilitySystemComponent.h"
#include "EternalReturn.h"
#include "GAS/ERGameplayAbility.h"
#include "GAS/ERGameplayTags.h"
#include "TimerManager.h"
#include "Core/ERPlayerState.h"
#include "Growth/ERGrowthComponent.h"

void UERSkillFragment_SelfBuff::OnExecute(FERSkillContext& Ctx) const
{
	if (!bOnlyOnHit && !ScalesByHits())
	{
		ApplyEffects(Ctx);
	}
}

void UERSkillFragment_SelfBuff::OnTargetsResolved(FERSkillContext& Ctx, const TArray<AActor*>& Targets) const
{
	if (ScalesByHits())
	{
		// 진실의 칼날 — 판정 뒤 맞힌 수로 (bOnlyOnHit 면 0명일 때 안 건다)
		if (!bOnlyOnHit || !Targets.IsEmpty())
		{
			ApplyEffects(Ctx, Targets.Num());
		}
		return;
	}
	if (bOnlyOnHit && !Targets.IsEmpty())
	{
		ApplyEffects(Ctx);   // 여러 명 맞아도 한 번
	}
}

void UERSkillFragment_SelfBuff::ApplyEffects(FERSkillContext& Ctx, int32 HitCount) const
{
	UERGameplayAbility* A = Ctx.Ability;
	if (!A || !Ctx.ASC || !Ctx.bAuthority || Effects.IsEmpty())
	{
		return;
	}
	const int32 Level = Ctx.Level;
	for (const FERSelfEffect& SE : Effects)
	{
		if (!SE.Effect)
		{
			UE_LOG(LogEternalReturn, Error, TEXT("[스킬] %s 의 자기 버프 조각에 GE 가 비어 있다."), *GetNameSafe(Ctx.Skill));
			continue;
		}
		const FGameplayEffectSpecHandle SpecHandle = A->MakeOutgoingGameplayEffectSpec(A->GetCurrentAbilitySpecHandle(), A->GetCurrentActorInfo(), A->GetCurrentActivationInfo(), SE.Effect, Level);
		FGameplayEffectSpec* Spec = SpecHandle.Data.Get();
		if (!Spec)
		{
			continue;
		}
		const float Duration = UERSkillData::LevelValue(SE.Duration, Level);
		float Magnitude = UERSkillData::LevelValueSigned(SE.Magnitude, Level);   // 음수 허용
		if (const float PerBonusHP = UERSkillData::LevelValue(SE.MagnitudePerBonusHP, Level); PerBonusHP != 0.f)
		{
			const float BonusHP = ERSkill::CasterBonusMaxHP(Ctx.ASC);
			UE_LOG(LogEternalReturn, Log, TEXT("[스킬] %s 자기 버프 추가 체력 %.0f × %.4f → 크기 %.3f + %.3f"), *GetNameSafe(Ctx.Skill), BonusHP, PerBonusHP, Magnitude, BonusHP * PerBonusHP);
			Magnitude += BonusHP * PerBonusHP;
		}
		if (const float PerCharLevel = UERSkillData::LevelValue(SE.MagnitudePerCharLevel, Level); PerCharLevel != 0.f)
		{
			const UERGrowthComponent* Growth = Ctx.PlayerState ? Ctx.PlayerState->GetGrowth() : nullptr;
			const int32 CharLevel = Growth ? Growth->GetLevel() : 0;
			UE_LOG(LogEternalReturn, Log, TEXT("[스킬] %s 자기 버프 실험체 레벨 %d × %.3f → 크기 %.3f + %.3f"), *GetNameSafe(Ctx.Skill), CharLevel, PerCharLevel, Magnitude, CharLevel * PerCharLevel);
			Magnitude += CharLevel * PerCharLevel;
		}
		if (HitCount >= 0)
		{
			const float Base = Magnitude;
			Magnitude += FMath::Max(0, HitCount - 1) * UERSkillData::LevelValue(MagnitudePerExtraHit, Level);
			if (const float Max = UERSkillData::LevelValue(MagnitudeMax, Level); Max > 0.f)
			{
				Magnitude = FMath::Min(Magnitude, Max);
			}
			UE_LOG(LogEternalReturn, Log, TEXT("[스킬] %s 자기 버프 맞힌 수 %d → 크기 %.2f → %.2f"), *GetNameSafe(Ctx.Skill), HitCount, Base, Magnitude);
		}
		if (Duration > 0.f)   { Spec->SetSetByCallerMagnitude(ERTags::SetByCaller_CCDuration, Duration); }
		if (Magnitude != 0.f) { Spec->SetSetByCallerMagnitude(ERTags::SetByCaller_OnHitMagnitude, Magnitude); }
		if (SE.GrantTag.IsValid()) { Spec->DynamicGrantedTags.AddTag(SE.GrantTag); }
		if (SE.Charges > 0)
		{
			// "다음 N회 기본 공격" — 평타가 적중마다 1 줄인다 (UERBasicAttackAbility).
			Spec->DynamicGrantedTags.AddTag(ERTags::State_ConsumeOnAttack);
			Spec->SetSetByCallerMagnitude(ERTags::SetByCaller_Charges, static_cast<float>(SE.Charges));
		}
		const float Delay = UERSkillData::LevelValue(SE.StartDelay, Level);
		if (Delay > 0.f)
		{
			// "이동이 끝난 후" — 스펙은 지금 만들고 적용만 늦춘다. 람다 타이머 — EndAbility 가 못 지운다 (E19).
			TWeakObjectPtr<UAbilitySystemComponent> WeakASC(Ctx.ASC);
			FTimerHandle Unused;
			Ctx.ASC->GetWorld()->GetTimerManager().SetTimer(Unused, FTimerDelegate::CreateLambda([WeakASC, SpecHandle]()
			{
				if (UAbilitySystemComponent* ASC = WeakASC.Get(); ASC && SpecHandle.IsValid())
				{
					ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data);
				}
			}), Delay, false);
		}
		else
		{
			A->ApplySpecToSelf(SpecHandle);
		}
		// GE 애셋이 거는 태그도 찍는다 — 바이너리라 에디터를 안 열고는 모른다 (재키 R 중 평타가 State.Block.BasicAttack 에 막힘 · 2026-10-05)
		const FGameplayTagContainer AssetGrants = SE.Effect->GetDefaultObject<UGameplayEffect>()->GetGrantedTags();
		UE_LOG(LogEternalReturn, Log, TEXT("[스킬] %s <- %s 자기 버프 %s (지속 %.1f · 크기 %.2f%s%s) · 거는 태그 [%s]"),
			*GetNameSafe(A->GetOwningActorFromActorInfo()), *GetNameSafe(Ctx.Skill), *SE.Effect->GetName(), Duration, Magnitude,
			SE.Charges > 0 ? *FString::Printf(TEXT(" · %d회"), SE.Charges) : TEXT(""),
			Delay > 0.f ? *FString::Printf(TEXT(" · %.1f초 뒤"), Delay) : TEXT(""), *AssetGrants.ToStringSimple());
	}
}
