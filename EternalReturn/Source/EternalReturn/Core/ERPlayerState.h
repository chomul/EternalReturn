// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "AbilitySystemInterface.h"
#include "GameplayTagContainer.h"
#include "GameplayEffectTypes.h"
#include "GAS/ERSkillData.h"
#include "ERPlayerState.generated.h"

class UAbilitySystemComponent;
class UERAttributeSet;
class UERInventoryComponent;
class UERGrowthComponent;
struct FGameplayAbilitySpecHandle;

/**
 * 플레이어의 ASC 소유자.
 *
 * ⭐ ASC 를 Pawn 이 아니라 PlayerState 에 두는 이유:
 *   이 게임은 부활이 코어 루프에 있다(자동 → 크레딧 200 → 불가 3단).
 *   Pawn 에 두면 죽을 때마다 레벨·경험치·숙련도를 서버가 손으로 복원해야 하고,
 *   그 복원 코드가 성장·인벤토리 시스템과 얽히면서 계속 자란다.
 *   PlayerState 에 두면 Pawn 이 파괴돼도 살아남는다.
 *
 * ⚠ 야생동물·보스는 PlayerState 가 없으므로 ASC 를 Pawn 에 붙인다.
 *   즉 이 프로젝트는 두 배치가 공존한다. 호출부가 그것을 몰라도 되도록
 *   양쪽 다 IAbilitySystemInterface 를 구현한다.
 *
 * ⚠ 이 결정을 바꾸려면 어트리뷰트셋·데미지·스킬 작업을 함께 고쳐야 한다.
 */
UCLASS()
class AERPlayerState : public APlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AERPlayerState();

	/** IAbilitySystemInterface */
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/**
	 * ⭐ 둔화 재계산을 ASC 에 연결하는 자리다 (F06-02).
	 *
	 * ⚠ **폰이 아니라 여기서 거는 이유**: AERCharacterBase::InitAbilityActorInfo 는
	 *   PossessedBy · OnRep_PlayerState · 재소유 때마다 **여러 번 불린다.**
	 *   거기서 걸면 델리게이트가 중복 등록되어 재계산이 N번 돈다.
	 *   PlayerState 는 ASC 와 수명이 같고 BeginPlay 가 한 번만 불린다.
	 */
	virtual void BeginPlay() override;

	/**
	 * 소속 팀. INDEX_NONE 은 미배정.
	 *
	 * 전체 복제한다 — 적이 어느 팀인지는 모두가 알아야 한다.
	 * 아군/적군 색 구분, 아군 오사 방지, HUD 표시가 전부 이 값을 쓴다. 숨길 정보가 아니다.
	 *
	 * ⚠ 직접 읽지 말고 ERTeamStatics::GetTeamId() 를 쓴다.
	 *   야생동물처럼 PlayerState 가 없는 액터도 있어서, 호출부가 여기를 직접 캐스팅하면
	 *   나중에 팀 정보를 옮길 때 전부 고쳐야 한다.
	 */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Team")
	int32 TeamId = INDEX_NONE;

	// ── 스킬 포인트 (F07-03 · F10-03) ──────────────────────────
	// ⭐ 포인트는 여기, 스킬 레벨은 ASC 의 스펙에 있다. 둘 다 서버가 바꾸고 소유자에게 복제된다.
	//   지급: 시작 포인트(BeginPlay · ERGrowthSettings.StartingSkillPoints) + 레벨업(Growth->OnLevelUp · FERLevelExpRow.SkillPointGranted).

	/** 미배분 포인트. 소유자만 본다 (COND_OwnerOnly). */
	UPROPERTY(ReplicatedUsing = OnRep_SkillPoints, BlueprintReadOnly, Category = "Skill")
	int32 SkillPoints = 0;

	/** [서버] 포인트 지급. 레벨업 훅과 디버그가 부른다. */
	void AddSkillPoints(int32 Amount);

	/**
	 * [클라 -> 서버] 이 슬롯에 1포인트 쓴다. 서버가 검증한다 — 클라 값을 믿지 않는다:
	 *   포인트 > 0 · 슬롯 존재 · bUsesSkillPoints · Level < MaxLevel · 실험체 레벨(MinCharacterLevel) (뒤 넷은 ERSkill::LevelUpSkill).
	 * ⚠ "포인트가 남았는데 전부 만렙" 은 정상이다 (역기획서 §2.1 — 20레벨에 2포인트 남는다). assert 없음.
	 */
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerLevelUpSkill(FGameplayTag SlotTag);

	// ── 스킨 (F12.5-01 · Argument 39) ──────────────────────────
	// ⭐ 전원 복제 — 남의 스킨도 보인다. 서버도 쓴다 (스킨 전용 애니를 서버가 골라 재생한다).
	//   인덱스 = UERCharacterData.Skins. 로비가 생기기 전에는 ER.Skin.Set 이 바꾼다.

	int32 GetSkinIndex() const { return SkinIndex; }

	/** [서버] */
	void SetSkinIndex(int32 NewIndex);

	/** 스킨이 바뀌었다 — 서버는 SetSkinIndex, 클라는 OnRep 에서. 폰이 구독해 몸 · 연출을 바꾼다. */
	DECLARE_MULTICAST_DELEGATE(FOnSkinChanged);
	FOnSkinChanged OnSkinChanged;

	// ── 전술 스킬 F (Argument 80 · C2 · G1) ─────────────────────
	// ⭐ 인덱스 = ER Combat TacticalSkills. 전원 복제 — 캐릭터 선택 UI 가 팀원 선택도 읽는다. -1 = 없음.

	int32 GetTacticalIndex() const { return TacticalIndex; }

	/** [서버] 옛 F 회수 → 목록의 Index 번 부여 (레벨 1). 범위 밖이면 Warning 후 그대로. */
	void SetTacticalSkill(int32 Index);

	/** [클라 -> 서버] 캐릭터 선택 UI 가 부르는 입구 (로비 전에는 ER.Tactical). 범위는 서버가 다시 본다. */
	UFUNCTION(Server, Reliable, WithValidation, BlueprintCallable, Category = "Skill")
	void ServerSelectTacticalSkill(int32 Index);

	/** 전술 스킬이 바뀌었다 — 서버는 SetTacticalSkill, 클라는 OnRep 에서. UI 가 구독한다. */
	DECLARE_MULTICAST_DELEGATE(FOnTacticalChanged);
	FOnTacticalChanged OnTacticalChanged;

	// ── 채집 시간 (F12.5-04 · Argument 47 E2) ──────────────────
	// ⭐ 채집 포즈 애니를 채집 시간에 맞춰 재생하려고 **모든 머신이** 알아야 한다. 채집 태그(State.Gathering)와 같은 액터(ASC 가 여기)라 같은 번들로 도착한다.
	float GetGatherSeconds() const { return GatherSeconds; }
	/** [서버] 채집 시작 때 (ERItemDropActor::TryBeginGather). */
	void SetGatherSeconds(float Seconds) { GatherSeconds = Seconds; }

protected:
	UFUNCTION()
	void OnRep_SkillPoints();

	UFUNCTION()
	void OnRep_SkinIndex();

	UPROPERTY(ReplicatedUsing = OnRep_SkinIndex)
	int32 SkinIndex = 0;

	UPROPERTY(Replicated)
	float GatherSeconds = 0.f;

	UFUNCTION()
	void OnRep_TacticalIndex();

	UPROPERTY(ReplicatedUsing = OnRep_TacticalIndex)
	int32 TacticalIndex = INDEX_NONE;

	/** [서버] 지금 F 스킬 핸들 — 바꿀 때 회수 */
	FERGrantedSkillHandles TacticalSkills;

	/**
	 * 초기화 전에는 유효하지 않을 수 있다. 호출부는 항상 null 검사를 한다.
	 * 실제 초기화(InitAbilityActorInfo)는 AERCharacterBase 가 한다 —
	 * Avatar 가 폰이라서 폰이 준비된 시점을 알아야 하기 때문이다.
	 */
	UPROPERTY(VisibleAnywhere, Category = "GAS")
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	/**
	 * 공통 스탯. 생성자에서 만들면 ASC 가 자동으로 SpawnedAttributes 에 넣는다.
	 *
	 * ⭐ 캐릭터 고유 리소스(에키온 VF 게이지 등)는 여기 넣지 않는다.
	 *   별도 AttributeSet 을 만들어 **병렬로 추가**한다. 상속하지 않는다 -
	 *   ASC 조회가 IsA() 라 같은 계층 둘을 등록하면 조용히 틀린 세트를 쓴다.
	 *   근거: Docs/4_Argument/3_어트리뷰트셋_구조.md E절
	 */
	UPROPERTY(VisibleAnywhere, Category = "GAS")
	TObjectPtr<UERAttributeSet> AttributeSet;

public:
	/**
	 * [서버 → 이 플레이어 화면만] 음성 키 하나 (Argument 73 V-B 본인) — 처치 · 제작 · 채집 · 상자 · 시체 · D 습득 혼잣말.
	 * Unreliable — 놓쳐도 대사 하나. 받는 클라가 자기 폰의 연출 컴포넌트로 2D 재생.
	 */
	UFUNCTION(Client, Unreliable)
	void ClientPlayVoice(FGameplayTag VoiceKey);

	/** [서버 → 이 플레이어 화면만] 효과음 키 하나를 2D 로 (팀 알림 — 팀원 빈사 · 소생 · 전멸 · F14 · 사용자 2026-10-08). 소리는 자기 캐릭터 동작표 → 없으면 공용 ER Sound (Argument 78) */
	UFUNCTION(Client, Unreliable)
	void ClientPlaySfx(FGameplayTag SfxKey);

	/** [서버 → 이 플레이어 화면만] 소리 애셋 하나를 2D 로 — 키가 아니라 데이터 칸이 애셋을 직접 가리킬 때 (상자 모양마다 다른 여는 소리 · DT_Loot OpenSounds) */
	UFUNCTION(Client, Unreliable)
	void ClientPlaySoundPath(const FSoftObjectPath& SoundPath);

protected:
	/** [서버] 이번 판 실험체 처치 수 (막타) — 누적 처치 음성 kill<N>Player. 복제 안 함 (UI 는 F14) */
	int32 PlayerKills = 0;

public:
	// ── 빈사 · 사망 · 소생 (F14 · Argument 75) ── 상태는 태그 (State.Downed · State.Dead) — 어느 머신에서나 읽힌다
	bool IsDowned() const;
	bool IsDead() const;
	/** 폰이 있고 빈사도 사망도 아니다 (빈사 판단 · 전멸 판단) */
	bool IsStanding() const;
	/** [서버] 아군 소생 완료 (UERReviveAbility) — 빈사를 풀고 체력 = 최대 × ReviveHPRatio. 자가 제세동은 By = 자기 */
	void Revive(AERPlayerState* By);
	/** [서버] 빈사 스킬 분열 폭발 뒤 사망 (Argument 79) — 빈사일 때만. 처치 = 빈사로 만든 사람 (K2) */
	void SelfDestruct();

protected:
	/** [서버] 체력 0 알림 (UERAttributeSet::OnOutOfHealth) — 빈사 중이면 사망 · 서 있는 팀원이 있으면 빈사 · 없으면 사망 · 그 뒤 전멸 검사 */
	void HandleOutOfHealth(AActor* LastHitter);
	void EnterDowned(AERPlayerState* By);
	/** 빈사 GE · 체력 감소 GE · 받는 피해 감소를 걷는다 (소생 · 사망) */
	void ClearDowned();
	/** 사망 — 처치자 = 빈사로 만든 사람 (K2) · 없으면 LastHitter (마지막 가해자). 시체 · 처치 이벤트 · 처치 경험치 · 음성 */
	void Die(AActor* LastHitter);
	/** 스킬의 "처치 시" 이벤트 (Event.Kill.Dealt) — **빈사로 만든 순간만** (사용자 2026-10-08). 바로 사망 · 빈사에서 사망은 안 보낸다 */
	void SendKillEvent(AERPlayerState* KillerPS, APawn* VictimPawn);
	/** 우리 팀에 서 있는 사람이 없으면 빈사자 전원 사망 (전멸) · 전멸시킨 팀에 알림 (Terminator = 이번에 쓰러뜨린 사람) */
	void CheckTeamWipe(AERPlayerState* Terminator);
	/** 팀원들(나 빼고 · bIncludeSelf 면 나도) 화면에 효과음 2D */
	void SendTeamSfx(FGameplayTag SfxKey, bool bIncludeSelf);
	/** 시전 중 어빌리티 전부 취소 (패시브 제외) */
	void CancelActiveAbilities();
	/** 같은 팀 다른 플레이어 (GameState PlayerArray) */
	void ForEachTeammate(TFunctionRef<void(AERPlayerState&)> Fn) const;

	FActiveGameplayEffectHandle DownedHandle;
	FActiveGameplayEffectHandle BleedHandle;
	/** 빈사 스킬 (ER Combat DownedSkills) — 모든 실험체 · BeginPlay 에서 한 번 */
	FERGrantedSkillHandles DownedSkills;
	/** [서버] 집념 태그가 붙고 빠질 때 — 빈사 GE 의 이속 SetByCaller 를 바꾼다 (Override 라 버프 GE 로는 안 먹는다 · Argument 79 D4) */
	void OnObstinacyChanged(const FGameplayTag Tag, int32 Count);
	/** 지금 빈사 이속 (집념 중이면 × (1 + 보너스)) */
	float GetDownedMoveSpeed() const;
	/** [서버] 나를 빈사로 만든 사람 (K2 처치 인정) — 소생 · 사망 때 비운다 */
	TWeakObjectPtr<AERPlayerState> DownedBy;

public:
	/** 인벤토리 (F08). ⭐ ASC 와 같은 액터에 — 장비 GE 와 핸들의 수명이 같다 (Docs/4_Argument/21). */
	UERInventoryComponent* GetInventory() const { return Inventory; }

protected:
	UPROPERTY(VisibleAnywhere, Category = "Item")
	TObjectPtr<UERInventoryComponent> Inventory;

public:
	/** 성장 — 경험치 · 레벨 (F10). 부활해도 남아야 하니 여기 (Docs/4_Argument/22). */
	UERGrowthComponent* GetGrowth() const { return Growth; }

protected:
	UPROPERTY(VisibleAnywhere, Category = "Growth")
	TObjectPtr<UERGrowthComponent> Growth;

	// ── 무기 스킬 (F11-02) ─────────────────────────────────────
	// ⭐ 무기가 D · 평타를 소유한다. 실험체 스킬(폰의 GrantedSkills)과 **별도 핸들** — 부활로 폰이 바뀌어도 ASC 는 여기라 그대로.
	//   클라에는 아무것도 따로 안 보낸다: Equipped(F08) + ASC 스펙 복제 + AttackRange 어트리뷰트 (역기획서 §8.1).

	/**
	 * [서버] 장착 무기에 맞춰 D · 평타를 갈아끼운다: 회수 → (계열 행) 부여 → 사거리 Base 교체 (Argument 24 B).
	 * 무기가 없으면 아무것도 부여하지 않고 State.Unarmed 를 건다 — 평타 · 스킬 전부 차단 (원작 확인 2026-09-19). 사거리는 실험체 맨몸 값으로.
	 * 장착 변경(Inventory->OnEquippedChanged Weapon) 과 BeginPlay 에서 부른다.
	 */
	void RefreshWeaponSkills();

	/**
	 * [서버] 전투 상태 진입 · 갱신 (F11-04). 피해를 주거나 받을 때마다 State.InCombat 을 CombatStateSeconds 로 다시 건다 (UERSkillPhaseEffect 재사용).
	 * 원작 확인 (사용자 2026-09-19): 전투 중에는 무기를 못 바꾼다 (알렉스 예외 — 실험체 플래그 자리, 지금 없음).
	 */
	void EnterCombat();

	/** [서버] AttackRange 의 BaseValue 를 교체한다 (UERWeaponRangeEffect · Instant Override). 폰이 없으면 Warning. */
	void ApplyWeaponRange(float RangeMeters);

	/**
	 * [서버] 장착 계열의 숙련도 레벨로 D 스펙 레벨을 맞춘다 (F11-03): 숙련도 < UnlockLevel → 0 · 아니면 1 + (UpgradeLevels 중 도달한 수).
	 * 5/10/15 는 계열 행 값. 부여 직후(RefreshWeaponSkills) 와 Growth->OnWeaponProficiencyLevelUp 이벤트에서 — 매 프레임 비교 없음.
	 */
	/** @return D 가 이번에 처음 해금됐다 (0 → 1 · 습득 음성) */
	bool SyncWeaponSkillLevel();

public:
	/**
	 * [서버] 기본 공격 슬롯 교체 (F11-05 D 저격 모드 · Argument 27 ②A). UERGameplayAbility::EnterMode/CleanupMode 가 부른다. Data 가 있으면 지금 Attack 슬롯 스펙을 빼고 Data 를 그 레벨로 부여,
	 * nullptr 이면 계열 행의 AttackData 로 복구. 핸들은 WeaponSkills 에 같이 두어 무기 교체(TakeSkills)가 함께 회수한다.
	 */
	void SetModeAttack(UERSkillData* Data, int32 Level);
	FGameplayAbilitySpecHandle GetModeAttackHandle() const { return ModeAttackHandle; }

	/** [서버] 모드 평타 조준 제한 — 진입 시 방향 ± 반각. HalfAngleDeg 0 = 없음. SetModeAttack(nullptr) 이 같이 지운다. UERGameplayAbility::ResolveAim 이 읽는다. */
	void SetModeAimLimit(const FVector& CenterDir, float HalfAngleDeg) { ModeAimCenter = CenterDir.GetSafeNormal2D(); ModeAimHalfAngleDeg = HalfAngleDeg; }
	const FVector& GetModeAimCenter() const { return ModeAimCenter; }
	float GetModeAimHalfAngleDeg() const { return ModeAimHalfAngleDeg; }

protected:
	FERGrantedSkillHandles WeaponSkills;
	FGameplayAbilitySpecHandle ModeAttackHandle;
	FVector ModeAimCenter = FVector::ForwardVector;
	float ModeAimHalfAngleDeg = 0.f;
	FActiveGameplayEffectHandle UnarmedHandle;
	FActiveGameplayEffectHandle CombatHandle;
	/** 무기 사거리로 Base 를 바꿔 둔 상태인가 — 해제 때 맨몸 값으로 되돌릴지 판단 (부여 스킬 수와 무관, E17 후속). */
	bool bWeaponRangeApplied = false;
};
