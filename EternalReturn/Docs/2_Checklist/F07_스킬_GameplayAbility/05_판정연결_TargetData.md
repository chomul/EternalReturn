# ✅ F07-05 — 판정 연결 (F04 → TargetData) ⭐

> Task [`../../1_Task/F07_스킬_GameplayAbility/05_판정연결_TargetData.md`](../../1_Task/F07_스킬_GameplayAbility/05_판정연결_TargetData.md)

## 선행

- [x] [F04 판정 · 타게팅](../F04_판정_타게팅/00_기능_완료판정.md) 완료
- [x] [04 실행 파이프라인](04_실행파이프라인_채널링.md) 완료

## 구현

- [x] ⭐ **`ERTargeting` 함수를 재사용**한다 — `ExecuteSkill` 이 `FERSkillShape` + 조준을 `FTargetQuery` 로 옮겨 `ERTargeting::Query` 호출뿐
- [x] 결과를 `FGameplayAbilityTargetData` 로 감싼다 — `AbilityTargetDataFromActorArray` → `ApplyGameplayEffectSpecToTarget`. 조준은 `_SingleTargetHit` (커서 HitResult)
- [x] ⭐ **자체 RPC 구조체를 만들지 않는다** — PC `ServerActivateSkill(FGameplayTag, FGameplayAbilityTargetDataHandle)`. RPC 는 PC 에 있으나 그릇은 GAS (Argument 18 — 엔진이 클라→서버 이벤트 데이터 경로를 ServerInitiated 에 안 열어 둠)
- [x] ⭐ 광역 스킬이 GE Spec 에 **`Damage.Shape.AoE`** 를 붙인다 — `FERSkillShape::IsAoE()` (SingleTarget · 비관통 Projectile 만 단일, 자체 결정값)

## 빌드

- [x] `EternalReturnEditor` 빌드 통과 — 2026-09-13 에러 0
- [x] `EternalReturnServer` 빌드 통과 — 2026-09-13 에러 0

## 검증 — `Play As Listen Server, Number of Players: 2`

> 절차: [`../../3_EditorTasks/F07_스킬.md`](../../3_EditorTasks/F07_스킬.md) "F07-05" · ⚠ 에디터 재시작

- [x] ⭐ 레니 W 가 **중앙 1.25m / 외곽 2.25m 를 다른 효과**로 처리한다 → **분리** 확인 (`적중 0 (중앙 1)` / `적중 1 (중앙 0)`, 2026-09-13). 다른 효과는 레니 W 파생의 `OnTargetsResolved` 오버라이드 (M-레니)
- [x] ⭐ 판정이 **서버에서** 일어난다 — 판정 로그가 서버 인스턴스에서만 한 번
- [x] ⭐⭐ **흡혈 회복이 공격자에게 간다** (F03-05 완결) — PS_0 이 PS_1 을 때려 PS_0 HP 500 → 517
- [x] ⭐ **Source / Target 캡처가 뒤바뀌지 않았다** (F03-01 완결) — 공격자 HP 그대로, 대상만 66.7 씩 감소
- [x] 광역 스킬에서 흡혈이 절반이 된다 (`Damage.Shape.AoE`) — +16.7 = 66.7 × 0.5 × 0.5

## ⭐ 가장 위험한 실수

- [x] ⭐ **F04 판정 함수를 다시 만들지 않았다**
- [x] ⭐ **자체 RPC 구조체를 만들지 않았다**
- [x] ⚠ 판정 형상 태그를 **어빌리티가** 붙인다 (Execution 이 추측하지 않는다) — `ApplySkillDamage`
