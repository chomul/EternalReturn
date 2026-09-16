// Copyright Epic Games, Inc. All Rights Reserved.

#include "GAS/ERBasicAttackAbility.h"

#include "AbilitySystemComponent.h"
#include "EternalReturn.h"
#include "Combat/ERTargetingTypes.h"
#include "GAS/ERAttributeSet.h"
#include "GAS/ERCooldownEffect.h"
#include "GAS/ERGameplayTags.h"
#include "GAS/ERSkillData.h"

UERBasicAttackAbility::UERBasicAttackAbility()
{
	// 무장 해제 축. 베이스가 넣은 State.Block.Skill 은 평타에는 해당 없으니 뺀다 — 침묵 중에도 평타는 된다 (역기획서 §3.3).
	ActivationBlockedTags.RemoveTag(ERTags::State_Block_Skill);
	ActivationBlockedTags.AddTag(ERTags::State_Block_BasicAttack);
}

float UERBasicAttackAbility::GetRangeMax(const UERSkillData& Skill) const
{
	// ⭐ 사거리는 무기(AttackRange 어트리뷰트, m)가 정한다. 0 이면 애셋 값으로.
	const UAbilitySystemComponent* ASC = CurrentActorInfo ? CurrentActorInfo->AbilitySystemComponent.Get() : nullptr;
	const float RangeAttr = ASC ? ASC->GetNumericAttribute(UERAttributeSet::GetAttackRangeAttribute()) : 0.f;
	return RangeAttr > 0.f ? RangeAttr : Skill.Shape.RangeMax;
}

void UERBasicAttackAbility::ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	// ⭐ 평타 간격 = 1 / 공격 속도. 스킬 가속 · Cooldowns 배열과 무관. 베이스의 ApplyCooldown 을 부르지 않는다.
	if (!ActorInfo || !ActorInfo->IsNetAuthority())
	{
		return;
	}
	const UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
	const FGameplayTagContainer* Tags = GetCooldownTags();
	if (!ASC || !Tags || Tags->IsEmpty())
	{
		return;
	}

	const float AttackSpeed = FMath::Max(ASC->GetNumericAttribute(UERAttributeSet::GetAttackSpeedAttribute()), 0.01f);
	const float Interval = 1.f / AttackSpeed;

	const FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(
		Handle, ActorInfo, ActivationInfo, UERCooldownEffect::StaticClass(), GetAbilityLevel(Handle, ActorInfo));
	if (FGameplayEffectSpec* Spec = SpecHandle.Data.Get())
	{
		Spec->DynamicGrantedTags.AppendTags(*Tags);
		Spec->SetSetByCallerMagnitude(ERTags::SetByCaller_Cooldown, Interval);
		ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, SpecHandle);
		UE_LOG(LogEternalReturn, Log, TEXT("[평타] %s 간격 %.2f초 (공격 속도 %.2f)"),
			*GetNameSafe(ActorInfo->OwnerActor.Get()), Interval, AttackSpeed);
	}
}

void UERBasicAttackAbility::OnTargetsResolved(const FTargetResult& Result)
{
	TArray<AActor*> Targets;
	for (AActor* A : Result.HitActors) { if (A) { Targets.Add(A); } }
	if (Targets.IsEmpty())
	{
		return;   // 빗나감 — 강화는 소비되지 않는다 (자체 결정값)
	}

	// ① 평타 자체
	ApplySkillDamage(Targets);

	// ② ⭐ 다음 평타 강화 — 걸려 있으면 그 스킬의 피해 · 적중 효과를 얹고 소비 (Argument 19 ②A)
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo_Ensured();
	if (!ASC)
	{
		return;
	}
	FGameplayTagContainer Q; Q.AddTag(ERTags::State_NextAttackBuff);
	const TArray<FActiveGameplayEffectHandle> Buffs = ASC->GetActiveEffects(FGameplayEffectQuery::MakeQuery_MatchAllOwningTags(Q));
	for (const FActiveGameplayEffectHandle& BuffHandle : Buffs)
	{
		const FActiveGameplayEffect* Buff = ASC->GetActiveGameplayEffect(BuffHandle);
		const UERSkillData* BuffSkill = Buff ? Cast<UERSkillData>(Buff->Spec.GetContext().GetSourceObject()) : nullptr;
		if (!BuffSkill)
		{
			UE_LOG(LogEternalReturn, Error, TEXT("[평타] State.NextAttackBuff GE 에 SourceObject(UERSkillData) 가 없다. GrantOnHitStates 를 거치지 않은 GE 다."));
			continue;
		}
		const int32 BuffLevel = FMath::Max(1, static_cast<int32>(Buff->Spec.GetLevel()));
		ApplySkillDamage(Targets, BuffSkill, BuffLevel);
		UE_LOG(LogEternalReturn, Log, TEXT("[평타] %s 강화 소비: %s Lv.%d"),
			*GetNameSafe(GetOwningActorFromActorInfo()), *GetNameSafe(BuffSkill), BuffLevel);
	}
	// ⭐ 1회성 — 얹었으면 지운다. 여러 개였어도 전부 (같은 평타에 다 실린다).
	if (Buffs.Num() > 0)
	{
		ASC->RemoveActiveEffects(FGameplayEffectQuery::MakeQuery_MatchAllOwningTags(Q));
	}
}
