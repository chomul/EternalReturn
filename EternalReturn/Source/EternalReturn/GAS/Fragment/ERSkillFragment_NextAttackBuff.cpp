// Copyright Epic Games, Inc. All Rights Reserved.

#include "GAS/Fragment/ERSkillFragment_NextAttackBuff.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "Core/ERTeamStatics.h"
#include "GAS/ERAttributeSet.h"
#include "EternalReturn.h"
#include "GAS/ERGameplayAbility.h"
#include "GAS/ERGameplayTags.h"
#include "GAS/ERSkillData.h"
#include "GAS/ERSkillStateEffects.h"

namespace
{
	/** 평타 초기화 — 진행 중 평타 끊기 · 평타 쿨 지우기 · 바로 평타 (서버 · 플레이어 입력과 같은 길: Event.Skill.Aim + 대상 히트) */
	void AttackNow(FERSkillContext& Ctx)
	{
		UAbilitySystemComponent* ASC = Ctx.ASC;
		AActor* Me = Ctx.Avatar;
		FGameplayAbilitySpec* AttackSpec = nullptr;
		for (FGameplayAbilitySpec& S : ASC->GetActivatableAbilities())
		{
			if (S.DynamicAbilityTags.HasTagExact(ERTags::Ability_Slot_Attack)) { AttackSpec = &S; break; }
		}
		if (!AttackSpec || !Me)
		{
			return;
		}
		auto Alive = [Me](AActor* T)
		{
			const UAbilitySystemComponent* TA = T ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(T) : nullptr;
			return TA && ERTeamStatics::IsHostile(Me, T) && TA->GetNumericAttribute(UERAttributeSet::GetHPAttribute()) > 0.f;
		};
		// 평타를 **치고 있을 때만** (사용자 2026-10-05) — 평타 진행 중이거나 평타 간격(쿨다운) 안. 아니면 강화만 걸고 끝 (다음 평타가 강화)
		const bool bWasActive = AttackSpec->IsActive();
		FGameplayTagContainer AttackCooldown;
		AttackCooldown.AddTag(ERTags::Cooldown_Slot_Attack);
		const bool bAttacking = bWasActive || ASC->HasAnyMatchingGameplayTags(AttackCooldown);
		const UERGameplayAbility* Prev = Cast<UERGameplayAbility>(AttackSpec->GetPrimaryInstance());
		AActor* Last = Prev ? Prev->GetAimActorForFragment() : nullptr;
		AActor* Target = Alive(Last) ? Last : nullptr;   // 치던 대상
		const TCHAR* From = TEXT("치던 대상");
		if (!Target && Alive(Ctx.AimActor))
		{
			Target = Ctx.AimActor;
			From = TEXT("커서");
		}
		if (!bAttacking || !Target)
		{
			UE_LOG(LogEternalReturn, Log, TEXT("[스킬] %s 평타 초기화 안 함 — %s · 다음 평타가 강화"),
				*GetNameSafe(Me), !bAttacking ? TEXT("평타 치는 중이 아님") : TEXT("대상 없음"));
			return;
		}
		const FGameplayAbilitySpecHandle Handle = AttackSpec->Handle;
		if (bWasActive)
		{
			ASC->CancelAbilityHandle(Handle);   // 휘두르던 평타를 끊는다 (몽타주는 강화 평타가 덮는다)
		}
		ASC->RemoveActiveEffects(FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(AttackCooldown));
		FHitResult Hit(Target, nullptr, Target->GetActorLocation(), FVector::UpVector);
		FGameplayEventData Payload;
		Payload.EventTag = ERTags::Event_Skill_Aim;
		Payload.Instigator = Me;
		Payload.TargetData = FGameplayAbilityTargetDataHandle(new FGameplayAbilityTargetData_SingleTargetHit(Hit));
		const bool bFired = ASC->TriggerAbilityFromGameplayEvent(Handle, ASC->AbilityActorInfo.Get(), ERTags::Event_Skill_Aim, &Payload, *ASC);
		UE_LOG(LogEternalReturn, Log, TEXT("[스킬] %s 평타 초기화 — 평타 %s · 쿨 지움 · %s %s → 강화 평타 %s"),
			*GetNameSafe(Me), bWasActive ? TEXT("끊음") : TEXT("없었음"), From, *GetNameSafe(Target), bFired ? TEXT("발동") : TEXT("실패 (사거리 · CC 등)"));
	}
}

void UERSkillFragment_NextAttackBuff::OnTargetsResolved(FERSkillContext& Ctx, const TArray<AActor*>& Targets) const
{
	UERGameplayAbility* A = Ctx.Ability;
	if (!A || !Ctx.ASC || !Ctx.bAuthority || !Ctx.Skill)
	{
		return;
	}
	// 강화 내용의 주인 — BuffSkill 이 있으면 그 스킬 (레벨도 그 스킬의 것). 안 배웠으면 걸지 않는다
	const UERSkillData* Source = BuffSkill ? BuffSkill.Get() : Ctx.Skill;
	int32 SourceLevel = Ctx.Level;
	if (BuffSkill)
	{
		const FGameplayAbilitySpec* Found = Ctx.ASC->GetActivatableAbilities().FindByPredicate(
			[this](const FGameplayAbilitySpec& S) { return S.SourceObject.Get() == BuffSkill; });
		SourceLevel = Found ? Found->Level : 0;
		if (SourceLevel <= 0)
		{
			UE_LOG(LogEternalReturn, Log, TEXT("[스킬] %s <- %s 다음 평타 강화 안 걺 — %s 미습득 (Lv.0)"),
				*GetNameSafe(A->GetOwningActorFromActorInfo()), *GetNameSafe(Ctx.Skill), *GetNameSafe(BuffSkill));
			return;
		}
	}

	if (Grant(Ctx, Source, SourceLevel, Duration) && bAttackNow)
	{
		AttackNow(Ctx);
	}
}

bool UERSkillFragment_NextAttackBuff::Grant(FERSkillContext& Ctx, const UERSkillData* Source, int32 SourceLevel, float InDuration)
{
	UERGameplayAbility* A = Ctx.Ability;
	if (!A || !Ctx.ASC || !Ctx.bAuthority || !Ctx.Skill || !Source)
	{
		return false;
	}
	// 이미 있으면 새로 건다 (같은 스킬이면 갱신, 다른 스킬이면 나중 것).
	FGameplayTagContainer Q; Q.AddTag(ERTags::State_NextAttackBuff);
	Ctx.ASC->RemoveActiveEffects(FGameplayEffectQuery::MakeQuery_MatchAllOwningTags(Q));

	const bool bInfinite = InDuration <= 0.f;
	const FGameplayEffectSpecHandle SpecHandle = A->MakeOutgoingGameplayEffectSpec(
		A->GetCurrentAbilitySpecHandle(), A->GetCurrentActorInfo(), A->GetCurrentActivationInfo(),
		bInfinite ? UERNextAttackBuffInfiniteEffect::StaticClass() : UERNextAttackBuffEffect::StaticClass(), SourceLevel);
	if (FGameplayEffectSpec* Spec = SpecHandle.Data.Get())
	{
		Spec->DynamicGrantedTags.AddTag(ERTags::State_NextAttackBuff);
		if (!bInfinite)
		{
			Spec->SetSetByCallerMagnitude(ERTags::SetByCaller_StateDuration, InDuration);
		}
		// ⭐ 평타가 이걸 읽어 "무슨 스킬의 피해·효과를 얹을지" 를 안다. 레벨은 Spec Level 로 간다.
		Spec->GetContext().AddSourceObject(Source);
		A->ApplySpecToSelf(SpecHandle);
		A->SendEventCue(ERTags::GameplayCue_Pres_Ready, *Ctx.Skill);   // 강화 걸림 소리 (카티야 Reinforce_Ready · K8)
		UE_LOG(LogEternalReturn, Log, TEXT("[스킬] %s <- %s 다음 평타 강화 대기 — %s Lv.%d (%s)"),
			*GetNameSafe(A->GetOwningActorFromActorInfo()), *GetNameSafe(Ctx.Skill), *GetNameSafe(Source), SourceLevel,
			bInfinite ? TEXT("만료 없음") : *FString::Printf(TEXT("%.1f초"), InDuration));
		return true;
	}
	return false;
}
