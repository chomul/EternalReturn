# ✅ F08-03 — 장비 스탯 = Infinite GE ⭐⭐

> Task [`../../1_Task/F08_아이템_인벤토리/03_장비스탯_InfiniteGE.md`](../../1_Task/F08_아이템_인벤토리/03_장비스탯_InfiniteGE.md)

## 선행

- [ ] [02 장착 제한](02_장착제한_무기군.md) 완료
- [x] ⚠ [`../../4_Argument/5_추가공격력_산출방식.md`](../../4_Argument/5_추가공격력_산출방식.md) 을 읽었다 — `UEREquipmentEffect` 클래스 주석에 인용

## 구현

- [x] ⭐⭐ 장비 GE 가 **`Infinite`** 다 — `UEREquipmentEffect` 생성자 `DurationPolicy = Infinite`. 하나를 전 장비가 공유, 모디파이어 = 장비 가능 어트리뷰트 25개 Additive SetByCaller(미사용 0)
- [x] ⭐ 벗을 때 **`RemoveActiveGameplayEffect(Handle)`** 로 제거한다 — `UERInventoryComponent::Unequip`
- [x] ⭐ **수동으로 스탯을 빼지 않는다**
- [x] 인벤토리가 GE 핸들을 보관한다 — `FEREquippedSlot.EffectHandle` (서버). 컴포넌트는 **PlayerState** (Argument 21 — ASC 와 같은 수명)
- [x] 장착 · 해제가 서버 권위다 — `ServerEquip/ServerUnequip` RPC → `Equip/Unequip` (HasAuthority 가드 · `CanEquip` 서버 재판정)

## 빌드

- [x] `EternalReturnEditor` 빌드 통과 — 2026-09-17 에러 0
- [x] `EternalReturnServer` 빌드 통과 — 2026-09-17 에러 0

## 검증 — `Play As Listen Server, Number of Players: 2`

> 절차: [`../../3_EditorTasks/F08_아이템.md`](../../3_EditorTasks/F08_아이템.md) "F08-03" · ⚠ 에디터 재시작

- [x] ⭐ 장착 시 어트리뷰트가 변하고 **클라에 복제**된다 — 클라 창 `Stats` 추가 +20 (2026-09-17)
- [x] ⭐⭐ **벗으면 정확히 원복**된다 — `현재 0.00 / 추가 +0.00`
- [x] ⭐⭐ 장비를 낀 상태에서 **추가 공격력이 장비분만큼** 잡힌다 — 기본 0 / 현재 20 / 추가 +20 (Argument 5 실증)
- [x] 장비 2개 중 하나만 벗으면 나머지는 유지된다 — Defense +10 유지
- [x] ⭐ GE 종류가 `Infinite` 임을 실제로 확인했다 — `Equipped` 출력 `(Infinite)`

## ⭐ 가장 위험한 실수

- [x] ⭐⭐ **`Instant` 로 만들지 않았다** — 만들면 추가 공격력이 조용히 0 이 되고
- [ ]   증상이 *"스킬이 약하다"* 정도로만 보여 원인을 못 찾는다
- [x] ⭐ **수동 차감을 하지 않았다** — 부동소수 오차가 쌓인다
- [x] ⚠ GE 핸들을 잃어버리지 않는다 — 슬롯 구조체가 들고, 교체 시 먼저 Unequip
