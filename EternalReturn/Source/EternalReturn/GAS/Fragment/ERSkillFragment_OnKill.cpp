// Copyright Epic Games, Inc. All Rights Reserved.

#include "GAS/Fragment/ERSkillFragment_OnKill.h"

#include "AbilitySystemComponent.h"
#include "EternalReturn.h"
#include "GAS/ERGameplayTags.h"
#include "GAS/ERSkillData.h"
#include "GAS/ERSkillStateEffects.h"
#include "GAS/Fragment/ERSkillFragment_DoT.h"

void UERSkillFragment_OnKill::OnKillDealt(FERSkillContext& Ctx, AActor* Victim) const
{
	UAbilitySystemComponent* ASC = Ctx.ASC;
	if (!Ctx.bAuthority || !ASC)
	{
		return;
	}
	const FString Me = GetNameSafe(Ctx.Avatar);

	// ① 쿨다운 초기화
	if (!ResetCooldownTags.IsEmpty())
	{
		const int32 Removed = ASC->RemoveActiveEffects(FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(ResetCooldownTags));
		UE_LOG(LogEternalReturn, Log, TEXT("[처치] %s %s 쿨다운 초기화 (%d)"), *Me, *ResetCooldownTags.ToStringSimple(), Removed);
	}

	// ② 자기 상태
	if (SelfTag.IsValid())
	{
		const float D = UERSkillData::LevelValue(SelfTagDuration, Ctx.Level);
		UERSkillFragment_DoT::ApplySelfTimedTag(ASC, SelfTag, D);
		UE_LOG(LogEternalReturn, Log, TEXT("[처치] %s %s %.0f초"), *Me, *SelfTag.ToString(), D);
	}

	// ③ 연장 · 재사용 창
	if (ExtendSeconds > 0.f && ExtendWhileTag.IsValid() && ASC->HasMatchingGameplayTag(ExtendWhileTag))
	{
		const float Now = ASC->GetWorld()->GetTimeSeconds();
		float Remaining = 0.f;
		FGameplayTagContainer Extend;
		Extend.AddTag(ExtendWhileTag);
		for (const FActiveGameplayEffectHandle& H : ASC->GetActiveEffects(FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(Extend)))
		{
			ASC->ModifyActiveEffectStartTime(H, ExtendSeconds);   // 시작을 늦추면 남은 시간이 는다
			if (const FActiveGameplayEffect* E = ASC->GetActiveGameplayEffect(H))
			{
				Remaining = FMath::Max(Remaining, E->GetTimeRemaining(Now));
			}
		}
		FString RecastNote;
		if (RecastTag.IsValid())
		{
			FGameplayTagContainer RecastQuery;
			RecastQuery.AddTag(RecastTag);
			const TArray<FActiveGameplayEffectHandle> Windows = ASC->GetActiveEffects(FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(RecastQuery));
			if (!Windows.IsEmpty())
			{
				for (const FActiveGameplayEffectHandle& H : Windows) { ASC->ModifyActiveEffectStartTime(H, ExtendSeconds); }
				RecastNote = TEXT(" · 재사용 창도 연장");
			}
			else if (Remaining > 0.f)
			{
				FGameplayEffectSpec Spec(GetDefault<UERRecastWindowEffect>(), ASC->MakeEffectContext(), 1.f);
				Spec.DynamicGrantedTags.AddTag(RecastTag);
				Spec.SetSetByCallerMagnitude(ERTags::SetByCaller_StateDuration, Remaining);
				ASC->ApplyGameplayEffectSpecToSelf(Spec);
				RecastNote = FString::Printf(TEXT(" · 재사용 창 다시 열기 %.1f초 (%s)"), Remaining, *RecastTag.ToString());
			}
		}
		UE_LOG(LogEternalReturn, Log, TEXT("[처치] %s %s +%.0f초 → 남은 %.1f초%s"), *Me, *ExtendWhileTag.ToString(), ExtendSeconds, Remaining, *RecastNote);
	}
}
