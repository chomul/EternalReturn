# E17 — `EquipDirect` 가 거부될 때 있던 장비가 증발하고 아무도 모른다

> 발견 **2026-09-19** · F11-02 검증 로그에서 (사용자가 "테스트 다 했어" → 로그 대조 중 발견)

## 증상

```
08:40:18 Equip hammer      → 장착 · D · 평타 부여 · 사거리 2.0
08:40:57 Equip bat_t1      → Warning 장착 거부 — 무기군 Bat 를 이 실험체는 못 든다
08:41:13 Stats             → AttackPower 현재 0.00        ← 망치의 +16 이 사라졌다
08:41:17 Unequip Weapon    → (로그 없음)                   ← 이미 무기가 없다
08:41:21 평타               → 나간다                        ← Attack 스펙이 남아 있고 State.Unarmed 도 안 걸렸다
```

## 원인

`UERInventoryComponent::EquipDirect` (디버그 · 초기 장비용) 가 "있던 장비는 버려진다" 를 **먼저** 실행한 뒤 `ApplyEquip` 을 불렀다.
`ApplyEquip` 이 `CanEquip` 에서 거부되면 — 기존 GE 는 이미 제거됐고 `Equipped` 에서도 빠졌는데 **`OnEquippedChanged` 를 쏘지 않아**
F10-04 숙련도 증폭 · F11-02 무기 스킬이 갱신되지 않았다. 게임 경로(`EquipFromBag`)는 `ApplyEquip` 이 먼저 검사하므로 영향 없음.
같이 드러난 것: `Unequip` 이 빈 칸이면 아무 로그 없이 false — "아무 일도 없다" 의 원인을 못 찾는다.

## 수정

- `EquipDirect`: 지우기 **전에** `ERItem::CanEquip` 으로 검사 → 거부면 그대로 반환. 지웠으면 `OnEquippedChanged.Broadcast(Slot)`
- `Unequip`: 빈 칸이면 Warning 로그

## ⚠ 재발 방지

- **상태를 바꾸기 전에 거부 조건을 전부 본다** (F09-02 `Craft` 의 "①~③ 읽기만" 과 같은 규칙). 디버그 경로도 예외 없음
- 장착 배열을 바꾸는 곳은 반드시 `OnEquippedChanged` — 이제 세 곳 (`ApplyEquip` · `Unequip` · `EquipDirect` 제거 · `Craft` 소비) 전부 쏜다
- 실패가 조용하면 안 된다 — false 를 돌려주는 경로마다 로그

- [x] 수정 후 빌드 — 2026-09-19 에러 0
- [x] 재검증 08:57:54 — `Axe_t1 장착 거부` 뒤 `Stats` AttackPower 추가 +16 그대로

## 후속 (같은 세션)
해제 시 맨몸 사거리 복원을 "회수한 스킬 수 > 0" 에 걸어 둬서 **스킬 없는 무기(저격총 행에 D · 평타 없음)를 벗으면 사거리가 6.5 로 남았다.** `bWeaponRangeApplied` 플래그로 판단하게 수정 — 조건은 "무엇을 했었나" 로 걸지 부수 효과의 개수로 걸지 않는다.
