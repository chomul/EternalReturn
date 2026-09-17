// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "AttributeSet.h"
#include "Item/ERItemTypes.h"
#include "ERItemData.generated.h"

class UTexture2D;
class UERCharacterData;

/**
 * 아이템 **정의** 한 행. 데이터 테이블 `DT_Items` 의 행이고, **행 이름이 아이템 ID** 다.
 *
 * ⭐ 정의 / 인스턴스를 나눈다 (Docs/4_Argument/20_아이템데이터_저장방식.md):
 *   - 정의 (이 구조체) : "망치 1단계 = 영웅 · 무기 · 공격력 +30". 전원이 같은 표를 갖는다. 게임 중 안 바뀐다
 *   - 인스턴스 (F08-04) : "PS_1 의 3번 칸에 망치 1개". 서버 권위 + COND_OwnerOnly 복제. 수량 · 강화 같은 실시간 상태는 거기에
 *   런타임에 오가는 것은 **행 이름(FName) 하나**다.
 *
 * ⚠ 행 이름을 바꾸면 재료 참조(F09) · 저장된 인벤토리가 끊긴다. 처음부터 안정적으로 (`hammer_t1` 같은 영문 스네이크).
 * ⚠ 필드는 **끝에** 붙인다. 순서를 바꾸면 테이블에 저장된 값이 어긋난다 (CLAUDE.md §3).
 * ⚠ 제작 재료(A/B)는 F09 에서, 고유 효과(격동 · 처형)는 6순위 별도 시스템에서 붙는다. 여기 없는 것이 맞다.
 */
USTRUCT(BlueprintType)
struct FERItemRow : public FTableRowBase
{
	GENERATED_BODY()

	/** 표시 이름. 원작 한글명. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EERItemGrade Grade = EERItemGrade::Common;

	/** None 이면 장비가 아니다 (재료 · 소모품). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EEREquipSlot Slot = EEREquipSlot::None;

	/** Slot 이 Weapon 일 때만. 장착 제한(F08-02)이 실험체의 무기군과 비교한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (EditCondition = "Slot == EEREquipSlot::Weapon"))
	EERWeaponType WeaponType = EERWeaponType::None;

	/**
	 * 스탯 옵션 (역기획서 §6.1). ⭐ F02 어트리뷰트를 **직접** 가리킨다 — "옵션 enum → 어트리뷰트" 매핑 표를 따로 두지 않는다.
	 * 값은 어트리뷰트 단위 그대로 (공격력 30 = 30, 치명타 확률 10% = 0.1, 이동 속도 0.05 m/s = 0.05).
	 * F08-03 이 이 맵을 Infinite GE 로 바꿔 건다. ⚠ 어트리뷰트에 없는 옵션(보호막 · 고유 효과)은 여기 못 들어간다 — 별도 단계.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TMap<FGameplayAttribute, float> StatModifiers;

	/** 아이콘. ⚠ 소프트 참조 — 수백 행이 한꺼번에 텍스처를 로드하면 안 된다. UI 가 필요할 때 로드한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftObjectPtr<UTexture2D> Icon;

	/**
	 * 한 칸에 쌓이는 최대 수량 (F08-04). ⭐ 원작 확인 — 재료마다 다르다 (가죽 · 돌멩이 3, 꽃 · 못 2 — 나무위키 아이템/재료).
	 * 장비는 1. ⚠ 필드는 끝에 붙인다 (CLAUDE.md §3).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "1"))
	int32 MaxStack = 1;

	bool IsEquipment() const { return Slot != EEREquipSlot::None; }
};

namespace ERItem
{
	/**
	 * 아이템 정의 조회. 없으면 nullptr + Error 로그 (조용히 nullptr 을 돌려주지 않는다 — 오타를 즉시 드러낸다).
	 * 테이블은 UERItemSettings 가 가리킨다. 첫 호출에 동기 로드하고 그 뒤로는 캐시.
	 * ⚠ 포인터는 테이블이 살아 있는 동안 유효하다. 저장해 두지 말고 그때그때 찾는다 (FindRow 는 해시 조회, O(1)).
	 */
	const FERItemRow* Find(FName ItemId);

	/** 테이블 전체 (F09 제작 트리 · 디버그). 로드 실패면 nullptr. */
	const UDataTable* GetTable();

	/**
	 * 이 실험체가 이 아이템을 장착할 수 있는가 (F08-02). 순수 함수 — 상태를 안 바꾼다.
	 *   장비가 아니면 ✗ · 무기면 CharacterData.WeaponTypes 에 있어야 ✓ · 방어구는 제한 없음 (장비 역기획서 §2)
	 *
	 * ⭐ **서버와 클라가 같은 함수를 쓴다.** 클라는 UI 회색 처리용 선판정, 서버(ServerEquip, F08-03)가 최종 판정 — 클라 결과를 믿지 않는다.
	 * @param OutReason  거부 사유 (로그 · UI 용). nullptr 가능
	 */
	bool CanEquip(const UERCharacterData& Character, const FERItemRow& Item, FString* OutReason = nullptr);
}
