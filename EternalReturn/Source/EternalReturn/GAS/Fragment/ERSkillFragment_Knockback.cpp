// Copyright Epic Games, Inc. All Rights Reserved.

#include "GAS/Fragment/ERSkillFragment_Knockback.h"

#include "Combat/ERForcedMove.h"
#include "Combat/ERForcedMoveComponent.h"
#include "GameFramework/Character.h"
#include "EternalReturn.h"
#include "GAS/ERGameplayAbility.h"
#include "GAS/ERSkillData.h"
#include "TimerManager.h"
#include "Presentation/ERPresentationComponent.h"

void UERSkillFragment_Knockback::OnTargetsResolved(FERSkillContext& Ctx, const TArray<AActor*>& Targets) const
{
	UERGameplayAbility* A = Ctx.Ability;
	if (!A || !Ctx.Avatar || !Ctx.bAuthority || (Distance <= 0.f && Height <= 0.f))
	{
		return;
	}
	const int32 Level = Ctx.Level;
	const UERSkillData* WallSkill = WallImpactSkill;
	// 두 번째 점 방향 (레니 R) — 조준점 → 뗄 때의 커서. 모든 대상 · 시전자가 같은 방향
	FVector CursorDir = FVector::ZeroVector;
	if (bSecondAimDirection)
	{
		FVector Second;
		if (A->GetSecondAimPoint(Second))
		{
			CursorDir = Second - Ctx.AimPoint;
			CursorDir.Z = 0.f;
			if (CursorDir.Size() < 50.f) { CursorDir = FVector::ZeroVector; }   // 자리 바로 위 — 방향 없음
		}
		const bool bFromCursor = CursorDir.Normalize();
		if (!bFromCursor)
		{
			CursorDir = Ctx.AimDirection;
			CursorDir.Z = 0.f;
			CursorDir.Normalize();
		}
		UE_LOG(LogEternalReturn, Log, TEXT("[스킬] %s 날릴 방향 — %s (%.0f°)"), *GetNameSafe(Ctx.Skill),
			bFromCursor ? TEXT("커서 (뗄 때)") : TEXT("시전자 → 자리"), CursorDir.Rotation().Yaw);
	}
	if (CasterAlsoInRadius > 0.f && FVector::Dist2D(Ctx.Avatar->GetActorLocation(), Ctx.AimPoint) <= CasterAlsoInRadius * 100.f)
	{
		if (ACharacter* Me = Cast<ACharacter>(Ctx.Avatar))
		{
			FVector D = CursorDir.IsNearlyZero() ? Ctx.AimDirection : CursorDir; D.Z = 0.f;
			if (D.Normalize())
			{
				ERForcedMove::ApplySelfMove(Me, D, Distance * 100.f, Duration);
				UERPresentationComponent::SendAnimCue(Me, CasterAnimKey);
				UE_LOG(LogEternalReturn, Log, TEXT("[스킬] %s 시전자도 범위 안 — 같이 날아감 %.1fm"), *GetNameSafe(Ctx.Skill), Distance);
			}
		}
	}
	for (AActor* Target : Targets)
	{
		// ⭐ 실험체 · 야생동물 모두 (Argument 44 — 전에는 AERCharacterBase 만 밀렸다).
		ACharacter* Character = Cast<ACharacter>(Target);
		if (!Character)
		{
			continue;
		}
		FVector Dir = bCasterLeft ? -Ctx.Avatar->GetActorRightVector()
			: bAwayFromCaster ? (Character->GetActorLocation() - Ctx.Avatar->GetActorLocation())
			: !CursorDir.IsNearlyZero() ? CursorDir : Ctx.AimDirection;
		Dir.Z = 0.f;
		if (!Dir.Normalize())
		{
			Dir = Ctx.Avatar->GetActorForwardVector();
		}
		if (!ERForcedMove::ApplyForcedMove(Character, Dir, Distance * 100.f, Duration, Height * 100.f))
		{
			continue;   // 면역 등 — ERForcedMove 가 로그
		}
		UERForcedMoveComponent* Receiver = Character->FindComponentByClass<UERForcedMoveComponent>();
		if (!WallSkill || !Receiver)
		{
			continue;   // 컴포넌트 없음은 ERForcedMove 가 로그
		}
		// 벽 충돌 1회 구독 — 넉백 시간 + 0.5초 뒤 자동 해제. 약참조 — 어빌리티 · 대상이 사라져도 안전. 람다 타이머 (E19).
		TWeakObjectPtr<UERGameplayAbility> WeakAbility(A);
		TWeakObjectPtr<UERForcedMoveComponent> WeakReceiver(Receiver);
		TWeakObjectPtr<ACharacter> WeakTarget(Character);
		TSharedRef<FDelegateHandle> HandleRef = MakeShared<FDelegateHandle>();
		*HandleRef = Receiver->OnForcedMoveWallImpact.AddLambda([WeakAbility, WeakTarget, WeakReceiver, WallSkill, Level, HandleRef, WallSfxKey = WallSfx](ACharacter* Hit, const FHitResult&)
		{
			UERGameplayAbility* Self = WeakAbility.Get();
			ACharacter* T = WeakTarget.Get();
			if (UERForcedMoveComponent* R = WeakReceiver.Get()) { R->OnForcedMoveWallImpact.Remove(*HandleRef); }
			if (!Self || !T || Hit != T)
			{
				return;
			}
			UE_LOG(LogEternalReturn, Log, TEXT("[스킬] %s <- 벽 충돌: %s 에게 %s"), *GetNameSafe(Self->GetOwningActorFromActorInfo()), *GetNameSafe(T), *GetNameSafe(WallSkill));
			UERPresentationComponent::SendSfxCue(Self->GetAvatarActorFromActorInfo(), WallSfxKey, T->GetActorLocation());
			Self->ApplyOnTargets(WallSkill, { T }, 1.f, Level);
		});
		FTimerHandle Unused;
		Character->GetWorldTimerManager().SetTimer(Unused, FTimerDelegate::CreateLambda([WeakReceiver, HandleRef]()
		{
			if (UERForcedMoveComponent* R = WeakReceiver.Get()) { R->OnForcedMoveWallImpact.Remove(*HandleRef); }
		}), Duration + 0.5f, false);
		UE_LOG(LogEternalReturn, Log, TEXT("[스킬] %s <- %s 넉백 %s %.1fm · 높이 %.1fm / %.2f초%s"),
			*GetNameSafe(A->GetOwningActorFromActorInfo()), *GetNameSafe(Ctx.Skill), *GetNameSafe(Character), Distance, Height, Duration,
			WallSkill ? TEXT(" (벽 충돌 감시)") : TEXT(""));
	}
}

