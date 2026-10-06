// Copyright Epic Games, Inc. All Rights Reserved.

#include "GAS/Fragment/ERSkillFragment_TeamSplit.h"

#include "Abilities/GameplayAbilityTypes.h"
#include "AbilitySystemComponent.h"
#include "Core/ERTeamStatics.h"
#include "EternalReturn.h"
#include "GAS/ERGameplayAbility.h"
#include "GAS/ERGameplayTags.h"
#include "GAS/ERSkillData.h"

void UERSkillFragment_TeamSplit::OnTargetsResolved(FERSkillContext& Ctx, const TArray<AActor*>& Targets) const
{
	UERGameplayAbility* A = Ctx.Ability;
	if (!Ctx.bAuthority || !A || !Ctx.Avatar)
	{
		return;
	}
	TArray<AActor*> Enemies, InnerEnemies, Allies;
	bool bSelf = false;
	for (AActor* T : Targets)
	{
		if (!T) { continue; }
		if (T == Ctx.Avatar) { bSelf = true; continue; }
		if (ERTeamStatics::IsHostile(Ctx.Avatar, T))
		{
			const bool bInner = InnerRadius > 0.f && EnemyInnerSkill && FVector::Dist2D(T->GetActorLocation(), Ctx.AimPoint) <= InnerRadius * 100.f;
			(bInner ? InnerEnemies : Enemies).Add(T);
		}
		else
		{
			Allies.Add(T);
		}
	}
	// 아군 실험체 적중 알림 (곰돌이 — 레니 패시브 · Argument 70 B1). 자신은 빼고
	for (AActor* Ally : Allies)
	{
		FGameplayEventData E;
		E.EventTag = ERTags::Event_Ally_SkillHit;
		E.Instigator = Ctx.Avatar;
		E.Target = Ally;
		Ctx.ASC->HandleGameplayEvent(ERTags::Event_Ally_SkillHit, &E);
	}
	if (bSelf || (SelfRule == EERTeamSplitSelf::IfAnyAlly && !Allies.IsEmpty()))
	{
		Allies.Add(Ctx.Avatar);
	}
	UE_LOG(LogEternalReturn, Log, TEXT("[스킬] %s 적 · 아군 나누기 — 적 %d (중앙 %d) · 아군 %d%s"),
		*GetNameSafe(Ctx.Skill), Enemies.Num() + InnerEnemies.Num(), InnerEnemies.Num(), Allies.Num(),
		Allies.Contains(Ctx.Avatar) ? TEXT(" (자신 포함)") : TEXT(""));
	if (EnemySkill && !Enemies.IsEmpty())        { A->ApplyOnTargets(EnemySkill, Enemies, 1.f, Ctx.Level, Ctx.ShapeOwner ? Ctx.ShapeOwner : Ctx.Skill); }
	if (EnemyInnerSkill && !InnerEnemies.IsEmpty()) { A->ApplyOnTargets(EnemyInnerSkill, InnerEnemies, 1.f, Ctx.Level, Ctx.ShapeOwner ? Ctx.ShapeOwner : Ctx.Skill); }
	if (AllySkill && !Allies.IsEmpty())          { A->ApplyOnTargets(AllySkill, Allies, 1.f, Ctx.Level, Ctx.ShapeOwner ? Ctx.ShapeOwner : Ctx.Skill); }
}
