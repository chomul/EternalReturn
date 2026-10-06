// Copyright Epic Games, Inc. All Rights Reserved.

#include "GAS/Fragment/ERSkillFragment_OnHitEffect.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "EternalReturn.h"
#include "GAS/ERCCLibrary.h"
#include "GAS/ERGameplayTags.h"
#include "GAS/ERSkillData.h"
#include "GAS/ERAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "TimerManager.h"

void UERSkillFragment_OnHitEffect::OnTargetsResolved(FERSkillContext& Ctx, const TArray<AActor*>& Targets) const
{
	if (!Ctx.bAuthority || Targets.IsEmpty())
	{
		return;
	}
	if (!Effect)
	{
		UE_LOG(LogEternalReturn, Error, TEXT("[스킬] %s 의 적중 효과 조각에 GE 가 비어 있다."), *GetNameSafe(Ctx.Skill));
		return;
	}
	const float Dur = UERSkillData::LevelValue(Duration, Ctx.Level);
	const float Slow = UERSkillData::LevelValue(SlowPercent, Ctx.Level);
	float Mag = UERSkillData::LevelValueSigned(Magnitude, Ctx.Level);   // 음수 허용 (치유 감소)
	if (const float PerAmp = UERSkillData::LevelValueSigned(MagnitudePerSkillAmp, Ctx.Level); PerAmp != 0.f && Ctx.ASC)
	{
		Mag += PerAmp * Ctx.ASC->GetNumericAttribute(UERAttributeSet::GetSkillAmpAttribute());
	}
	// GE 모디파이어가 실제로 이 값을 읽을 때만 경고 (둔화는 SlowPercent 를 읽는다 — 2026-10-01 트리플렛 오경보)
	const UGameplayEffect* EffectCDO = Effect->GetDefaultObject<UGameplayEffect>();
	const bool bReadsMagnitude = EffectCDO && EffectCDO->Modifiers.ContainsByPredicate([](const FGameplayModifierInfo& M)
	{
		return M.ModifierMagnitude.GetMagnitudeCalculationType() == EGameplayEffectMagnitudeCalculation::SetByCaller
			&& M.ModifierMagnitude.GetSetByCallerFloat().DataTag == ERTags::SetByCaller_OnHitMagnitude;
	});
	if (Mag == 0.f && bReadsMagnitude)
	{
		// GE 가 SetByCaller.OnHitMagnitude 를 읽는데 값이 0 이면 넣지 않는다 → GAS 가 "not yet been set" 에러만 남긴다 (F12.6-06 신경 가스). 어느 DA 인지 찍는다 — 스킬당 한 번
		static TSet<const UObject*> Warned;
		if (!Warned.Contains(Ctx.Skill))
		{
			Warned.Add(Ctx.Skill);
			UE_LOG(LogEternalReturn, Warning, TEXT("[스킬] %s 적중 효과 %s — Magnitude 칸 %d개 · Lv.%d 값 0 → SetByCaller.OnHitMagnitude 안 넣음 (GE 가 이 값을 쓰면 DA 에 크기를 넣는다)"),
				*GetNameSafe(Ctx.Skill), *GetNameSafe(Effect), Magnitude.Num(), Ctx.Level);
		}
	}
	if (StartDelay > 0.f && Ctx.Avatar)
	{
		// 늦게 걸기 — 어빌리티에 묶지 않은 타이머 (EndAbility 가 지운다 · E40)
		TWeakObjectPtr<UAbilitySystemComponent> WeakSource(Ctx.ASC);
		TArray<TWeakObjectPtr<UAbilitySystemComponent>> WeakTargets;
		for (AActor* Target : Targets) { WeakTargets.Add(UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target)); }
		const TSubclassOf<UGameplayEffect> GE = Effect;
		FTimerHandle Unused;
		Ctx.Avatar->GetWorldTimerManager().SetTimer(Unused, FTimerDelegate::CreateLambda([WeakSource, WeakTargets, GE, Dur, Slow, Mag]()
		{
			for (const TWeakObjectPtr<UAbilitySystemComponent>& T : WeakTargets)
			{
				if (UAbilitySystemComponent* TargetASC = T.Get()) { ERCC::ApplyCC(WeakSource.Get(), TargetASC, GE, Dur, Slow, Mag); }
			}
		}), StartDelay, false);
		return;
	}
	for (AActor* Target : Targets)
	{
		if (UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target))
		{
			ERCC::ApplyCC(Ctx.ASC, TargetASC, Effect, Dur, Slow, Mag);
		}
	}
}
