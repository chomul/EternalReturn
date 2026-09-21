// Copyright Epic Games, Inc. All Rights Reserved.

#include "GAS/Fragment/ERSkillFragment_SelfMove.h"

#include "Combat/ERForcedMove.h"
#include "Combat/ERTargeting.h"
#include "EternalReturn.h"
#include "GAS/ERGameplayAbility.h"
#include "GameFramework/Character.h"

const TCHAR* UERSkillFragment_SelfMove::BlinkFailReason(const FERSkillContext& Ctx) const
{
	if (!Ctx.Avatar || !Ctx.AimActor || Ctx.AimActor == Ctx.Avatar)
	{
		return TEXT("지정 대상이 없다");
	}
	const float RangeUU = (Ctx.Skill && Ctx.Ability) ? Ctx.Ability->GetRangeMaxFor(*Ctx.Skill) * 100.f : 0.f;
	if (FVector::Dist2D(Ctx.Avatar->GetActorLocation(), Ctx.AimActor->GetActorLocation()) > RangeUU + 50.f)   // 캡슐 여유 0.5m
	{
		return TEXT("사거리 밖");
	}
	return nullptr;
}

bool UERSkillFragment_SelfMove::CanExecute(const FERSkillContext& Ctx, FString& OutReason) const
{
	if (Mode == ESkillSelfMove::BlinkBehindTarget)
	{
		if (const TCHAR* Fail = BlinkFailReason(Ctx))
		{
			OutReason = Fail;
			return false;
		}
	}
	return true;
}

void UERSkillFragment_SelfMove::OnExecute(FERSkillContext& Ctx) const
{
	ACharacter* Avatar = Cast<ACharacter>(Ctx.Avatar);
	if (!Ctx.bAuthority || !Avatar || !Ctx.Skill || Mode == ESkillSelfMove::None)
	{
		return;
	}
	const AActor* Owner = Ctx.Ability->GetOwningActorFromActorInfo();
	FVector Direction = Ctx.AimDirection;
	float DistanceUU = Distance * 100.f;

	switch (Mode)
	{
	case ESkillSelfMove::BlinkBehindTarget:
	{
		// 단검 D — 대상 건너편으로 **텔레포트** (지형 통과 ○). RootMotion 이 아니라 SetActorLocation. 이동 복제로 클라에 간다.
		if (const TCHAR* Fail = BlinkFailReason(Ctx))
		{
			UE_LOG(LogEternalReturn, Warning, TEXT("[스킬] %s <- %s 블링크 실패 — %s"), *GetNameSafe(Owner), *GetNameSafe(Ctx.Skill), Fail);
			return;
		}
		const FVector From = Avatar->GetActorLocation();
		const FVector To = Ctx.AimActor->GetActorLocation();
		FVector Through = To - From; Through.Z = 0.f;
		if (!Through.Normalize()) { Through = Ctx.AimDirection; }
		FVector Dest = To + Through * DistanceUU; Dest.Z = From.Z;
		Avatar->SetActorLocation(Dest, /*bSweep=*/false, nullptr, ETeleportType::TeleportPhysics);
		Avatar->SetActorRotation(FRotator(0.f, (-Through).Rotation().Yaw, 0.f));   // 대상을 등 뒤에서 바라본다
		UE_LOG(LogEternalReturn, Log, TEXT("[스킬] %s <- %s 블링크 %s 뒤 %.0fcm -> %s"),
			*GetNameSafe(Owner), *GetNameSafe(Ctx.Skill), *GetNameSafe(Ctx.AimActor), DistanceUU, *Dest.ToCompactString());
		return;
	}
	case ESkillSelfMove::TowardAim:
		break;
	case ESkillSelfMove::AwayFromAim:
		// ⚠ 카티야 E — 조준의 **반대**.
		Direction = -Ctx.AimDirection;
		break;
	case ESkillSelfMove::ToAimPoint:
		// 조준점은 ResolveAim 이 이미 사거리로 클램프했다 → 거리 = 발밑에서 조준점까지.
		DistanceUU = FVector::Dist2D(ERTargeting::GetTargetingLocation(Avatar), Ctx.AimPoint);
		break;
	default:
		return;
	}

	const bool bStarted = ERForcedMove::ApplySelfMove(Avatar, Direction, DistanceUU, Duration);
	UE_LOG(LogEternalReturn, Log, TEXT("[스킬] %s <- %s 자기 이동 %s %.0fcm / %.2f초 -> %s"),
		*GetNameSafe(Owner), *GetNameSafe(Ctx.Skill), *UEnum::GetValueAsString(Mode), DistanceUU, Duration, bStarted ? TEXT("시작") : TEXT("실패"));
}
