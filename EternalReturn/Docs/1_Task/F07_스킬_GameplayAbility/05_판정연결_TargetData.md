# F07-05 — 판정 연결 (F04 → TargetData) ⭐

> 기능 [F07](00_개요.md) · 근거 [`스킬_프레임워크_역기획서.md`](../../0_GameDesign/Systems/스킬_프레임워크_역기획서.md) · 우선순위 5
> 체크리스트 [`../../2_Checklist/F07_스킬_GameplayAbility/05_판정연결_TargetData.md`](../../2_Checklist/F07_스킬_GameplayAbility/05_판정연결_TargetData.md)

## 할 일

F04 의 판정 결과를 **`FGameplayAbilityTargetData`** 로 감싸 F03 에 넘긴다.

```
F04 ERTargeting::Query* → TargetData → GE Spec → F03 ERDamageExecution
```

## ⭐ F04 는 이미 완성돼 있다

`ERTargeting` 의 함수 6종이 F04 에서 검증까지 끝났다:
`QuerySingleTarget` / `QuerySelfRadius` / `QueryProjectile` / `QueryGroundCircle` / `QueryCone` / `QueryDualRadius`

⭐ **판정 코드를 다시 만들지 않는다.** 여기서는 **결과를 GAS 형식으로 옮기기만** 한다.

## ⭐ 자체 RPC 구조체를 만들지 않는다

조준 데이터 전송은 `FGameplayAbilityTargetData` 가 한다 (`CLAUDE.md` §8).

## 조준 데이터 (2026-09-13 결정)

클라 PC 가 커서 `FHitResult` 를 `FGameplayAbilityTargetData_SingleTargetHit` 로 감싸 `ServerActivateSkill` RPC 에 싣고,
서버가 `TriggerAbilityFromGameplayEvent` 로 어빌리티에 넘긴다. 어빌리티는 `ResolveAim` 에서 **사거리 클램프** 후 `AimPoint/AimDirection/AimActor` 로 든다.
근거 [`../../4_Argument/18_조준데이터_전달경로.md`](../../4_Argument/18_조준데이터_전달경로.md) — 엔진이 ServerInitiated 의 클라→서버 이벤트 데이터 경로를 안 열어 둬서 RPC 는 PC 에 있다.

⚠ 시셀라 R 자기 피해 하한 100 은 시셀라 R 파생 어빌리티의 일 (HP 코스트 하한 1 과 같은 `min` 처리) → M6.

## AoE 태그를 여기서 붙인다

F03-05 의 흡혈 치유 감소가 `Damage.Shape.AoE` 를 읽는다.
⭐ **광역 판정을 쓰는 스킬이 GE Spec 에 그 태그를 붙인다** — Execution 이 형상을 추측하지 않는다.

## 검증 — `Play As Client, Number of Players: 2`

- **빌드 통과** (Editor + Server)
- ⭐ 레니 W 가 **중앙 1.25m 와 외곽 2.25m 를 다른 효과**로 처리한다
- ⭐ 판정이 **서버에서** 일어난다
- ⭐ **흡혈 회복이 공격자에게 간다** (F03-05 검증 완결 — 다른 액터를 때리므로)
- ⭐ Source / Target 캡처가 뒤바뀌지 않았다 (F03-01 검증 완결)
- 광역 스킬에 `Damage.Shape.AoE` 가 붙어 흡혈이 절반이 된다

## 주의점

- ⭐ **F04 의 판정 함수를 다시 만들지 않는다**
- ⭐ **자체 RPC 구조체를 만들지 않는다** — `FGameplayAbilityTargetData`
- ⚠ 판정 형상 태그(`Damage.Shape.AoE`)를 **어빌리티가** 붙인다
