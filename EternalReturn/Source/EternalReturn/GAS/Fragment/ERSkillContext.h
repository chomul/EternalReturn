// F11.5 스킬 조각화 — 조각이 보는 "이번 실행" 의 전부 (Docs/1_Task/F11.5_스킬_조각화/00_개요.md §1)

#pragma once

#include "CoreMinimal.h"

class UERGameplayAbility;
class UAbilitySystemComponent;
class AERPlayerState;
class UERSkillData;
class UERSkillFragment;
class UERSkillFragmentState;

/**
 * 조각 훅에 넘기는 실행 문맥. 어빌리티가 실행마다 만든다 (MakeContext) — 조각은 어빌리티 내부(핸들 · ActorInfo)를 직접 만지지 않는다.
 * ⚠ 스택 구조체다. 훅 밖에서 들고 있지 않는다 — 타이머/콜백에 상태가 필요하면 GetState<T>() 로 어빌리티 인스턴스에 붙인 상태 객체를 쓴다.
 */
struct FERSkillContext
{
	// ── 어빌리티가 채우는 것 ──
	UERGameplayAbility* Ability = nullptr;
	UAbilitySystemComponent* ASC = nullptr;
	AActor* Avatar = nullptr;
	AERPlayerState* PlayerState = nullptr;
	/** 이번 실행의 데이터 — 자기 것 또는 리캐스트 · 2차 판정 · 벽 충돌로 넘겨받은 것. */
	const UERSkillData* Skill = nullptr;
	int32 Level = 1;
	FVector AimPoint = FVector::ZeroVector;
	FVector AimDirection = FVector::ForwardVector;
	AActor* AimActor = nullptr;
	bool bActivatedByRecast = false;
	bool bAuthority = false;
	/** 장판 감쇠 등 — 피해 조각이 곱한다. */
	float DamageScale = 1.f;
	/** 광역 태그의 형상 주인. 보통 Skill. 평타 강화는 평타의 형상 (ApplyOnTargets 가 넘긴다). null = Skill. */
	const UERSkillData* ShapeOwner = nullptr;
	/** 평타 강화로 얹힌 실행인가 — 로그 라벨. */
	bool bEnhancement = false;

	// ── 조각이 세팅하는 것 (같은 실행 안의 뒤 조각 · 어빌리티가 읽는다) ──
	/** 피해 조각이 하나라도 맞혔나 — 리캐스트(적중 시) · 다음 평타 강화가 본다. */
	bool bHitAnything = false;
	/** 판정을 건너뛴다 — 장판이 대신 판정한다. OnExecute 에서 세운다. */
	bool bSkipTargeting = false;
	/** 어빌리티를 끝내지 말고 활성으로 둔다 — 모드. 세운 조각이 나중에 Ability->EndFromFragment 로 끝낸다. */
	bool bKeepActive = false;
	/** 판정 결과 (OnTargetsResolved 에서). */
	TArray<AActor*> Targets;

	/** 다른 데이터로 파이프라인 재진입 — 2차 판정 · 리캐스트 · 벽 충돌. bWithExecuteHooks 면 OnExecute 조각도 돈다 (리캐스트), 아니면 판정+적중만 (2차 판정). */
	void ExecuteOther(const UERSkillData* Other, float Scale = 1.f, bool bWithExecuteHooks = false) const;

	/** 조각별 런타임 상태 (Outer = 어빌리티 인스턴스). 없으면 만든다. 조각 객체는 애셋이라 상태를 못 든다. */
	template <class T>
	T* GetState(const UERSkillFragment* Owner) const
	{
		return Cast<T>(GetStateInternal(Owner, T::StaticClass()));
	}

private:
	UERSkillFragmentState* GetStateInternal(const UERSkillFragment* Owner, TSubclassOf<UERSkillFragmentState> Class) const;
};
