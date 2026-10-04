// Copyright Epic Games, Inc. All Rights Reserved.

#include "GAS/ERPassiveAbility.h"

#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystemComponent.h"
#include "EternalReturn.h"
#include "GAS/ERGameplayTags.h"
#include "GAS/ERSkillData.h"
#include "GAS/Fragment/ERSkillFragment.h"

UERPassiveAbility::UERPassiveAbility()
{
	// 서버만 — 듣고 GE 를 거는 일뿐이라 클라 인스턴스가 필요 없다
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	// 패시브는 CC · 무장 해제 · 후딜과 무관 — 베이스가 넣은 차단을 비운다
	ActivationBlockedTags.Reset();
}

void UERPassiveAbility::OnAvatarSet(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	Super::OnAvatarSet(ActorInfo, Spec);
	// 부여 · 몸이 생기면 바로 켠다 (서버). 이미 켜져 있으면 엔진이 무시한다
	if (ActorInfo && ActorInfo->IsNetAuthority() && !Spec.IsActive())
	{
		ActorInfo->AbilitySystemComponent->TryActivateAbility(Spec.Handle);
	}
}

void UERPassiveAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	// ⚠ 베이스 파이프라인(조준 · 시전 · 판정 · 쿨다운)을 타지 않는다 — 듣기만
	UAbilityTask_WaitGameplayEvent* Wait = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, ERTags::Event_Hit_Dealt, nullptr, /*OnlyTriggerOnce=*/false, /*OnlyMatchExact=*/true);
	Wait->EventReceived.AddDynamic(this, &UERPassiveAbility::OnHitDealt);
	Wait->ReadyForActivation();
	UE_LOG(LogEternalReturn, Log, TEXT("[패시브] %s <- %s 켜짐 (Lv.%d · 적중 이벤트 듣기)"),
		*GetNameSafe(GetOwningActorFromActorInfo()), *GetNameSafe(GetSkillData(Handle, ActorInfo)), GetAbilityLevel(Handle, ActorInfo));
}

void UERPassiveAbility::OnHitDealt(FGameplayEventData Payload)
{
	const UERSkillData* Skill = GetSkillData(CurrentSpecHandle, CurrentActorInfo);
	AActor* Target = const_cast<AActor*>(Payload.Target.Get());
	if (!Skill || !Target)
	{
		return;
	}
	FERSkillContext Ctx = MakeContext(Skill, 1.f);
	for (const TObjectPtr<UERSkillFragment>& F : Skill->Fragments)
	{
		if (F) { F->OnHitDealt(Ctx, Target, Payload.InstigatorTags); }
	}
}
