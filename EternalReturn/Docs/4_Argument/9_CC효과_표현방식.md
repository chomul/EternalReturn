# 9. CC 효과를 어떻게 표현할 것인가

> F06-01 · 근거 [`전투판정_상태이상_역기획서.md`](../0_GameDesign/Systems/전투판정_상태이상_역기획서.md) §3

## ⭐ 결정 — 2026-09-10

| 항목 | 결정 |
|---|---|
| CC GE 표현 | ⭐ **방안 A — CC 종류마다 애셋 1개** |
| 지속시간 | ⭐ **`SetByCaller`** (`SetByCaller.CCDuration`) |
| 누락 대응 | 부여 함수 **한 곳**에서 검사 + `UE_LOG(Error)` |

사용자 결정: *"둘 다 A 로 해서 진행하자"*

⚠⚠ **함께 발견된 것 — UE 5.3 에서 GE 의 태그 부여 방식이 바뀌었다.**
`InheritableOwnedTagsContainer` 가 deprecated 되고 **`UTargetTagsGameplayEffectComponent`** 로 옮겨졌다
(`GameplayEffect.h:2285`). 에디터에서 GE 애셋의 "Granted Tags" 칸을 찾으면 **없다.**
→ 자세한 절차는 [`../3_EditorTasks/F06_상태이상.md`](../3_EditorTasks/F06_상태이상.md)

## 결정할 것

**기절 · 속박 · 침묵 같은 CC 를 `UGameplayEffect` 로 만들 때, 애셋으로 둘 것인가 C++ 네이티브 클래스로 둘 것인가.**
그리고 **스킬마다 다른 지속시간을 어떻게 표현할 것인가.**

---

## ⭐ 먼저 — 지속시간이 이 결정을 가른다

역기획서 §3.1 · §3.3 이 실측한 값이다.

| CC | 지속시간 (6인 사례) |
|---|---|
| 기절 | **0.5 ~ 1.3초** |
| 침묵 | 0.5초 (다니엘 R) |
| 둔화 | 0.85 ~ 2초 |

⭐ **같은 "기절"인데 스킬마다 길이가 다르다.**
그래서 "GE 를 애셋으로 두나 코드로 두나" 보다 **"지속시간을 어디서 주나"** 가 먼저다.

### 엔진이 무엇을 허용하나

```cpp
// GameplayEffect.h:2200
FGameplayEffectModifierMagnitude DurationMagnitude;
```

```cpp
// GameplayEffect.h:65-75
enum class EGameplayEffectMagnitudeCalculation : uint8
{
    ScalableFloat,
    AttributeBased,
    CustomCalculationClass,
    SetByCaller,        // ← 스펙을 만드는 쪽이 값을 넣는다
};
```

⭐ **지속시간도 `SetByCaller` 로 받을 수 있다.** 모디파이어 전용이 아니다.
→ **기절 GE 는 애셋 1개면 되고, 길이는 스킬이 넣는다.**

⚠ 이걸 모르면 `GE_CC_Stun_0_5s`, `GE_CC_Stun_1_0s` 처럼 **길이별로 애셋이 불어난다.**

---

## 방안 A — CC 종류마다 애셋 1개 + 지속시간은 SetByCaller ⭐

```
GE_CC_Stun      (HasDuration, DurationMagnitude = SetByCaller "SetByCaller.CCDuration")
GE_CC_Snare
GE_CC_Silence
GE_CC_Disarm
```

스킬 쪽:

```cpp
FGameplayEffectSpecHandle Spec = MakeOutgoingSpec(StunEffect, 1.f, Context);
Spec.Data->SetSetByCallerMagnitude(ERTags::SetByCaller_CCDuration, 0.8f);
```

**장점**

- ⭐ **애셋 수가 CC 종류만큼**이다. 길이별로 늘지 않는다
- 기획자가 CC 의 **성질**(부여 태그 · 정화 가능 여부 · 중첩)을 소유한다
- **C-6 결정과 일관** — 애셋 + C++ 검사기 ([`3_어트리뷰트셋_구조⭐.md`](3_어트리뷰트셋_구조⭐.md) C-6)
- 지속시간이 **스킬 데이터 옆에** 있게 된다. 기절 0.8초는 기절의 성질이 아니라 **그 스킬의 성질**이다

**단점**

- ⚠ **`SetByCaller` 를 빠뜨리면 조용히 깨진다.** 값이 없으면 지속시간이 0 이 되어 CC 가 안 걸린다
  → 부여하는 쪽에서 **검사하고 로그를 남긴다** (C-6 이 정한 방식과 동일)
- Claude 가 애셋을 못 만든다 → `Docs/3_EditorTasks/` 로 넘어간다 (`CLAUDE.md` §9)

---

## 방안 B — 네이티브 GE 클래스

```cpp
UCLASS()
class UERStunEffect : public UGameplayEffect
{
    UERStunEffect();   // 생성자에서 DurationPolicy, GrantedTags 설정
};
```

**장점**

- 빌드가 검증한다. 애셋 검사기가 필요 없다
- `UERLifestealEffect` 와 같은 방식이라 일관된다

**단점**

- ⚠⚠ **`UERLifestealEffect` 와 상황이 다르다.** 흡혈 GE 는 **조정 대상 수치가 0개**여서 네이티브가 맞았다.
  CC 는 **정화 가능 여부 · 중첩 정책 · 부여 태그**가 전부 조정 대상이다
- 기획자가 CC 하나를 추가하려면 **C++ 를 고쳐야 한다**
- ⚠ **C-6 결정을 뒤집는다.** 같은 프로젝트에서 GE 정책이 둘로 갈린다

---

## 방안 C — 스킬마다 GE 애셋을 따로 만든다

`GE_Sissela_E_Stun`, `GE_Daniel_R_Silence` … 지속시간을 애셋에 박는다.

**장점**

- `SetByCaller` 를 안 써도 된다. 빠뜨릴 것이 없다
- 기획자가 스킬 애셋만 보면 길이를 안다

**단점**

- ⚠⚠ **애셋이 스킬 수만큼 늘어난다.** 6인 × 스킬 4~5개 = 30개 내외, 캐릭터가 늘면 비례해서 는다
- CC 의 성질(정화 가능 여부 등)이 **30곳에 복사**된다. 한 곳을 고치면 나머지가 어긋난다
- ⚠ 정화 규칙을 바꾸면 **전부 열어서 고쳐야 한다**

---

## 비교

| 축 | A 애셋+SetByCaller | B 네이티브 | C 스킬마다 애셋 |
|---|---|---|---|
| 구현 난이도 | 중 (검사기 필요) | 하 | 하 |
| 애셋 수 | ⭐ **CC 종류만큼** | 0 | ❌ 스킬 수만큼 |
| 확장성 | ⭐ 높음 | 낮음 (C++ 수정) | ❌ 낮음 (복사본 증가) |
| 네트워크 | 동일 (GE 복제) | 동일 | 동일 |
| GAS 정합성 | ⭐ `SetByCaller` 는 이 용도다 | 정합 | 정합하나 낭비 |
| C-6 과의 일관성 | ⭐ **일치** | ❌ 뒤집음 | 일치 |
| 조용히 깨질 위험 | ⚠ 있음 → **검사로 막는다** | 없음 | 없음 |

---

## ⭐ 추천 — 방안 A

**이유 세 가지.**

1. ⭐ **지속시간은 CC 의 성질이 아니라 스킬의 성질이다.** 같은 기절이 0.5초이기도 1.3초이기도 하다(§3.1).
   길이를 CC 쪽에 두면 A 가 아니라 C 가 되고, 애셋이 스킬 수만큼 늘어난다.
2. **C-6 이 이미 같은 문제를 풀었다** — 애셋 + C++ 검사기. GE 정책을 둘로 가르지 않는다.
3. `UERLifestealEffect` 를 네이티브로 둔 이유(**조정 대상 수치 0개**)가 CC 에는 성립하지 않는다.

### ⚠ A 를 고르면 반드시 함께 할 것

`SetByCaller` 누락은 **지속시간 0** 으로 조용히 나타난다. GAS 는 경고하지 않는다
(`GameplayEffect.cpp:4238` 이 없는 것을 건너뛰는 것과 같은 부류).

→ **CC 를 부여하는 함수 한 곳**에서 값을 검사하고 `UE_LOG(Error)` 를 남긴다.
부여 경로를 한 곳으로 모으는 이유가 이것이다.

---

## 근거

| 주장 | 출처 |
|---|---|
| 지속시간도 `SetByCaller` 로 받을 수 있다 | `GameplayEffect.h:2200` + `GameplayEffect.h:65-75` |
| 기절이 0.5~1.3초로 스킬마다 다르다 | 역기획서 §3.1 |
| 애셋 + 검사기가 이 프로젝트의 GE 정책이다 | [`3_어트리뷰트셋_구조⭐.md`](3_어트리뷰트셋_구조⭐.md) C-6 |
| GAS 는 없는 것을 조용히 건너뛴다 | `GameplayEffect.cpp:4238` |
| Claude 는 애셋을 만들지 않는다 | `CLAUDE.md` §9 |
