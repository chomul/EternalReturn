// Copyright Epic Games, Inc. All Rights Reserved.

#include "GAS/Delivery/ERSkillDelivery.h"

#include "Combat/ERProjectileBase.h"
#include "Combat/ERProjectile_Homing.h"
#include "Combat/ERTargeting.h"
#include "EternalReturn.h"
#include "GAS/ERGameplayAbility.h"
#include "GAS/ERGameplayTags.h"
#include "GAS/ERSkillData.h"
#include "GAS/Shape/ERSkillShape.h"
#include "Presentation/ERPresentationData.h"
#include "TimerManager.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

// ── 즉시 ───────────────────────────────────────────────────
void UERDelivery_Instant::Deliver(UERGameplayAbility& Ability, const UERSkillData& Skill, const FTargetQuery& Q, const FERShapeContext& Ctx) const
{
	const UWorld* World = Ctx.Avatar ? Ctx.Avatar->GetWorld() : nullptr;
	if (!World || !Skill.Area)
	{
		return;
	}
	FTargetResult Result;
	if (FanCount > 1)
	{
		// 여러 줄 — 조준 방향을 가운데로 부채처럼 (위클라인 트리플렛 · F12.6-06). 합친다 (같은 액터는 한 번)
		for (int32 i = 0; i < FanCount; ++i)
		{
			FTargetQuery P = Q;
			const float Yaw = (i - (FanCount - 1) * 0.5f) * FanAngleDeg;
			P.Direction = Q.Direction.GetSafeNormal2D().RotateAngleAxis(Yaw, FVector::UpVector);
			const FTargetResult One = Skill.Area->Query(World, P, Ctx);
			for (AActor* A : One.HitActors) { if (A) { Result.HitActors.AddUnique(A); } }
			Ability.DebugDrawQuery(Skill, P, One);
		}
	}
	else
	{
		Result = Skill.Area->Query(World, Q, Ctx);
		Ability.DebugDrawQuery(Skill, Q, Result);   // ER.Skill.DebugDraw 1
	}
	Ability.ResolveInstantHits(Skill, Q, Result);
}

FString UERDelivery_Instant::Describe() const
{
	return FanCount > 1 ? FString::Printf(TEXT("Instant(부채 %d줄 · %.0f°)"), FanCount, FanAngleDeg) : TEXT("Instant");
}

// ── 투사체 ─────────────────────────────────────────────────
UClass* UERDelivery_Projectile::ResolveClass() const
{
	if (ProjectileClass)
	{
		return ProjectileClass.Get();
	}
	return bHoming ? AERProjectile_Homing::StaticClass() : AERProjectileBase::StaticClass();
}

void UERDelivery_Projectile::Deliver(UERGameplayAbility& Ability, const UERSkillData& Skill, const FTargetQuery& Q, const FERShapeContext& Ctx) const
{
	if (!Ability.IsExecAuthority() || !Ctx.Avatar)
	{
		return;
	}
	if (FirePattern == EERFirePattern::Sequential)
	{
		FireSequential(Ability, Skill, Q, Ctx);
		return;
	}
	const AActor* Avatar = Ctx.Avatar;
	FVector Aim = Q.Direction.GetSafeNormal2D();
	if (Aim.IsNearlyZero())
	{
		Aim = Avatar->GetActorForwardVector().GetSafeNormal2D();
	}
	const int32 N = FirePattern == EERFirePattern::Simultaneous ? FMath::Max(1, Count) : 1;
	for (int32 i = 0; i < N; ++i)
	{
		const float Yaw = (i - (N - 1) * 0.5f) * SpreadAngleDeg;   // 여러 발이면 부채처럼 (즉시 판정과 같은 규칙)
		FERProjectileLaunch L;
		// 판정 원점(Q.Origin)은 바닥 높이다 — 모습이 바닥에 붙어 날았다 (2026-10-01 로그 `높이 2`). 높이만 몸 가운데로 (판정 스윕은 캡슐 전체라 영향 없음)
		L.Start = FVector(Q.Origin.X, Q.Origin.Y, Avatar->GetActorLocation().Z);
		L.Direction = Aim.RotateAngleAxis(Yaw, FVector::UpVector);
		L.SpeedUU = Speed * 100.f;
		L.RadiusUU = FMath::Max(Radius * 100.f, 1.f);
		L.HomingTarget = bHoming ? Q.DesignatedTarget.Get() : nullptr;
		// 따라가는 탄은 사거리 대신 시간 (AERProjectile_Homing) — 대상이 순간이동해도 끝까지 (사용자 2026-10-01 · Argument 60)
		L.RangeUU = L.HomingTarget ? 0.f : Q.RangeMax * 100.f;
		L.bOnlyHomingTarget = bHitOnlyTarget && L.HomingTarget != nullptr;
		Ability.SpawnProjectile(Skill, ResolveClass(), L, Q, Ability.GetAbilityLevel(), /*ShotIndex=*/-1, bPierce);
	}
	UE_LOG(LogEternalReturn, Log, TEXT("[스킬] %s <- %s 투사체 %d발 발사 (%.0f m/s · 사거리 %.1fm) — 적중은 도착 때"),
		*GetNameSafe(Ability.GetOwningActorFromActorInfo()), *GetNameSafe(&Skill), N, Speed, Q.RangeMax);
	Ability.BeginDeferredHits(Skill, /*bAttackCue=*/!Ability.IsExecutingOther());
}

void UERDelivery_Projectile::FireSequential(UERGameplayAbility& Ability, const UERSkillData& Skill, const FTargetQuery& Q, const FERShapeContext& Ctx) const
{
	UWorld* World = Ctx.Avatar ? Ctx.Avatar->GetWorld() : nullptr;
	if (!World || !Skill.Area)
	{
		return;
	}
	const FTargetResult Scan = Skill.Area->Query(World, Q, Ctx);
	Ability.DebugDrawQuery(Skill, Q, Scan);   // ER.Skill.DebugDraw 1
	FString Names;
	for (const AActor* A : Scan.HitActors) { Names += TEXT(" ") + GetNameSafe(A); }
	UE_LOG(LogEternalReturn, Log, TEXT("[스킬] %s <- %s 스캔 %d명 (최대 %d · 가까운 순):%s"),
		*GetNameSafe(Ability.GetOwningActorFromActorInfo()), *GetNameSafe(&Skill), Scan.HitActors.Num(), Q.MaxTargets, *Names);

	if (Scan.HitActors.IsEmpty())
	{
		// 쏠 사람이 없다 — 채널 Loop 에서 끝으로 (통 몽타주 · 소유 클라는 조준 0 큐)
		Ability.JumpSkillSection(Skill, ERPresSection::End);
		Ability.SendEventCue(ERTags::GameplayCue_Pres_Aim, Skill, 0);
	}
	FTargetQuery Filter = Q;             // 팀 · 시전자 · 실험체만 — 투사체 스윕이 쓴다
	Filter.DesignatedTarget = nullptr;   // 타이머 동안 들고 있으니 사라질 수 있는 포인터는 뺀다
	const int32 Lv = Ability.GetAbilityLevel();
	// 인식된 사람 수만큼 (최대 MaxTargets) **한 명에 한 발씩** · 가까운 순 — 발마다 Fire 애니 · 사격음 (사용자 2026-10-02 "shot 애니가 인식된 사람 수만큼 한 명씩")
	for (int32 i = 0; i < Scan.HitActors.Num(); ++i)
	{
		TWeakObjectPtr<AActor> WeakTarget(Scan.HitActors[i].Get());
		// 발마다 조준 → 사격 (사용자 2026-10-02 "Loop 시작할 때 Aiming · 소리 끝나면 Fire 잠시 · 다시 Loop + Aiming_02")
		//   i 번째 조준 = i × (사격 Interval + 조준 AimTime) · 사격 = 그 + AimTime. 1발도 스캔 직후 조준부터
		const float AimAt = i * (Interval + AimTime);
		const float Delay = AimAt + AimTime;
		// ⚠ 객체에 묶지 않는 람다 — EndAbility 가 `ClearAllTimersForObject(this)` 로 **어빌리티에 묶인 타이머를 지운다** (GameplayAbility.cpp:707 · E19).
		//   R 은 1발째 직후 끝나서 CreateWeakLambda 로 묶었던 2 · 3발이 사라졌다 (2026-10-02). 인스턴스는 남으니 약참조로 부른다
		TWeakObjectPtr<UERGameplayAbility> WeakAbility(&Ability);
		TWeakObjectPtr<const UERDelivery_Projectile> WeakThis(this);
		TWeakObjectPtr<const UERSkillData> WeakSkill(&Skill);
		// 조준 — Loop 섹션 (다른 클라 복제) + 조준음 발 번호 (`Aiming` → `_02` → `_03` · 소유 클라 섹션)
		if (AimTime > 0.f)
		{
			const int32 ShotNumber = i + 1;
			auto Aim = [WeakAbility, WeakSkill, ShotNumber]()
			{
				if (UERGameplayAbility* A = WeakAbility.Get(); A && WeakSkill.IsValid())
				{
					A->JumpSkillSection(*WeakSkill.Get(), ERPresSection::Loop);
					A->SendEventCue(ERTags::GameplayCue_Pres_Aim, *WeakSkill.Get(), ShotNumber);
				}
			};
			if (AimAt <= 0.f)
			{
				Aim();
			}
			else
			{
				FTimerHandle AimHandle;   // ⚠ 발 타이머와 핸들을 나눈다 — 같은 핸들로 SetTimer 하면 앞 타이머가 지워진다
				World->GetTimerManager().SetTimer(AimHandle, FTimerDelegate::CreateLambda(Aim), AimAt, false);
			}
		}
		if (Delay <= 0.f)
		{
			FireShotAt(Ability, Skill, WeakTarget.Get(), i, Lv, Filter);
			continue;
		}
		FTimerHandle Unused;
		World->GetTimerManager().SetTimer(Unused, FTimerDelegate::CreateLambda([WeakAbility, WeakThis, WeakSkill, WeakTarget, i, Lv, Filter]()
		{
			UERGameplayAbility* A = WeakAbility.Get();
			const UERDelivery_Projectile* Self = WeakThis.Get();
			const UERSkillData* S = WeakSkill.Get();
			if (A && Self && S)
			{
				Self->FireShotAt(*A, *S, WeakTarget.Get(), i, Lv, Filter);
			}
		}), Delay, false);
	}
	Ability.BeginDeferredHits(Skill, /*bAttackCue=*/false);   // 시전음 · Fire 애니는 발마다 (FireShotAt)
}

void UERDelivery_Projectile::FireShotAt(UERGameplayAbility& Ability, const UERSkillData& Skill, AActor* Target, int32 ShotIndex, int32 Lv, const FTargetQuery& Filter) const
{
	AActor* Avatar = Ability.GetAvatarActorFromActorInfo();
	if (!Avatar || !Target)
	{
		UE_LOG(LogEternalReturn, Log, TEXT("[스킬] %s %d번째 발 — 대상이 사라졌다 · 건너뜀"), *GetNameSafe(&Skill), ShotIndex + 1);
		Ability.JumpSkillSection(Skill, ERPresSection::End);   // 조준 Loop 에 멈춰 있지 않게 (다음 발이 있으면 그 조준이 다시 Loop 로)
		Ability.SendEventCue(ERTags::GameplayCue_Pres_Aim, Skill, 0);
		return;
	}
	const float Dist = FVector::Dist2D(Avatar->GetActorLocation(), Target->GetActorLocation());
	if (CancelDistance > 0.f && Dist > CancelDistance * 100.f)
	{
		UE_LOG(LogEternalReturn, Log, TEXT("[스킬] %s %d번째 발 — %s 가 %.1fm (> %.0fm) · 건너뜀"),
			*GetNameSafe(&Skill), ShotIndex + 1, *GetNameSafe(Target), Dist / 100.f, CancelDistance);
		Ability.JumpSkillSection(Skill, ERPresSection::End);
		Ability.SendEventCue(ERTags::GameplayCue_Pres_Aim, Skill, 0);
		return;
	}
	// 쏘는 대상 쪽으로 몸을 돌린다 (Yaw 만 · 사용자 2026-10-02 "시전 땐 커서 · Shot 땐 쏘는 방향") — 서버 · 다른 클라는 이동 복제 · 소유 클라는 공격 큐로
	const FVector Face = (Target->GetActorLocation() - Avatar->GetActorLocation()).GetSafeNormal2D();
	if (!Face.IsNearlyZero())
	{
		Avatar->SetActorRotation(FRotator(0.f, Face.Rotation().Yaw, 0.f));
	}
	FERProjectileLaunch L;
	L.Start = Avatar->GetActorLocation();
	L.Direction = (Target->GetActorLocation() - Avatar->GetActorLocation()).GetSafeNormal();
	L.SpeedUU = Speed * 100.f;
	L.RangeUU = 0.f;   // 따라가는 탄은 사거리 대신 시간 (AERProjectile_Homing)
	L.RadiusUU = FMath::Max(Radius * 100.f, 1.f);
	L.HomingTarget = bHoming ? Target : nullptr;
	L.bOnlyHomingTarget = bHitOnlyTarget && bHoming;
	if (!Ability.SpawnProjectile(Skill, ResolveClass(), L, Filter, Lv, ShotIndex, bPierce))
	{
		return;
	}
	// 발마다 Execute 섹션 (다른 클라 복제) + 공격 큐 — 사격음 · 소유 클라 섹션 (몽타주가 없으면 각 머신이 Execute 애니)
	Ability.JumpSkillSection(Skill, ERPresSection::Execute);
	Ability.SendPresCues(Skill, Avatar, {}, /*bWithAttack=*/true, /*ShotNumber=*/ShotIndex + 1, Face);
	UE_LOG(LogEternalReturn, Log, TEXT("[스킬] %s %d번째 발 → %s (%.1fm)"), *GetNameSafe(&Skill), ShotIndex + 1, *GetNameSafe(Target), Dist / 100.f);
}

FString UERDelivery_Projectile::Describe() const
{
	const TCHAR* Pattern = FirePattern == EERFirePattern::Sequential ? TEXT("순차") : FirePattern == EERFirePattern::Simultaneous ? TEXT("동시") : TEXT("한 발");
	FString S = FString::Printf(TEXT("Projectile(%s · %.0f m/s · 반경 %.2f%s%s · %s"), Pattern, Speed, Radius,
		bPierce ? TEXT(" · 관통") : TEXT(""), bHoming ? (bHitOnlyTarget ? TEXT(" · 따라감(대상만)") : TEXT(" · 따라감")) : TEXT(""), *GetNameSafe(ProjectileClass.Get()));
	if (FirePattern == EERFirePattern::Simultaneous) { S += FString::Printf(TEXT(" · %d발 %.0f°"), Count, SpreadAngleDeg); }
	if (FirePattern == EERFirePattern::Sequential)   { S += FString::Printf(TEXT(" · 간격 %.2f · 조준 %.2f · 취소 %.0f"), Interval, AimTime, CancelDistance); }
	return S + TEXT(")");
}

#if WITH_EDITOR
void UERDelivery_Projectile::ValidateDelivery(FDataValidationContext& Context, const FString& Owner, int32 MaxTargets) const
{
	// 순차는 "잡힌 대상마다" — 상한이 없으면 모양 안 전원에게 쏜다 (의도면 경고만)
	if (FirePattern == EERFirePattern::Sequential && MaxTargets <= 0)
	{
		Context.AddWarning(FText::FromString(FString::Printf(TEXT("%s: 순차 사격인데 대상 MaxTargets 0 — 모양 안 전원에게 한 발씩"), *Owner)));
	}
	if (FirePattern == EERFirePattern::Sequential && !bHoming)
	{
		Context.AddWarning(FText::FromString(FString::Printf(TEXT("%s: 순차 사격인데 따라가지 않음 — 대상 방향으로 직선"), *Owner)));
	}
}
#endif
