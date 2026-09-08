// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "ERStatCapSettings.generated.h"

/**
 * 스탯 상한(캡)을 한 곳에 모은다.
 *
 * 역기획서 §5 가 이 영역을 "클론 구현 시 자체 결정 필요"로 남겼고,
 * **캡을 코드에 흩뿌리지 말고 한 곳에 모으라**고 못 박았다.
 *
 * ⚠ 각 값에 **원작 확인값인지 자체 결정값인지**를 주석으로 표시한다.
 *   나중에 원작 값을 확인했을 때 무엇을 고쳐야 하는지 즉시 알아야 한다.
 *
 * 데이터 테이블이 아니라 DeveloperSettings 를 쓴 이유:
 *   - 캡은 실험체별이 아니라 **프로젝트 전역** 값이다 (행이 하나뿐인 테이블이 된다)
 *   - 애셋을 만들지 않아도 되고 Project Settings 에 그대로 보인다
 *   - config(ini) 로 저장되어 텍스트로 diff 가 된다
 *
 * 위치: Project Settings > Game > ER Stat Caps
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "ER Stat Caps"))
class UERStatCapSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UERStatCapSettings();

	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	static const UERStatCapSettings& Get();

	// ── 원작 확인값 ─────────────────────────────────────────

	/**
	 * 초당 기본 공격 횟수 상한. **원작 확인값 2.5.**
	 *
	 * ⚠ 돌격소총 "과열"은 이 상한을 무시한다. 그 예외는 무기 작업에서 붙인다 —
	 *   지금은 예외 경로가 없으므로 항상 클램프된다.
	 */
	UPROPERTY(config, EditAnywhere, Category = "확인값", meta = (ClampMin = "0.1"))
	float MaxAttackSpeed = 2.5f;

	// ── 자체 결정값 (원작 미확인) ───────────────────────────
	// 아래는 전부 "100%에 도달하면 게임이 깨지는" 스탯이다.
	// 원작 값을 모르지만 **비워두면 더 위험해서** 임시 캡을 넣는다.

	/** 치명타 확률 상한. ⚠ 자체 결정값. 100% 초과분이 낭비되는지 다른 효과로 바뀌는지 (미확인) */
	UPROPERTY(config, EditAnywhere, Category = "자체 결정값 (미확인)", meta = (ClampMin = "0.0"))
	float MaxCritChance = 1.0f;

	/** 피해 감소 계열 상한. ⚠ 자체 결정값. **1.0 을 넘기면 맞을 때마다 체력이 차오른다.** */
	UPROPERTY(config, EditAnywhere, Category = "자체 결정값 (미확인)", meta = (ClampMin = "0.0", ClampMax = "0.99"))
	float MaxDamageDown = 0.8f;

	/** 방어 관통(%) 상한. ⚠ 자체 결정값. 1.0 이면 방어력 스탯 자체가 무의미해진다. */
	UPROPERTY(config, EditAnywhere, Category = "자체 결정값 (미확인)", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MaxDefPenPercent = 0.8f;

	/** 방해 저항 / 이속 감소 저항 상한. ⚠ 자체 결정값. 1.0 이면 CC 완전 면역이 된다. */
	UPROPERTY(config, EditAnywhere, Category = "자체 결정값 (미확인)", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MaxResist = 0.8f;

	/** 이동 속도 상한. ⚠ 자체 결정값. 너무 크면 서버 이동 검증·충돌 판정이 깨진다. */
	UPROPERTY(config, EditAnywhere, Category = "자체 결정값 (미확인)", meta = (ClampMin = "0.1"))
	float MaxMoveSpeed = 12.0f;

	/**
	 * 이동 속도 하한. ⚠ 자체 결정값.
	 *
	 * ⭐ **0 이 되면 영구 행동 불능이다.** 역기획서가 "하한 필수"로 표시한 항목이다.
	 *   원작은 "일정 수준 이하로 안 내려감"만 확인됐고 값은 (미확인).
	 */
	UPROPERTY(config, EditAnywhere, Category = "자체 결정값 (미확인)", meta = (ClampMin = "0.01"))
	float MinMoveSpeed = 1.0f;

	// ── 클램프하지 않는 것 ──────────────────────────────────
	// 스킬 가속: **원작에서 상한 없음이 확인됐다.** 클램프하지 않는다.
	//   ⚠ "쿨감 30% 상한"은 구버전 서술이다. 그걸 따라 캡을 넣으면 안 된다.
};
