// Copyright Epic Games, Inc. All Rights Reserved.

#include "GAS/Fragment/ERSkillFragment_Knockback.h"

#include "Combat/ERForcedMove.h"
#include "Combat/ERForcedMoveComponent.h"
#include "GameFramework/Character.h"
#include "EternalReturn.h"
#include "GAS/ERGameplayAbility.h"
#include "GAS/ERSkillData.h"
#include "TimerManager.h"

void UERSkillFragment_Knockback::OnTargetsResolved(FERSkillContext& Ctx, const TArray<AActor*>& Targets) const
{
	UERGameplayAbility* A = Ctx.Ability;
	if (!A || !Ctx.Avatar || !Ctx.bAuthority || (Distance <= 0.f && Height <= 0.f))
	{
		return;
	}
	const int32 Level = Ctx.Level;
	const UERSkillData* WallSkill = WallImpactSkill;
	for (AActor* Target : Targets)
	{
		// ⭐ 실험체 · 야생동물 모두 (Argument 44 — 전에는 AERCharacterBase 만 밀렸다).
		ACharacter* Character = Cast<ACharacter>(Target);
		if (!Character)
		{
			continue;
		}
		FVector Dir = bCasterLeft ? -Ctx.Avatar->GetActorRightVector()
			: bAwayFromCaster ? (Character->GetActorLocation() - Ctx.Avatar->GetActorLocation()) : Ctx.AimDirection;
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
		*HandleRef = Receiver->OnForcedMoveWallImpact.AddLambda([WeakAbility, WeakTarget, WeakReceiver, WallSkill, Level, HandleRef](ACharacter* Hit, const FHitResult&)
		{
			UERGameplayAbility* Self = WeakAbility.Get();
			ACharacter* T = WeakTarget.Get();
			if (UERForcedMoveComponent* R = WeakReceiver.Get()) { R->OnForcedMoveWallImpact.Remove(*HandleRef); }
			if (!Self || !T || Hit != T)
			{
				return;
			}
			UE_LOG(LogEternalReturn, Log, TEXT("[스킬] %s <- 벽 충돌: %s 에게 %s"), *GetNameSafe(Self->GetOwningActorFromActorInfo()), *GetNameSafe(T), *GetNameSafe(WallSkill));
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
