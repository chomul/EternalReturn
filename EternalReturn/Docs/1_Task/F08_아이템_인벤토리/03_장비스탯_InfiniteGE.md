# F08-03 — 장비 스탯 = Infinite GE ⭐⭐

> 기능 [F08](00_개요.md) · 근거 [`장비_아이템_역기획서.md`](../../0_GameDesign/Systems/장비_아이템_역기획서.md) · 우선순위 2
> 체크리스트 [`../../2_Checklist/F08_아이템_인벤토리/03_장비스탯_InfiniteGE.md`](../../2_Checklist/F08_아이템_인벤토리/03_장비스탯_InfiniteGE.md)

## 할 일

장비의 스탯을 **`Infinite` GameplayEffect** 로 적용하고, 벗을 때 **GE 를 제거**한다.

## ⭐⭐ 이 Task 가 F02 의 전제를 실증한다

[`../../4_Argument/5_추가공격력_산출방식.md`](../../4_Argument/5_추가공격력_산출방식.md) 결정:

> **추가 공격력 = `EvaluateBonus()` = `Evaluate() - GetBaseValue()`**

| GE 종류 | 바꾸는 것 | 의미 |
|---|---|---|
| `Instant` | **BaseValue** | 기본 공격력 (초기값 · 레벨 성장) |
| `Infinite` ✅ | **CurrentValue 만** | ⭐ **추가 공격력** (장비 · 버프) |

⚠⚠ **장비 GE 를 `Instant` 로 만들면:**
- 장비를 벗어도 **스탯이 안 돌아온다**
- **추가 공격력이 0 이 된다** → `BonusAPRatio` 를 쓰는 스킬이 조용히 약해진다
- 증상이 *"스킬 데미지가 이상하다"* 정도로만 보여 원인을 못 찾는다

⭐ 그 결정 문서가 *"F09(장비) 작업 시 EditorTasks 문서에 '장비 GE 는 Infinite 여야 한다' 를
체크 항목으로 넣는다"* 고 적어 뒀다. **여기서 지킨다.**

## 벗을 때 — 핸들로 제거한다

```cpp
FActiveGameplayEffectHandle Handle = ASC->ApplyGameplayEffectToSelf(...);
// 벗을 때
ASC->RemoveActiveGameplayEffect(Handle);
```

⭐ **수동으로 스탯을 빼지 않는다.** GE 를 제거하면 GAS 가 정확히 원복한다.
직접 빼면 부동소수 오차가 쌓이고, 중첩된 장비에서 어긋난다.

## 검증 — `Play As Client, Number of Players: 2`

- **빌드 통과** (Editor + Server)
- ⭐ 장착 시 서버에서 어트리뷰트가 변하고 **클라에 복제**된다
- ⭐⭐ **벗으면 정확히 원복**된다 (부동소수 오차 없이)
- ⭐⭐ **`ER.Damage.BonusTest` 로 확인한 것과 같은 동작** —
  장비를 낀 상태에서 추가 공격력이 **장비분만큼** 잡힌다
- 장비 2개를 끼고 하나만 벗으면 나머지는 유지된다
- ⭐ 장비 GE 가 **`Infinite`** 다 (`Instant` 아님)

## 주의점

- ⭐⭐ **`Infinite` 여야 한다.** `Instant` 면 추가 공격력 계산이 통째로 깨진다
- ⭐ **수동으로 스탯을 빼지 않는다** — `RemoveActiveGameplayEffect`
- ⚠ 핸들을 잃어버리면 GE 를 못 지운다. 인벤토리가 핸들을 들고 있어야 한다
- 장착·해제는 **서버 권위**
