// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/ERProjectile_Wilson.h"

#include "AbilitySystemComponent.h"
#include "Combat/ERForcedMove.h"
#include "Combat/ERTargeting.h"
#include "Combat/ERWilson.h"
#include "Components/CapsuleComponent.h"
#include "EternalReturn.h"
#include "GameFramework/Character.h"
#include "GAS/ERAttributeSet.h"
#include "GAS/ERGameplayAbility.h"
#include "GAS/ERShield.h"
#include "GAS/ERSkillData.h"
#include "GAS/Fragment/ERSkillFragment_Wilson.h"
#include "GAS/ERGameplayTags.h"
#include "Presentation/ERPresentationComponent.h"

// ─────────────────────────────────────────────────────────────
// Q — 날아가는 윌슨
// ─────────────────────────────────────────────────────────────

void AERProjectile_Wilson::InitLaunch(UERGameplayAbility* InAbility, const UERSkillData* InSkill, int32 InLevel, const FTargetQuery& InFilter, const FERProjectileLaunch& InLaunch, int32 InShotIndex, bool bInPierce)
{
	Super::InitLaunch(InAbility, InSkill, InLevel, InFilter, InLaunch, InShotIndex, bInPierce);
	const AActor* Sissela = InAbility ? InAbility->GetAvatarActorFromActorInfo() : nullptr;
	if (AERWilson* Wilson = AERWilson::FindFor(Sissela))
	{
		const FVector From = Wilson->GetActorLocation();
		const FVector Aim = InAbility->GetAimPointForFragment();
		Launch.Start = FVector(From.X, From.Y, Launch.Start.Z);
		const FVector Dir = (Aim - From).GetSafeNormal2D();
		if (!Dir.IsNearlyZero())
		{
			Launch.Direction = Dir;
		}
		Launch.RangeUU = FMath::Max(1.f, FVector::Dist2D(From, Aim));
		Wilson->SetInFlight(true);   // 날아가는 동안 숨김 · 줍기 없음 — 착지(Drop)가 푼다
		UE_LOG(LogEternalReturn, Log, TEXT("[윌슨] Q — 떨어진 윌슨 자리에서 날아감 (%.1fm)"), Launch.RangeUU / 100.f);
	}
	// 날기 시작 — 한 번 (Skill01_Move · 사용자 2026-10-06 "움직이는 동안 한 번")
	if (InAbility && InAbility->IsExecAuthority())
	{
		UERPresentationComponent::SendSfxCue(InAbility->GetAvatarActorFromActorInfo(), ERTags::Pres_Sfx_SkillMove_Q, Launch.Start);
	}
}

void AERProjectile_Wilson::EndFlight(const TCHAR* Why)
{
	if (!bEnded && HasAuthority())
	{
		Land();
	}
	Super::EndFlight(Why);
}

void AERProjectile_Wilson::Land()
{
	UERGameplayAbility* A = Ability.Get();
	ACharacter* Sissela = A ? Cast<ACharacter>(A->GetAvatarActorFromActorInfo()) : nullptr;
	const UERSkillFragment_WilsonThrow* Throw = Skill ? Skill->FindFragment<UERSkillFragment_WilsonThrow>() : nullptr;
	if (!A || !Sissela || !Throw)
	{
		UE_LOG(LogEternalReturn, Error, TEXT("[윌슨] Q 착지 — 어빌리티 · 시셀라 · '윌슨 던지기' 조각 중 없는 것이 있다 (%s)"), *GetNameSafe(Skill));
		return;
	}
	const FVector Here = GetActorLocation();
	if (Throw->LandSkill)
	{
		// 길에서 맞은 적도 착지 범위에선 또 맞는다 (두 피해가 따로 · 툴팁) — 거르는 건 시전자만
		FTargetQuery Q = Filter;
		Q.Shape = ESkillTargeting::GroundCircle;
		Q.Origin = FVector(Here.X, Here.Y, Sissela->GetActorLocation().Z);
		Q.RadiusOuter = Throw->LandRadius;
		Q.IgnoredActors.Reset();
		Q.IgnoredActors.Add(Sissela);
		TArray<AActor*> Targets;
		for (AActor* T : ERTargeting::Query(GetWorld(), Q).HitActors) { if (T) { Targets.Add(T); } }
		UE_LOG(LogEternalReturn, Log, TEXT("[윌슨] Q 착지 — 반경 %.1fm 안 %d명 (%s)"), Throw->LandRadius, Targets.Num(), *GetNameSafe(Throw->LandSkill));
		if (!Targets.IsEmpty())
		{
			A->ApplyOnTargets(Throw->LandSkill, Targets, 1.f, Level, Throw->LandSkill);
		}
	}
	// 착지 폭발 소리 — 맞힌 사람이 없어도 · 대상마다 타격음 대신 한 번 (Skill01_Hit2 · Audio/Sissela.md 6)
	UERPresentationComponent::SendSfxCue(Sissela, ERTags::Pres_Sfx_SkillLand_Q, Here);
	AERWilson::Drop(Sissela, Here, Throw->WilsonClass);
}

// ─────────────────────────────────────────────────────────────
// E — 몸 뻗기
// ─────────────────────────────────────────────────────────────

void AERProjectile_WilsonTether::InitLaunch(UERGameplayAbility* InAbility, const UERSkillData* InSkill, int32 InLevel, const FTargetQuery& InFilter, const FERProjectileLaunch& InLaunch, int32 InShotIndex, bool bInPierce)
{
	Super::InitLaunch(InAbility, InSkill, InLevel, InFilter, InLaunch, InShotIndex, bInPierce);
	const AActor* Sissela = InAbility ? InAbility->GetAvatarActorFromActorInfo() : nullptr;
	if (const AERWilson* Wilson = AERWilson::FindFor(Sissela))
	{
		bFromWilson = true;
		const FVector From = Wilson->GetActorLocation();
		Launch.Start = FVector(From.X, From.Y, Launch.Start.Z);
		const FVector Dir = (InAbility->GetAimPointForFragment() - From).GetSafeNormal2D();
		if (!Dir.IsNearlyZero())
		{
			Launch.Direction = Dir;
		}
		UE_LOG(LogEternalReturn, Log, TEXT("[윌슨] E — 윌슨 자리에서 뻗음 (시셀라까지 %.1fm)"), FVector::Dist2D(From, Sissela->GetActorLocation()) / 100.f);
	}
}

void AERProjectile_WilsonTether::Tick(float DeltaSeconds)
{
	// 시셀라도 맞는다 (윌슨이 떨어져 있을 때만) — 팀 필터가 시전자를 빼므로 따로 본다
	if (HasAuthority() && bFlying && !bEnded && bFromWilson)
	{
		const UERGameplayAbility* A = Ability.Get();
		const ACharacter* Sissela = A ? Cast<ACharacter>(A->GetAvatarActorFromActorInfo()) : nullptr;
		if (Sissela)
		{
			const FVector Now = GetActorLocation();
			const FVector P = Sissela->GetActorLocation();
			const FVector OnSeg = FMath::ClosestPointOnSegment(FVector(P.X, P.Y, Now.Z), FVector(LastLocation.X, LastLocation.Y, Now.Z), Now);
			const float Reach = Launch.RadiusUU + Sissela->GetCapsuleComponent()->GetScaledCapsuleRadius();
			if (FVector::Dist2D(OnSeg, P) <= Reach)
			{
				HitSissela();
				return;
			}
		}
	}
	Super::Tick(DeltaSeconds);
}

void AERProjectile_WilsonTether::OnHitTarget(AActor* Target)
{
	Super::OnHitTarget(Target);   // 피해 · 기절 (E DA 조각) · 비관통이라 여기서 끝
	if (!HasAuthority())
	{
		return;
	}
	UERGameplayAbility* A = Ability.Get();
	const AActor* Sissela = A ? A->GetAvatarActorFromActorInfo() : nullptr;
	const UERSkillFragment_WilsonTether* Tether = Skill ? Skill->FindFragment<UERSkillFragment_WilsonTether>() : nullptr;
	ACharacter* Victim = Cast<ACharacter>(Target);
	if (!Sissela || !Tether || !Victim)
	{
		return;
	}
	const AERWilson* Wilson = AERWilson::FindFor(Sissela);
	const FVector Dest = Wilson ? Wilson->GetActorLocation() : Sissela->GetActorLocation();
	FVector To = Dest - Victim->GetActorLocation();
	To.Z = 0.f;
	const float DistUU = To.Size() - Tether->PullStopShort * 100.f;
	if (DistUU > 10.f)
	{
		ERForcedMove::ApplyForcedMove(Victim, To, DistUU, DistUU / (Tether->PullSpeed * 100.f), 0.f, /*bThroughWalls=*/true);   // E 는 벽을 넘는다 (사용자 2026-10-06)
	}
	UE_LOG(LogEternalReturn, Log, TEXT("[윌슨] E 적중 %s → %s 쪽으로 %.1fm 끌어옴"), *GetNameSafe(Victim), Wilson ? TEXT("윌슨") : TEXT("시셀라"), FMath::Max(DistUU, 0.f) / 100.f);
	// 기절 · 끌기 소리 — 타격음(SkillHit.E)과 겹쳐도 된다 (사용자 2026-10-06)
	AActor* SisselaActor = const_cast<AActor*>(Sissela);
	UERPresentationComponent::SendSfxCue(SisselaActor, ERTags::Pres_Sfx_SkillStun_E, Victim->GetActorLocation());
	UERPresentationComponent::SendSfxCue(SisselaActor, ERTags::Pres_Sfx_SkillPull_E, Victim->GetActorLocation());
}

void AERProjectile_WilsonTether::HitSissela()
{
	UERGameplayAbility* A = Ability.Get();
	AActor* Sissela = A ? A->GetAvatarActorFromActorInfo() : nullptr;
	UAbilitySystemComponent* ASC = A ? A->GetAbilitySystemComponentFromActorInfo() : nullptr;
	const UERSkillFragment_WilsonTether* Tether = Skill ? Skill->FindFragment<UERSkillFragment_WilsonTether>() : nullptr;
	AERWilson* Wilson = AERWilson::FindFor(Sissela);
	if (!ASC || !Tether || !Wilson)
	{
		EndFlight(TEXT("시셀라 (처리 못 함)"));
		return;
	}
	UE_LOG(LogEternalReturn, Log, TEXT("[윌슨] E 시셀라 적중 — 윌슨에게 끌려감 · 보호막 · 쿨 −%.1f초"), Tether->SelfHitCooldownCut);
	Wilson->PullOwner(Tether->PullSpeed);
	const float Amount = UERSkillData::LevelValue(Tether->ShieldBase, Level)
		+ ASC->GetNumericAttribute(UERAttributeSet::GetSkillAmpAttribute()) * UERSkillData::LevelValue(Tether->ShieldSkillAmpRatio, Level);
	ERShield::Apply(ASC, Amount, Tether->ShieldDuration, GetNameSafe(Skill));
	UERPresentationComponent::SendSfxCue(Sissela, ERTags::Pres_Sfx_SkillPull_E, Sissela->GetActorLocation());     // Skill03_Take
	UERPresentationComponent::SendSfxCue(Sissela, ERTags::Pres_Sfx_SkillShield_E, Sissela->GetActorLocation());   // Skill03_Shield
	const FGameplayTagContainer& Cooldown = A->GetCooldownTagsForFragment();
	if (Tether->SelfHitCooldownCut > 0.f && !Cooldown.IsEmpty())
	{
		for (const FActiveGameplayEffectHandle& H : ASC->GetActiveEffects(FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(Cooldown)))
		{
			ASC->ModifyActiveEffectStartTime(H, -Tether->SelfHitCooldownCut);
		}
	}
	EndFlight(TEXT("시셀라"));
}
