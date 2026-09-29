// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameplayCueInterface.h"
#include "GameFramework/Character.h"
#include "GAS/ERSkillData.h"
#include "ERWildlifeCharacter.generated.h"

class UAbilitySystemComponent;
class UBoxComponent;
class UERAttributeSet;
class UERWildlifeData;
class UGameplayEffect;

/**
 * 야생동물 한 마리 (F12-01). ASC + UERAttributeSet 을 **폰이** 든다 — PlayerState 가 없다. 종 정의는 UERWildlifeData (Argument 30 B).
 *
 * ⭐ 복제 모드 **Minimal** (CLAUDE.md §8 · Argument 3). 소유자가 없어서 맞는다 — AI 컨트롤러는 Owner 가 아니다 (PossessedBy 가 되돌린다).
 *   GE 는 클라에 안 가고 어트리뷰트(OnRep_HP) · 태그만 복제된다. 체력바에 충분하다.
 * ⭐ 스탯 = Base + PerLevel × (Lv − 1) 을 **F02 와 같은 초기화 GE** 한 번으로 (ERAttributeInit::ApplyStatRow). 스킬은 실험체와 같은 GrantSkills.
 * ⭐ 복제 비용 (역기획서 §7.3): NetUpdateFrequency 5 · DORM_DormantAll. 맞으면 FlushNetDormancy — 04 가 어그로와 같이 전환한다.
 * 태그 Actor.Type.Wildlife / Actor.Type.Boss — F03 이 흡혈 감소 · 비례 피해 감쇠에 읽는다.
 */
UCLASS()
class ETERNALRETURN_API AERWildlifeCharacter : public ACharacter, public IAbilitySystemInterface, public IGameplayCueInterface
{
	GENERATED_BODY()

public:
	AERWildlifeCharacter();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override { return AbilitySystemComponent; }

	/** IGameplayCueInterface — 연출 큐를 연출 컴포넌트로 (F12.5-05 · 실험체와 같다). */
	using IGameplayCueInterface::HandleGameplayCue;
	virtual void HandleGameplayCue(UObject* Self, FGameplayTag GameplayCueTag, EGameplayCueEvent::Type EventType, const FGameplayCueParameters& Parameters) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PossessedBy(AController* NewController) override;

	/**
	 * [서버] 종 정의 · 레벨로 초기화. 스폰 직후 한 번, 시간 성장(03)이 레벨을 올릴 때 다시 (스탯만 재적용 · 스킬은 1회).
	 * 스탯 GE(Override) → 어트리뷰트 → 클라 복제. 태그 · 스킬 · 복제 값(Data · Level)도 여기서.
	 */
	bool Initialize(const UERWildlifeData* InData, int32 InLevel);

	const UERWildlifeData* GetData() const { return Data; }
	int32 GetLevel() const { return Level; }
	bool IsMutant() const;
	/** AnimBP 가 사망(시체) 포즈를 고르는 데 읽는다 — 복제 값이라 늦게 relevant 된 클라도 누운 시체를 본다 (Argument 36). */
	UFUNCTION(BlueprintPure, Category = "야생동물")
	bool IsDead() const { return bDead; }

	/** [서버] 자기 자리 — 귀환 지점 · 어그로 한계 기준 · 무리 조회. 스폰 서브시스템이 넣는다 (디버그 스폰은 AI 가 빙의 위치로). */
	void SetHome(class AERWildlifeSpawnPoint* Point, const FVector& Location);
	const FVector& GetHomeLocation() const { return HomeLocation; }
	bool HasHome() const { return bHasHome; }
	class AERWildlifeSpawnPoint* GetHomePoint() const { return HomePoint.Get(); }

	/**
	 * [서버] 몸을 깨우거나 재운다 — CMC · 메시 틱 · 도먼시 · 복제 빈도를 **한 곳에서** (Argument 31: 유휴 틱 끔이 ACharacter 유지 조건 · §7.3).
	 * AI 상태 전이(대기 ↔ 전투/귀환)가 부른다.
	 */
	void SetBodyActive(bool bActive);
	bool IsBoss() const;

	/** 사망 (OnOutOfHealth). 02 가 드랍 · 사냥 숙련도를 붙인다. 지금은 로그 + 제거. */
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnWildlifeKilled, AERWildlifeCharacter* /*Self*/, AActor* /*Killer*/);
	FOnWildlifeKilled OnWildlifeKilled;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	void HandleOutOfHealth(AActor* Killer);
	/** [서버] 시체가 들고 있을 루트 컨테이너(AERItemDropActor)를 만든다 — 클릭해서 여는 대상. 비어도 남고 시체와 같이 사라진다. */
	void SpawnCorpse();
	/** [서버] 시체 상태로 전환 — 판정 · 충돌 · 이동을 끄고 1분 뒤 사라진다. 클릭(Select)만 살려 둔다. */
	void EnterCorpseState();
	/** [서버] 처치자에게 사냥 숙련도 (= 경험치 · F10-05 입구 하나). 어시스트 분배 없음 [자체]. */
	void GrantKillRewards(AActor* Killer);
	/** 종 정의의 메시 · 애님 · 스케일 · 히트 박스 적용. 메시는 캡슐 바닥에 (피벗 발끝 규약). 서버 · 클라(OnRep) 양쪽. */
	void ApplyVisuals();

	UFUNCTION()
	void OnRep_Data();

	/** 사망 포즈 (F12.5-04) — 클라. BeginPlay 전이면 늦게 relevant 된 것: 누운 채로. */
	UFUNCTION()
	void OnRep_Dead();

	UPROPERTY(VisibleAnywhere, Category = "GAS")
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, Category = "GAS")
	TObjectPtr<UERAttributeSet> AttributeSet;

	/** 피격 판정(SkillTarget) · 커서 클릭 · 폰 충돌. 캡슐은 이동 · 지형만 (SkillTarget 무시). BP 뷰포트에서 보이게 기본 서브오브젝트 — 크기는 DA.HitBoxExtent 가 덮어쓴다. */
	UPROPERTY(VisibleAnywhere, Category = "충돌")
	TObjectPtr<UBoxComponent> HitBox;

	/** 연출 해석기 (F12.5-01) — 기본 표 = Data->Presentation. 스킨 · 무기 없음 (변이는 DA 가 따로다). */
	UPROPERTY(VisibleAnywhere, Category = "연출")
	TObjectPtr<class UERPresentationComponent> Presentation;

	/** 강제 이동 받기 (Argument 44 K1 — 실험체와 공용). 야생동물도 넉백 · 벽 충돌 기절을 받는다 (사용자 2026-09-29). */
	UPROPERTY(VisibleAnywhere, Category = "전투")
	TObjectPtr<class UERForcedMoveComponent> ForcedMove;

	/** 초기 스탯 Instant GE — 실험체와 같은 GE_ERInitStats. BP 가 지정한다. */
	UPROPERTY(EditDefaultsOnly, Category = "스탯")
	TSubclassOf<UGameplayEffect> InitStatsEffect;

	/** 종 정의. 스폰 시 1회 복제 (애셋 경로) — 클라는 표시 · 메시에 쓴다. 값 변경은 서버 Initialize 만. */
	UPROPERTY(ReplicatedUsing = OnRep_Data)
	TObjectPtr<const UERWildlifeData> Data;

	UPROPERTY(Replicated)
	int32 Level = 1;

	/**
	 * 시체가 들고 있는 루트 컨테이너. **클라도 알아야 한다** — 클릭한 클라가 `ServerPickup(Drop, 칸)` 으로 이 액터를 가리킨다 (F08-05 경로 그대로).
	 * 원작: 죽으면 사망 애니메이션 상태로 남고, **클릭하면 인벤토리처럼 열어** 필요한 것만 가져간다 (사용자 2026-09-23). 창은 F17.
	 */
	UPROPERTY(Replicated)
	TObjectPtr<class AERItemDropActor> Corpse;

	/** 부여한 스킬 핸들 — 실험체 GrantedSkills 와 같은 용도. */
	FERGrantedSkillHandles GrantedSkills;
	bool bSkillsGranted = false;
	UPROPERTY(ReplicatedUsing = OnRep_Dead)
	bool bDead = false;
	/** 죽은 서버 시각 (GetServerWorldTimeSeconds) — 클라가 "쓰러지는 걸 볼 때인가" 를 판정 (F12.5-06 · 한 번 본 뒤 다시 relevant 된 클라). */
	UPROPERTY(Replicated)
	float DeathServerTime = 0.f;

	bool bInitializedOnce = false;
	FVector HomeLocation = FVector::ZeroVector;
	bool bHasHome = false;
	TWeakObjectPtr<class AERWildlifeSpawnPoint> HomePoint;
};
