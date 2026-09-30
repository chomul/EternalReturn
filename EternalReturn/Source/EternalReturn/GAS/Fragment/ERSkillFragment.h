// F11.5 스킬 조각화 — 기능 하나 = 조각 클래스 하나 (Docs/4_Argument/28 B · Lyra LyraInventoryItemDefinition.h:15-45 의 Fragment 패턴)

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "ERSkillContext.h"
#include "ERSkillFragment.generated.h"

class UERSkillData;

/**
 * 스킬 기능 조각. UERSkillData.Fragments 에 Instanced 로 들어간다 — 애셋에는 그 스킬이 쓰는 조각만 보인다.
 *
 * ⭐ 조각은 **데이터 + 무상태 로직**이다. 같은 애셋을 모든 플레이어가 공유하므로 멤버에 런타임 값을 쓰지 않는다 —
 *   타이머 · 카운터가 필요하면 Ctx.GetState<T>() (어빌리티 인스턴스에 붙는 UERSkillFragmentState 파생).
 * ⭐ 훅은 **서버**에서 돈다 (OnLocalExecute 만 소유 클라 · 리슨 호스트). 순서는 배열 순서.
 * ⚠ BP 조각을 만들지 않는다 (CLAUDE.md §7) — EditInlineNew 는 데이터 입력용.
 */
UCLASS(DefaultToInstanced, EditInlineNew, Abstract, CollapseCategories)
class ETERNALRETURN_API UERSkillFragment : public UObject
{
	GENERATED_BODY()

public:
	/** 커밋 **전** — false 면 발동을 없던 일로 (쿨다운 · 리캐스트 창 소비 없음). 단검 블링크의 "대상 없음 · 사거리 밖". */
	virtual bool CanExecute(const FERSkillContext& Ctx, FString& OutReason) const { return true; }

	/** [서버] **선딜 시작** (CastTime > 0 인 스킬만 · 커밋 전) — 충전하는 동안의 자기 효과 (위클라인 「격리」 피해 감소 · 회복 · F12.6-06). */
	virtual void OnCastStart(FERSkillContext& Ctx) const {}

	/** [서버] 커밋 뒤 · 판정 **전** — 자기 이동 · 자기 버프 · 장판 스폰(bSkipTargeting) · 모드 진입(bKeepActive). */
	virtual void OnExecute(FERSkillContext& Ctx) const {}

	/** [서버] 판정 **뒤** — 피해 · 적중 효과 · 넉백 · 2차 판정 예약 · 리캐스트 창. Targets 는 판정 결과 (빈 배열 = 빗나감). */
	virtual void OnTargetsResolved(FERSkillContext& Ctx, const TArray<AActor*>& Targets) const {}

	/** [소유 클라 · 리슨 호스트] 연출성 로컬 작업 — 모드 카메라. 게임 로직 금지. */
	virtual void OnLocalExecute(FERSkillContext& Ctx) const {}

	/** [서버] EndAbility — 어떤 경로든 (정상 · 취소 · 외부). 모드 정리 · 쿨 반환. */
	virtual void OnEnd(FERSkillContext& Ctx, bool bCancelled) const {}

#if WITH_EDITOR
	/** 데이터 실수 경고 — GrantSkills 가 애셋마다 부른다 (순서 · 빈 참조). */
	virtual void Validate(const UERSkillData& Owner, int32 Index) const {}
#endif

	/** 로그용 짧은 이름 ("피해" · "넉백"). */
	virtual FString GetDebugName() const { return GetClass()->GetName(); }

	/** 이 조각이 발동 뒤 어빌리티를 활성으로 남기나 (모드). 클라 인스턴스가 "모드 중" 을 알기 위해 — 서버는 Ctx.bKeepActive. */
	virtual bool KeepsAbilityActive() const { return false; }

	/** 리캐스트로 발동했을 때 대신 실행할 데이터 (단검 망토 → 단검). 없으면 nullptr = 같은 데이터. */
	virtual const UERSkillData* GetRecastExecData() const { return nullptr; }

	/**
	 * "대상에게 주는 효과" 인가 (피해 · 적중 효과). ApplyOnTargets(장판 펄스 · 벽 충돌 · 평타 강화)는 **이것만** 돌린다 —
	 * 리캐스트 창 · 다음 평타 강화 · 2차 판정 같은 "시전 결과" 조각이 강화 평타마다 다시 돌면 무한 강화가 된다.
	 */
	virtual bool IsHitEffect() const { return false; }
};

/**
 * 조각별 런타임 상태. Outer = 어빌리티 인스턴스(InstancedPerActor) — 어빌리티와 수명이 같다.
 * UFUNCTION 콜백(AbilityTask 델리게이트)이 여기 붙는다.
 */
UCLASS()
class ETERNALRETURN_API UERSkillFragmentState : public UObject
{
	GENERATED_BODY()
};
