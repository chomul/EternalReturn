// Copyright Epic Games, Inc. All Rights Reserved.

#include "GAS/Fragment/ERSkillFragment_WhileBuff.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Core/ERPlayerState.h"
#include "EternalReturn.h"
#include "GameFramework/Pawn.h"
#include "GAS/ERCCLibrary.h"
#include "GAS/ERGameplayAbility.h"
#include "GAS/ERGameplayTags.h"
#include "GAS/ERSkillData.h"
#include "TimerManager.h"

void UERSkillFragment_WhileBuff::OnExecute(FERSkillContext& Ctx) const
{
	if (!Ctx.bAuthority || !Ctx.ASC || !WhileTag.IsValid())
	{
		return;
	}
	UERWhileBuffState* S = Ctx.GetState<UERWhileBuffState>(this);
	if (!S)
	{
		return;
	}
	S->Frag = this;
	S->Skill = Ctx.Skill;
	S->Ability = Ctx.Ability;
	S->Level = Ctx.Level;   // 다시 쓰면 레벨만 바뀐다 (이미 듣는 중)
	S->Bind(Ctx.ASC);
}

void UERWhileBuffState::Bind(UAbilitySystemComponent* InASC)
{
	const UERSkillFragment_WhileBuff* F = Frag.Get();
	if (!InASC || !F || HitHandle.IsValid())
	{
		return;
	}
	ASC = InASC;
	HitHandle = InASC->GenericGameplayEventCallbacks.FindOrAdd(ERTags::Event_Hit_Dealt).AddUObject(this, &UERWhileBuffState::OnHit);
	TagHandle = InASC->RegisterGameplayTagEvent(F->WhileTag, EGameplayTagEventType::NewOrRemoved).AddUObject(this, &UERWhileBuffState::OnTagChanged);
	UE_LOG(LogEternalReturn, Log, TEXT("[스킬] %s <- %s 버프 중 적중 듣기 시작 (%s)"), *GetNameSafe(InASC->GetAvatarActor()), *GetNameSafe(Skill.Get()), *F->WhileTag.ToString());
}

void UERWhileBuffState::Unbind()
{
	UAbilitySystemComponent* A = ASC.Get();
	const UERSkillFragment_WhileBuff* F = Frag.Get();
	if (A && HitHandle.IsValid())
	{
		if (FGameplayEventMulticastDelegate* D = A->GenericGameplayEventCallbacks.Find(ERTags::Event_Hit_Dealt))
		{
			D->Remove(HitHandle);
		}
	}
	if (A && F && TagHandle.IsValid())
	{
		A->RegisterGameplayTagEvent(F->WhileTag, EGameplayTagEventType::NewOrRemoved).Remove(TagHandle);
	}
	HitHandle.Reset();
	TagHandle.Reset();
	UE_LOG(LogEternalReturn, Log, TEXT("[스킬] %s <- %s 버프 중 적중 듣기 끝"), *GetNameSafe(A ? A->GetAvatarActor() : nullptr), *GetNameSafe(Skill.Get()));
}

void UERWhileBuffState::OnTagChanged(const FGameplayTag Tag, int32 Count)
{
	if (Count <= 0)
	{
		Unbind();
	}
}

void UERWhileBuffState::OnHit(const FGameplayEventData* Payload)
{
	UAbilitySystemComponent* A = ASC.Get();
	const UERSkillFragment_WhileBuff* F = Frag.Get();
	if (!A || !F || !Payload)
	{
		return;
	}
	if (!A->HasMatchingGameplayTag(F->WhileTag))
	{
		Unbind();   // 태그 이벤트를 놓쳤어도 (붙기 전에 묶였거나) 여기서 끊는다
		return;
	}
	if (F->bBasicAttackOnly && !Payload->InstigatorTags.HasTag(ERTags::Damage_Type_BasicAttack))
	{
		return;
	}
	AActor* Target = const_cast<AActor*>(Payload->Target.Get());
	UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target);
	const APawn* TargetPawn = Cast<APawn>(Target);
	const bool bPlayer = TargetPawn && TargetPawn->GetPlayerState<AERPlayerState>();
	if (F->bPlayersOnly && !bPlayer)
	{
		return;   // 야생동물 적중은 안 센다 (스트라이더)
	}
	if (F->HitSkill && Target)
	{
		if (F->bOnce)
		{
			// 같은 프레임의 적중을 모아 다음 틱에 가장 가까운 한 명 — 광역기 (Argument 80 D6 c)
			Pending.AddUnique(Target);
			if (!bResolveQueued)
			{
				bResolveQueued = true;
				A->GetWorld()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateUObject(this, &UERWhileBuffState::ResolveOnce));
			}
		}
		else if (UERGameplayAbility* GA = Ability.Get())
		{
			UE_LOG(LogEternalReturn, Log, TEXT("[스킬] %s <- %s 버프 중 적중 %s — %s"), *GetNameSafe(A->GetAvatarActor()), *GetNameSafe(Skill.Get()),
				*GetNameSafe(Target), *GetNameSafe(F->HitSkill));
			GA->ApplyOnTargets(F->HitSkill, {Target}, 1.f, Level);
		}
	}
	if (F->HitEffect && TargetASC)
	{
		ERCC::ApplyCC(A, TargetASC, F->HitEffect, UERSkillData::LevelValue(F->HitDuration, Level),
			UERSkillData::LevelValue(F->HitSlowPercent, Level), UERSkillData::LevelValueSigned(F->HitMagnitude, Level));
	}
	const float Extend = UERSkillData::LevelValue(F->ExtendSeconds, Level);
	if (Extend > 0.f && (!F->bExtendOnPlayersOnly || bPlayer))
	{
		FGameplayTagContainer Tags;
		Tags.AddTag(F->WhileTag);
		const int32 N = ERSkill::ShiftCooldown(A, Tags, -Extend);   // 시작을 늦추면 남은 시간이 는다 (쿨다운 함수지만 태그로 GE 를 찾을 뿐)
		UE_LOG(LogEternalReturn, Log, TEXT("[스킬] %s <- %s 적중 %s — 버프 %d개 +%.1f초"), *GetNameSafe(A->GetAvatarActor()), *GetNameSafe(Skill.Get()),
			*GetNameSafe(Target), N, Extend);
	}
}

void UERWhileBuffState::ResolveOnce()
{
	bResolveQueued = false;
	UAbilitySystemComponent* A = ASC.Get();
	const UERSkillFragment_WhileBuff* F = Frag.Get();
	UERGameplayAbility* GA = Ability.Get();
	const AActor* Me = A ? A->GetAvatarActor() : nullptr;
	TArray<TWeakObjectPtr<AActor>> Candidates = MoveTemp(Pending);
	Pending.Reset();
	if (!A || !F || !GA || !Me || !F->HitSkill)
	{
		return;
	}
	AActor* Nearest = nullptr;
	float BestSq = TNumericLimits<float>::Max();
	for (const TWeakObjectPtr<AActor>& W : Candidates)
	{
		if (AActor* T = W.Get())
		{
			const float D = FVector::DistSquared2D(T->GetActorLocation(), Me->GetActorLocation());
			if (D < BestSq) { BestSq = D; Nearest = T; }
		}
	}
	if (!Nearest)
	{
		return;
	}
	Unbind();   // 먼저 끊는다 — 아래 HitSkill 피해가 다시 적중 이벤트를 낸다
	UE_LOG(LogEternalReturn, Log, TEXT("[스킬] %s <- %s 버프 중 적중 %s — %s (같은 프레임 %d명 중 가장 가까움)"), *GetNameSafe(Me), *GetNameSafe(Skill.Get()),
		*GetNameSafe(Nearest), *GetNameSafe(F->HitSkill), Candidates.Num());
	GA->ApplyOnTargets(F->HitSkill, {Nearest}, 1.f, Level);

	// 발동 뒤 버프 남은 시간을 N초로 다시 (스트라이더 "5초 동안 이동 속도 증가" · T1)
	const float Refresh = UERSkillData::LevelValue(F->RefreshOnTrigger, Level);
	if (Refresh > 0.f)
	{
		FGameplayTagContainer Tags;
		Tags.AddTag(F->WhileTag);
		float Remaining = 0.f;
		for (const float R : A->GetActiveEffectsTimeRemaining(FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(Tags)))
		{
			Remaining = FMath::Max(Remaining, R);
		}
		if (Remaining > 0.f && Refresh > Remaining)
		{
			const int32 N = ERSkill::ShiftCooldown(A, Tags, -(Refresh - Remaining));
			UE_LOG(LogEternalReturn, Log, TEXT("[스킬] %s <- %s 발동 — 버프 %d개 남은 %.1f초 → %.1f초"), *GetNameSafe(Me), *GetNameSafe(Skill.Get()), N, Remaining, Refresh);
		}
	}
}
