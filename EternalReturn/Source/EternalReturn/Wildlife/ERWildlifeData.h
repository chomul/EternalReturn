// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GAS/ERAttributeTypes.h"
#include "Wildlife/ERWildlifeTypes.h"
#include "ERWildlifeData.generated.h"

class UERSkillData;
class USkeletalMesh;
class UAnimInstance;
class UERPresentationData;

/**
 * 야생동물 한 종의 정의 (F12-01 · Argument 30 B). `UERCharacterData` 와 같은 모양 — 스탯 · 레벨당 · 스킬 · 연출 애셋.
 * ⭐ 변이체도 **별도 애셋** (`DA_Wild_MutantBear`) — 배율 없음, 값이 바뀌면 애셋만 (사용자 2026-09-21).
 * 이름 규약: `Content/Wildlife/DA_Wild_<이름>` — 스포너(03) · 디버그가 이름으로 찾는다 (UERWildlifeSettings.DataPath).
 * [확인] 공격력/레벨당: 닭 18/19 · 박쥐 30/20 · 멧돼지 27/21 · 곰 90/16 · 변이 곰 155/19 (12.0). 나머지 `[자체]`.
 */
UCLASS(BlueprintType)
class ETERNALRETURN_API UERWildlifeData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, Category = "분류")
	EERWildlifeType Type = EERWildlifeType::None;

	/** 변이 단계 — 일반 · 변이체 · 잠식 변이체. 표시 · 드랍 분기용. 스탯 · 크레딧은 이 애셋 값 그대로 (배율 없음). */
	UPROPERTY(EditDefaultsOnly, Category = "분류")
	EERWildlifeVariant Variant = EERWildlifeVariant::Normal;

	/** 보스 — Actor.Type.Boss (비례 피해 감쇠 · 도먼시 Never). */
	UPROPERTY(EditDefaultsOnly, Category = "분류")
	bool bBoss = false;

	/**
	 * 체력 비례 피해 저항 (0~1) — 스폰 때 어트리뷰트 ProportionalDamageResist 로 들어간다. 0 = 감쇠 없음.
	 * [확인] 알파 0.3 (70% 만 받음) · 오메가 0.4 (60%) · 위클라인 0.5 (50%) — 나무위키 (사용자 제공 2026-09-24 · Argument 38 B).
	 */
	UPROPERTY(EditDefaultsOnly, Category = "분류", meta = (ClampMin = "0", ClampMax = "1"))
	float ProportionalDamageResist = 0.f;

	/** 귀환하면 체력을 회복하나. 일반 야생동물 · 알파 · 오메가 true. **위클라인은 회복하지 않는다** [확인] (역기획서 §5.3 · 사용자 제공 2026-09-24). */
	UPROPERTY(EditDefaultsOnly, Category = "AI")
	bool bHealOnReturn = true;

	/**
	 * 스폰 직후 이 반경(m) 안에 실험체가 있으면 먼저 공격한다. 0 = 없음 (선공 없음 — 기본 · **전 종 0**).
	 * ⚠ 위클라인도 0 이다 (사용자 확인 2026-09-24): "실험 대상 추적"은 20m 안 실험체에게 **다가가기만** 하고, 전투는 독을 밟거나 먼저 때려야 시작된다.
	 *   다가가기(추적)는 위클라인 배회와 함께 (F13). 이 값은 선제 공격하는 종(잠식 변이체 후보)을 위해 남겨 둔다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "AI", meta = (ClampMin = "0"))
	float SpawnAggroRadius = 0.f;

	/**
	 * **생성 때 한 번** 이 반경(m) 안 가장 가까운 실험체를 **추적** — 다가가기만, 공격 안 함 (F12.6-06 위클라인 "실험 대상 추적" 20m).
	 * 전투가 한 번이라도 시작되거나 대상이 반경 밖으로 나가면 풀린다 · 아무도 없거나 풀리면 순찰 (⏸ F13). 0 = 없음. CSV `SpawnTrackRadius`.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "AI", meta = (ClampMin = "0"))
	float SpawnTrackRadius = 0.f;

	/**
	 * 맵을 돌아다니는 종 (F12.6-06 위클라인) — 고정 자리가 없어서 **전투가 시작된 지점 · 전투 중 맞은 지점이 새 자리**가 된다 (사용자 2026-10-01).
	 * 어그로 한계(10m) · 귀환 목적지가 거기 기준. 리스폰 자리는 그대로. 순찰 자체는 ⏸ F13. CSV `bRoams`.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "AI")
	bool bRoams = false;

	/** 다가가면 경계 (F12.6-02 · 연출만). 원본에 beware 애니가 있는 종 — 닭 · 박쥐 · 멧돼지 · 들개 · 늑대 · 곰. CSV `bCanBeware`. */
	UPROPERTY(EditDefaultsOnly, Category = "AI")
	bool bCanBeware = false;

	/** 근처에 아무도 없으면 잔다 (F12.6-02 · 연출만). 원본에 sleep 애니가 있는 종 — 곰 · 늑대 · 들개. CSV `bCanSleep`. */
	UPROPERTY(EditDefaultsOnly, Category = "AI")
	bool bCanSleep = false;

	/** 티어 (추정 1~4 · 보스 5). 스폰 · 표시용. */
	UPROPERTY(EditDefaultsOnly, Category = "분류", meta = (ClampMin = "0"))
	int32 Tier = 1;

	/**
	 * 기초 레벨 [확인] — 닭 · 박쥐 1 · 멧돼지 · 들개 2 · 늑대 3 · 곰 · 까마귀 6 (역기획서 §1.2).
	 * 스폰 레벨 = max(이 값, 섬 최고 실험체 레벨) `[자체]` — 원작은 "섬의 가장 높은 실험체 레벨에 따라" 라고만 한다 (공식 미상).
	 */
	UPROPERTY(EditDefaultsOnly, Category = "분류", meta = (ClampMin = "1"))
	int32 BaseLevel = 1;

	// ── 스폰 규칙 (F12-03 · 역기획서 §3.3 [확인] · 매치 역기획서 §1) ──

	/** 최초 생성 일차. */
	UPROPERTY(EditDefaultsOnly, Category = "스폰", meta = (ClampMin = "1"))
	int32 FirstSpawnDay = 1;

	/** 최초 생성이 밤인가 (곰). */
	UPROPERTY(EditDefaultsOnly, Category = "스폰")
	bool bFirstSpawnNight = false;

	/**
	 * 최초 생성 시각 — **그 페이즈 타이머의 남은 시간** (원작 표기 그대로, 카운트다운). 닭 140(=02:20, 1일차 낮 시작 즉시) · 늑대 60(=01:00, 80초 뒤).
	 * 페이즈 길이 이상이면 페이즈 시작 즉시.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "스폰", meta = (ClampMin = "0"))
	float FirstSpawnTimer = 999.f;

	/** 재생성 방식 — 고정 주기 · 낮마다(늑대) · 밤마다(곰) · 한 번(보스). */
	UPROPERTY(EditDefaultsOnly, Category = "스폰")
	EERWildlifeRespawn RespawnMode = EERWildlifeRespawn::Interval;

	/** 고정 주기(초) — **처치 순간부터** (시체 1분 포함). RespawnMode == Interval 일 때만. */
	UPROPERTY(EditDefaultsOnly, Category = "스폰", meta = (ClampMin = "0", EditCondition = "RespawnMode == EERWildlifeRespawn::Interval"))
	float RespawnSeconds = 120.f;

	/**
	 * 1레벨 스탯. AttackRange · MoveSpeed · Sight 도 여기 (맨몸 값이 곧 전부).
	 * ⭐ 야생동물의 AttackRange 는 **몸 끝에서 더 뻗는 길이**다 — 사거리를 시전자 표면에서 재기 때문 (Argument 32 · F12-04).
	 *   닭 · 박쥐 · 까마귀 0.3 · 멧돼지 · 들개 · 늑대 0.4 · 곰 0.5 · 보스 1.0 `[자체]` (사용자 2026-09-24 "싹다 짧게" — 처음 2.0 이던 곰이 중심에서 3.9m 로 물었다).
	 */
	UPROPERTY(EditDefaultsOnly, Category = "스탯")
	FERCharStats BaseStats;

	/** 레벨당 증가 — 스탯 = Base + PerLevel × (Lv − 1). 시간 성장(03)이 레벨을 올린다. */
	UPROPERTY(EditDefaultsOnly, Category = "스탯")
	FERCharStatGrowth PerLevel;

	/** 스킬 (곰 기절기 · 보스 스킬). 실험체와 같은 GrantSkills 경로 — F11.5 조각 그대로. 비면 없음. */
	UPROPERTY(EditDefaultsOnly, Category = "스킬")
	TArray<TObjectPtr<UERSkillData>> Skills;

	/** 처치 시 루트 테이블 행 (F09-03 `DT_Loot`). 비면 드랍 없음. */
	UPROPERTY(EditDefaultsOnly, Category = "드랍")
	FName LootRow;

	/**
	 * 처치 시 사냥 숙련도 = `HuntExpBase + HuntExpPerLevel × 레벨`.
	 * ⭐ [확인] 종별 값이다 (역기획서 §1.2 · 가이드 2026-09-23): 닭 46/+7 · 박쥐 90/+10 · 들개 110/+10 · 멧돼지 145/+15 · 늑대 144/+12 · 곰 400/+20.
	 *   "닭 대비 배수" 가 아니다 — 0 이면 `UERGrowthSettings` 의 공통 값으로 폴백한다 (변이 · 보스는 (미확인)).
	 */
	UPROPERTY(EditDefaultsOnly, Category = "드랍", meta = (ClampMin = "0"))
	float HuntExpBase = 0.f;

	UPROPERTY(EditDefaultsOnly, Category = "드랍", meta = (ClampMin = "0"))
	float HuntExpPerLevel = 0.f;

	/**
	 * 처치 시 사냥 크레딧 — **처치자** 몫 [확인] 나무위키 (사용자 2026-09-24 "나무위키 기준"): 닭 1 · 박쥐 1 · 멧돼지 · 들개 · 늑대 2 · 곰 6 · 변이 곰 14 · 잠식 곰 40.
	 * 크레딧 경제는 F15 — 지금은 값만 들고 있다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "드랍", meta = (ClampMin = "0"))
	int32 Credit = 0;

	/** 처치자의 **팀원** 몫 (처치자와 별도로 받는다) [확인] 나무위키: 닭 1 · 곰 4 · 변이 곰 10 · 잠식 곰 8. F15. */
	UPROPERTY(EditDefaultsOnly, Category = "드랍", meta = (ClampMin = "0"))
	int32 TeamCredit = 0;

	/** 연출 — 비면 BP 의 기본 메시. 애셋 단계에서 채운다. */
	UPROPERTY(EditDefaultsOnly, Category = "연출")
	TSoftObjectPtr<USkeletalMesh> Mesh;

	UPROPERTY(EditDefaultsOnly, Category = "연출")
	TSoftClassPtr<UAnimInstance> AnimClass;

	/** 메시 스케일 (균일). 1 = 그대로. 피벗이 발끝인 메시 기준 — 위치는 코드가 캡슐 바닥에 맞춘다. */
	UPROPERTY(EditDefaultsOnly, Category = "연출", meta = (ClampMin = "0.01"))
	float MeshScale = 1.f;

	/**
	 * 히트 박스 반크기 (cm, X=앞뒤 · Y=좌우 · Z=위아래). 0 이면 BP_ERWildlife 의 HitBox 기본값 유지.
	 * ⭐ 캡슐은 세로만 되고 반지름 ≤ 반높이라 가로로 긴 몸(멧돼지 · 곰)을 못 담는다 (사용자 2026-09-22) →
	 *   루트 캡슐 = 이동 · 지형 (BP 기본 하나 · 종별 조정 없음), HitBox = 피격 판정(SkillTarget) · 커서 클릭(Pawn) · 다른 폰과의 충돌. 서버 · 클라 양쪽.
	 *   값은 [임시] `ER.Wild.FitHitBox` 가 메시 바운드에서 재 넣는다 (메시 · MeshScale 을 바꾸면 다시 실행).
	 */
	UPROPERTY(EditDefaultsOnly, Category = "연출")
	FVector HitBoxExtent = FVector::ZeroVector;

	/**
	 * 히트 박스 중심 (cm, 액터 기준). 0 이면 박스 바닥을 발끝에 맞춘다 (손으로 넣은 값 호환).
	 * ⭐ 메시마다 몸이 앞뒤로 치우치거나(늑대 · 곰) 공중에 뜬다(박쥐 · 까마귀) — 바닥 정렬로는 안 맞아서 중심을 따로 둔다 (2026-09-23).
	 */
	UPROPERTY(EditDefaultsOnly, Category = "연출")
	FVector HitBoxOffset = FVector::ZeroVector;

	/**
	 * 연출 표 (F12.5-01 · Argument 39) — 평타 · 사망 애니 … 변이 · 잠식은 **자기 스켈레톤**이라 DA 마다 따로 (`DA_Pres_Bear_Mutant`).
	 * 하드 — 야생동물은 매 판 전부 나온다. `ER.Pres.Fill Wild` 가 채운다 (CSV 필드가 아니라 ImportCSV Force 가 안 덮는다).
	 */
	UPROPERTY(EditDefaultsOnly, Category = "연출")
	TObjectPtr<UERPresentationData> Presentation;
};
