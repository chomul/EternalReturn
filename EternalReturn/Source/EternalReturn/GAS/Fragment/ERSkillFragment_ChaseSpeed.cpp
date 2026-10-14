// Copyright Epic Games, Inc. All Rights Reserved.

#include "GAS/Fragment/ERSkillFragment_ChaseSpeed.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Core/ERTeamStatics.h"
#include "EternalReturn.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "GAS/ERAttributeSet.h"
#include "GAS/ERGameplayTags.h"
#include "GAS/ERSkillData.h"
#include "TimerManager.h"

void UERSkillFragment_ChaseSpeed::OnExecute(FERSkillContext& Ctx) const
{
	if (!Ctx.bAuthority || !Ctx.ASC || !Ctx.Avatar || !WhileTag.IsValid() || !Effect)
	{
		return;
	}
	UERChaseSpeedState* S = Ctx.GetState<UERChaseSpeedState>(this);
	if (!S)
	{
		return;
	}
	S->Frag = this;
	S->ASC = Ctx.ASC;
	S->Avatar = Ctx.Avatar;
	S->Skill = Ctx.Skill;
	S->Level = Ctx.Level;
	S->Start();
}

void UERChaseSpeedState::Start()
{
	const UERSkillFragment_ChaseSpeed* F = Frag.Get();
	AActor* Me = Avatar.Get();
	if (!F || !Me)
	{
		return;
	}
	FTimerManager& TM = Me->GetWorld()->GetTimerManager();
	if (!TM.IsTimerActive(Timer))
	{
		TM.SetTimer(Timer, FTimerDelegate::CreateUObject(this, &UERChaseSpeedState::Check), F->CheckInterval, true);
		UE_LOG(LogEternalReturn, Log, TEXT("[스킬] %s <- %s 적 쪽 이속 검사 시작 (%.0fm · %.0f° · %.2f초마다)"), *GetNameSafe(Me), *GetNameSafe(Skill.Get()),
			F->Radius, F->MaxAngleDeg, F->CheckInterval);
	}
}

void UERChaseSpeedState::Stop()
{
	UAbilitySystemComponent* A = ASC.Get();
	if (A && Buff.IsValid())
	{
		A->RemoveActiveGameplayEffect(Buff);
	}
	Buff.Invalidate();
	if (const AActor* Me = Avatar.Get())
	{
		Me->GetWorld()->GetTimerManager().ClearTimer(Timer);
	}
	UE_LOG(LogEternalReturn, Log, TEXT("[스킬] %s <- %s 적 쪽 이속 검사 끝"), *GetNameSafe(Avatar.Get()), *GetNameSafe(Skill.Get()));
}

void UERChaseSpeedState::Check()
{
	UAbilitySystemComponent* A = ASC.Get();
	const UERSkillFragment_ChaseSpeed* F = Frag.Get();
	AActor* Me = Avatar.Get();
	if (!A || !F || !Me || !A->HasMatchingGameplayTag(F->WhileTag))
	{
		Stop();
		return;
	}
	if (Buff.IsValid() && !A->GetActiveGameplayEffect(Buff))
	{
		Buff.Invalidate();   // 시간이 다 돼 스스로 빠졌다
	}

	// 움직이는 방향이 Radius 안 적 실험체 중 하나를 향하나 (서 있으면 X)
	bool bToward = false;
	const FVector Vel = Me->GetVelocity().GetSafeNormal2D();
	const AGameStateBase* GS = Me->GetWorld()->GetGameState();
	if (!Vel.IsNearlyZero() && GS)
	{
		const float CosMax = FMath::Cos(FMath::DegreesToRadians(F->MaxAngleDeg));
		const float RadiusUU = F->Radius * 100.f;
		for (const APlayerState* PS : GS->PlayerArray)
		{
			const APawn* Other = PS ? PS->GetPawn() : nullptr;
			if (!Other || !ERTeamStatics::IsHostile(Me, Other))
			{
				continue;
			}
			const UAbilitySystemComponent* OtherASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Other);
			if (!OtherASC || OtherASC->GetNumericAttribute(UERAttributeSet::GetHPAttribute()) <= 0.f || OtherASC->HasMatchingGameplayTag(ERTags::State_Untargetable))
			{
				continue;
			}
			const FVector To = Other->GetActorLocation() - Me->GetActorLocation();
			if (To.SizeSquared2D() <= FMath::Square(RadiusUU) && FVector::DotProduct(Vel, To.GetSafeNormal2D()) >= CosMax)
			{
				bToward = true;
				break;
			}
		}
	}

	if (bToward && !Buff.IsValid())
	{
		// 지속 = WhileTag 남은 시간 (검사가 못 떼도 버프와 같이 끝난다)
		float Remaining = 0.f;
		for (const float R : A->GetActiveEffectsTimeRemaining(FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(FGameplayTagContainer(F->WhileTag))))
		{
			Remaining = FMath::Max(Remaining, R);
		}
		const float Mag = UERSkillData::LevelValueSigned(F->Magnitude, Level);
		const FGameplayEffectSpecHandle Spec = A->MakeOutgoingSpec(F->Effect, Level, A->MakeEffectContext());
		if (Spec.IsValid() && Remaining > 0.f)
		{
			Spec.Data->SetSetByCallerMagnitude(ERTags::SetByCaller_CCDuration, Remaining);
			Spec.Data->SetSetByCallerMagnitude(ERTags::SetByCaller_OnHitMagnitude, Mag);
			Buff = A->ApplyGameplayEffectSpecToSelf(*Spec.Data);
			UE_LOG(LogEternalReturn, Log, TEXT("[스킬] %s <- %s 적 쪽 이동 — %s 크기 %.2f (최대 %.1f초)"), *GetNameSafe(Me), *GetNameSafe(Skill.Get()),
				*GetNameSafe(F->Effect.Get()), Mag, Remaining);
		}
	}
	else if (!bToward && Buff.IsValid())
	{
		A->RemoveActiveGameplayEffect(Buff);
		Buff.Invalidate();
		UE_LOG(LogEternalReturn, Log, TEXT("[스킬] %s <- %s 적 쪽 아님 — 이속 뗌"), *GetNameSafe(Me), *GetNameSafe(Skill.Get()));
	}
}
