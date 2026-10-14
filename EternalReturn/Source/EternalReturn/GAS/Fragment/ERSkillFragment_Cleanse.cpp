// Copyright Epic Games, Inc. All Rights Reserved.

#include "GAS/Fragment/ERSkillFragment_Cleanse.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "EternalReturn.h"
#include "GAS/ERAttributeSet.h"
#include "GAS/ERCCLibrary.h"
#include "GAS/ERGameplayTags.h"

void UERSkillFragment_Cleanse::OnExecute(FERSkillContext& Ctx) const
{
	if (bSelf && Ctx.bAuthority)
	{
		CleanseOne(Ctx, Ctx.ASC);
	}
}

void UERSkillFragment_Cleanse::OnTargetsResolved(FERSkillContext& Ctx, const TArray<AActor*>& Targets) const
{
	if (!bTargets || !Ctx.bAuthority)
	{
		return;
	}
	for (AActor* T : Targets)
	{
		UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(T);
		if (TargetASC && TargetASC != Ctx.ASC)   // 시전자는 bSelf 가 맡는다 (두 번 X)
		{
			CleanseOne(Ctx, TargetASC);
		}
	}
}

void UERSkillFragment_Cleanse::CleanseOne(FERSkillContext& Ctx, UAbilitySystemComponent* Who) const
{
	if (!Who)
	{
		return;
	}
	FGameplayTagContainer Remove = RemoveTags;
	if (Remove.IsEmpty())
	{
		Remove.AddTag(ERTags::State_CC);
	}
	int32 Removed = 0;
	FString Names;
	for (const FActiveGameplayEffectHandle& H : Who->GetActiveEffects(FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(Remove)))
	{
		const FActiveGameplayEffect* AGE = Who->GetActiveGameplayEffect(H);
		if (!AGE)
		{
			continue;
		}
		FGameplayTagContainer Granted;
		AGE->Spec.GetAllGrantedTags(Granted);
		if (!KeepTags.IsEmpty() && Granted.HasAny(KeepTags))
		{
			continue;   // 에어본 등 — 남긴다
		}
		Names += TEXT(" ") + GetNameSafe(AGE->Spec.Def);
		Who->RemoveActiveGameplayEffect(H);
		++Removed;
	}
	// 이동 속도 = 해제 직후 값 (둔화 재계산은 GE 제거 델리게이트에서 바로 돈다 — ERCCLibrary BindSlowRecalculation)
	UE_LOG(LogEternalReturn, Log, TEXT("[스킬] %s <- %s 해제 %d개%s (%s) · 이동 속도 %.2f"), *GetNameSafe(Who->GetAvatarActor()), *GetNameSafe(Ctx.Skill), Removed, *Names,
		*Remove.ToStringSimple(), Who->GetNumericAttribute(UERAttributeSet::GetMoveSpeedAttribute()));
	if (Removed <= 0)
	{
		return;
	}
	for (const FERSelfEffect& SE : IfRemoved)
	{
		if (SE.Effect)
		{
			ERCC::ApplyCC(Ctx.ASC, Who, SE.Effect, UERSkillData::LevelValue(SE.Duration, Ctx.Level), 0.f, UERSkillData::LevelValueSigned(SE.Magnitude, Ctx.Level));
		}
	}
}
