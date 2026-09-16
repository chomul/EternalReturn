// Copyright Epic Games, Inc. All Rights Reserved.

#include "GAS/ERSkillData.h"

#include "AbilitySystemComponent.h"
#include "EternalReturn.h"
#include "GAS/ERGameplayAbility.h"
#include "GAS/ERGameplayTags.h"

bool FERSkillShape::IsAoE() const
{
	switch (Shape)
	{
	case ESkillTargeting::SingleTarget: return false;
	case ESkillTargeting::Projectile:   return bPenetrate;
	default:                            return true;
	}
}

float UERSkillData::LevelValue(const TArray<float>& Values, int32 Level)
{
	if (Values.IsEmpty())
	{
		return 0.f;
	}

	// 레벨은 1부터. 배열 밖이면 마지막 값 — 5레벨 스킬에 3칸만 채워도 죽지 않게.
	const int32 Index = FMath::Clamp(Level - 1, 0, Values.Num() - 1);
	return FMath::Max(Values[Index], 0.f);
}

float UERSkillData::GetCooldown(int32 Level) const
{
	return LevelValue(Cooldowns, Level);
}

float UERSkillData::GetCost(int32 Level) const
{
	return CostType == ESkillCostType::None ? 0.f : LevelValue(Costs, Level);
}

namespace ERSkill
{

void GrantSkills(UAbilitySystemComponent* ASC, const TArray<TObjectPtr<UERSkillData>>& Skills,
	FERGrantedSkillHandles& OutHandles)
{
	if (!ASC)
	{
		UE_LOG(LogEternalReturn, Error, TEXT("[스킬] ASC 가 없어 부여할 수 없다."));
		return;
	}

	// ⚠ 서버 권위. Lyra 도 같은 가드를 둔다 (LyraAbilitySet.cpp:36-40).
	if (!ASC->IsOwnerActorAuthoritative())
	{
		return;
	}

	for (int32 Index = 0; Index < Skills.Num(); ++Index)
	{
		const UERSkillData* Skill = Skills[Index];

		// ⚠ 하나가 잘못됐다고 나머지까지 못 받으면 안 된다. 로그 남기고 건너뛴다.
		if (!Skill)
		{
			UE_LOG(LogEternalReturn, Error,
				TEXT("[스킬] %s 의 Skills[%d] 가 비어 있다. 애셋을 지정하지 않았다."),
				*GetNameSafe(ASC->GetOwnerActor()), Index);
			continue;
		}

		if (!Skill->AbilityClass)
		{
			UE_LOG(LogEternalReturn, Error,
				TEXT("[스킬] %s 의 Ability Class 가 비어 있다."), *GetNameSafe(Skill));
			continue;
		}

		if (!Skill->SlotTag.IsValid())
		{
			UE_LOG(LogEternalReturn, Error,
				TEXT("[스킬] %s 의 Slot Tag 가 비어 있다. 입력이 이 스킬을 찾지 못한다."), *GetNameSafe(Skill));
			continue;
		}

		// ⚠ 데이터 실수 경고 — 배열이 짧으면 GetCooldown/GetCost 가 마지막 값으로 채워 동작은 하지만,
		//   "5레벨인데 4레벨 값" 이 조용히 나가는 것은 드러내야 한다.
		if (!Skill->Cooldowns.IsEmpty() && Skill->Cooldowns.Num() < Skill->MaxLevel)
		{
			UE_LOG(LogEternalReturn, Warning, TEXT("[스킬] %s Cooldowns 가 %d칸인데 MaxLevel 은 %d 다."),
				*GetNameSafe(Skill), Skill->Cooldowns.Num(), Skill->MaxLevel);
		}
		if (Skill->CostType != ESkillCostType::None && Skill->Costs.Num() < Skill->MaxLevel)
		{
			UE_LOG(LogEternalReturn, Warning, TEXT("[스킬] %s Costs 가 %d칸인데 MaxLevel 은 %d 다."),
				*GetNameSafe(Skill), Skill->Costs.Num(), Skill->MaxLevel);
		}
		// ⚠ 하한 > 상한이면 FMath::Clamp 가 전부 하한으로 보내고 판정은 아무것도 못 맞힌다 — 조용히 죽는다.
		//   2026-09-14 Projectile 테스트에서 RangeMin 에 5 를 넣어 6번 전부 적중 0 이 났다.
		if (Skill->Shape.RangeMin > Skill->Shape.RangeMax)
		{
			UE_LOG(LogEternalReturn, Error, TEXT("[스킬] %s Shape.RangeMin(%.2f) > RangeMax(%.2f). 사거리 하한이 상한보다 크면 아무것도 못 맞힌다."),
				*GetNameSafe(Skill), Skill->Shape.RangeMin, Skill->Shape.RangeMax);
		}

		// ⭐ 레벨은 애셋이 정한다. 0 = 미습득(CanActivateAbility 가 막는다). 포인트로 올린다.
		FGameplayAbilitySpec Spec(Skill->AbilityClass, FMath::Min(Skill->InitialLevel, Skill->MaxLevel));

		// ⭐ 어빌리티가 자기 데이터를 꺼내는 자리. GAS 에 이미 있다 (FGameplayAbilitySpec::SourceObject).
		//   const 를 벗기는 이유: SourceObject 가 TWeakObjectPtr<UObject> 라 non-const 를 받는다.
		//   어빌리티 쪽은 다시 const 로 읽는다 (UERGameplayAbility::GetSkillData).
		Spec.SourceObject = const_cast<UERSkillData*>(Skill);

		// ⭐ 입력이 이 태그로 찾는다 (ERPlayerController::OnSkillSlotPressed — 스펙 순회, E10).
		Spec.DynamicAbilityTags.AddTag(Skill->SlotTag);

		const FGameplayAbilitySpecHandle Handle = ASC->GiveAbility(Spec);
		OutHandles.AbilityHandles.Add(Handle);

		UE_LOG(LogEternalReturn, Log, TEXT("[스킬] %s <- %s (%s) Lv.%d/%d"),
			*GetNameSafe(ASC->GetOwnerActor()), *GetNameSafe(Skill), *Skill->SlotTag.ToString(),
			Spec.Level, Skill->MaxLevel);
	}
}

void TakeSkills(UAbilitySystemComponent* ASC, FERGrantedSkillHandles& Handles)
{
	if (!ASC || !ASC->IsOwnerActorAuthoritative())
	{
		return;
	}

	for (const FGameplayAbilitySpecHandle& Handle : Handles.AbilityHandles)
	{
		if (Handle.IsValid())
		{
			ASC->ClearAbility(Handle);
		}
	}

	Handles.AbilityHandles.Reset();
}

FGameplayTag CooldownTagForSlot(const FGameplayTag& SlotTag)
{
	if (SlotTag == ERTags::Ability_Slot_P) { return ERTags::Cooldown_Slot_P; }
	if (SlotTag == ERTags::Ability_Slot_Q) { return ERTags::Cooldown_Slot_Q; }
	if (SlotTag == ERTags::Ability_Slot_W) { return ERTags::Cooldown_Slot_W; }
	if (SlotTag == ERTags::Ability_Slot_E) { return ERTags::Cooldown_Slot_E; }
	if (SlotTag == ERTags::Ability_Slot_R) { return ERTags::Cooldown_Slot_R; }
	if (SlotTag == ERTags::Ability_Slot_D) { return ERTags::Cooldown_Slot_D; }
	if (SlotTag == ERTags::Ability_Slot_Attack) { return ERTags::Cooldown_Slot_Attack; }

	// 슬롯 태그를 새로 만들었는데 여기에 안 붙인 것이다. 빈 태그를 돌려주면
	// 쿨다운이 걸리지 않으므로(CheckCooldown 통과) 반드시 로그로 드러낸다.
	UE_LOG(LogEternalReturn, Error, TEXT("[스킬] %s 에 대응하는 쿨다운 태그가 없다."), *SlotTag.ToString());
	return FGameplayTag();
}

FGameplayTag RecastTagForSlot(const FGameplayTag& SlotTag)
{
	if (SlotTag == ERTags::Ability_Slot_P) { return ERTags::Recast_Slot_P; }
	if (SlotTag == ERTags::Ability_Slot_Q) { return ERTags::Recast_Slot_Q; }
	if (SlotTag == ERTags::Ability_Slot_W) { return ERTags::Recast_Slot_W; }
	if (SlotTag == ERTags::Ability_Slot_E) { return ERTags::Recast_Slot_E; }
	if (SlotTag == ERTags::Ability_Slot_R) { return ERTags::Recast_Slot_R; }
	if (SlotTag == ERTags::Ability_Slot_D) { return ERTags::Recast_Slot_D; }
	return FGameplayTag();   // 평타 등 — 리캐스트 없음. 로그 안 남김 (정상)
}

bool LevelUpSkill(UAbilitySystemComponent* ASC, const FGameplayTag& SlotTag)
{
	if (!ASC || !ASC->IsOwnerActorAuthoritative())
	{
		return false;
	}

	// 슬롯 태그로 스펙을 찾는다 — 입력(OnSkillSlotPressed)과 같은 방식 (E10).
	// ⚠ 포인터는 다음 ASC 호출 전까지만 유효하다 (AbilitySystemComponent.h:1142).
	FGameplayAbilitySpec* Found = nullptr;
	for (FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (Spec.Ability && Spec.DynamicAbilityTags.HasTagExact(SlotTag))
		{
			Found = &Spec;
			break;
		}
	}

	if (!Found)
	{
		UE_LOG(LogEternalReturn, Warning, TEXT("[스킬] %s 에 %s 슬롯이 없다."),
			*GetNameSafe(ASC->GetOwnerActor()), *SlotTag.ToString());
		return false;
	}

	const UERSkillData* Skill = Cast<UERSkillData>(Found->SourceObject.Get());
	if (!Skill)
	{
		UE_LOG(LogEternalReturn, Error, TEXT("[스킬] %s 슬롯 스펙에 SkillData 가 없다. GrantSkills 로 부여해야 한다."),
			*SlotTag.ToString());
		return false;
	}

	if (!Skill->bUsesSkillPoints)
	{
		UE_LOG(LogEternalReturn, Warning, TEXT("[스킬] %s 는 포인트로 올리는 스킬이 아니다."), *GetNameSafe(Skill));
		return false;
	}

	if (Found->Level >= Skill->MaxLevel)
	{
		UE_LOG(LogEternalReturn, Warning, TEXT("[스킬] %s 는 이미 최대 레벨(%d)이다."), *GetNameSafe(Skill), Skill->MaxLevel);
		return false;
	}

	Found->Level += 1;

	// ⭐ 스펙은 FastArray 라 바꾼 뒤 더티 표시를 해야 클라로 간다.
	ASC->MarkAbilitySpecDirty(*Found);

	UE_LOG(LogEternalReturn, Log, TEXT("[스킬] %s %s -> Lv.%d/%d"),
		*GetNameSafe(ASC->GetOwnerActor()), *GetNameSafe(Skill), Found->Level, Skill->MaxLevel);
	return true;
}

} // namespace ERSkill
