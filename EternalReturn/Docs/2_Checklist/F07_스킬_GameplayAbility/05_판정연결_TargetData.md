# ✅ F07-05 — 판정 연결 (F04 → TargetData) ⭐

> Task [`../../1_Task/F07_스킬_GameplayAbility/05_판정연결_TargetData.md`](../../1_Task/F07_스킬_GameplayAbility/05_판정연결_TargetData.md)

## 선행

- [ ] [F04 판정 · 타게팅](../F04_판정_타게팅/00_기능_완료판정.md) 완료
- [ ] [04 실행 파이프라인](04_실행파이프라인_채널링.md) 완료

## 구현

- [ ] ⭐ **`ERTargeting` 함수를 재사용**한다 (판정을 다시 만들지 않는다)
- [ ] 결과를 `FGameplayAbilityTargetData` 로 감싼다
- [ ] ⭐ **자체 RPC 구조체를 만들지 않는다**
- [ ] ⭐ 광역 스킬이 GE Spec 에 **`Damage.Shape.AoE`** 를 붙인다

## 빌드

- [ ] `EternalReturnEditor` 빌드 통과
- [ ] `EternalReturnServer` 빌드 통과

## 검증 — `Play As Client, Number of Players: 2`

- [ ] ⭐ 레니 W 가 **중앙 1.25m / 외곽 2.25m 를 다른 효과**로 처리한다
- [ ] ⭐ 판정이 **서버에서** 일어난다
- [ ] ⭐⭐ **흡혈 회복이 공격자에게 간다** (F03-05 완결)
- [ ] ⭐ **Source / Target 캡처가 뒤바뀌지 않았다** (F03-01 완결)
- [ ] 광역 스킬에서 흡혈이 절반이 된다 (`Damage.Shape.AoE`)

## ⭐ 가장 위험한 실수

- [ ] ⭐ **F04 판정 함수를 다시 만들지 않았다**
- [ ] ⭐ **자체 RPC 구조체를 만들지 않았다**
- [ ] ⚠ 판정 형상 태그를 **어빌리티가** 붙인다 (Execution 이 추측하지 않는다)
