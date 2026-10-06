// Copyright Epic Games, Inc. All Rights Reserved.

#include "GAS/Fragment/ERSkillFragment_Bear.h"

#include "Abilities/GameplayAbilityTypes.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Core/ERTeamStatics.h"
#include "EternalReturn.h"
#include "GAS/ERGameplayAbility.h"
#include "GAS/ERGameplayTags.h"
#include "GAS/ERSkillData.h"
#include "GAS/ERSkillStateEffects.h"
#include "Presentation/ERPresentationComponent.h"

void UERSkillFragment_Bear::GetPassiveEventTags(FGameplayTagContainer& OutTags) const
{
	OutTags.AddTag(ERTags::Event_Ally_SkillHit);
	OutTags.AddTag(ERTags::Event_Leni_BearTriggered);
}

void UERSkillFragment_Bear::OnPassiveEvent(FERSkillContext& Ctx, const FGameplayEventData& Payload) const
{
	if (!Ctx.bAuthority || !Ctx.ASC || !Ctx.Ability)
	{
		return;
	}
	FGameplayTagContainer BearTag; BearTag.AddTag(ERTags::State_Leni_Bear);

	// ① 아군 실험체에게 곰돌이
	if (Payload.EventTag == ERTags::Event_Ally_SkillHit)
	{
		const AActor* Ally = Payload.Target.Get();
		UAbilitySystemComponent* AllyASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Ally);
		if (!AllyASC || Ally == Ctx.Avatar || ERTeamStatics::GetTeamId(Ally) == INDEX_NONE)
		{
			return;   // 자신 · 팀 없는 액터(실험체 아님) 제외
		}
		AllyASC->RemoveActiveEffects(FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(BearTag));
		FGameplayEffectSpec Spec(GetDefault<UERTimedTagEffect>(), Ctx.ASC->MakeEffectContext(), 1.f);   // 출처 = 레니 — 어트리뷰트셋이 주인을 찾는다
		Spec.DynamicGrantedTags.AddTag(ERTags::State_Leni_Bear);
		Spec.SetSetByCallerMagnitude(ERTags::SetByCaller_StateDuration, Duration);
		Ctx.ASC->ApplyGameplayEffectSpecToTarget(Spec, AllyASC);
		UE_LOG(LogEternalReturn, Log, TEXT("[곰돌이] %s → %s 곰돌이 %.0f초"), *GetNameSafe(Ctx.Avatar), *GetNameSafe(Ally), Duration);
		UERPresentationComponent::SendSfxCue(Ctx.Avatar, ERTags::Pres_Sfx_BearAppear, Ally->GetActorLocation());
		return;
	}

	// ② 곰돌이 아군이 적을 때렸다 — 소모 · 추가 피해 (레니 몫) · 쿨 감소
	if (Payload.EventTag == ERTags::Event_Leni_BearTriggered)
	{
		UAbilitySystemComponent* AllyASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Payload.Instigator.Get());
		AActor* Enemy = const_cast<AActor*>(Payload.Target.Get());
		if (!AllyASC || !Enemy)
		{
			return;
		}
		AllyASC->RemoveActiveEffects(FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(BearTag));
		Ctx.Ability->ApplyOnTargets(Ctx.Skill, { Enemy }, 1.f, Ctx.Level);
		if (const AActor* AllyActor = Payload.Instigator.Get())
		{
			UERPresentationComponent::SendSfxCue(Ctx.Avatar, ERTags::Pres_Sfx_BearShot, AllyActor->GetActorLocation());
		}
		UERPresentationComponent::SendSfxCue(Ctx.Avatar, ERTags::Pres_Sfx_SkillHit_P, Enemy->GetActorLocation());
		const float Cut = UERSkillData::LevelValue(CooldownCut, Ctx.Level);
		if (Cut > 0.f)
		{
			FGameplayTagContainer CD;
			CD.AddTag(ERTags::Cooldown_Slot_Q); CD.AddTag(ERTags::Cooldown_Slot_W); CD.AddTag(ERTags::Cooldown_Slot_E);
			for (const FActiveGameplayEffectHandle& H : Ctx.ASC->GetActiveEffects(FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(CD)))
			{
				Ctx.ASC->ModifyActiveEffectStartTime(H, -Cut);
			}
		}
		UE_LOG(LogEternalReturn, Log, TEXT("[곰돌이] %s 의 곰돌이 → %s 추가 피해 · 레니 Q · W · E 쿨 −%.2f초 · 곰돌이 사라짐"),
			*GetNameSafe(Payload.Instigator.Get()), *GetNameSafe(Enemy), Cut);
	}
}
