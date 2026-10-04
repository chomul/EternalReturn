// Copyright Epic Games, Inc. All Rights Reserved.

#include "GAS/Fragment/ERSkillFragment_Ride.h"

#include "AbilitySystemComponent.h"
#include "Combat/ERRideComponent.h"
#include "Combat/ERTargeting.h"
#include "EternalReturn.h"
#include "GameplayEffect.h"
#include "GAS/ERGameplayAbility.h"
#include "GAS/ERGameplayTags.h"
#include "GAS/ERSkillData.h"

void UERSkillFragment_Ride::OnExecute(FERSkillContext& Ctx) const
{
	if (!Ctx.bAuthority || !Ctx.Avatar || !Ctx.Ability || !Ctx.ASC)
	{
		return;
	}
	UERRideComponent* Ride = Ctx.Avatar->FindComponentByClass<UERRideComponent>();
	if (!Ride)
	{
		UE_LOG(LogEternalReturn, Warning, TEXT("[탑승] %s 에 UERRideComponent 가 없다"), *GetNameSafe(Ctx.Avatar));
		return;
	}
	if (bDismount)
	{
		Ride->EndServer(TEXT("재사용 — 바이크 발사"));
		return;
	}
	if (!RideEffect)
	{
		UE_LOG(LogEternalReturn, Warning, TEXT("[탑승] %s — 탑승 GE(RideEffect) 가 비었다"), *GetNameSafe(Ctx.Skill));
		return;
	}
	UERGameplayAbility* A = Ctx.Ability;
	const FGameplayEffectSpecHandle SpecHandle = A->MakeOutgoingGameplayEffectSpec(A->GetCurrentAbilitySpecHandle(), A->GetCurrentActorInfo(), A->GetCurrentActivationInfo(), RideEffect, Ctx.Level);
	if (FGameplayEffectSpec* Spec = SpecHandle.Data.Get())
	{
		Spec->SetSetByCallerMagnitude(ERTags::SetByCaller_CCDuration, Duration);
		Spec->SetSetByCallerMagnitude(ERTags::SetByCaller_OnHitMagnitude, SpeedMax);
		A->ApplySpecToSelf(SpecHandle);
	}
	Ride->BeginServer(this, A);
	UE_LOG(LogEternalReturn, Log, TEXT("[탑승] %s <- %s 시작 %.0f초 (%.0f → %.0f m/s · %.1f초 · 초당 %.0f°)"),
		*GetNameSafe(Ctx.Avatar), *GetNameSafe(Ctx.Skill), Duration, SpeedStart, SpeedMax, RampTime, TurnRateDeg);
}

void UERSkillFragment_Ride::OnLocalExecute(FERSkillContext& Ctx) const
{
	// 재사용(리캐스트) 발동 · 내리기 데이터는 조종을 새로 시작하지 않는다
	if (bDismount || Ctx.bActivatedByRecast || !Ctx.Avatar)
	{
		return;
	}
	if (UERRideComponent* Ride = Ctx.Avatar->FindComponentByClass<UERRideComponent>())
	{
		Ride->BeginLocal(this);
	}
}

int32 UERSkillFragment_Ride::ExplodeAt(UERGameplayAbility* Ability, const FVector& Center, const TCHAR* Why) const
{
	AActor* Avatar = Ability ? Ability->GetAvatarActorFromActorInfo() : nullptr;
	if (!Avatar || !ExplodeSkill)
	{
		return 0;
	}
	FTargetQuery Q;
	Q.Shape = ESkillTargeting::SelfRadius;
	Q.Origin = Center;
	Q.RangeMax = ExplodeRadius;
	Q.TeamFilter = ExplodeSkill->Targets.Team;
	Q.bPlayersOnly = ExplodeSkill->Targets.bPlayersOnly;
	Q.Instigator = Avatar;
	const FTargetResult Result = ERTargeting::Query(Avatar->GetWorld(), Q);
	TArray<AActor*> Targets;
	for (AActor* T : Result.HitActors) { if (T) { Targets.Add(T); } }
	Ability->ApplyOnTargets(ExplodeSkill, Targets);
	Ability->SendPresCues(*ExplodeSkill, Avatar, Targets, /*bWithAttack=*/false);   // 타격음만 — 공격음이면 R 시전(탑승) 소리가 또 난다 (Audio/Magnus.md)
	UE_LOG(LogEternalReturn, Log, TEXT("[탑승] %s 폭발 (%s) @%s 반경 %.1fm — %d명"),
		*GetNameSafe(Avatar), Why, *Center.ToCompactString(), ExplodeRadius, Targets.Num());
	return Targets.Num();
}
