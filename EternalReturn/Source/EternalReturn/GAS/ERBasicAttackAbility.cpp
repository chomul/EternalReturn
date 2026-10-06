// Copyright Epic Games, Inc. All Rights Reserved.

#include "GAS/ERBasicAttackAbility.h"

#include "AbilitySystemComponent.h"
#include "EternalReturn.h"
#include "Combat/ERTargetingTypes.h"
#include "GAS/ERAttributeSet.h"
#include "GAS/ERCooldownEffect.h"
#include "GAS/ERGameplayTags.h"
#include "GAS/ERPassiveAbility.h"
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
	return RangeAttr > 0.f ? RangeAttr : Skill.GetMaxReach();
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

void UERBasicAttackAbility::OnProjectileTargetHit(const UERSkillData& Data, AActor* Target)
{
	FTargetResult Result;
	Result.HitActors.Add(Target);
	OnTargetsResolved(Result);
}

void UERBasicAttackAbility::OnTargetsResolved(const FTargetResult& Result)
{
	TArray<AActor*> Targets;
	for (AActor* A : Result.HitActors) { if (A) { Targets.Add(A); } }
	if (Targets.IsEmpty())
	{
		return;   // 빗나감 — 강화는 소비되지 않는다 (자체 결정값)
	}

	// ① 평타 자체 피해는 조각(피해)이 ExecuteSkill 에서 이미 줬다.

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
		// 강화 스킬의 적중 조각(피해 · 적중 효과)을 평타 판정에 얹는다. 형상(광역) 은 평타 것 (Argument 19 ②A).
		ApplyOnTargets(BuffSkill, Targets, 1.f, BuffLevel, GetExecSkill(), /*bEnhancement=*/true);
		UE_LOG(LogEternalReturn, Log, TEXT("[평타] %s 강화 소비: %s Lv.%d"),
			*GetNameSafe(GetOwningActorFromActorInfo()), *GetNameSafe(BuffSkill), BuffLevel);
		// 강화의 주인이 **패시브**면 지금 그 쿨다운 (시셀라 P 2초 — 쿨 중엔 윌슨과 합쳐도 장전 안 됨 · 사용자 2026-10-06).
		//   패시브가 아닌 스킬(재키 W)은 시전 때 이미 쿨이 돌았다 — 다시 걸지 않는다
		for (const FGameplayAbilitySpec& S : ASC->GetActivatableAbilities())
		{
			if (S.SourceObject.Get() == BuffSkill)
			{
				if (UERPassiveAbility* Passive = Cast<UERPassiveAbility>(S.GetPrimaryInstance()); Passive && Passive->IsActive())
				{
					Passive->StartCooldownNow();
				}
				break;
			}
		}
	}
	// ⭐ 1회성 — 얹었으면 지운다. 여러 개였어도 전부 (같은 평타에 다 실린다).
	if (Buffs.Num() > 0)
	{
		ASC->RemoveActiveEffects(FGameplayEffectQuery::MakeQuery_MatchAllOwningTags(Q));
	}

	// ③ "다음 N회" 자기 버프 소비 (F11-05 B 권총 D) — Charges 를 1 줄여 재적용, 0 이면 제거. 재적용은 지속시간을 새로 센다 `[자체]`.
	FGameplayTagContainer C; C.AddTag(ERTags::State_ConsumeOnAttack);
	for (const FActiveGameplayEffectHandle& Handle : ASC->GetActiveEffects(FGameplayEffectQuery::MakeQuery_MatchAllOwningTags(C)))
	{
		const FActiveGameplayEffect* Active = ASC->GetActiveGameplayEffect(Handle);
		if (!Active)
		{
			continue;
		}
		const int32 Left = FMath::RoundToInt(Active->Spec.GetSetByCallerMagnitude(ERTags::SetByCaller_Charges, false, 1.f)) - 1;
		FGameplayEffectSpec Copy = Active->Spec;
		const FString EffectName = GetNameSafe(Active->Spec.Def);
		ASC->RemoveActiveGameplayEffect(Handle);
		if (Left > 0)
		{
			Copy.SetSetByCallerMagnitude(ERTags::SetByCaller_Charges, static_cast<float>(Left));
			ASC->ApplyGameplayEffectSpecToSelf(Copy);
		}
		UE_LOG(LogEternalReturn, Log, TEXT("[평타] %s 버프 %s 소비 -> 남은 %d회%s"), *GetNameSafe(GetOwningActorFromActorInfo()), *EffectName, FMath::Max(Left, 0), Left > 0 ? TEXT("") : TEXT(" (제거)"));
	}
}
