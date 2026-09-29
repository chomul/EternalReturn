# 40 — AnimBP 구조: 무기 레이어 인터페이스 · 스레드 세이프 업데이트 · 최적화

> 기능 [F12.5-02](../1_Task/F12.5_연출_파이프라인/02_이동_애니메이션.md) · 선행 [Argument 39](39_몽타주_위치_모션세트.md) · [Argument 36](36_애니메이션_사운드_연출_레이어.md)
> 작성 2026-09-27

## 🗺 한눈에 보기 — 애니메이션 · 연출 구조 (2026-09-28 기준 · Argument 39 + 40)

> 기획자용 요약. 세부 근거는 아래 각 절 · [Argument 39](39_몽타주_위치_모션세트.md). 캐릭터별 작업 표는 [`3_EditorTasks/F12.5_연출.md` › 캐릭터별 작업표](../3_EditorTasks/F12.5_연출.md#캐릭터별-작업표).

### 1. 한 문장

> **캐릭터 = 몸(메시) + 두뇌(메인 AnimBP) + 무기 자세(레이어) + 동작표(연출 DA)** — 스킨은 이 중 "몸" 과 "동작표의 일부 줄" 만 바꾼다.

### 2. 화면의 포즈는 3층으로 쌓인다

```
        ┌─────────────────────────────────────────────────────────────┐
  위 ▲  │ ③ 동작        평타 · D · Q~R · (춤)       ← 버튼을 누를 때만 잠깐 덮는다  │  몽타주 (DefaultSlot)
        │                                            이동하면 끊긴다               │
        ├─────────────────────────────────────────────────────────────┤
        │ ② 무기 자세   대기 / 달리기               ← 든 무기마다 다르다          │  무기 레이어 (ABPL_*)
        │               맨손 · 도끼 · 단검 · 저격총 …   무기를 바꾸면 통째로 교체   │
        ├─────────────────────────────────────────────────────────────┤
  아래  │ ① 이동 판단   지금 멈췄나? 달리나?        ← 속도로 C++ 가 계산          │  메인 AnimBP (ABP_*)
        └─────────────────────────────────────────────────────────────┘
```

- ① 은 캐릭터 공통 (그래프 1벌 = 템플릿 `ABP_ERCharacter_Base`, 계산은 C++ `UERAnimInstance`)
- ② 는 캐릭터 × 무기마다 **애니 2개(대기 · 달리기)만** 다르다 (그래프 1벌 = 템플릿 `ABPL_ERWeapon_Base`)
- ③ 은 표(연출 DA)에서 **"이 캐릭터가 · 이 무기로 · 이 버튼을" → 애니** 를 찾아 재생

### 3. 파일은 이렇게 연결된다 (재키 예)

```
BP_Jackie (캐릭터)
 └ Character Data ─▶ DA_Char_Jackie ─────────────── 스탯 · 스킬 · 들 수 있는 무기
                      ├ Presentation ─▶ DA_Pres_Jackie ──────────── 기본 동작표 (무기 무관)
                      │                  │  춤 · 사망 · (음성)
                      │                  │  AnimLayer = ABPL_Jackie_Unarmed ─── 맨손 자세
                      │                  └ WeaponSets ─┬▶ DA_Pres_Jackie_Axe ───── 도끼 동작표 (평타 · D · Q~E)
                      │                   (필요할 때만   │   AnimLayer = ABPL_Jackie_Axe ─ 도끼 자세
                      │                    불러온다)     ├▶ DA_Pres_Jackie_Dagger    (단검 · 1Hand)
                      │                                 ├▶ DA_Pres_Jackie_TwoHandSword (2Hand)
                      │                                 └▶ DA_Pres_Jackie_DualSword    (Dual)
                      └ Skins ─┬▶ DA_Skin_Jackie_S000 ─── 몸(메시) · 두뇌(ABP_Jackie) · 덮어쓸 줄
                     (고른 것만 ├▶ DA_Skin_Jackie_S001
                      불러온다) └▶ …S005

ABP_ERCharacter_Base (템플릿 · 부모 C++ UERAnimInstance)   ① 이동 판단 + ② 자리 + ③ 자리
 └ ABP_Jackie (자식 · 재키 스켈레톤)
ABPL_ERWeapon_Base (템플릿 · ② 대기/달리기 그래프)
 ├ ABPL_Jackie_Unarmed   IdleAnim = Jackie_Common_wait   RunAnim = Jackie_Common_run
 └ ABPL_Jackie_Axe       IdleAnim = Jackie_Axe_wait      RunAnim = Jackie_Axe_run
```

### 4. 무슨 일이 생기면 무엇이 바뀌나

| 게임에서 | 바뀌는 것 | 누가 | 모두에게 보이나 |
|---|---|---|---|
| 스폰 | 몸 = 고른 스킨 · 자세 = **맨손** | 연출 컴포넌트 | ✅ 각자 자기 화면에서 |
| 무기 장착 · 교체 | 자세 레이어 교체 (맨손 → 도끼) · 동작표 = 도끼 세트 | 연출 컴포넌트 (세트를 그때 불러온다) | ✅ |
| 이동 / 멈춤 | 달리기 ↔ 대기 | C++ `UERAnimInstance` (속도) | ✅ |
| 평타 · 스킬 | 동작표에서 애니를 찾아 재생 (평타는 atk01 ↔ atk02 번갈아 · 공속만큼 빨라짐) | 스킬 → 연출 컴포넌트 | ✅ 서버가 틀고 복제 |
| 동작 중 이동 클릭 | 동작이 끊긴다 (후딜 캔슬) | 이동 명령 | ✅ |
| 스킨 변경 | 몸 교체 · 스킨 전용 줄만 덮어쓰기 (예: 다니엘 S003 Q · 재키 S002 선택 음성) | 연출 컴포넌트 | ✅ |
| 판정 (맞았나) | **애니와 무관** — 스킬 타이머가 정한다 | 스킬 (서버) | — |

### 5. 찾는 순서 — "이 동작은 어떤 애니?"

```
① 스킨의 이 무기 전용 줄   ─ 있으면 이것 (예: 다니엘 S003 가위 Q)
② 스킨의 공통 줄           ─ (예: 스킨 전용 춤 · 음성)
③ 지금 무기 세트의 줄      ─ (예: 도끼 평타 atk01 · atk02)
④ 캐릭터 기본 줄           ─ (예: 공통 춤 · 사망)
⑤ 없음                    ─ 애니 없이 판정만 (게임은 그대로 돈다)
```

### 6. 야생동물은 더 단순하다

```
DA_Wild_Bear ─ Anim Class ─▶ ABP_Bear_01 (자식) ◀─ ABP_ERWildlife_Base (템플릿 · 부모 C++ UERAnimInstance)
             │                IdleAnim = Bear_01_wait · RunAnim = Bear_01_run        대기 ↔ 달리기 + ③ 자리
             └ Presentation ─▶ DA_Pres_Bear_01   평타 = Bear_01_atk01/02 · 사망 = Bear_01_death
변이 · 잠식 = 스켈레톤이 따로 → 자식 ABP 도 따로 (ABP_Bear_Mutant …) · 무기 자세(②) 없음
```

### 7. 이름 규칙 (이름이 맞아야 `ER.Pres.Fill` 이 자동으로 연결한다)

| 종류 | 이름 | 예 |
|---|---|---|
| 캐릭터 동작표 | `DA_Pres_<캐릭터>` · `DA_Pres_<캐릭터>_<무기 enum>` | `DA_Pres_Jackie_Axe` |
| 스킨 | `DA_Skin_<캐릭터>_S0nn` | `DA_Skin_Jackie_S002` |
| 자세 레이어 | `ABPL_<캐릭터>_<무기 enum>` · 맨손 `ABPL_<캐릭터>_Unarmed` | `ABPL_Jackie_Dagger` |
| 메인 AnimBP | `ABP_<캐릭터>` | `ABP_Jackie` |
| 야생동물 동작표 | `DA_Pres_<종폴더>` | `DA_Pres_Bear_Mutant` |
| 무기 enum ↔ 애니 파일 토큰 | Dagger = `1Hand`(재키) · `OneHandSword`(다니엘) / TwoHandSword = `2Hand` / DualSword = `Dual` / SniperRifle = `Snipe` / Shuriken = `Shriken` | |

### 8. 얼마나 채워졌나 — 실험체 애니 420개 (2026-09-28 파일 목록 분류)

| 분류 | 개수 | 지금 | 언제 채우나 |
|---|---|---|---|
| 평타 · D · Q~R(한 동작) · 춤 · 사망 | 69 | ✅ 동작표에 들어감 (`ER.Pres.Fill`) | — |
| 대기 · 달리기 | 35 | ✅ 자세 레이어로 (캐릭터별 작업표 ☐ 진행 중) | F12.5-02 |
| 평타 끝동작 · 장전 (`atk01_End` · `Atk01_End_Reload` · 쌍검 `atk01_1_end` · 다니엘 `critical01`) | 9 | ⬜ | **F12.5-03** 평타 · D |
| 여러 단계 스킬 (`skill03_start/loop/end` · 카티야 `Skill04_Start/Loop/Fire/End` · 저격 D `Sniperrifle_Skill_*`) | 52 | ⬜ 몽타주 애셋(섹션)이 필요 | **F19** 캐릭터별 (저격 D 는 03) |
| 모드 자세 (재키 전기톱 `Saw` · 매그너스 `bike` · 다니엘 가면 `mask_*` · 시셀라 `Empty`) | 60 | ⬜ 모드 태그 → 레이어 교체 | **F19** |
| 상태 (기절 `Down_*` · 사망 `downdead` · 부활 `resurrect`) | 33 | ⬜ | 기절 **F12.5-04** · 빈사 · 부활 **F14** |
| 행동 (채집 · 제작 · 휴식 · 상자 · 낚시 · 함정 설치) | 84 | ⬜ | 채집 **F12.5-04** · 제작 · 상자는 기능이 있으니 04 에 추가 검토 · 휴식 · 낚시 · 함정은 기능이 생길 때 |
| 점프 (하이퍼루프 `Jump_*`) | 56 | ⬜ 레이어에 Jump 함수 추가 | **F13** (하이퍼루프) |
| 로비 · 승리 · 등장 (`Lobby_*` · `Victory` · `arrive`) | 21 | ⬜ | 로비 · 결과 화면 (F14 · F17) |
| 기타 (`Magnus_Common_dance_in`) | 1 | ⬜ 춤 시작 동작 | 04 또는 F17 (이모트) |

**야생동물** (종마다 14개 전후): 평타 `atk01/02` · 사망 `death` ✅ / 대기 · 달리기 `wait · run` ✅ 레이어 대신 자식 AnimBP / 나머지 `appear`(등장) · `beware_loop`(경계) · `dying` · `endbattle`(전투 끝) · `sleep · sleep_start · wake`(잠 — 도먼시 대기와 연결 검토) · `skill01`(동물 스킬) · `dance` ⬜ → 사망 · 쓰러짐 **04** · 동물 스킬 **F12 후속** · 잠 · 등장 · 경계 **F13**(배회 · 스폰 연출)

⭐ 빈 칸이 있어도 게임은 돈다 — 동작표에 없는 동작은 **애니 없이 판정만** (찾는 순서 ⑤). 채울 때는 **이름 규칙을 맞추고 `ER.Pres.Fill`** 을 다시 돌리거나(규칙 추가는 코드 한 줄), 규칙 밖은 DA 에 손으로 한 줄.

---|---|
| 여러 단계 스킬 (시작 · 반복 · 끝) — 재키 R 전기톱 · 카티야 R 등 · 모드 자세 (전기톱 · 오토바이 · 가면) | F19 캐릭터별 |
| 타격 이펙트 · 소리 · 발소리 · 음성 | F12.5-05 |
| 기절 · 사망 포즈 | F12.5-04 |
| 점프(하이퍼루프) 레이어 | 필요할 때 ② 에 함수 추가 |

---

## 결정 요약

| # | 항목 | 결정 |
|---|---|---|
| 0 | 무기별 자세 분리 방식 | ✅ **Animation Layer Interface** (사용자 결정 2026-09-27) |
| 0 | AnimBP 업데이트 | ✅ **Thread Safe Update** (사용자 결정 2026-09-27) — ⑥ N2 로 **C++ 판** (`NativeThreadSafeUpdateAnimation`) |
| ① | 레이어 AnimBP 를 어떻게 만드나 | ✅ (사용자 "추천대로" 2026-09-27) **L2 템플릿 레이어 1개 + 무기별 자식(애니 변수만)** |
| ② | 메인 AnimBP | ✅ **M2 동작 확인** 2026-09-28 (템플릿 자식 `ABP_Jackie` 를 메인으로 · 레이어 Link 성공) · (사용자 "추천대로" 2026-09-27) **M2 공용 메인 로직 1벌 + 캐릭터별은 스켈레톤만** (템플릿을 메인으로 쓸 수 있는지 (미확인) → 안 되면 M1) |
| ③ | 레이어 연결은 누가 | ✅ (사용자 "추천대로" 2026-09-27) **K2 C++ (연출 컴포넌트가 무기 세트의 레이어 클래스를 Link)** |
| ④ | 야생동물 | ✅ (사용자 "추천대로" 2026-09-27) **W2 템플릿 메인 1개 + 종 · 변이별 자식(애니 변수만)** |
| ⑤ | 최적화 항목 | 아래 표 — 적용 / 측정 후 / 안 씀 |
| ⑥ | 애님 인스턴스 부모 (상태 값 계산) | ✅ **N2 C++ `UERAnimInstance`** (사용자 2026-09-27 "N2로 진행") — `Presentation/ERAnimInstance` |

---

## 근거

### 공식 문서 — Animation Optimization

https://dev.epicgames.com/documentation/unreal-engine/animation-optimization-in-unreal-engine (2026-09-27 조회)

- **Thread Safe Update** — "Blueprint Thread Safe Update Animation" 오버라이드 · Thread Safe 함수 · 컴포넌트 값은 **Property Access** 로 읽는다
- **Fast Path** — AnimGraph 에 BP 로직 없이 멤버 변수만 읽는다 · Class Settings › Optimization › **Warn About Blueprint Usage**
- **Parallel Update** — 조건은 `UAnimInstance::NeedsImmediateUpdate` · 루트 모션이 필요하면 병렬 불가
- **URO** (Update Rate Optimizations) — 먼 캐릭터 15Hz 이하 · **Display Debug Update Rate Optimizations** 로 확인 · 대안 **Animation Budget Allocator** 플러그인
- **Component Use Fixed Skel Bounds** — 물리 애셋으로 바운드를 매 프레임 재계산하지 않는다
- 필요 없는 물리 갱신 끄기 · **노티파이는 BP 가 아닌 것** (BP VM 호출 회피) · 노티파이는 게임 스레드

### 엔진 코드

- 병렬 업데이트가 꺼지는 조건 — `RootMotionMode == RootMotionFromEverything` · `bUseMultiThreadedAnimationUpdate` 꺼짐 · 디버깅 중 (`Engine/Private/Animation/AnimInstance.cpp:779-800`)
  → 우리는 루트 모션을 안 쓴다 (이동은 CMC · 판정은 타이머 — Argument 36). AnimBP **Root Motion Mode = No Root Motion Extraction** 으로 확실히 한다

### Lyra

- C++ `ULyraAnimInstance` 는 얇다 — `GameplayTagPropertyMap`(태그 → BP 변수 자동) · `GroundDistance` 하나 (`Lyra/Source/LyraGame/Animation/LyraAnimInstance.h:41-45`). 나머지는 BP
- 무기 레이어 선택은 무기 인스턴스가 한다 — `ULyraWeaponInstance::PickBestAnimLayer` (`Weapons/LyraWeaponInstance.cpp:78`)
- 템플릿 레이어 베이스 + 무기별 자식이 애니 변수만 채우는 구조는 Lyra Content(ABP_ItemAnimLayersBase 계열)에 있다는 게 통설이나 Content 가 없어 **(미확인)**
- ⚠ 전제 차이: Lyra 는 마네킹 한 스켈레톤. 우리는 **캐릭터마다 스켈레톤이 다르다** (Argument 39 R1 · E25) → 레이어 AnimBP 도 캐릭터 × 무기로 늘어난다. 그래서 ①의 "자식은 변수만" 이 더 중요하다

---

## ① 레이어 AnimBP

인터페이스 `ALI_ERWeaponLayers` — 레이어 함수: **Idle** · **Run** (ER 애니는 걷기 없이 wait · run 뿐 — 파일명). ⏸ Jump(하이퍼루프 등) 는 필요할 때 추가.

| 방안 | 방식 | 판정 |
|---|---|---|
| L1 | 캐릭터 × 무기마다 레이어 AnimBP 를 통째로 (`ABPL_Jackie_Axe` 안에 그래프) | 재키만 4벌 · 전원 ~11벌 — 그래프가 복사된다. 블렌드 시간 하나 바꾸면 11곳 |
| **L2** | **템플릿 레이어 `ABPL_ERWeapon_Base`** (ALI 구현 · 그래프는 여기 한 번 · 애니는 변수 `IdleAnim` · `RunAnim`) → 무기별 **자식** `ABPL_Jackie_Axe` 는 **Class Defaults 에서 변수만** | 그래프 1벌. 자식은 스켈레톤 · 애니 2개만 |

**추천 L2.**

## ② 메인 AnimBP

메인이 하는 일: 이동 상태(대기 ↔ 뛰기) → **레이어 호출** → `Slot DefaultSlot` (Argument 39 몽타주) → Output.

| 방안 | 방식 | 판정 |
|---|---|---|
| M1 | 캐릭터마다 메인 AnimBP (`ABP_Jackie` …) — 같은 그래프 6벌 | 단순. 그래프 수정이 6곳 |
| **M2** | **템플릿 메인 `ABP_ERCharacter_Base`** + 캐릭터별 자식 (스켈레톤 지정만) | 그래프 1벌. ⚠ 템플릿 AnimBP 를 **메인 애님 클래스**로 · 자식으로 쓸 수 있는지 (미확인) — 에디터에서 먼저 확인, 안 되면 M1 |

**추천 M2** (확인 실패 시 M1).

## ③ 레이어 연결 (무기 교체 → 레이어 교체)

| 방안 | 방식 | 판정 |
|---|---|---|
| K1 | AnimBP 이벤트 그래프가 무기를 보고 `Link Anim Class Layers` | 로직이 BP 에 (CLAUDE.md §7 위반) · 무기 → 클래스 표를 BP 가 따로 가져야 한다 |
| **K2** | **무기 세트 DA(Argument 39)에 `AnimLayer` 클래스** · 연출 컴포넌트가 무기 세트를 로드 · 해석할 때 `LinkAnimClassLayers` (이전 것은 Unlink) · 스킨이 AnimClass 를 바꾸면 다시 Link | 무기 교체 · 비동기 로드 · 스킨 경로가 **이미 한 곳에** 있다 (F12.5-01). 각 머신이 로컬로 — 새 복제 없음 |

**추천 K2.**

## ④ 야생동물 · 보스

변이 · 잠식은 스켈레톤이 따로다 (Argument 39) → 21개 AnimBP 가 필요하다.

| 방안 | 방식 | 판정 |
|---|---|---|
| W1 | 종 · 변이마다 AnimBP 통째로 | 21벌 복사 |
| **W2** | **템플릿 `ABP_ERWildlife_Base`** (대기 ↔ 뛰기 · Slot · 변수 `IdleAnim` · `RunAnim`) + 자식은 변수만 · 보스는 필요하면 자식에서 확장 | 그래프 1벌 |

**추천 W2.** 레이어 인터페이스는 안 쓴다 (무기가 없다).

## ⑥ 애님 인스턴스 부모 — 상태 값을 누가 계산하나

AnimGraph 가 읽을 값: `GroundSpeed` · `bIsMoving` (02) · 사망 · 기절 · 모드 같은 상태 (04).

| 방안 | 방식 | 판정 |
|---|---|---|
| N1 | 부모 `AnimInstance` · BP **Blueprint Thread Safe Update Animation** 에서 Property Access 로 계산 | BP 만으로 된다. 단 **상태 계산 로직이 BP 에** (CLAUDE.md §7 "상태 전이 · 수치 계산은 C++") · 메인 · 야생동물 두 템플릿에 같은 계산을 두 번 |
| **N2** | **C++ `UERAnimInstance`** (메인 · 야생동물 템플릿의 부모). `NativeUpdateAnimation` 에서 **읽기만**(속도 벡터 복사 · 게임 스레드), `NativeThreadSafeUpdateAnimation` 에서 **계산**(워커 스레드) → `BlueprintReadOnly` 변수. AnimGraph 는 변수를 직접 읽는다 (Fast Path) | 엔진 권장 패턴 그대로 — *"It is usually a good idea to simply gather data in this step and for the bulk of the work to be done in NativeThreadSafeUpdateAnimation"* (`Engine/Classes/Animation/AnimInstance.h:1307-1308`). 변수 이름 · 타입이 C++ 한 곳에 — 템플릿 둘이 공유. 04 의 태그 → 변수는 Lyra 처럼 `GameplayTagPropertyMap` 으로 여기에 붙는다 (`LyraAnimInstance.h:41`) |

- 레이어 템플릿(`ABPL_*`)은 계산이 없다 (애니 변수 2개) → 부모 `AnimInstance` 그대로
- 부모는 나중에 Class Settings › Parent Class 로 바꿀 수도 있지만, **만들 때 고르는 게 싸다** (변수를 BP 에 먼저 만들면 이름이 겹친다)

**추천 N2.** 사용자 결정 "Thread Safe Update 사용" 은 그대로 — C++ 판 Thread Safe Update(`NativeThreadSafeUpdateAnimation`)다.

---

## ⑤ 최적화 항목

| 항목 | 적용 | 어디서 | 근거 |
|---|---|---|---|
| Thread Safe Update | ✅ | **C++ `UERAnimInstance`** (⑥ N2) — 게임 스레드는 속도 복사만, 계산은 `NativeThreadSafeUpdateAnimation` | 공식 문서 · 사용자 결정 · `AnimInstance.h:1307` |
| Fast Path (AnimGraph 로직 없음) + **Warn About Blueprint Usage** | ✅ | 모든 AnimBP Class Settings | 공식 문서 |
| Root Motion Mode = **No Root Motion Extraction** · Use Multi Threaded Animation Update 켜짐 | ✅ | 모든 AnimBP Class Settings | `AnimInstance.cpp:788,799` — 병렬 업데이트 조건 |
| 안 그려지면 몽타주만 (`OnlyTickMontagesWhenNotRendered`) | ✅ 이미 (P1) | C++ | F12.5-01 |
| **URO** (`bEnableUpdateRateOptimizations`) | ✅ 야생동물 · 실험체 | C++ 생성자 (메시) | 공식 문서 — 120~200 마리 (역기획서 §7.3) |
| **Component Use Fixed Skel Bounds** | ✅ | C++ 생성자 (메시) — 바운드용 물리 애셋 불필요 (판정은 캡슐 · HitBox) | 공식 문서 |
| 노티파이는 C++ (BP 노티파이 금지) | ✅ 규칙 | 05 `UERAnimNotify_PlayKey` | 공식 문서 · Argument 39 K2 |
| 메시 물리 · 콜리전 끄기 | ⏸ 측정 후 (06) | 메시 콜리전이 판정 형상(`FindSkillShape`)에 잡히는지 먼저 확인 — 모르고 끄면 판정 거리가 바뀔 수 있다 | 공식 문서 "필요 없는 물리 갱신" |
| Animation Budget Allocator | ✗ 지금은 | URO 로 부족하면 06 에서 | 공식 문서 (대안) |

---

## 네트워크

새 복제 없음. 무기 계열은 이미 복제되는 `Equipped` 에서 (F12.5-01 `OnRep_Equipped`), 이동 속도는 CMC 가 복제. 레이어 연결 · 애님 업데이트는 각 머신 로컬. 데디 서버는 안 그려서 P1 로 그래프 자체가 안 돈다.

## 추천

**L2 · M2(안 되면 M1) · K2 · W2 + ⑤ 표의 ✅.**
