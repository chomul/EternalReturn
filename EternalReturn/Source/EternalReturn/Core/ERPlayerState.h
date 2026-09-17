// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "AbilitySystemInterface.h"
#include "GameplayTagContainer.h"
#include "ERPlayerState.generated.h"

class UAbilitySystemComponent;
class UERAttributeSet;
class UERInventoryComponent;

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

	// ── 스킬 포인트 (F07-03) ───────────────────────────────────
	// ⭐ 포인트는 여기, 스킬 레벨은 ASC 의 스펙에 있다. 둘 다 서버가 바꾸고 소유자에게 복제된다.
	//   ⚠ 지급은 F10(성장·레벨)이 한다. 지금은 시작 포인트도 안 준다 — 디버그 명령으로 넣는다.

	/** 미배분 포인트. 소유자만 본다 (COND_OwnerOnly). */
	UPROPERTY(ReplicatedUsing = OnRep_SkillPoints, BlueprintReadOnly, Category = "Skill")
	int32 SkillPoints = 0;

	/** [서버] 포인트 지급. F10 레벨업이 부른다. */
	void AddSkillPoints(int32 Amount);

	/**
	 * [클라 -> 서버] 이 슬롯에 1포인트 쓴다. 서버가 검증한다 — 클라 값을 믿지 않는다:
	 *   포인트 > 0 · 슬롯 존재 · bUsesSkillPoints · Level < MaxLevel (뒤 셋은 ERSkill::LevelUpSkill).
	 * ⚠ "포인트가 남았는데 전부 만렙" 은 정상이다 (역기획서 §2.1 — 20레벨에 2포인트 남는다). assert 없음.
	 */
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerLevelUpSkill(FGameplayTag SlotTag);

protected:
	UFUNCTION()
	void OnRep_SkillPoints();

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
	/** 인벤토리 (F08). ⭐ ASC 와 같은 액터에 — 장비 GE 와 핸들의 수명이 같다 (Docs/4_Argument/21). */
	UERInventoryComponent* GetInventory() const { return Inventory; }

protected:
	UPROPERTY(VisibleAnywhere, Category = "Item")
	TObjectPtr<UERInventoryComponent> Inventory;
};
