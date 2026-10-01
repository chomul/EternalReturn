# 57 — 스킬 Shape 구조 단순화

> 발단: 사용자 2026-10-02 "Shape 쪽이 이거 추가되면서 좀 복잡해진 거 같은데 구조 단순하게 · 직관적으로 할 수 있는지"
> 관련: F19 개요 "판정 모양 조각화 검토" (사용자 2026-09-30) · Argument 54 (투사체) · 55 (스킬 JSON)

## 지금 — `FERSkillShape` 한 구조체에 **22칸**, 세 가지 일이 섞여 있다

| 하는 일 | 칸 |
|---|---|
| **어디를** (판정 모양) | `Shape` · `RangeMax` · `RangeMin` · `RadiusOuter` · `RadiusInner` · `ForwardOffset` · `AngleDeg` · `ProjectileRadius` · `TrapezoidNearWidth` · `TrapezoidFarWidth` · `TrapezoidLength` |
| **누구를** (대상 고르기) | `TeamFilter` · `bPlayersOnly` · `MaxTargets` · `AimAssistRadius` · `bPenetrate` |
| **어떻게 보내나** (발사 방식) | `ProjectileCount` · `SpreadAngleDeg` · `ProjectileSpeed` · `ProjectileClass` · `ShotInterval` · `ShotCancelDistance` |

헷갈리는 이유
- 모양마다 쓰는 칸이 다른데 한 줄에 다 보인다 (EditCondition 이 일부만)
- "발사 방식" 이 모양 안에 있다 — `Projectile` 모양 + 속도 0 = 즉시 선 판정 · 속도 > 0 = 투사체 · `Trapezoid` + 속도 = 순차 유도탄 … **조합으로 뜻이 바뀐다**
- 같은 뜻이 다른 이름: 사거리 `RangeMax` 가 모양에 따라 "반경" · "길이" · "조준점 당기는 거리" 로 쓰였다 (E35 가 이걸로 났다)
- 원점 규칙(시전자 / 조준점)이 코드 안에 숨어 있다 (`GroundCircle` · `Trapezoid` 만 조준점)

## 방안

### S0 — 칸은 그대로, 보이는 것만 정리
Category 로 셋으로 묶고, 모양마다 안 쓰는 칸은 EditCondition 으로 숨긴다.
- 장점: 데이터 · 코드 · 애셋 안 바뀜 · 반나절
- 단점: "조합으로 뜻이 바뀌는" 문제 그대로 · JSON 도 그대로 한 줄

### S1 — 구조체 셋으로 나누기 ⭐
```cpp
FERSkillShape    Shape;     // 어디를: Type · Origin(Caster/AimPoint) · Range(조준 사거리) · Size 칸들 (반경 · 폭 · 길이 · 각도 · 앞 띄움)
FERSkillTargets  Targets;   // 누구를: Team · bPlayersOnly · MaxTargets · AimAssist
FERSkillDelivery Delivery;  // 어떻게: Mode = Instant | Projectile | Sequential  +  Speed · Class · Count · Spread · Interval · CancelDistance · bPierce
```
- 모양에서 **원점 규칙을 칸으로** (`Origin = Caster | AimPoint`) — 코드에 숨은 분기 제거
- 발사 방식은 **Mode 하나로** — "속도 > 0 이면 투사체" 같은 암묵 규칙 제거
- `Projectile` 모양은 사라진다 → 모양 `Line`(폭 · 길이) + Delivery `Instant / Projectile`
- 장점
  - 에디터 · JSON 이 "어디 / 누구 / 어떻게" 세 덩어리로 읽힌다 — 카티야 R = `Shape(Trapezoid, AimPoint)` + `Targets(Players, 3)` + `Delivery(Sequential, 40m/s)`
  - 새 실험체 스킬도 셋을 고르는 것으로 끝 (매그너스 Q = `Line` + `Projectile` · 레니 Q = `Line` + `Projectile` + 폭발은 조각)
  - 추상 클래스 · 상속 없음 (구조체 셋) — CLAUDE §2
- 단점
  - **칸 경로가 바뀐다** → 손으로 넣은 기존 스킬 DA (무기 D 10여 개 · 야생동물 15여 개 · 테스트) 의 모양 값이 날아간다 → **옛 칸을 `_DEPRECATED` 로 남기고 `PostLoad` 에서 새 칸으로 옮긴다** (언리얼 표준 이관 · 한 번 저장하면 끝)
  - 판정 · AI · 미리보기 · 임포터가 읽는 곳을 고친다 (`Shape.` 사용처 ~14곳 + MakeTargetQuery · ERTargeting)
  - 카티야 JSON 3개 다시 씀

### S2 — 모양을 Instanced 클래스로 (모양마다 클래스)
`UERShape_Circle` · `_Cone` · `_Line` · `_Trapezoid` … 각자 칸 + `Query()` + `Draw()`. 발사 방식도 클래스.
- 장점: 고른 모양의 칸만 보인다 (가장 직관적) · 새 모양 = 새 클래스 (enum 분기 없음)
- 단점: 클래스 8개 + 발사 3개 · 임포터가 중첩 Instanced 를 다뤄야 · 이관 S1 보다 무겁다 · 모양 종류는 6인 기준 거의 다 있다 (F04 표) — **늘어날 일이 적은데 구조만 커진다**

### S3 — **늘어나는 축(어디를 · 어떻게)만 클래스**, 누구를은 구조체 ⭐ (2026-10-02 추가)
> 사용자 2026-10-02 "앞으로 모양이 더 다양하게 생길 수 있잖아" · "모양 말고 다른 것도 늘어날 수 있음 · **어떻게**가 다양해질 수도"

```cpp
UPROPERTY(EditDefaultsOnly, Instanced) TObjectPtr<UERSkillShape>    Shape;     // 어디를 — 클래스: Circle · Cone · Line · DualRadius · Trapezoid · SingleTarget … (각자 칸 + Query() + Draw() + 원점 규칙)
UPROPERTY(EditDefaultsOnly)            FERSkillTargets              Targets;   // 누구를 — 구조체: Team · bPlayersOnly · MaxTargets · AimAssist (거의 안 늘어난다)
UPROPERTY(EditDefaultsOnly, Instanced) TObjectPtr<UERSkillDelivery> Delivery;  // 어떻게 — 클래스: Instant · Projectile · Sequential(유도 순차) … (각자 칸 + Deliver())
```
- **새 모양 = 파일 하나** (칸 · 판정 · 미리보기 · 원점이 한 클래스) · **새 발사 방식 = 파일 하나** (예: 되돌아오는 부메랑 · 튕기는 탄 · 설치 후 폭발 · 시셀라 윌슨 던지기)
- 에디터: 클래스를 고르면 **그 클래스 칸만** 보인다 — 조각(Fragment)과 같은 사용법 · JSON 도 조각처럼 `{ "Class": …, "Props": … }`
- 장점
  - 늘어나는 두 축이 enum 분기 없이 늘어난다 — 지금 사다리꼴 하나 넣는 데 4곳(enum · 판정 분기 · 그리기 분기 · 칸)을 고쳤다
  - "속도 > 0 이면 투사체" · "Trapezoid + 속도면 순차" 같은 **숨은 조합 규칙이 사라진다** — 발사 방식은 고른 클래스가 곧 뜻
  - 늘지 않는 축(누구를)은 구조체 그대로 — 필요 없는 추상화를 안 만든다 (CLAUDE §2)
- 단점
  - 클래스: 모양 6~7 + 발사 3 · 베이스 2 · 임포터가 **칸 하나 자리의 Instanced** 도 읽게 (조각 코드 재사용)
  - 이관: 옛 칸 `_DEPRECATED` → `PostLoad` 에서 모양 · 발사 객체를 만들어 옮김 — S1 보다 조금 무겁다
  - AI 사거리 · 투사체 · 장판 · PlayerCircles(오메가) 처럼 모양을 읽는 곳이 모양 클래스에 물어보게 바뀐다
  - 2~3일

### S3.1 — S3 다듬기 (외부 검토 · 사용자 제공 2026-10-02) ✅ 받아들임
1. **`Sequential` 은 발사 방식 클래스가 아니라 `Projectile` 의 칸** — `FirePattern = Single | Simultaneous | Sequential` + `Count` · `Spread` · `Interval` · `bHoming`. 조합마다 클래스(`SequentialHomingProjectile` …)가 늘어나는 걸 막는다. 정말 다른 동작이 되면 그때 클래스로 승격
2. **모양 · 발사 객체에 실행 상태 금지** — DA 안의 설정 객체라 여러 캐릭터 · 동시 시전이 같이 쓴다. 현재 발 · 타이머 · 대상은 어빌리티 · 투사체 액터가 든다 (지금 R 도 그렇다). `Deliver(...) const`
3. **쓰는 쪽은 구체 클래스로 Cast 하지 않는다** — `GetMaxReach()` · `BuildQuery()` · `DrawPreview()` · `Validate()` 만. AI · 미리보기 · 판정이 `Cast<UERShape_Circle>` 하기 시작하면 enum switch 를 cast switch 로 바꾼 것뿐
4. 원점은 모양 베이스의 공통 칸 **`ShapeOrigin`** (Caster | AimPoint) — 투사체의 발사 위치와 헷갈리지 않게 이름 구분
5. `SingleTarget` 은 "어디" 보다 "누구 하나" 에 가깝다 — 지금은 모양으로 두고, LockOn · Nearest 같은 게 늘면 그때 네 번째 축(대상 획득)으로
6. 이상한 조합은 **`IsDataValid()`** (엔진 Data Validation — 저장 · 일괄 검사) 로 막는다 — 모양 · 발사 없음 · 투사체 클래스 없음 등
7. JSON 은 `"Type": "Trapezoid"` 논리 이름 → 임포터가 클래스로 (C++ 이름이 바뀌어도 JSON 유지) — 조각의 `Class` 와 같이 받을지는 구현 때
8. `FInstancedStruct` 는 "다형 데이터만" 일 때 — 우리는 동작(Query · Draw)까지 타입에 묶으니 Instanced UObject
9. 이관은 **데이터 버전** (`ShapeDeliveryRefactor`) 으로 명시 + 옛 칸 → 새 칸 이관 로그 (옛 암묵 규칙 "속도 > 0" 을 옮기는 거라 단순 이름 바꿈보다 위험)

## 비교

| | S0 | S1 | S2 | S3 ⭐ |
|---|---|---|---|---|
| 구현 난이도 | 반나절 | 1~2일 (이관 포함) | 3일+ | 2~3일 |
| 직관성 | △ (보이기만) | ✓ 세 덩어리 · Mode 하나 | ✓✓ 클래스별 | ✓✓ 모양 · 발사 클래스별 |
| 확장성 (모양 · 발사가 늘 때) | ✗ 조합 규칙 늘어남 | △ enum 분기 늘어남 | ✓✓ | ✓✓ (누구를은 구조체) |
| 기존 애셋 | 그대로 | PostLoad 이관 | 이관 무거움 | PostLoad 이관 |
| 네트워크 · GAS · 성능 | 무관 | 무관 | 무관 (가상 호출 조금) | 무관 (가상 호출 조금) |

## 추천

**S3.** (2026-10-02 갱신 — 처음엔 S1 추천) 늘어나는 건 **모양**과 **발사 방식** 두 축이다 (사용자). 둘을 클래스로 두면 새 모양 · 새 발사가 파일 하나로 끝나고 숨은 조합 규칙이 사라진다. 늘지 않는 "누구를" 까지 클래스로 만드는 S2 는 과하다. 조각(Fragment)과 같은 사용법이라 에디터 · JSON 도 이미 익숙한 형태다.

### S3 이면 순서 (S1 과 거의 같다)
1. 모양 · 발사 베이스 클래스 + 기존 7 · 3 종 + 누구를 구조체 + 옛 칸 `_DEPRECATED` + `PostLoad` 이관 (값 · 뜻 그대로 옮김 — "속도 > 0" → Mode 등)
2. 읽는 곳 고치기 (판정 · AI 사거리 · 미리보기 · 투사체 · 장판 · 임포터 — 칸 하나 자리 Instanced)
3. 빌드 → 에디터에서 **모든 스킬 DA 열어 저장** (한 번 — 명령 `ER.Skill.Resave` 로) → 로그로 이관 대조
4. 카티야 JSON 새 형식 · 리임포트 · Q · E · R 회귀 테스트 + 무기 D 하나 · 야생동물 스킬 하나

## 근거와 출처

- `Source/EternalReturn/GAS/ERSkillData.h` `FERSkillShape` (22칸)
- `Source/EternalReturn/GAS/ERGameplayAbility.cpp` `BuildQuery` (원점 분기) · `ExecuteSkill` (속도 > 0 · Trapezoid 분기)
- E35 (사거리 칸이 반경으로 쓰인 사고) · Argument 54 · 55

---

## ✅ 결정: S3.1 (사용자 2026-10-02 "2번 먼저 · 카티야 완성 뒤 사운드") — 착수 계획 (승인 대기)

### 1. 구조

| 새것 | 책임 |
|---|---|
| `UERSkillShape` (Abstract · EditInlineNew · DefaultToInstanced) | **어디를** — 공통 칸 `ShapeOrigin`(Caster · AimPoint) · `AimRange`(조준점 당기는 거리) · `MinReach` · 가상 `Query(문맥) → 결과` · `GetMaxReach()`(AI 사거리) · `DrawPreview()` · `IsSingleTarget()`(광역 판정) · `Validate()` · **상태 없음 · const** |
| `UERShape_Single` · `_Circle`(반경 · 앞 띄움) · `_DualCircle`(안 · 밖) · `_Cone`(길이 · 각도) · `_Line`(길이 · 폭) · `_Trapezoid`(길이 · 가까운 폭 · 먼 폭) · `_PlayerCircles`(반경 — 시전 시작 때 자리 저장) | 기존 7종을 클래스로. 계산은 지금 `ERTargeting` 함수를 그대로 부른다 (수학 재사용 — 판정 결과 동일) |
| `FERSkillTargets` (구조체) | **누구를** — `Team` · `bPlayersOnly` · `MaxTargets`(비관통 직선 = 1) · `AimAssistRadius` |
| `UERSkillDelivery` (Abstract · Instanced) | **어떻게** — 가상 `Deliver(어빌리티, 스킬, 질의) const` · `Validate()` · 상태 없음 (현재 발 · 타이머는 어빌리티 람다 · 투사체 액터가) |
| `UERDelivery_Instant` | 지금의 즉시 판정 — 부채꼴 여러 줄 `FanCount` · `FanAngle` (위클라인 트리플렛) |
| `UERDelivery_Projectile` | `FirePattern` = Single · Simultaneous(부채) · Sequential(대상마다 한 발씩) · `Speed` · `Radius` · `bPierce` · `bHoming` · `ProjectileClass` · `Interval` · `CancelDistance` · `Count` · `Spread` |
| `UERSkillData` | `Area`(모양) · `Targets` · `Delivery` + **`IsDataValid()`** (모양 · 발사 없음 · 투사체 클래스 없음 · Sequential 인데 MaxTargets 0 …) |

- ⚠ 칸 이름: 새 모양 칸은 **`Area`** — 옛 `Shape`(구조체)는 그대로 두고 `DeprecatedProperty` (이름이 같으면 옛 데이터 리다이렉트가 새 칸과 충돌). 전부 이관 · 저장 확인 뒤 옛 칸 삭제는 따로
- 쓰는 쪽은 **구체 클래스로 Cast 하지 않는다** — `Area->Query / GetMaxReach / DrawPreview` · `Delivery->Deliver` 만 (S3.1 ③)
- `ERTargeting` · `FTargetQuery` 는 아래층 계산 함수로 남긴다 (모양 클래스가 부른다)

### 2. 네트워크

- **바뀌는 것 없음** — 판정 · 발사 · 피해 전부 서버 (Deliver 는 서버에서만) · 투사체 복제 그대로 · 미리보기는 소유 클라에서 `Area->DrawPreview`

### 3. 세부

- **이관** — `FCustomVersion` (`ShapeDeliveryRefactor`) · `PostLoad` 에서 버전이 낮으면 옛 `Shape` → 새 칸:

  | 옛 | 새 Area | 새 Delivery | Targets |
  |---|---|---|---|
  | SingleTarget | Single(사거리 · 최소) | Instant | AimAssist |
  | SelfRadius | Circle(Caster · 반경=RangeMax · 앞 띄움) | Instant | |
  | GroundCircle | Circle(AimPoint · AimRange=RangeMax · 반경=RadiusOuter) | Instant | |
  | DualRadius | DualCircle(안 RadiusInner · 밖 RangeMax · 앞 띄움) | Instant | |
  | Cone | Cone(길이 RangeMax · 각도) | Instant | |
  | Projectile · 속도 0 | Line(길이 RangeMax · 폭 2×반경) | Instant(FanCount=ProjectileCount · FanAngle=Spread) | 비관통이면 MaxTargets 1 |
  | Projectile · 속도 > 0 | Line(…) | Projectile(Single / 여러 발이면 Simultaneous · 속도 · 클래스 · bPierce=관통) | |
  | Trapezoid | Trapezoid(AimPoint · AimRange · 길이 · 폭) | Projectile(Sequential · bHoming · 간격 · 취소 거리) | MaxTargets · bPlayersOnly |
  | PlayerCircles | PlayerCircles(사거리 · 반경) | Instant | |

  → 이관 로그 `[스킬 이관] DA_… 옛 Shape=… 속도=… → Area=… Delivery=… Pattern=…` (한 줄씩)
- **일괄 저장** — 에디터 명령 `ER.Skill.Resave` (모든 스킬 DA 를 불러 이관 → 저장 · 이관 로그 · `IsDataValid` 결과)
- **임포터** — JSON `"Area": { "Type": "Trapezoid", "Props": {…} }` · `"Delivery": { "Type": "Projectile", "Props": {…} }` (S3.1 ⑦ 논리 이름 → `UERShape_<Type>` · `UERDelivery_<Type>`) · `Katja.json` 새 형식으로
- **광역 판정** (흡혈 감소) — `Area->IsSingleTarget()` 이거나 `Targets.MaxTargets == 1` 이거나 비관통 투사체 → 단일 (지금 규칙과 같은 결과)

### 4. 검증

| 단계 | 성공 기준 |
|---|---|
| 빌드 | Editor · Server 에러 0 |
| 이관 | `ER.Skill.Resave` — 모든 스킬 DA 이관 줄 · 저장 · `IsDataValid` 에러 0 · 이관 로그를 표로 옮겨 사람이 대조 |
| 회귀 (PIE) | 카티야 Q(거리 보간) · E(뒤로 · 벽 넘기) · R(스캔 3 · 3발) · P / 무기 D 하나(저격 또는 도끼) / 야생동물: 곰 강타(원 · 앞 띄움) · 위클라인 트리플렛(즉시 부채 3줄) · 가스(장판) · 오메가 VF(PlayerCircles) · 알파 아크 블레이드 — **이관 전과 같은 판정 줄** |
| 미리보기 | Q · E · R 누르는 동안 모양이 이관 전과 같다 |
| 확장 확인 | 모양 · 발사를 읽는 쪽 코드에 `Cast<UERShape_` 가 **0** (grep) |

### 5. 순서
1. 베이스 · 7 + 2 클래스 · Targets · IsDataValid → 빌드
2. 이관 (CustomVersion · PostLoad · Resave 명령) → 빌드
3. 쓰는 곳 바꾸기 (판정 · 발사 · 미리보기 · AI · 장판 · 피해 · 평타 · 투사체) → 빌드
4. 임포터 (Type 객체) · `Katja.json` 새 형식 → 빌드
5. 에디터: `ER.Skill.Resave` → `ER.Skill.ImportJson Katja` → 회귀 PIE
