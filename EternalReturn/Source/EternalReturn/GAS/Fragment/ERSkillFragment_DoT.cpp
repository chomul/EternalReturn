// Copyright Epic Games, Inc. All Rights Reserved.

#include "GAS/Fragment/ERSkillFragment_DoT.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "EternalReturn.h"
#include "GAS/ERBleedEffect.h"
#include "GAS/ERGameplayAbility.h"
#include "GAS/ERGameplayTags.h"
#include "GAS/ERSkillData.h"
#include "GAS/ERSkillStateEffects.h"

void UERSkillFragment_DoT::ApplySelfTimedTag(UAbilitySystemComponent* ASC, FGameplayTag Tag, float Duration)
{
	if (!ASC || !Tag.IsValid() || Duration <= 0.f)
	{
		return;
	}
	FGameplayEffectSpec Spec(GetDefault<UERTimedTagEffect>(), ASC->MakeEffectContext(), 1.f);
	Spec.DynamicGrantedTags.AddTag(Tag);
	Spec.SetSetByCallerMagnitude(ERTags::SetByCaller_StateDuration, Duration);
	ASC->ApplyGameplayEffectSpecToSelf(Spec);
}

void UERSkillFragment_DoT::OnHitDealt(FERSkillContext& Ctx, AActor* Target, const FGameplayTagContainer& HitTags) const
{
	UERGameplayAbility* A = Ctx.Ability;
	UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);
	if (!Ctx.bAuthority || !A || !Ctx.ASC || !TargetASC)
	{
		return;
	}
	const int32 Level = Ctx.Level;
	const float Ticks = FMath::Max(1.f, FMath::RoundToFloat(Duration / UERBleedEffect::TickSeconds));
	const FGameplayEffectSpecHandle SpecHandle = A->MakeOutgoingGameplayEffectSpec(
		A->GetCurrentAbilitySpecHandle(), A->GetCurrentActorInfo(), A->GetCurrentActivationInfo(), UERBleedEffect::StaticClass(), Level);
	FGameplayEffectSpec* Spec = SpecHandle.Data.Get();
	if (!Spec)
	{
		return;
	}
	// 계수 = 중첩 하나의 한 틱 몫 — 실행 계산이 중첩 수를 곱한다
	Spec->SetSetByCallerMagnitude(ERTags::Data_Damage_Base, UERSkillData::LevelValue(DamagePerStack, Level) / Ticks);
	Spec->SetSetByCallerMagnitude(ERTags::Data_Damage_APRatio, UERSkillData::LevelValue(APRatioPerStack, Level) / Ticks);
	Spec->SetSetByCallerMagnitude(ERTags::SetByCaller_StateDuration, Duration);
	Spec->AddDynamicAssetTag(ERTags::Damage_Type_Skill);
	Spec->AddDynamicAssetTag(ERTags::Damage_Secondary);   // 틱은 적중 이벤트 없음 (어트리뷰트셋)
	Spec->DynamicGrantedTags.AddTag(ERTags::State_Bleeding);
	const bool bFull = FullStacksWhileTag.IsValid() && Ctx.ASC->HasMatchingGameplayTag(FullStacksWhileTag);
	Spec->SetStackCount(bFull ? UERBleedEffect::MaxStacks : StacksPerHit);   // 이미 있으면 GAS 가 더한다 (상한 5)
	Ctx.ASC->ApplyGameplayEffectSpecToTarget(*Spec, TargetASC);
	A->SendPresCues(*Ctx.Skill, Ctx.Avatar, { Target }, /*bWithAttack=*/false);   // 출혈 거는 소리 (재키 Passive_Activation · Pres.Sfx.SkillHit.P · Audio/Jackie.md 5)

	const int32 Stacks = UERBleedEffect::GetStacks(Target, Ctx.ASC);
	UE_LOG(LogEternalReturn, Log, TEXT("[출혈] %s -> %s %d/%d 중첩 (%.0f초 갱신%s)"),
		*GetNameSafe(Ctx.Avatar), *GetNameSafe(Target), Stacks, UERBleedEffect::MaxStacks, Duration, bFull ? TEXT(" · 아드레날린 — 한 번에 최대") : TEXT(""));

	if (Stacks >= UERBleedEffect::MaxStacks && OnMaxStacksSelfTag.IsValid() && !Ctx.ASC->HasMatchingGameplayTag(OnMaxStacksSelfTag))
	{
		const float SelfDuration = UERSkillData::LevelValue(OnMaxStacksSelfDuration, Level);
		ApplySelfTimedTag(Ctx.ASC, OnMaxStacksSelfTag, SelfDuration);
		UE_LOG(LogEternalReturn, Log, TEXT("[출혈] %s %s %.0f초 (출혈 최대)"), *GetNameSafe(Ctx.Avatar), *OnMaxStacksSelfTag.ToString(), SelfDuration);
	}
}
