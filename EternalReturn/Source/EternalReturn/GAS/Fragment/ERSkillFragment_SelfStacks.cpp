// Copyright Epic Games, Inc. All Rights Reserved.

#include "GAS/Fragment/ERSkillFragment_SelfStacks.h"

#include "AbilitySystemComponent.h"
#include "EternalReturn.h"
#include "GameFramework/Pawn.h"
#include "GameplayEffect.h"
#include "GAS/ERGameplayAbility.h"
#include "GAS/ERGameplayTags.h"
#include "GAS/ERSkillData.h"

void UERSkillFragment_SelfStacks::OnHitDealt(FERSkillContext& Ctx, AActor* Target, const FGameplayTagContainer& HitTags) const
{
	if (!Ctx.bAuthority || !Target)
	{
		return;
	}
	// 실험체만 — PlayerState 가 있는 폰 (야생동물은 AI 컨트롤러 · PlayerState 없음)
	const APawn* Pawn = Cast<APawn>(Target);
	if (bPlayersOnly && !(Pawn && Pawn->GetPlayerState()))
	{
		return;
	}
	const bool bBasic = HitTags.HasTagExact(ERTags::Damage_Type_BasicAttack);
	const int32 N = bBasic ? StacksOnBasicHit : StacksOnSkillHit;
	if (N > 0)
	{
		AddStacks(Ctx, *this, Ctx.Level, N, bBasic ? TEXT("평타 적중") : TEXT("스킬 적중"));
	}
}

void UERSkillFragment_SelfStacks::OnExecute(FERSkillContext& Ctx) const
{
	if (!Ctx.bAuthority || StacksOnExecute.IsEmpty() || !Ctx.ASC)
	{
		return;
	}
	const int32 N = StacksOnExecute[FMath::Clamp(Ctx.Level - 1, 0, StacksOnExecute.Num() - 1)];
	// 설정 · 레벨의 주인 — R 은 P 의 근성 (P 레벨 크기)
	const UERSkillFragment_SelfStacks* Cfg = this;
	int32 CfgLevel = Ctx.Level;
	if (StackSource)
	{
		Cfg = StackSource->FindFragment<UERSkillFragment_SelfStacks>();
		const FGameplayAbilitySpec* Found = Ctx.ASC->GetActivatableAbilities().FindByPredicate(
			[this](const FGameplayAbilitySpec& S) { return S.SourceObject.Get() == StackSource; });
		CfgLevel = Found ? Found->Level : 0;
		if (!Cfg || CfgLevel <= 0)
		{
			UE_LOG(LogEternalReturn, Warning, TEXT("[근성] %s — %s 에 자기 중첩 조각이 없거나 미습득 (Lv.%d) · 안 쌓음"), *GetNameSafe(Ctx.Skill), *GetNameSafe(StackSource), CfgLevel);
			return;
		}
	}
	AddStacks(Ctx, *Cfg, CfgLevel, N, TEXT("발동"));
}

void UERSkillFragment_SelfStacks::AddStacks(FERSkillContext& Ctx, const UERSkillFragment_SelfStacks& Cfg, int32 CfgLevel, int32 N, const TCHAR* Why)
{
	UAbilitySystemComponent* ASC = Ctx.ASC;
	UERGameplayAbility* A = Ctx.Ability;
	if (!ASC || !A || !Cfg.StackEffect)
	{
		UE_LOG(LogEternalReturn, Warning, TEXT("[근성] %s — 중첩 GE(StackEffect) 가 비었다"), *GetNameSafe(Ctx.Skill));
		return;
	}
	const int32 Before = ASC->GetGameplayEffectCount(Cfg.StackEffect, nullptr);
	const int32 After = FMath::Min(Cfg.MaxStacks, Before + N);
	const float PerStack = UERSkillData::LevelValue(Cfg.PerStack, CfgLevel);

	// 지금 레벨 크기로 다시 건다 — 레벨이 오른 뒤에도 맞는 값 (Argument 61 S1)
	ASC->RemoveActiveGameplayEffectBySourceEffect(Cfg.StackEffect, nullptr);
	const FGameplayEffectSpecHandle SpecHandle = A->MakeOutgoingGameplayEffectSpec(A->GetCurrentAbilitySpecHandle(), A->GetCurrentActorInfo(), A->GetCurrentActivationInfo(), Cfg.StackEffect, CfgLevel);
	if (FGameplayEffectSpec* Spec = SpecHandle.Data.Get())
	{
		Spec->SetSetByCallerMagnitude(ERTags::SetByCaller_OnHitMagnitude, 1.f + PerStack);
		Spec->SetStackCount(After);
		A->ApplySpecToSelf(SpecHandle);
	}

	// 최대 — 체력 재생 GE (레벨 값으로 다시)
	if (After >= Cfg.MaxStacks && Cfg.MaxStackEffect)
	{
		ASC->RemoveActiveGameplayEffectBySourceEffect(Cfg.MaxStackEffect, nullptr);
		const FGameplayEffectSpecHandle MaxHandle = A->MakeOutgoingGameplayEffectSpec(A->GetCurrentAbilitySpecHandle(), A->GetCurrentActorInfo(), A->GetCurrentActivationInfo(), Cfg.MaxStackEffect, CfgLevel);
		if (FGameplayEffectSpec* Spec = MaxHandle.Data.Get())
		{
			Spec->SetSetByCallerMagnitude(ERTags::SetByCaller_OnHitMagnitude, UERSkillData::LevelValue(Cfg.MaxStackMagnitude, CfgLevel));
			A->ApplySpecToSelf(MaxHandle);
		}
	}
	UE_LOG(LogEternalReturn, Log, TEXT("[근성] %s %s +%d → %d/%d (중첩당 %.1f%% · Lv.%d)%s"),
		*GetNameSafe(A->GetOwningActorFromActorInfo()), Why, N, ASC->GetGameplayEffectCount(Cfg.StackEffect, nullptr), Cfg.MaxStacks, PerStack * 100.f, CfgLevel,
		After >= Cfg.MaxStacks ? TEXT(" · 최대 — 체력 재생") : TEXT(""));
}
