// Copyright Epic Games, Inc. All Rights Reserved.

#include "GAS/ERSkillData.h"
#include "GAS/Fragment/ERSkillFragment.h"

#include "AbilitySystemComponent.h"
#include "EternalReturn.h"
#include "GAS/ERGameplayAbility.h"
#include "GAS/ERGameplayTags.h"
#include "GAS/Delivery/ERSkillDelivery.h"
#include "GAS/Shape/ERSkillShapes.h"
#include "Serialization/CustomVersion.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

bool FERSkillShape::IsAoE() const
{
	switch (Shape)
	{
	case ESkillTargeting::SingleTarget: return false;
	case ESkillTargeting::Projectile:   return bPenetrate;
	default:                            return true;
	}
}

namespace ERSkillDataVersion
{
	enum Type
	{
		Initial = 0,
		/** 옛 FERSkillShape 하나 → Area(모양) · Targets · Delivery(발사) (Argument 57 S3.1) */
		ShapeDeliveryRefactor = 1,

		VersionPlusOne,
		Latest = VersionPlusOne - 1
	};
	const FGuid GUID(0x6A1E3C52, 0x8B7F4D10, 0x9E2A5C34, 0x17D0B8F1);
}
static FCustomVersionRegistration GRegisterERSkillDataVersion(ERSkillDataVersion::GUID, ERSkillDataVersion::Latest, TEXT("ERSkillDataVersion"));

void UERSkillData::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	Ar.UsingCustomVersion(ERSkillDataVersion::GUID);
}

void UERSkillData::PostLoad()
{
	Super::PostLoad();
	// ⚠ 버전이 낮고 새 칸이 비었을 때만 — 이미 옮긴(저장된) 애셋을 옛 값으로 덮지 않는다
	if (GetLinkerCustomVersion(ERSkillDataVersion::GUID) < ERSkillDataVersion::ShapeDeliveryRefactor && !Area && !Delivery)
	{
		const FString Line = MigrateLegacyShape();
		bShapeMigratedOnLoad = true;
		UE_LOG(LogEternalReturn, Log, TEXT("%s"), *Line);
	}
}

FString UERSkillData::MigrateLegacyShape()
{
	const FERSkillShape& S = Shape;
	const EObjectFlags Flags = GetMaskedFlags(RF_PropagateToSubObjects) | RF_Transactional;
	auto MakeArea = [this, Flags](TSubclassOf<UERSkillShapeBase> Cls) { return NewObject<UERSkillShapeBase>(this, Cls, NAME_None, Flags); };

	Targets.Team = S.TeamFilter;
	Targets.bPlayersOnly = S.bPlayersOnly;
	Targets.MaxTargets = S.MaxTargets;
	Targets.AimAssistRadius = S.AimAssistRadius;

	// 발사 — 옛 암묵 규칙 "속도 > 0 = 날아간다" 를 칸으로 (S3.1 ⑨)
	auto MakeInstant = [this, Flags](int32 Fan, float FanAngle)
	{
		UERDelivery_Instant* D = NewObject<UERDelivery_Instant>(this, NAME_None, Flags);
		D->FanCount = FMath::Max(1, Fan);
		D->FanAngleDeg = FanAngle;
		return D;
	};
	auto MakeProjectile = [this, Flags, &S](EERFirePattern Pattern)
	{
		UERDelivery_Projectile* D = NewObject<UERDelivery_Projectile>(this, NAME_None, Flags);
		D->FirePattern = Pattern;
		D->Speed = S.ProjectileSpeed;
		D->Radius = S.ProjectileRadius;
		D->bPierce = S.bPenetrate;
		D->ProjectileClass = S.ProjectileClass;
		D->Count = FMath::Max(1, S.ProjectileCount);
		D->SpreadAngleDeg = S.SpreadAngleDeg;
		return D;
	};

	switch (S.Shape)
	{
	case ESkillTargeting::SingleTarget:
	{
		UERShape_Single* A = CastChecked<UERShape_Single>(MakeArea(UERShape_Single::StaticClass()));
		A->Range = S.RangeMax;
		Area = A;
		Delivery = MakeInstant(1, 0.f);
		break;
	}
	case ESkillTargeting::SelfRadius:
	{
		UERShape_Circle* A = CastChecked<UERShape_Circle>(MakeArea(UERShape_Circle::StaticClass()));
		A->Radius = S.RangeMax;
		A->ForwardOffset = S.ForwardOffset;
		Area = A;
		Delivery = MakeInstant(1, 0.f);
		break;
	}
	case ESkillTargeting::GroundCircle:
	{
		UERShape_Circle* A = CastChecked<UERShape_Circle>(MakeArea(UERShape_Circle::StaticClass()));
		A->ShapeOrigin = EERShapeOrigin::AimPoint;
		A->AimRange = S.RangeMax;
		A->Radius = S.RadiusOuter > 0.f ? S.RadiusOuter : S.RangeMax;   // 반경 0 이면 옛 동작 = RangeMax (E35)
		Area = A;
		Delivery = MakeInstant(1, 0.f);
		break;
	}
	case ESkillTargeting::DualRadius:
	{
		// ⚠ 판정은 RadiusOuter 가 바깥 (ERTargeting::QueryDualRadius) — 옛 그림 · AI 사거리는 RangeMax 였다. 로그에 둘 다
		UERShape_DualCircle* A = CastChecked<UERShape_DualCircle>(MakeArea(UERShape_DualCircle::StaticClass()));
		A->InnerRadius = S.RadiusInner;
		A->OuterRadius = S.RadiusOuter;
		A->ForwardOffset = S.ForwardOffset;
		Area = A;
		Delivery = MakeInstant(1, 0.f);
		break;
	}
	case ESkillTargeting::Cone:
	{
		UERShape_Cone* A = CastChecked<UERShape_Cone>(MakeArea(UERShape_Cone::StaticClass()));
		A->Length = S.RangeMax;
		A->AngleDeg = S.AngleDeg;
		Area = A;
		Delivery = MakeInstant(1, 0.f);
		break;
	}
	case ESkillTargeting::Projectile:
	{
		UERShape_Line* A = CastChecked<UERShape_Line>(MakeArea(UERShape_Line::StaticClass()));
		A->Length = S.RangeMax;
		A->Width = S.ProjectileRadius * 2.f;
		Area = A;
		if (S.ProjectileSpeed > 0.f)
		{
			Delivery = MakeProjectile(S.ProjectileCount > 1 ? EERFirePattern::Simultaneous : EERFirePattern::Single);
		}
		else
		{
			Delivery = MakeInstant(S.ProjectileCount, S.SpreadAngleDeg);
			if (!S.bPenetrate) { Targets.MaxTargets = 1; }   // 비관통 = 줄마다 가장 가까운 하나
		}
		break;
	}
	case ESkillTargeting::Trapezoid:
	{
		UERShape_Trapezoid* A = CastChecked<UERShape_Trapezoid>(MakeArea(UERShape_Trapezoid::StaticClass()));
		A->AimRange = S.RangeMax;
		A->Length = S.TrapezoidLength > 0.f ? S.TrapezoidLength : S.RangeMax;
		A->NearWidth = S.TrapezoidNearWidth;
		A->FarWidth = S.TrapezoidFarWidth;
		Area = A;
		if (S.ProjectileSpeed > 0.f)
		{
			UERDelivery_Projectile* D = MakeProjectile(EERFirePattern::Sequential);
			D->bHoming = true;
			D->bPierce = false;
			D->Interval = S.ShotInterval;
			D->CancelDistance = S.ShotCancelDistance;
			Delivery = D;
		}
		else
		{
			Delivery = MakeInstant(1, 0.f);
		}
		break;
	}
	case ESkillTargeting::PlayerCircles:
	{
		UERShape_PlayerCircles* A = CastChecked<UERShape_PlayerCircles>(MakeArea(UERShape_PlayerCircles::StaticClass()));
		A->CaptureRange = S.RangeMax;
		A->Radius = S.RadiusOuter;
		Area = A;
		Delivery = MakeInstant(1, 0.f);
		break;
	}
	default:
		break;
	}
	if (Area)
	{
		Area->MinReach = S.RangeMin;
	}

	return FString::Printf(TEXT("[스킬 이관] %s 옛 Shape=%s RangeMax=%.2f RangeMin=%.2f RadiusOuter=%.2f RadiusInner=%.2f 속도=%.1f 관통=%d 발=%d → Area=%s · Delivery=%s · Targets(팀 %s · 실험체만 %d · 최대 %d · 보조 %.2f)"),
		*GetName(), *UEnum::GetValueAsString(S.Shape), S.RangeMax, S.RangeMin, S.RadiusOuter, S.RadiusInner, S.ProjectileSpeed, S.bPenetrate ? 1 : 0, S.ProjectileCount,
		Area ? *Area->Describe() : TEXT("없음"), Delivery ? *Delivery->Describe() : TEXT("없음"),
		*UEnum::GetValueAsString(Targets.Team), Targets.bPlayersOnly ? 1 : 0, Targets.MaxTargets, Targets.AimAssistRadius);
}

float UERSkillData::GetMaxReach() const
{
	return Area ? Area->GetMaxReach() : 0.f;
}

float UERSkillData::GetMinReach() const
{
	return Area ? Area->MinReach : 0.f;
}

bool UERSkillData::IsAoE() const
{
	if ((Area && Area->IsSingleTarget()) || Targets.MaxTargets == 1 || (Delivery && Delivery->IsSingleHit()))
	{
		return false;
	}
	return true;
}

#if WITH_EDITOR
EDataValidationResult UERSkillData::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	const FString Owner = GetName();
	const uint32 ErrorsBefore = Context.GetNumErrors();
	// 모양 · 발사가 없으면 판정이 안 돈다 — 조각이 판정을 대신하는 스킬(장판 · 모드)도 빈 원이라도 둔다
	if (!Area)
	{
		Context.AddError(FText::FromString(FString::Printf(TEXT("%s: 판정 모양(Area) 없음"), *Owner)));
	}
	else
	{
		Area->ValidateShape(Context, Owner);
	}
	if (!Delivery)
	{
		Context.AddError(FText::FromString(FString::Printf(TEXT("%s: 발사 방식(Delivery) 없음"), *Owner)));
	}
	else
	{
		Delivery->ValidateDelivery(Context, Owner, Targets.MaxTargets);
	}
	if (Context.GetNumErrors() > ErrorsBefore)
	{
		Result = EDataValidationResult::Invalid;
	}
	return Result == EDataValidationResult::NotValidated ? EDataValidationResult::Valid : Result;
}
#endif

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

float UERSkillData::LevelValueSigned(const TArray<float>& Values, int32 Level)
{
	if (Values.IsEmpty())
	{
		return 0.f;
	}
	return Values[FMath::Clamp(Level - 1, 0, Values.Num() - 1)];
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
	FERGrantedSkillHandles& OutHandles, int32 LevelOverride)
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

#if WITH_EDITOR
		// F11.5 — 조각별 데이터 검사 (빈 참조 · 순서). 에디터 PIE 에서만.
		for (int32 FI = 0; FI < Skill->Fragments.Num(); ++FI)
		{
			if (!Skill->Fragments[FI])
			{
				UE_LOG(LogEternalReturn, Warning, TEXT("[스킬] %s Fragments[%d] 가 비어 있다."), *GetNameSafe(Skill), FI);
				continue;
			}
			Skill->Fragments[FI]->Validate(*Skill, FI);
		}
#endif

		// ⭐ 레벨은 애셋이 정한다. 0 = 미습득(CanActivateAbility 가 막는다). 포인트로 올린다.
		//   ⚠ 레벨은 **스펙을 만들 때** 정한다 — GiveAbility 뒤에 FindAbilitySpecFromHandle 로 고치면 어빌리티 활성 중(스코프 락)에는
		//     스펙이 AbilityPendingAdds 에 있어 못 찾고 조용히 0 으로 남는다 (E21 · AbilitySystemComponent_Abilities.cpp:287).
		FGameplayAbilitySpec Spec(Skill->AbilityClass, FMath::Min(LevelOverride >= 0 ? LevelOverride : Skill->InitialLevel, Skill->MaxLevel));

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

bool LevelUpSkill(UAbilitySystemComponent* ASC, const FGameplayTag& SlotTag, int32 CharacterLevel)
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

	// ⭐ 실험체 레벨 조건 (F10-03) — 다음 스킬 레벨 = Found->Level + 1, 그 요구치는 [Found->Level].
	if (Skill->MinCharacterLevel.IsValidIndex(Found->Level) && CharacterLevel < Skill->MinCharacterLevel[Found->Level])
	{
		UE_LOG(LogEternalReturn, Warning, TEXT("[스킬] %s Lv.%d 은 실험체 레벨 %d 필요 (지금 %d)."),
			*GetNameSafe(Skill), Found->Level + 1, Skill->MinCharacterLevel[Found->Level], CharacterLevel);
		return false;
	}

	Found->Level += 1;

	// ⭐ 스펙은 FastArray 라 바꾼 뒤 더티 표시를 해야 클라로 간다.
	ASC->MarkAbilitySpecDirty(*Found);

	UE_LOG(LogEternalReturn, Log, TEXT("[스킬] %s %s -> Lv.%d/%d"),
		*GetNameSafe(ASC->GetOwnerActor()), *GetNameSafe(Skill), Found->Level, Skill->MaxLevel);
	return true;
}

bool SetSkillLevel(UAbilitySystemComponent* ASC, const FGameplayTag& SlotTag, int32 NewLevel, const TCHAR* Reason)
{
	if (!ASC || !ASC->IsOwnerActorAuthoritative())
	{
		return false;
	}
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
		return false;   // 슬롯이 없다 (무기 행에 D 가 없다) — 정상. 로그 없음
	}
	const UERSkillData* Skill = Cast<UERSkillData>(Found->SourceObject.Get());
	if (!Skill || Skill->bUsesSkillPoints)
	{
		UE_LOG(LogEternalReturn, Warning, TEXT("[스킬] %s 는 포인트 스킬이다 — SetSkillLevel 대상이 아니다."), *GetNameSafe(Skill));
		return false;
	}
	const int32 Clamped = FMath::Clamp(NewLevel, 0, Skill->MaxLevel);
	if (Found->Level == Clamped)
	{
		return false;
	}
	Found->Level = Clamped;
	ASC->MarkAbilitySpecDirty(*Found);
	UE_LOG(LogEternalReturn, Log, TEXT("[스킬] %s %s -> Lv.%d/%d (%s)"),
		*GetNameSafe(ASC->GetOwnerActor()), *GetNameSafe(Skill), Found->Level, Skill->MaxLevel, Reason);
	return true;
}

} // namespace ERSkill
