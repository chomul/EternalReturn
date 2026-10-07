// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/ERProjectile_Explode.h"

#include "Combat/ERTargeting.h"
#include "EternalReturn.h"
#include "GAS/ERGameplayAbility.h"
#include "GAS/ERSkillData.h"
#include "GAS/Fragment/ERSkillFragment_Explode.h"
#include "GAS/ERGameplayTags.h"
#include "Presentation/ERPresentationComponent.h"

void AERProjectile_Explode::EndFlight(const TCHAR* Why)
{
	if (!bEnded && HasAuthority())
	{
		UERGameplayAbility* A = Ability.Get();
		AActor* Caster = A ? A->GetAvatarActorFromActorInfo() : nullptr;
		const UERSkillFragment_Explode* Ex = Skill ? Skill->FindFragment<UERSkillFragment_Explode>() : nullptr;
		if (A && Caster && Ex && Ex->ExplodeSkill)
		{
			const FVector Here = GetActorLocation();
			TArray<AActor*> Targets = ERTargeting::QueryCircleAt(GetWorld(), Filter, Here, Ex->Radius, Caster);
			if (Ex->bIncludeCaster && FVector::Dist2D(Caster->GetActorLocation(), Here) <= Ex->Radius * 100.f)
			{
				Targets.Add(Caster);
			}
			UE_LOG(LogEternalReturn, Log, TEXT("[투사체] %s 폭발 (%s) — 반경 %.1fm 안 %d명"), *GetNameSafe(this), Why, Ex->Radius, Targets.Num());
			if (!Targets.IsEmpty())
			{
				A->ApplyOnTargets(Ex->ExplodeSkill, Targets, 1.f, Level, Ex->ExplodeSkill);
				A->SendPresCues(*Ex->ExplodeSkill, Caster, Targets, /*bWithAttack=*/false);   // 타격음 — 나누기 조각이 적만 남긴다
			}
			UERPresentationComponent::SendSfxCue(Caster, ERTags::Pres_Sfx_SkillLand_Q, Here);   // 폭발 (맞힌 사람 없어도 · 레니 Skill01_Boom)
		}
		else
		{
			UE_LOG(LogEternalReturn, Error, TEXT("[투사체] %s 폭발 못 함 — '끝에서 폭발' 조각 · 폭발 데이터가 없다 (%s)"), *GetNameSafe(this), *GetNameSafe(Skill));
		}
	}
	Super::EndFlight(Why);
}
