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
	UAbilityTask_WaitGameplayEvent* WaitKill = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, ERTags::Event_Kill_Dealt, nullptr, /*OnlyTriggerOnce=*/false, /*OnlyMatchExact=*/true);
	WaitKill->EventReceived.AddDynamic(this, &UERPassiveAbility::OnKillDealt);
	WaitKill->ReadyForActivation();
	// 조각이 고른 이벤트 (시셀라 P 윌슨 합침) — 태그마다 하나
	const UERSkillData* Skill = GetSkillData(Handle, ActorInfo);
	TArray<FGameplayTag> Listened;
	if (Skill)
	{
		for (const TObjectPtr<UERSkillFragment>& F : Skill->Fragments)
		{
			const FGameplayTag Tag = F ? F->GetPassiveEventTag() : FGameplayTag();
			if (Tag.IsValid() && !Listened.Contains(Tag))
			{
				Listened.Add(Tag);
				UAbilityTask_WaitGameplayEvent* WaitTag = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, Tag, nullptr, /*OnlyTriggerOnce=*/false, /*OnlyMatchExact=*/true);
				WaitTag->EventReceived.AddDynamic(this, &UERPassiveAbility::OnPassiveEvent);
				WaitTag->ReadyForActivation();
			}
		}
	}
	UE_LOG(LogEternalReturn, Log, TEXT("[패시브] %s <- %s 켜짐 (Lv.%d · 적중 · 처치 이벤트 듣기%s)"),
		*GetNameSafe(GetOwningActorFromActorInfo()), *GetNameSafe(Skill), GetAbilityLevel(Handle, ActorInfo),
		Listened.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" · %s"), *FString::JoinBy(Listened, TEXT(" · "), [](const FGameplayTag& T) { return T.ToString(); })));
	RefreshPassive();
}

void UERPassiveAbility::RefreshPassive()
{
	const UERSkillData* Skill = GetSkillData(CurrentSpecHandle, CurrentActorInfo);
	if (!Skill || !IsActive())
	{
		return;
	}
	FERSkillContext Ctx = MakeContext(Skill, 1.f);
	for (const TObjectPtr<UERSkillFragment>& F : Skill->Fragments)
	{
		if (F) { F->OnPassiveStart(Ctx); }
	}
}

void UERPassiveAbility::OnPassiveEvent(FGameplayEventData Payload)
{
	const UERSkillData* Skill = GetSkillData(CurrentSpecHandle, CurrentActorInfo);
	if (!Skill)
	{
		return;
	}
	FERSkillContext Ctx = MakeContext(Skill, 1.f);
	for (const TObjectPtr<UERSkillFragment>& F : Skill->Fragments)
	{
		if (F && F->GetPassiveEventTag() == Payload.EventTag) { F->OnPassiveEvent(Ctx, Payload); }
	}
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

void UERPassiveAbility::OnKillDealt(FGameplayEventData Payload)
{
	const UERSkillData* Skill = GetSkillData(CurrentSpecHandle, CurrentActorInfo);
	AActor* Victim = const_cast<AActor*>(Payload.Target.Get());
	if (!Skill)
	{
		return;
	}
	UE_LOG(LogEternalReturn, Log, TEXT("[패시브] %s <- %s 처치 %s (막타 — 처치 관여 임시)"),
		*GetNameSafe(GetOwningActorFromActorInfo()), *GetNameSafe(Skill), *GetNameSafe(Victim));
	FERSkillContext Ctx = MakeContext(Skill, 1.f);
	for (const TObjectPtr<UERSkillFragment>& F : Skill->Fragments)
	{
		if (F) { F->OnKillDealt(Ctx, Victim); }
	}
}
