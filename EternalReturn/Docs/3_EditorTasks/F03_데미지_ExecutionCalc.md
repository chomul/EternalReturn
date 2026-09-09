# ✅ F03 — 데미지 ExecutionCalculation · 에디터 작업 및 테스트

> **2026-09-08 검증 완료** — 사용자 확인: *"테스트 완료했어 오류 없음"*
> ✅ 2-1 ~ 2-5 검증 완료 (F03-02 · F03-03).
> ✅ 2-6 (치명타 · 채널 분기) 검증 완료.
> ✅ 2-7 (흡혈) 검증 완료.
> ✅ 2-8 (체력 비례 피해 · 보스 감쇠) 검증 완료. **② 테스트 전 항목 완료.**

> Task [`../1_Task/F03_데미지_ExecutionCalc/00_개요.md`](../1_Task/F03_데미지_ExecutionCalc/00_개요.md)
> 관련 결정 [`../4_Argument/5_추가공격력_산출방식.md`](../4_Argument/5_추가공격력_산출방식.md)

## 목적

끝나면 **피해 계산식이 숫자로 맞는지 확인된 상태**가 된다.
방어력 감산 `100/(100+DEF)`, 고정 피해 채널, 추가 공격력 경로가 의도대로 도는지
콘솔 커맨드로 검증한다.

## 선행 조건

- [x] C++ 빌드 통과 (Editor + Server)
- [x] ⚠ **에디터 재시작.** 게임플레이 태그 `Data.Damage.*` 4개가 새로 추가됐다
- [x] F02 의 에디터 작업이 끝나 있을 것 (`DA_TestDummy`, `GE_ERInitStats`, `BP_ERCharacterBase` 연결)

---

# ① 에디터 작업

## **없음.**

⭐ 이 단계에서 만들 애셋이 없다. 검증용 GE 를 **디버그 코드가 런타임에 만든다**
(`ERDamageDebug.cpp` 의 `MakeDamageEffect()`).

> 왜 애셋을 안 만드나: 이 GE 는 임시 검증용이라 `Content/` 에 흔적을 남기지 않는다.
> **실제 스킬용 GE 애셋은 F07 에서** 스킬별로 만든다.

⚠ 사용자가 지금 에디터에서 할 일은 **재시작뿐**이다. 태그 4개가 반영돼야 한다.

---

# ② 테스트 — PIE 콘솔로 확인만 한다

## 준비

- [x] **Play (Selected Viewport), Number of Players: 1**
  ⚠ `Play As Client` 로 하면 안 된다. **Execution 은 서버에서만** 돌기 때문에
  클라 창에서 치면 `"서버가 아니다"` 로그가 나온다
- [x] `` ` `` (백틱) 으로 콘솔을 연다

## 2-1. ⭐ 방어력 공식 — 정수로 떨어지는 4가지

각 줄을 콘솔에 그대로 치고, Output Log 의 `[피해테스트]` 를 본다.

| # | 명령 | 기대 로그 |
|---|---|---|
| 1 | `ER.Damage.Test 200 100` | `기대=100.00 실제=100.00 \| 일치` |
| 2 | `ER.Damage.Test 200 0` | `기대=200.00 실제=200.00 \| 일치` |
| 3 | `ER.Damage.Test 200 100 true` | `기대=200.00 실제=200.00 \| 일치` ⭐ 고정 피해는 방어력 무시 |
| 4 | `ER.Damage.Test 200 300` | `기대=50.00 실제=50.00 \| 일치` |

- [x] 1번 — 방어력 100 에서 **정확히 절반**
- [x] 2번 — 방어력 0 이면 감산 없음
- [x] 3번 — ⭐ **고정 피해가 방어력을 무시**한다
- [x] 4번 — 방어력 300 에서 1/4

**성공 판정:** 네 줄 모두 `| 일치` 로 끝난다.
**실패로 보이는 것:** `*** 불일치 ***` 가 있으면 그 아래 `Error` 줄이 확인 순서를 알려준다.

## 2-2. 음수 방어력에서 크래시하지 않는다

- [x] `ER.Damage.Test 200 -50`

**성공 판정:** 크래시 없이 `기대=200.00 실제=200.00 | 일치`
(음수 방어력은 `FMath::Max(Defense, 0.f)` 로 0 취급 — 추가 피해로 바뀌지 않는다)

## 2-3. ⭐ 추가 공격력 경로 — 이번에 새로 정한 방식

[`../4_Argument/5_추가공격력_산출방식.md`](../4_Argument/5_추가공격력_산출방식.md) 의 전제를 실제로 시험한다.

- [x] `ER.Damage.BonusTest 100 50 2.0`

**성공 판정:**

```
[추가공격력] 기본=100.0 추가=50.0 총합=150.0 | 계수=2.00 | 기대=100.00 실제=100.00 | 일치
```

읽는 법:
- **총합이 150** → `Infinite` GE 가 `CurrentValue` 에 +50 을 얹었다
- **피해가 100** → `EvaluateBonus()` 가 **50**(추가분만)을 돌려줬고 × 2.0 이 됐다
- ⭐ 만약 피해가 **300** 이면 총합(150)을 쓴 것이고, **0** 이면 Bonus 가 0 이다

- [x] 계수를 바꿔서도 확인: `ER.Damage.BonusTest 100 50 1.0` → 기대 **50**
- [x] 추가 공격력 0: `ER.Damage.BonusTest 100 0 2.0` → 기대 **0**

⚠ 이게 틀리면 **`Instant` / `Infinite` 구분 전제가 깨진 것**이다.
그 경우 Argument 5 의 방안 2(`BaseAttackPower` 어트리뷰트 추가)로 되돌려야 한다.

## 2-4. 경로 전체가 이어졌는지

위 커맨드들은 Execution 만 보는 게 아니라 **`IncomingDamage` → 체력 차감(F02-04)** 까지 탄다.

- [x] 로그의 `체력 X -> Y` 줄에서 체력이 실제로 줄었다
- [x] 두 번 연속 실행해도 **매번 같은 양**만 줄어든다 (Meta 가 안 남는다 — F02-04 검증)
  ```
  ER.Damage.Test 200 100
  ER.Damage.Test 200 100
  ```
  ⭐ 두 번 다 `실제=100.00` 이어야 한다. 두 번째가 200 이면 `SetIncomingDamage(0.f)` 가 안 된 것

## 2-5. ⭐ 방어 관통 (F03-03)

`ER.Damage.Test <원시> <방어력> false <퍼센트관통> <고정관통>`

⚠ **퍼센트 관통은 `0~1` 이다.** `50` 이 아니라 `0.5` — 100배 차이가 난다.

| # | 명령 | 적용 방어력 | 기대 피해 |
|---|---|---|---|
| 1 | `ER.Damage.Test 200 100 false 0.5 30` | **20** | 166.67 |
| 2 | `ER.Damage.Test 200 100 false 0 0` | 100 | 100.00 |
| 3 | `ER.Damage.Test 200 100 false 0.5 999` | **0** (음수 아님) | 200.00 |
| 4 | `ER.Damage.Test 200 0 false 0.5 30` | **0** | 200.00 |
| 5 | `ER.Damage.Test 200 100 false 1.0 0` | **20** (상한 0.8) | 166.67 |
| 6 | `ER.Damage.Test 200 100 true 0.5 30` | — | 200.00 (고정 피해) |

- [x] 1번 — ⭐ **순서 검증.** 로그의 `적용방어력` 이 **20** 이어야 한다
  ⚠ **35 가 나오면 순서가 뒤집힌 것**이다 (고정을 먼저 뺐다: `(100-30)×0.5`)
- [x] 2번 — 관통 없음, F03-02 와 같은 결과
- [x] 3번 — 고정 관통 과다에서 **0 으로 멈춘다** (음수 방어력이 추가 피해가 되지 않는다)
- [x] 4번 — 방어력 0 에서 관통을 걸어도 0
- [x] 5번 — ⭐ **관통 상한 0.8** 이 걸린다. 로그의 `관통=80%` 확인 (`100%` 로 안 나온다)
- [x] 6번 — 고정 피해 채널에서는 관통이 무의미하다
- [x] 어느 경우에도 **피해가 원시값(200)을 넘지 않는다**

### ⭐ 대상 방어력이 변하지 않는지 — 자동 검사된다

관통을 Target 어트리뷰트에 GE 수정자로 걸면 **다른 공격자의 피해까지** 깎인 방어력으로 계산된다.
디버그 커맨드가 매번 검사한다.

- [x] 로그에 `대상 방어력 100.00 그대로 (관통이 어트리뷰트를 안 건드렸다)` 가 나온다
- [x] `⚠ 대상 방어력이 ... 변했다` **Error 가 없다**

---

## 2-6. ⭐ 치명타 · 채널 분기 (F03-04)

⚠ 이 절은 **밸런스의 뼈대**다. §7 4순위의 검증 기준이 *"스킬 채널에서는 어떤 경우에도 치명타가 발생하지 않음"* 하나다.

### 준비 — 스탯을 먼저 세팅한다

```
ER.Damage.SetStat CritChance 1
ER.Damage.SetStat CritDamageUp 0
```

⚠ `CritChance` 는 상한 1.0 이다. `2` 를 넣으면 `클램프되어 1.000` 로그가 나온다 — 정상이다.

### ⭐ 채널별 치명타

방어력 0 으로 두면 원시값이 그대로 나와 배율을 눈으로 본다.

| # | 명령 | 기대 | 왜 |
|---|---|---|---|
| 1 | `ER.Damage.Test 200 0 basic` | **350** | 200 × 1.75 |
| 2 | `ER.Damage.Test 200 0 skill` | **200** | ⭐ 스킬에는 치명타가 **절대** 안 붙는다 |
| 3 | `ER.Damage.Test 200 0 true` | **200** | ⭐ 고정 피해도 안 붙는다 |

- [x] 1번 — 기본 공격 + 확률 100% → **1.75배**
- [x] 2번 — ⭐ **스킬 + 확률 100% → 치명타 없음**
- [x] 3번 — ⭐ **고정 피해 + 확률 100% → 치명타 없음**

### 확률 0%

```
ER.Damage.SetStat CritChance 0
ER.Damage.Test 200 0 basic
```

- [x] 항상 **200** (치명타 없음)

### ⭐ 태그 누락 경고

```
ER.Damage.Test 200 0 none
```

- [x] Output Log 에 **Warning** 이 나온다:
  `[데미지] ... 에 피해 채널 태그가 없다. Damage.Type.BasicAttack / Skill / True 중 하나가 필요하다.`
- [x] 그리고 **스킬로 취급**되어 치명타가 안 붙는다 (피해 200)

⚠ 이 경고가 없으면 나중에 스킬 GE 에 태그를 빠뜨렸을 때 **조용히 통과**한다.

### 확률 표본

```
ER.Damage.SetStat CritChance 0.5
ER.Damage.CritSample 1000
```

- [x] `설정=50.0% 관측=4X.X%` 정도로 나온다 (정확히 50 일 필요는 없다)
- [x] `ER.Damage.SetStat CritChance 1` → `ER.Damage.CritSample 200` → **관측 100%**
- [x] `ER.Damage.SetStat CritChance 0` → `ER.Damage.CritSample 200` → **관측 0%**

⚠ 0% / 100% 에서 어긋나면 `Error` 로그가 난다.

### 증감 단계 (7 · 8 · 1 · 10)

```
ER.Damage.SetStat CritChance 0
ER.Damage.SetStat DamageUp 0.5
ER.Damage.Test 200 0 skill
```

- [x] 피해 **300** (200 × 1.5)
- [x] `ER.Damage.SetStat DamageDown 0.2` 추가 → **260** (200 × 1.3)
- [x] `ER.Damage.SetStat SkillDamageDown 0.5` 추가 → **130** (260 × 0.5)
- [x] ⭐ `ER.Damage.Test 200 0 true` → **200** — 고정 피해는 위 증감을 **전부 무시**한다

⚠ 마지막 줄이 F03-04 에서 고친 부분이다. 예전에는 방어력만 우회했다.

### 끝나면 스탯을 되돌린다

```
ER.Damage.SetStat DamageUp 0
ER.Damage.SetStat DamageDown 0
ER.Damage.SetStat SkillDamageDown 0
ER.Damage.SetStat CritChance 0
```

- [x] 되돌렸다 (또는 PIE 재시작)

---

## 2-7. ⭐ 흡혈 (F03-05)

⚠ **이 커맨드는 자기 자신을 때린다.** 흡혈 회복이 같은 HP 에 섞여 들어와서
"회복량" 만 따로 볼 수 없다. 로그가 **기대 순감소**를 계산해 주므로 그걸 본다.

```
[피해테스트] ... | 기대=200.00 실제=160.00 | ...
[흡혈] 흡혈률=20% 치유감소=0% | 기대회복=40.00 (피해 200.00 기준) | 순감소=160.00
[흡혈]   기대 순감소 = 200.00 - 40.00 = 160.00
```

⭐ **`기대 순감소` 와 `실제` 가 같으면 통과.** 회복이 안 갔으면 `실제` 가 200 으로 나온다.

### 준비

```
ER.Damage.SetStat CritChance 0
ER.Damage.SetStat OmniLifesteal 0.2
ER.Damage.SetStat Lifesteal 0
ER.Damage.SetStat HealAmp 0
```

### 모든 피해 흡혈 (`OmniLifesteal`)

| # | 명령 | 치유 감소 | 기대 회복 | 기대 순감소 |
|---|---|---|---|---|
| 1 | `ER.Damage.Test 200 0 skill` | 0% | 40 | **160** |
| 2 | `ER.Damage.Test 200 0 skill+aoe` | 50% | 20 | **180** |
| 3 | `ER.Damage.Test 200 0 skill+wild` | 60% | 16 | **184** |
| 4 | `ER.Damage.Test 200 0 skill+aoe+wild` | ⭐ **60%** | 16 | **184** |
| 5 | `ER.Damage.Test 200 0 true` | 0% | 40 | **160** |

- [x] 1번 — 단일 대상 · 플레이어
- [x] 2번 — ⭐ 광역이면 회복이 **절반**
- [x] 3번 — 야생동물이면 회복이 **40%**
- [x] 4번 — ⭐⭐ **겹쳐도 가장 높은 60% 하나만.** 3번과 **같은 값**이어야 한다
  ⚠ 곱셈 중복이면 순감소가 **192** 가 된다 (회복 8). 그러면 규칙이 틀린 것
- [x] 5번 — ⭐ **고정 피해에도 흡혈이 붙는다** (`OmniLifesteal` = "모든 유형")

### 기본 공격 전용 흡혈 (`Lifesteal`)

```
ER.Damage.SetStat OmniLifesteal 0
ER.Damage.SetStat Lifesteal 0.2
```

| # | 명령 | 기대 회복 | 기대 순감소 |
|---|---|---|---|
| 6 | `ER.Damage.Test 200 0 basic` | 40 | **160** |
| 7 | `ER.Damage.Test 200 0 skill` | ⭐ **0** | **200** |
| 8 | `ER.Damage.Test 200 0 true` | ⭐ **0** | **200** |

- [x] 6번 — 기본 공격에서는 붙는다
- [x] 7번 — ⭐ **스킬에는 안 붙는다** (기본 공격 전용)
- [x] 8번 — 고정 피해에도 안 붙는다

### 합산 확인

```
ER.Damage.SetStat Lifesteal 0.2
ER.Damage.SetStat OmniLifesteal 0.1
ER.Damage.Test 200 0 basic
```

- [x] 기대 회복 **60** (0.2 + 0.1 = 30%), 순감소 **140**
  ⭐ 기본 공격에서는 **둘 다** 적용된다

### `HealAmp` (회복량 증폭)

```
ER.Damage.SetStat HealAmp 0.5
ER.Damage.Test 200 0 basic
```

- [x] 기대 회복이 **1.5배**가 된다 (60 → 90)
  ⭐ `IncomingHealing` 메타를 거치므로 `UERAttributeSet` 한 곳에서 곱해진다

### 피해 0

- [x] `ER.Damage.SetStat Lifesteal 0` 후 `ER.Damage.Test 0 0 basic` → 회복 **0**, 크래시 없음

### 끝나면 되돌린다

```
ER.Damage.SetStat Lifesteal 0
ER.Damage.SetStat OmniLifesteal 0
ER.Damage.SetStat HealAmp 0
```

- [x] 되돌렸다 (또는 PIE 재시작)

### ⚠ 이 커맨드로 증명 못 하는 것

- [x] ⬜ **"회복이 공격자에게 간다"** — 공격자와 피격자가 같은 액터라 구분되지 않는다.
  **F04 타게팅으로 다른 액터를 때릴 수 있게 된 뒤**에 다시 본다 (F07)

---

## 2-8. ⭐ 체력 비례 피해 · 보스 감쇠 (F03-06)

⚠ **에디터 재시작 필요** — 태그 4개(`Actor.Type.Boss`, `Data.Damage.MaxHPRatio` / `CurHPRatio` / `LostHPRatio`)가 추가됐다.

### 준비

```
ER.Damage.SetStat CritChance 0
ER.Damage.SetStat Lifesteal 0
ER.Damage.SetStat OmniLifesteal 0
ER.Damage.SetStat DamageUp 0
ER.Damage.SetStat DamageDown 0
ER.Damage.SetStat SkillDamageDown 0
```

⚠ 커맨드는 **자기 자신**을 때린다. 그래서 "대상 최대 체력" 과 "자신 최대 체력" 이 같다.
`ER.Damage.Test` 는 체력을 `max(원시, 1000) × 2` 로 채우므로, 원시 0 을 주면 **최대 체력 2000 · 현재 2000** 이 된다.

### ⭐ 대상 최대 체력 비례 + 보스 감쇠

```
ER.Damage.SetProp 0.08 0 0
```

| # | 명령 | 비례 성분 | 기대 피해 |
|---|---|---|---|
| 1 | `ER.Damage.Test 0 0 skill` | 2000 × 8% = **160** | **160** |
| 2 | `ER.Damage.Test 0 0 skill+boss` | 160 × 0.5 = **80** | **80** |

- [x] 1번 — 일반 대상은 8% 그대로
- [x] 2번 — ⭐ **보스는 절반**. `Project Settings > Game > ER Stat Caps > Boss Proportional Damage Scale` 이 0.5

### ⭐⭐ 감쇠가 일반 피해에 곱해지지 않는지 — 가장 위험한 실수

```
ER.Damage.SetProp 0 0 0
ER.Damage.Test 200 0 skill+boss
```

- [x] 피해 **200** 이 그대로 나온다
  ⚠ **100 이 나오면 감쇠가 일반 피해에도 곱해진 것**이다. 보스가 모든 피해를 절반만 받는 다른 게임이 된다

```
ER.Damage.SetProp 0.08 0 0
ER.Damage.Test 200 0 skill+boss
```

- [x] 피해 **280** (일반 200 + 비례 160×0.5=80). ⚠ 180 이면 일반 피해에도 곱해진 것

### 「최대」와 「현재」가 구분되는지

체력이 가득 찬 상태(2000/2000)에서는 둘이 같다. **먼저 체력을 깎아서 구분한다.**

```
ER.Damage.SetProp 0 0 0
ER.Damage.Test 1000 0 skill          ← 체력을 2000 -> 1000 으로 만든다
ER.Damage.SetProp 0.10 0 0
ER.Damage.Test 0 0 skill             ← 최대 체력 2000 × 10%
```

- [x] **200** 이 나온다 (최대 체력 기준)

```
ER.Damage.SetProp 0 0.10 0
ER.Damage.Test 0 0 skill             ← 현재 체력 기준
```

- [x] ⭐ **200 보다 작게** 나온다 (현재 체력이 최대보다 적으므로)
  ⚠ 같은 값이 나오면 최대/현재가 뒤바뀐 것이다

### 자신이 잃은 체력 비례

```
ER.Damage.SetProp 0 0 2.0
ER.Damage.Test 0 0 skill
```

- [x] 잃은 체력 × 2 만큼 피해가 들어간다
  ⚠ 체력이 가득 차 있으면 잃은 체력이 0 이라 피해도 0 이다. 먼저 깎아 두어야 한다

### 고정 피해에는 비례 성분이 안 붙는다

```
ER.Damage.SetProp 0.08 0 0
ER.Damage.Test 200 0 true
```

- [x] 피해 **200** (비례 성분 없음)
  ⚠ 계산식 §2.2 의 2 단계가 3·3-b 보다 앞이라 그렇다. **(미확인)** 항목이다 — 실험체를 붙일 때 다시 본다

### 끝나면 되돌린다

```
ER.Damage.SetProp 0 0 0
```

- [x] 되돌렸다 (또는 PIE 재시작)

---

---

# ⬜ 아직 못 하는 검증

| 항목 | 언제 |
|---|---|
| 실제 스킬로 피해가 나가는 것 | F07 |
| ⭐ **스킬 GE 에 채널 태그가 제대로 붙는지** (회귀 항목) | F07 |
| ⭐ **흡혈 회복이 피격자가 아니라 공격자에게 가는지** | F07 (다른 액터를 때릴 수 있을 때) |
| 치명타 여부를 UI 에 알리기 (`FGameplayEffectContext` 상속) | F17 / 연출 |
| 고정 피해가 공격력 계수를 타야 하는가 **(미확인)** | 실험체 추가 시 |
| E04 사망 알림 중복 · E05 클라 권위 | 아래 참고 |

## E04 · E05 는 이 커맨드로 부분 확인이 된다

- [ ] **E04** — 체력을 0 으로 만든 뒤 한 번 더 때린다
  ```
  ER.Damage.Test 100000 0
  ER.Damage.Test 100000 0
  ```
  ⭐ 사망 관련 로그(듣는 쪽이 붙은 뒤)가 **한 번만** 나와야 한다.
  ⚠ 지금은 `OnOutOfHealth` 를 듣는 쪽이 없어 육안 확인이 안 된다 — **F14(사망 처리) 이후**로 미룬다

---

## 주의점

- ⚠ **`Play As Client` 로 시험하지 않는다.** Execution 은 서버 전용이다
- ⚠ 이 커맨드는 **방어력·최대체력·체력을 강제로 덮어쓴다.** 시험 후 값이 이상하면 PIE 를 다시 시작한다
- ⚠ `ER.Damage.BonusTest` 는 `Infinite` 버프를 걸고 **떼지 않는다.** 연속 실행하면 버프가 쌓인다 —
  총합 로그로 확인하고, 깨끗한 상태가 필요하면 PIE 재시작
- ⚠⚠ **`ERDamageDebug.cpp` 는 임시 파일이다.** F07 에서 통째로 삭제한다

## 끝나면

- `Docs/2_Checklist/F03_데미지_ExecutionCalc/02_핵심계산_방어력공식.md` 의 **수치 검증** 6항목이 채워진다
- F03-03(방어 관통)으로 넘어갈 수 있다
