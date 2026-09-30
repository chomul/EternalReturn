// Copyright Epic Games, Inc. All Rights Reserved.

#include "GAS/Fragment/ERSkillFragment_CastBuff.h"

#include "AbilitySystemComponent.h"
#include "EternalReturn.h"
#include "GAS/ERGameplayTags.h"

void UERSkillFragment_CastBuff::OnCastStart(FERSkillContext& Ctx) const
{
	if (!Ctx.bAuthority || !Ctx.ASC || !Ctx.Skill)
	{
		return;
	}
	UERCastBuffState* State = Ctx.GetState<UERCastBuffState>(this);
	if (!State)
	{
		return;
	}
	RemoveAll(Ctx, TEXT("다시 시작"));
	for (const FERSelfEffect& SE : Effects)
	{
		if (!SE.Effect)
		{
			UE_LOG(LogEternalReturn, Error, TEXT("[스킬] %s 의 충전 중 자기 효과 조각에 GE 가 비어 있다."), *GetNameSafe(Ctx.Skill));
			continue;
		}
		FGameplayEffectContextHandle EffectCtx = Ctx.ASC->MakeEffectContext();
		EffectCtx.AddSourceObject(Ctx.Skill);
		const FGameplayEffectSpecHandle SpecHandle = Ctx.ASC->MakeOutgoingSpec(SE.Effect, Ctx.Level, EffectCtx);
		FGameplayEffectSpec* Spec = SpecHandle.Data.Get();
		if (!Spec)
		{
			continue;
		}
		const float Given = UERSkillData::LevelValue(SE.Duration, Ctx.Level);
		const float Duration = Given > 0.f ? Given : Ctx.Skill->CastTime;   // 비면 선딜 길이
		const float Magnitude = UERSkillData::LevelValueSigned(SE.Magnitude, Ctx.Level);
		Spec->SetSetByCallerMagnitude(ERTags::SetByCaller_CCDuration, Duration);
		if (Magnitude != 0.f) { Spec->SetSetByCallerMagnitude(ERTags::SetByCaller_OnHitMagnitude, Magnitude); }
		State->Handles.Add(Ctx.ASC->ApplyGameplayEffectSpecToSelf(*Spec));
		UE_LOG(LogEternalReturn, Log, TEXT("[스킬] %s <- %s 충전 중 자기 효과 %s (최대 %.1f초 · 크기 %.2f)"),
			*GetNameSafe(Ctx.Avatar), *GetNameSafe(Ctx.Skill), *SE.Effect->GetName(), Duration, Magnitude);
	}
}

void UERSkillFragment_CastBuff::OnExecute(FERSkillContext& Ctx) const
{
	RemoveAll(Ctx, TEXT("충전 끝"));
}

void UERSkillFragment_CastBuff::OnEnd(FERSkillContext& Ctx, bool bCancelled) const
{
	RemoveAll(Ctx, bCancelled ? TEXT("취소") : TEXT("종료"));
}

void UERSkillFragment_CastBuff::RemoveAll(FERSkillContext& Ctx, const TCHAR* Why) const
{
	UERCastBuffState* State = Ctx.bAuthority && Ctx.ASC ? Ctx.GetState<UERCastBuffState>(this) : nullptr;
	if (!State || State->Handles.IsEmpty())
	{
		return;
	}
	for (const FActiveGameplayEffectHandle& H : State->Handles)
	{
		if (H.IsValid())
		{
			Ctx.ASC->RemoveActiveGameplayEffect(H);
		}
	}
	UE_LOG(LogEternalReturn, Log, TEXT("[스킬] %s <- %s 충전 중 자기 효과 %d개 뗌 (%s)"),
		*GetNameSafe(Ctx.Avatar), *GetNameSafe(Ctx.Skill), State->Handles.Num(), Why);
	State->Handles.Reset();
}
