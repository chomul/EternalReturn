// Copyright Epic Games, Inc. All Rights Reserved.

#include "GAS/ERShield.h"

#include "AbilitySystemComponent.h"
#include "EternalReturn.h"
#include "GAS/ERAttributeSet.h"
#include "GAS/ERGameplayTags.h"
#include "GAS/ERSkillStateEffects.h"
#include "TimerManager.h"

void ERShield::Apply(UAbilitySystemComponent* ASC, float Amount, float Duration, const FString& Why)
{
	if (!ASC || !ASC->IsOwnerActorAuthoritative() || Amount <= 0.f || Duration <= 0.f)
	{
		return;
	}
	ASC->ApplyModToAttribute(UERAttributeSet::GetShieldAttribute(), EGameplayModOp::Additive, Amount);

	FGameplayEffectSpec Spec(GetDefault<UERTimedTagEffect>(), ASC->MakeEffectContext(), 1.f);
	Spec.DynamicGrantedTags.AddTag(ERTags::State_Shielded);
	Spec.SetSetByCallerMagnitude(ERTags::SetByCaller_StateDuration, Duration);
	const FActiveGameplayEffectHandle Handle = ASC->ApplyGameplayEffectSpecToSelf(Spec);
	UE_LOG(LogEternalReturn, Log, TEXT("[보호막] %s +%.1f (%s · %.1f초) → %.1f"),
		*GetNameSafe(ASC->GetAvatarActor()), Amount, *Why, Duration, ASC->GetNumericAttribute(UERAttributeSet::GetShieldAttribute()));

	// 끝나면 0 — 다른 보호막 태그가 아직 남아 있으면 그대로 (그게 끝날 때 0). 제거 알림 안에서는 태그가 아직 셀 수 있어 다음 틱에 본다
	if (FOnActiveGameplayEffectRemoved_Info* Removed = ASC->OnGameplayEffectRemoved_InfoDelegate(Handle))
	{
		TWeakObjectPtr<UAbilitySystemComponent> WeakASC(ASC);
		Removed->AddLambda([WeakASC](const FGameplayEffectRemovalInfo&)
		{
			UAbilitySystemComponent* A = WeakASC.Get();
			UWorld* World = A ? A->GetWorld() : nullptr;
			if (!World)
			{
				return;
			}
			World->GetTimerManager().SetTimerForNextTick([WeakASC]()
			{
				UAbilitySystemComponent* B = WeakASC.Get();
				if (!B || B->HasMatchingGameplayTag(ERTags::State_Shielded))
				{
					return;
				}
				const float Left = B->GetNumericAttribute(UERAttributeSet::GetShieldAttribute());
				if (Left > 0.f)
				{
					B->SetNumericAttributeBase(UERAttributeSet::GetShieldAttribute(), 0.f);
					UE_LOG(LogEternalReturn, Log, TEXT("[보호막] %s 시간 끝 — 남은 %.1f 사라짐"), *GetNameSafe(B->GetAvatarActor()), Left);
				}
			});
		});
	}
}
