// Copyright Epic Games, Inc. All Rights Reserved.

#include "GAS/Fragment/ERSkillFragment_FollowUp.h"

#include "EternalReturn.h"
#include "GAS/ERGameplayAbility.h"
#include "GAS/ERSkillData.h"
#include "TimerManager.h"

void UERSkillFragment_FollowUp::OnTargetsResolved(FERSkillContext& Ctx, const TArray<AActor*>& Targets) const
{
	UERGameplayAbility* A = Ctx.Ability;
	if (!A || !Ctx.bAuthority || !Skill)
	{
		return;
	}
	TWeakObjectPtr<UERGameplayAbility> WeakAbility(A);
	const UERSkillData* Own = Ctx.Skill;
	const UERSkillData* Next = Skill;
	FTimerHandle Unused;
	A->GetWorld()->GetTimerManager().SetTimer(Unused, FTimerDelegate::CreateLambda([WeakAbility, Own, Next]()
	{
		if (UERGameplayAbility* Self = WeakAbility.Get())
		{
			UE_LOG(LogEternalReturn, Log, TEXT("[스킬] %s <- %s 2차 판정 (%s)"), *GetNameSafe(Self->GetOwningActorFromActorInfo()), *GetNameSafe(Own), *GetNameSafe(Next));
			Self->ExecuteOther(Next, 1.f, /*bWithExecuteHooks=*/false);
		}
	}), Delay, false);
}
