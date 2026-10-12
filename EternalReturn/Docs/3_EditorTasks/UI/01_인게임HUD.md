# 01 인게임 HUD — 에디터 작업 · 위젯 설계

> 레퍼런스: `C:\Sung Unreal Project\Eternal Return\UI Image\인게임.png` (1919×1079 스크린샷 · 원작 인게임 · 1일차 낮 02:11)
> 공통 규칙: [`00_UI_공통원칙.md`](00_UI_공통원칙.md) — 바인딩 금지 · Invalidation · Volatile · Tick 끄기 · 텍스처 임포트 · 9-slice · 네이밍은 **여기서 다시 쓰지 않는다.** `공통 §번호` 로 링크한다
> 원작 HUD 규칙: [`../../0_GameDesign/Systems/UI_HUD_역기획서.md`](../../0_GameDesign/Systems/UI_HUD_역기획서.md) §2 · §3 · §9
> 기능 번호: **F17 UI · HUD** (`Docs/1_Task/00_기능목록_및_의존성.md:69` — ⬜ 미작성)

**목적:** 이 문서대로 하면 C++ 가 없어도 **인게임 HUD 의 위젯 BP 껍데기(배치 · 스타일 · 이미지)** 가 완성되고,
나중에 C++ 베이스(`UER~Widget`)가 생기면 **Reparent 만으로** 실제 게임 데이터가 이벤트로 흘러 들어오게 된다.
이 문서는 ① 이미지 분해 ② 위젯 트리 · 이름 ③ 영역별 데이터 출처 · 갱신 방식 · 최적화 지정 ④ 에디터 작업 순서 ⑤ C++ 가 나중에 할 일을 분리해 적는다.

**선행 조건**
- ⚠ **C++ 위젯 코드가 아직 하나도 없다.** `Source/EternalReturn` 에 `UUserWidget` 파생 · `AHUD` 파생 없음 (grep). `EternalReturn.Build.cs:13` 에 `UMG` `Slate` `SlateCore` 없음 — 공통 §0
- 그래서 지금 만드는 WBP 의 부모는 **임시로 `UserWidget`** 이다. C++ 베이스가 생기면 `File > Reparent Blueprint` 로 바꾼다 (§6 ① 7단계)
- ⚠ 이름이 중요하다. 나중에 C++ 가 `meta=(BindWidget)` 로 **WBP 안 위젯 이름을 그대로** 찾는다. §2 의 이름을 **글자 하나 다르지 않게** 쓴다 (공통 §3)
- 에디터 재시작: 지금 단계는 불필요. C++ 베이스가 추가되면 필요할 수 있다 (CLAUDE.md §4)

---

## 0. 표기

| 기호 | 뜻 |
|---|---|
| ✅ | 소스에 값 **과** 클라 이벤트가 있다 — 바로 연결 가능 |
| ⚠ | 값은 복제되지만 **클라 이벤트가 없다** (`OnRep` 안이 로그뿐 등) — C++ 에 델리게이트 한 줄 추가가 필요 |
| ❌ 미구현 | 소스에 **값 자체가 없다** — 해당 기능(F13 · F14 · F15 …)이 먼저 |
| (미확인) | 이미지에서 뜻을 확정 못 함 / 근거 문서에 없음 |

모든 "데이터 출처"는 `Source/EternalReturn/...:라인` 을 2026-10-12 에 grep 해 확인했다.

---

## 1. 이미지 분해

### 1.1 배치도 (스크린 좌표 대략값 · 1919×1079 기준)

```
 y=0 ┌───────────────────────────────────────────────────────────────────────────────────┐
     │                [A 매치 정보 바]  ☁ ≋ │1일차☀│ 02:11 │♥│ 1 TEAMS / 👤 ?   [B TK0 K0 A0]│
     │                                 ⏱ 20                          [C 목표장비 P ▣▣▣▣▣ ]│
     │                                                                     1/4 1/4 0/4…  │
     │      (R 132s 곰)       (S 상자 마커 ○ 여러 개 — 월드)                               │
     │                         (Q 네임플레이트 "Chomul" Lv1 ▮▮▮▮)                          │
     │ [O 접기 ^]                                                                         │
     │  번역                                                          [D 지역 재료 패널]   │
     │                                                                절 탐색 재료 ▣▣▣▣▣ │
     │ [O 시스템 메시지 3줄]                                           채집/사냥 ▣ ▣       │
     │                         [K 버프 1칸]                          ┌───────────────────┐│
     │                         [J 스킬 강화 포인트 +1  ◆◆◆◇◇]        │ E 미니맵 "절" ⚙   ││
     │        ┌F 스탯 8┬G 장비5┬H 초상┬I Q W E R ?  D F G ┬N 가방 1~5│                   ││
     │        │ 58  0  │ ▫     │ (Lv1)│  ▣ ▣ ▣ ▣ ▣  ▣ ▣ ▣ │  ▣ ▫ ▫ ▫ ▫│                   ││
     │        │ 0|0% 61│ ▫     │      │ L HP 890/890  M B 20 V│ 6~0 ▫▫▫▫▫│                   ││
 y=1079[P 00:09 0ms]──┴───────┴──────┴─────────────────────┴──────────┴───────────────────┘│
       x=0                    x≈300          x≈660            x≈1090   x≈1650        x=1919
```

- 하단 덩어리(F~N)는 **x≈300~1450 · y≈960~1079 의 한 띠**다. 이미지에서 배경 판이 하나로 이어져 있다 → 하나의 하단 패널로 묶는다 (§2)
- 월드 위 표시(Q · R · S)는 **HUD 가 아니라 월드 위젯**이다 (§3.Q~S)

### 1.2 영역 목록 — 이미지에 실제로 있는 것만

| ID | 영역 | 이미지에서 보이는 것 | 위치 |
|---|---|---|---|
| A | 매치 정보 바 | 날씨 아이콘 2개(구름 · 바람) · "1일차" + 해 아이콘 · **02:11** · 녹색 하트 아이콘 · "**1** TEAMS" · 사람 아이콘 "?" · 그 아래 시계 아이콘 + 주황 "20" | 상단 중앙 |
| B | 처치 수 | `TK 0  K 0  A 0` | 우상단 |
| C | 목표 장비 진행 | "P" 키 + 전환 버튼 · 장비 아이콘 5칸 · 각 칸 아래 `1/4` `1/4` `0/4` `0/4` `1/4` | 우상단 |
| D | 지역 재료 패널 | "절 탐색 재료" 제목 · 재료 아이콘 + `0/2` 여러 칸 · 체크된 칸 · "채집/사냥" 제목 · 아이콘 + `0/2` | 우측 중간 |
| E | 미니맵 | 지역 이름 "절" · 아이콘 버튼 4개(톱니 포함) · 지도 · 지역명("골목길" "경찰서" "소방서" "절" "개울") · 마커들 · 자기 위치 원 | 우하단 |
| F | 스탯 패널 | 8개: `58` · `0` · `0 \| 0%` · `61` · `0.6` · `0 (0%)` · `0%` · `3.73` (각각 아이콘 붙음) | 하단 좌 |
| G | 장비 5칸 | 스탯과 초상 사이의 **세로 작은 칸 5개** | 하단 좌 |
| H | 초상 | 실험체 얼굴 원형 · 아래 레벨 원 "**1**" · 우상단 파란 육각 배지 | 하단 |
| I | 스킬바 | `Q` `W` `E` `R` 아이콘(아래 레벨 눈금 `— — —`) · **글자 없는 5번째 아이콘** · 붉은 `D` `F` `G` · G 아래 `0/15` | 하단 중앙 |
| J | 스킬 강화 알림 | "스킬 강화 포인트 +1" · 노란 마름모 3개(밝음) + 2개(어두움) · 노란 세로 하이라이트 3줄 | 스킬바 바로 위 |
| K | 버프 칸 | 스킬바 위 왼쪽 끝 분홍 아이콘 1칸 | 스킬바 위 |
| L | 체력 바 | 녹색 바 `+890 / 890` | 스킬바 아래 |
| M | 크레딧 · 버튼 | `B` + 크레딧 아이콘 `20` · `V` + 순환 화살표 아이콘 | 체력 바 오른쪽 |
| N | 가방 10칸 | 2줄 × 5칸 · 칸마다 키 숫자 `1~5` `6~0` · 1번 칸에 수박 아이콘 ×3 + 녹색 `+` | 하단 중앙 우 |
| O | 채팅 · 시스템 메시지 | 노란/하늘색 3줄(공정 플레이 안내 · 상품 안내) · "번역" 버튼 · 위 화살표 접기 버튼 | 좌측 |
| P | 시간 · 핑 | `00:09` · `0ms` | 좌하단 |
| Q | 네임플레이트 (월드) | "Chomul" · 레벨 칸 "1" · 칸으로 나뉜 녹색 체력 바 | 캐릭터 머리 위 |
| R | 야생동물 재생성 (월드) | 곰 얼굴 원형 아이콘 · "132s" · 아래 녹색 작은 점 | 월드 바닥 |
| S | 상자 마커 (월드) | 원 안 상자 아이콘 + 아래로 내린 선 · 여러 개 | 상자 위 |

**이미지에 없어서 이 문서가 다루지 않는 것:** 팀원 3인 프레임 · 킬 어나운스 · 금지구역 예고 · 기력(VP) 바 · 제작 창 · 핑.
(UI_HUD 역기획서 §2 에는 있다. 다른 스크린샷이 오면 추가한다.)
- ⚠ VP 바가 **안 보인다.** 원작 실험체 중 VP 를 안 쓰는 경우인지, 이 화면에서만 숨겨진 것인지 **(미확인)**. 우리 소스에는 `VP` · `MaxVP` 어트리뷰트가 있다 (`GAS/ERAttributeSet.h:180-186`) → §3.L 에 **Collapsed 자리만** 둔다

### 1.3 이미지에서 뜻을 확정 못 한 것

| 위치 | 보이는 것 | 상태 |
|---|---|---|
| A | 날씨 아이콘 2개 | (미확인) 날씨 시스템 — 역기획서에 없음 |
| A | 녹색 하트 아이콘 | (미확인) 부활 가능 여부 표시로 보이나 근거 없음 |
| A | 사람 아이콘 + "?" | (미확인) 생존 인원 / 시야 밖이라 모름? |
| A | 시계 + "20" (주황) | (미확인) 다음 금지구역까지 / 다른 카운트다운 |
| H | 파란 육각 배지 | (미확인) 무기 숙련도 / 특성 |
| I | 글자 없는 5번째 아이콘 | (미확인) 패시브(P)로 보인다. 우리 `Ability_Slot_P` 는 있다 (`GAS/ERGameplayTags.h:140`) |
| I | `F` · `G` 슬롯 · `0/15` | (미확인) F = 전술 스킬. G 는 뜻 모름. ⚠ 역기획서 §9 는 **F 슬롯을 컷**했다 |
| M | `V` + 순환 아이콘 | (미확인) |
| P | `00:09` | (미확인) 경과 시간 / 다른 것 |
| C | `1/4` | (미확인) 목표 장비 하나에 필요한 재료 중 보유 수로 보인다 |

→ 위 항목은 **레이아웃 자리만 만들고 데이터 연결은 하지 않는다.** 뜻이 정해지면 이 표를 고친다.

---

## 2. 위젯 트리 · 이름

공통 §1.1 의 `WBP_ER_HUD` 아래에 붙는다. 네이밍은 공통 §1.3 (`WBP_ER_~` / `UER~Widget`).
**굵은 이름 = `BindWidget` 대상** — WBP 안 위젯 이름을 이대로 짓는다.

```
WBP_ER_HUD                                  (C++ 부모: UERHUDWidget · 루트 · AddToViewport 1회)
└─ CanvasPanel  RootCanvas                  ← Canvas 는 여기 하나만 (공통 §4.5)
   ├─ WBP_ER_MatchInfo        MatchInfo      [A] 상단 중앙 앵커
   ├─ WBP_ER_KillScore        KillScore      [B] 우상단 앵커
   ├─ WBP_ER_RouteGoals       RouteGoals     [C] 우상단 앵커
   ├─ WBP_ER_AreaMaterials    AreaMaterials  [D] 우측 앵커
   ├─ WBP_ER_Minimap          Minimap        [E] 우하단 앵커
   ├─ WBP_ER_SystemMessage    SystemMessage  [O] 좌측 앵커
   ├─ WBP_ER_NetInfo          NetInfo        [P] 좌하단 앵커
   ├─ WBP_ER_SkillUpPrompt    SkillUpPrompt  [J] 하단 중앙 앵커 (스킬바 위)
   ├─ WBP_ER_BuffBar          BuffBar        [K] 하단 중앙 앵커 (스킬바 위)
   ├─ WBP_ER_Bag              Bag            [N] 하단 중앙 앵커 — ⭐ 판 밖 (배경 없음 · 칸마다 반투명 바탕) · 2026-10-12 사용자 승인으로 BottomPanel 에서 분리
   │  └─ UniformGridPanel (5열 × 2행) → WBP_ER_ItemSlot × 10 (BagSlot0 ~ BagSlot9)
   └─ WBP_ER_BottomPanel      BottomPanel    [F~M] 하단 중앙 앵커 (판 783×117 — 레이아웃 표)
      └─ Overlay
         ├─ Image  BottomPanelBg             (9-slice 판 1장)
         └─ HorizontalBox
            ├─ WBP_ER_StatPanel    StatPanel        [F]  └─ UniformGridPanel (2열 × 4행) → WBP_ER_StatRow × 8
            ├─ VerticalBox         EquipColumn      [G]  └─ WBP_ER_ItemSlot × 5  (EquipWeapon · EquipHead · EquipChest · EquipArm · EquipLeg)
            ├─ WBP_ER_Portrait     Portrait         [H]
            └─ VerticalBox
               ├─ WBP_ER_SkillBar  SkillBar         [I]  └─ HorizontalBox → WBP_ER_SkillSlot × 8 (SlotQ · SlotW · SlotE · SlotR · SlotP · SlotD · SlotF · SlotG)
               └─ HorizontalBox
                  ├─ WBP_ER_VitalBar  VitalBar      [L]
                  └─ WBP_ER_CreditBox CreditBox     [M]

(월드 — HUD 트리 밖, UWidgetComponent 로 액터에 붙는다)
WBP_ER_Nameplate          [Q]  실험체 머리 위
WBP_ER_WildlifeRespawn    [R]  야생동물 재생성 자리
WBP_ER_LootMarker         [S]  상자 위
```

### 2.1 재사용 단위

| 위젯 | 쓰이는 곳 | 개수 | 왜 나누나 (공통 §1.2) |
|---|---|---|---|
| `WBP_ER_SkillSlot` | I | 8 | 같은 모양 반복 · 쿨다운 이벤트가 슬롯마다 따로 |
| `WBP_ER_ItemSlot` | N 가방 · G 장비 · C 목표 장비 · D 재료 | 10 + 5 + 5 + n | **아이콘 + 등급 테두리 + 수량 텍스트**가 전부 같다. 옵션으로 키 글자 · `n/m` 텍스트를 켠다 |
| `WBP_ER_StatRow` | F | 8 | 아이콘 + 숫자 한 줄 반복 |
| `WBP_ER_BuffIcon` | K | 가변 (미리 N칸) | 아이콘 + 남은 시간 + 중첩 수 |

- `WBP_ER_ItemSlot` 하나를 4곳에서 쓰려고 **표시 옵션은 디자이너 변수로** 둔다: `bShowKeyLabel`(N 의 `1~0`) · `bShowRatio`(C · D 의 `1/4`) · `bCompact`(G 의 작은 칸). 이 셋은 **Instance Editable 표시 옵션**일 뿐 로직이 아니다 (CLAUDE.md §7)
- ⚠ 장비 칸(G)과 가방 칸(N)은 크기가 다르다. 같은 WBP 를 쓰되 바깥 `SizeBox` 없이 **부모 슬롯 크기**로 정한다 — Size Box 는 여러 패스가 든다 (공통 §4.5)

---

## 레이아웃 표 (1920×1080 기준)

레퍼런스 `인게임.png` 를 Python(PIL) 으로 **픽셀 경계를 직접 재서** 만든 표다 (배경 판의 밝기 경계 · 아이콘 외곽 · 글자 bounding box). 눈대중 아님.

### 기준과 환산

| 항목 | 값 | 근거 |
|---|---|---|
| 이미지 해상도 | **1919×1079** (픽셀 0~1918 · 0~1078) | PIL `Image.size` |
| 1920×1080 환산 | 배율 1920/1919 = **1.0005** · 종횡비 1.7785 vs 1.7778 — 차이 **1px 미만**이라 측정값을 **그대로** 쓴다. 단, 오른쪽 · 아래 끝은 이미지 끝(1919 · 1079)을 화면 끝(1920 · 1080)으로 본다 | 계산 |
| UMG DPI 스케일 | `UIScaleRule=ShortestSide` · 곡선 `1080 → 1.0` (`480→0.444` `720→0.666` `8640→8.0`) — 프로젝트 `Config/` 에 덮어쓴 값 없음 (grep 0건) | `Engine/Config/BaseEngine.ini:1287-1288` |
| 그래서 | 이 스크린샷은 DPI 1.0 → **잰 픽셀 = UMG 단위**. 가로 화면은 짧은 변이 세로라 **가상 높이는 언제나 1080** → 세로 앵커(위/아래)는 어느 해상도에서도 같은 자리에 온다. **달라지는 건 가로 폭**(16:10 → 1728 · 21:9 → 2520) → 가로 앵커(좌/중/우)가 중요하다 | 위 곡선에서 도출 |
| 글자 크기 | Slate `RenderDPI = 96` → UMG Font Size `N` = 글자 em 높이 `N×96/72` px. **UMG Size ≈ em px × 0.75**. em 은 숫자·영문 높이 ÷ 0.7, 한글 높이 ÷ 0.9 로 추정 → 전부 **(근사)** · 폰트 모양에 따라 ±1~2 | `SlateCore/Public/Fonts/SlateFontInfo.h:15` |

**앵커 · Offset 부호 규칙 (Canvas Panel Slot)** — `Position` 은 **앵커 점에서 위젯의 Alignment 점까지**의 거리다. +X 오른쪽 · +Y 아래.
→ 우측 앵커(Anchor X=1)면 화면 안쪽이 왼쪽이라 **X 가 0 또는 음수**, 하단 앵커(Anchor Y=1)면 **Y 가 0 또는 음수**. Alignment 를 앵커와 같은 모서리(우하단 앵커 → Alignment (1,1))로 두면 Position 이 곧 "화면 끝에서 띄운 거리(부호만 음수)"가 된다.
- 아래 표의 앵커는 전부 **점 앵커**(Minimum = Maximum) — 늘어나는 위젯이 없다
- 원본 픽셀 박스 `(x, y, w, h)` 는 이미지 좌상단 기준 · `x,y` 포함 · `w,h` 는 개수

### 루트 `RootCanvas` 직속

| 위젯 | 앵커 (Min = Max) | Alignment | Position (X, Y) | Size (X × Y) | ZOrder | 원본 픽셀 박스 (x, y, w, h) |
|---|---|---|---|---|---|---|
| `MatchInfo` [A] | 상단 중앙 (0.5, 0) | (0, 0) | (**−233**, 0) | 550 × 76 | 10 | (727, 0, 549, 76) — 사선 띠 y 0~47 + 아래 시계 줄 y 58~73 |
| `KillScore` [B] | 우상단 (1, 0) | (1, 0) | (**−16**, 18) (근사) | Size To Content | 10 | 글자 (1768, 23, 135, 14) — 줄 높이 여백 때문에 Y 는 글자 위보다 5px 위 (근사) |
| `RouteGoals` [C] | 우상단 (1, 0) | (1, 0) | (**−9**, 57) | 357 × 52 | 10 | (1553, 57, 357, 51) — P 키 · 전환 버튼 · 칸 5개 · `n/4` |
| `AreaMaterials` [D] | 우측 중앙 (1, 0.5) | (1, 0) | (0, **+92**) | 264 × 165 | 10 | (1655, 632, 264, 165) — 판 위 632 · 아래 ≈796 (근사) |
| `Minimap` [E] | 우하단 (1, 1) | (1, 1) | (0, 0) | 263 × 268 | 10 | (1656, 811, 263, 268) — 머리줄 811~838 · 지도 839~1078 |
| `SystemMessage` [O] | 좌측 중앙 (0, 0.5) | (0, 0) | (0, **−16**) | 514 × 263 | 5 | (0, 524, 514, 263) — 접기 버튼 524 ~ 메시지 판 아래 787 |
| `NetInfo` [P] | 좌하단 (0, 1) | (0, 1) | (16, **−8**) (근사) | Size To Content | 10 | 글자 (16, 1056, 87, 11) |
| `SkillUpPrompt` [J] | 하단 중앙 (0.5, 1) | (0, 1) | (**−302**, **−117**) | 272 × 100 | 20 | (658, 863, 271, 99) — 아래 끝이 하단 패널 위(962)에 붙는다 |
| `BuffBar` [K] | 하단 중앙 (0.5, 1) | (0, 1) | (**−302**, **−221**) | 252 × 28 (8칸 기준 · 근사) | 20 | 보이는 1칸 (658, 830, 28, 28) |
| `BottomPanel` [F~N] | 하단 중앙 (0.5, 1) | (0, 1) | (**−660**, 0) | 1145 × 117 | 0 | (300, 962, 1145, 117) — 판 300~1082 + 가방 1089~1444 |

- ZOrder 는 **자체 결정값** — 하단 패널 0 < 일반 10 < 패널 위에 겹치는 J · K 20 · 시스템 메시지 5 (월드 위 문구라 다른 HUD 아래). 이미지에서 겹침 순서를 판정할 수 있는 곳은 없다 (미확인)
- `MatchInfo` 는 띠 전체 중심(x≈1001)이 아니라 **시간 `02:11` 중심이 화면 정중앙 960** 이다 (글자 925~994). 그래서 Alignment (0,0) + X −233 (= 727 − 960) 으로 띠 왼쪽 끝을 맞춘다. 내부에서 시간 텍스트 중심은 띠 왼쪽에서 233px
- `AreaMaterials` 는 문서 §2 의 "우측 앵커" 를 따랐다. 위 해설대로 가상 높이가 늘 1080 이라 우하단 앵커 (1,1) + Position (0, −283) 와 결과가 같다 — 미니맵 위에 붙어 있는 관계(간격 14px)를 강조하려면 그쪽도 된다
- `SystemMessage` 의 메시지 판 위쪽 경계는 그라데이션이라 **(미확인)** — 글자 첫 줄 727 부터 판 아래 787 은 확실
- ⚠ **하단 패널 배경판은 x 300~1082 까지만 있다.** 가방(N, x 1089~1444)은 **판 밖 · 배경 없음**(월드가 그대로 보임 — 밝기 프로파일 x=1083 에서 판 색이 끊긴다). §2 트리처럼 `Overlay[BottomPanelBg + HorizontalBox(…Bag)]` 로 두면 판이 가방 뒤까지 늘어난다 → `BottomPanelBg` 를 판 크기(783 × 117)로 막으려면 Overlay 슬롯 HAlign = Left + 별도 크기 지정이 필요 **(결정 필요 · 미확인)**. 이 표는 트리를 바꾸지 않는다
  - ✅ **결정 (사용자 2026-10-12 "그렇게 진행")**: `Bag` 을 `BottomPanel` 밖 루트 Canvas 직속으로 뺐다 (§2 트리). 앵커 하단 중앙 (0.5, 1) · Alignment (0, 1) · Position (**129**, −11) (= 원본 x 1089 − 960 · 아래 1079 − 1068) · Size 356 × 87. 판은 BottomPanel 크기 783 × 117 그대로 채운다
- 초상 원(H)은 판 위(962)에서 **잘린다** (y 962 위로는 월드). 판 높이 밖으로 튀어나오지 않는다

### 하단 패널 내부 (`BottomPanel` 왼쪽 위 = (0, 0) = 원본 (300, 962))

| 영역 | 상대 박스 (x, y, w, h) | 칸 크기 | 간격 · 반복 | 글자 크기 (UMG · 근사) | 원본 박스 |
|---|---|---|---|---|---|
| 판 `BottomPanelBg` | (0, 0, 783, 117) | — | — | — | (300, 962, 783, 117) · 색 ≈ RGB(32, 40, 48) |
| F `StatPanel` | (0, 0, 160, 117) | 줄: 아이콘 16×16 + 값 텍스트 · 아이콘↔값 10px | **2열 × 4행** · 열 간격(피치) **78** · 행 피치 **≈29.5** · 첫 아이콘 (12, 7) | 값 **11** (숫자 높이 10px) | 아이콘 1열 x 312 · 2열 x 390 · 행 중심 y 977 / 1006 / 1036 / 1065 |
| G `EquipColumn` | (162, 0, 35, 117) | **34 × 22** | 세로 **5칸** · 피치 **≈23.5** (간격 ≈1.5) | — | (462, 961, 35, 117) |
| H `Portrait` | (209, 0, 136, 117) | 바깥 원 지름 **136** · 얼굴 원 ≈109 · 중심 (277, 50) | 레벨 원 **30×30** at (214, 87) · 파란 육각 배지 **25×28** at (324, 3) | 레벨 숫자 **17** | 원 (509, ≈944, 136, 136) — 위 18px 잘림 |
| I `SkillBar` | (360, 8, 418, 52) | Q·W·E·R **48 × 48** (+ 아래 레벨 눈금 줄 ≈3px · y+49) · P **32 × 36** · D·F 원 **≈40** · G 원 ≈36 | Q·W·E·R 피치 **≈59.5** (간격 ≈11) · Q 왼쪽 360 · P 597 · D 642 · F 688 · G 740 | 키 글자 Q/W/E/R/D **10** (검은 칸 ≈12×14 · 좌하단) · `0/15` **11** | Q (660, 970) · W 719 · E 779 · R 838 · P 897 · D 942 · F 988 · G 1040~1076 |
| L `VitalBar` | (361, 77, 268, 16) | 바 **268 × 16** (검은 테 1px 포함) | — | `890 / 890` **12** (중앙 정렬) | (661, 1039, 268, 16) |
| M `CreditBox` | (638, 71, 92, 30) | 판 ≈90×30 · 크레딧 아이콘 ≈18 | `B` 키 칸 (638, 93) 12×12 | 숫자 `20` **14** | (938, 1033, 92, 30) |
| M `V` · G 세로 기둥 | (740, 0, 38, 106) | 둥근 기둥 ≈37 × 105 — G 슬롯 · `0/15` · `V` 버튼이 한 기둥 | — | — | (1040, ≈962, 38, 106) (근사) |
| N `Bag` | (789, 19, 356, 87) | **68 × 40** | **5열 × 2행** · 가로 피치 **72**(간격 4) · 세로 피치 **48**(간격 8) | 키 숫자 **8** (숫자 7px · 검은 칸 ≈10×13 · 칸 좌하단 (0, 27)) · 수량 `3` **10** (칸 우하단) | (1089, 981, 356, 87) · 1행 y 981~1020 · 2행 1029~1068 · 열 x 1089 / 1161 / 1233 / 1305 / 1377 |

- I 의 `SlotF` · `SlotG` 는 Collapsed 로 시작한다(§6 ① 4) — Collapsed 면 HorizontalBox 가 폭을 당기므로, 이미지 자리(D 다음 F·G)를 비워 두려면 **Hidden** 이 맞다 **(결정 필요)**
  - ✅ **결정 (2026-10-12)**: 안 쓰는 동안 **Collapsed** (빈자리 안 남김). F 는 **전술 스킬로 추가 예정** (사용자 "전술스킬은 추가되야") — 구현되면 Visible. G (`0/15`) 는 뜻 (미확인)
- I 는 레벨 눈금 줄이 아이콘 아래 3px 띠 · 눈금 개수 3~5 — 눈금 1개 폭 ≈8 · 간격 ≈2 (근사)
- G 장비 칸 순서는 §3.G 의 (미확인) 그대로. 크기만 잰 것

### 상단 · 우측 · 좌측 영역 내부

| 영역 | 칸 크기 | 간격 · 반복 | 글자 크기 (UMG · 근사) | 원본 측정 |
|---|---|---|---|---|
| A `MatchInfo` | 사선 띠 높이 **≈31** (y 16~46 · 날씨칸) · 시간칸은 y 0~47 | 띠 조각 4개: 날씨(727~843) · 일차/시간(843~1030) · 하트(1030~1100) · 팀(1100~1276) (경계 근사) | 시간 `02:11` **22** (숫자 21px) · `1` TEAMS 의 `1` **20** · `TEAMS` **9** · `1일차` **9** · 시계 `20` **16** (주황) | 시간 글자 (925, 16, 69, 21) · `20` (940, 58, 42, 15) |
| B `KillScore` | — | `TK 0  K 0  A 0` 한 줄 · 글자 사이 ≈14 | **15** (글자 14px · 라벨 TK/K/A 는 회색 · 숫자 흰색) | (1768, 23, 135, 14) |
| C `RouteGoals` | 장비 칸 **56 × 35** (아래 보라 줄 2px 포함) | **5칸** · 피치 **62** (간격 6) · `n/4` 는 칸 아래 6px · 전환 버튼 **40×16** at (6, 9) · `P` 키 13×13 at (0, 22) | `1/4` **11** (숫자 10px) | 칸 1 (1606, 57) ~ 칸 5 (1854, 57) · 글자 y 98~108 |
| D `AreaMaterials` | 재료 칸 **41 × 28** (아이콘 ≈25 + 아래 노란 줄 2px) | 1줄 최대 **5칸** · 피치 **43** (간격 2) · 줄 사이 ≈10 · 섹션 2개(탐색 재료 · 채집/사냥) | 제목 **10** (한글 12px · 노란 꺾쇠 ≈10×10) · `0/2` **10** | 제목1 y 640~652 · 1줄 y 660~688 · 2줄 698~722 · 제목2 735~750 · 3줄 760~787 · 칸 x 1656 / 1699 / 1742 / 1785 / 1828 |
| E `Minimap` | 머리줄 **263 × 28** · 지도 **≈259 × 240** (오른쪽 테 3px) · 자기 위치 원 ≈26 | 버튼 **4개** ≈20×20 · 피치 **≈28** · 오른쪽 끝에서 12 | 지역명 `절` **10** · 지도 위 지역명 **10** | 버튼 x ≈1806 / 1832 / 1860 / 1888 · y ≈814~832 |
| O `SystemMessage` | 접기 버튼 바깥 **55 × 66** · 안쪽 파란 칸 **34 × 33** at (11, 13) | 메시지 **3줄** · 줄 피치 **≈19.5** · 왼쪽 여백 20 · 판 너비 514 (반투명) | 메시지 **12** (한글 13~14px) · `번역` **9** | 버튼 (0, 524, 55, 66) · 번역 (22, 592, 40, 10) · 1줄 y 727 · 2줄 747 · 3줄 765 |
| P `NetInfo` | — | `00:09` 와 `0ms` 사이 ≈26 | **11** (숫자 10px · 회청색) | `00:09` (16, 1056, 35, 11) · `0ms` (76, 1056, 28, 11) |
| J `SkillUpPrompt` | 하이라이트 기둥 **57 × 99** · 마름모 **46 × 45** | 마름모 **5개** · 중심 x 685 / 744 / 803 / 864 / 915 → **스킬 슬롯 Q·W·E·R·P 위에 하나씩** (피치 ≈59.5) · 마름모 위쪽 y 905 | `스킬 강화 포인트 +1` **13** (한글 15px · 노란색) | 글자 (777, 880, 135, 15) · 기둥 x 658 / 717 / 772 |
| K `BuffBar` | 버프 칸 **28 × 28** | 보이는 1칸 · 간격 **(미확인)** → 4 (자체 결정값) | — | (658, 830, 28, 28) — 왼쪽 끝이 스킬 Q 칸(660)과 맞는다 |

- ⚠ J 의 마름모가 **5개 = Q·W·E·R + P 자리**에 하나씩 놓인다(x 중심이 슬롯 중심과 1~5px 이내). §3.J 의 "올릴 수 있는 슬롯" 이 **슬롯별 위치 고정**이라는 근거 1건. 그래서 마름모는 SkillBar 슬롯 피치(59.5)에 맞춰 배치한다
- 측정 오차: 판 경계 · 칸 경계는 **±1px**, 사선 · 그라데이션 · 반투명 경계는 **±3px (근사)**, 글자 크기는 **±1~2 (근사)**
- 실제 해상도 테스트는 §6 ② 의 1920×1080 겹쳐 보기로 확인한다

---

## 3. 영역별 상세

각 영역: **데이터 출처 → 갱신 방식 → 최적화 지정 → 이미지 조각**.
"갱신 방식"의 이벤트 원천은 공통 §2.2 표를 따른다.

**`내부 구조` 소단락 읽는 법** (2026-10-12 추가 — 각 영역 끝의 `내부 구조` 만 보고 UMG 디자이너에서 위에서 아래로 따라 만들면 된다. 형식은 [`02_캐릭터선택.md`](02_캐릭터선택.md) §2.2c 와 같다)
> - 수치는 레퍼런스 PNG 를 PIL 로 **다시 잰 값**이다 (밝기/색 마스크 bbox · 행/열 픽셀 덤프). 위 레이아웃 표와 다르면 **실측을 따르고 `⚠ 실측` 으로 표시**했다
> - 좌표는 **그 WBP 의 로컬** (원점 = 레이아웃 표의 원본 박스 왼쪽 위). `실측` 칸은 화면(원본) px `(x, y, w, h)`
> - **색은 픽셀에서 뽑은 sRGB Hex** → 색 선택기의 **`Hex sRGB`** 칸에 넣는다 (`Hex Linear` 아님). 알파를 안 적었으면 1.0. 반투명 판 위 글자는 배경이 섞여 ±10 정도 어긋난다 **(근사)**
> - 글자 크기 = UMG Font Size. 위 "기준과 환산" 식 그대로: 숫자/영문 ≈ 글자 높이 px × 1.07 · 한글 ≈ 높이 × 0.83 **(근사 ±1)**. 폰트는 엔진 기본(Roboto · 한글은 엔진 폴백) — 프로젝트 폰트 지정은 **(미확인 — 공통원칙에 없음)**
> - TextBlock 은 상자 위쪽에 글자 여백이 있다 → **Padding Top = 글자 위 y − 여백**. 여백 ≈ Font Size × 0.29 (숫자/영문 · Roboto ascent 0.93em − cap 0.71em) · 한글 ≈ Font Size × 0.15 **(근사 — 겹쳐 보기로 ±2 보정)**
> - 영역 내부 배치는 **Overlay + 자식마다 HAlign Left · VAlign Top · Padding(L,T,0,0)** 을 기본으로 했다 (Canvas 금지 — 공통 §4.5). 이렇게 하면 미확인 자리를 **Collapsed 로 숨겨도 다른 위젯이 밀리지 않는다** (§8 "뜻이 확정되지 않은 자리는 Collapsed"). 같은 간격으로 반복되는 것만 HorizontalBox · VerticalBox · UniformGridPanel
> - 표시만 하는 위젯(텍스트 · 장식 이미지)은 Visibility = **`Not Hit-Testable (Self & All Children)`** (공통 §4.7) → 표에서 `NHT`. 클릭 위젯만 `Visible`
> - **BindWidget 대상**은 **굵게**. §2 · §3 · §6 에 이미 있는 이름은 그대로 두고, 새로 붙인 이름은 `(추가)` 로 표시했다 — C++ 베이스가 생기면 이 이름으로 잡는다
> - Image 의 `Image Size` 는 Brush 의 Image Size 칸. Overlay 슬롯이 Fill 이 아니면 위젯 크기 = Image Size
> - Hover/Pressed 모양은 레퍼런스에 **기본 상태만** 있어 전부 **(미확인)** — 색(Tint)만 바꾸는 가안을 적었다

### A. 매치 정보 바 — `WBP_ER_MatchInfo`

| 요소 | 데이터 출처 (클래스::멤버 · 갱신 이벤트) | 상태 |
|---|---|---|
| "1일차" | `AERGameState::Day` (`Core/ERGameState.h:81` · ReplicatedUsing=OnRep_Phase) → `OnPhaseChanged(Day, bNight)` (`:60`, OnRep 에서 Broadcast `Core/ERGameState.cpp:40-43`) | ✅ |
| 해/달 아이콘 | `AERGameState::bNight` (`:84`) · 같은 이벤트 | ✅ |
| 남은 시간 `02:11` | `AERGameState::GetPhaseRemaining()` (`:73`) = `PhaseEndServerTime − GetServerWorldTimeSeconds()` (`Core/ERGameState.cpp:35-38`). **값은 페이즈 바뀔 때만 복제** (`ERGameState.h:55-57` 주석) | ✅ (값) · 매초 표시는 로컬 타이머 |
| 생존 팀 수 "1 TEAMS" | 팀 명단 `AERGameState::Teams` (`:94`) 은 있으나 **생존 여부 집계 없음** | ❌ 미구현 (F14) |
| 날씨 · 하트 · "?" · 시계 20 | — | ❌ 미구현 · (미확인) §1.3 |

**갱신 방식**
- 일차 · 낮밤: `OnPhaseChanged` 구독 → 텍스트 · 아이콘 교체. 구독 직후 `GetDay()` · `IsNight()` 로 한 번 초기화 (공통 §2.3)
- 남은 시간: `OnPhaseChanged` 에서 **1초 주기 `FTimerManager` 타이머**를 걸고 `GetPhaseRemaining()` 을 `mm:ss` 로 넣는다. 시계가 멈추면(`IsClockRunning()==false`) 타이머 해제
  - 근거: 남은 시간을 매 프레임 복제하지 않고 종료 시각 1개만 복제하는 구조가 이미 있다 (`ERGameState.h:89-91`) · 역기획서 §3 #8
  - Active Timer 를 쓰지 않는 이유는 공통 §4.4 — Epic: Active Timer 는 *"should be utilized when working on editor UI"*, 실시간 뷰포트가 있는 게임 UI 용이 아니다 ([Slate UI Sleeping and Active Timers](https://dev.epicgames.com/documentation/unreal-engine/slate-ui-sleeping-and-active-timers-in-unreal-engine))

**최적화**
- 바 전체: Invalidation Box 로 감싼다. 1초에 한 번 텍스트만 바뀌므로 캐시 이득이 크다 (공통 §4.1)
- `02:11` 텍스트는 **Volatile 아님** — 매 프레임이 아니라 1초마다. 공통 §4.2 기준
- 숫자 폭이 바뀌면 Layout 무효화가 난다 — Epic: *"If you move or resize a widget ... it will trigger Layout Invalidation"* ([Invalidation in Slate and UMG](https://dev.epicgames.com/documentation/unreal-engine/invalidation-in-slate-and-umg-for-unreal-engine)). **고정폭 숫자 폰트**(또는 Text 의 Min Desired Width)로 폭을 고정한다. 텍스트 변경이 Layout 까지 가는지 Paint 에서 끝나는지는 **(미확인 — 엔진 `STextBlock::SetText` 미대조)**

**이미지 조각**
- `T_UI_MatchBar_Bg` — 기울어진 띠 배경 (좌우 끝 사선) · 9-slice 가능 여부: 사선 끝 때문에 **가운데만 늘리는 3-slice(가로)** 로 자른다 **(미확인 — 원본 소스 이미지 없음)**
- `T_UI_Icon_Day` · `T_UI_Icon_Night` · `T_UI_Icon_Team` · `T_UI_Icon_Clock`
- 날씨 · 하트 아이콘은 기능이 없으므로 **지금 만들지 않는다**

**내부 구조** — 로컬 원점 = 원본 (727, 0) · 크기 550 × 76

```
WBP_ER_MatchInfo
SizeBox  SB_MatchRoot                         (추가) 루트 — Canvas 아님 · 550×76 고정
└─ InvalidationBox  MatchInvalidation         (추가) §6 ① 6
   └─ Overlay  MatchRoot                      (추가)
      ├─ Image      MatchBarBg                (추가) 사선 띠 판 한 장
      ├─ Image      WeatherIcon_0             (추가) 구름  — Collapsed
      ├─ Image      WeatherIcon_1             (추가) 바람  — Collapsed
      ├─ TextBlock  **DayText**   "1일차"     (추가)
      ├─ Image      **DayNightIcon**          (추가) 해/달
      ├─ SizeBox    SB_Time                   (추가) 자릿수가 바뀌어도 중심 960 고정
      │  └─ TextBlock  **TimeText**  "02:11"  (추가)
      ├─ Image      HeartIcon                 (추가) Collapsed
      ├─ HorizontalBox  TeamsRow              (추가)
      │  ├─ TextBlock  **AliveTeamsText**  "1"   (추가)
      │  ├─ TextBlock  Txt_TeamsLabel  "TEAMS"   (추가)
      │  ├─ TextBlock  Txt_TeamsSlash  "/"       (추가)
      │  ├─ Image      PersonIcon                (추가) Collapsed
      │  └─ TextBlock  UnknownText  "?"          (추가) Collapsed
      └─ HorizontalBox  ClockRow              (추가) Collapsed (뜻 미확인 §1.3)
         ├─ Image      ClockIcon
         └─ TextBlock  ClockText  "20"
```

| 위젯 | 부모 슬롯 | 위젯 자체 설정 | 실측 (원본 px) |
|---|---|---|---|
| `SB_MatchRoot` | 루트 (RootCanvas 의 슬롯은 레이아웃 표) | Width **550** · Height **76** | (727,0,549,76) |
| `MatchInvalidation` | SizeBox 자식 | Can Cache ✔ | — |
| `MatchRoot` | InvalidationBox 자식 | Visibility **NHT** (클릭할 것 없음) | — |
| `MatchBarBg` | Overlay · HAlign **Fill** · VAlign **Top** · Padding 0 | Brush `T_UI_MatchBar_Bg` · Draw As **Image** (고정 크기라 늘릴 일 없음 — 3-slice 는 폭이 바뀔 때만) · Image Size **550 × 48** · Tint `#FFFFFF` | 칸 4개가 반투명 어두운 판: 날씨 칸 (727–844, y 10–45) 바탕 `#134351`~`#0D2A32` · 일차/시간 칸 (843–1030, y 0–47) `#1B2020`~`#212E34` + 아래 테두리 y 46 `#354954` · 하트 칸 (1030–1100) `#333C43` · 팀 칸 (1100–1276, y 13–44) 파란 판 `#2F4F6F`~`#2B3F4A` **(근사 — 반투명이라 월드가 비침)** |
| `WeatherIcon_0` | Overlay · Left · Top · Padding **(18, 25, 0, 0)** | Brush 구름 아이콘 · Image Size **20 × 13** · Tint `#48D0F0` · NHT · **Collapsed** | (745,25,20,13) |
| `WeatherIcon_1` | Overlay · Left · Top · Padding **(61, 24, 0, 0)** | Brush 바람 아이콘 · Image Size **19 × 17** · Tint `#6090A0` · NHT · **Collapsed** | (788,24,19,17) |
| `DayText` | Overlay · Left · Top · Padding **(123, 9, 0, 0)** (글자 위 10 − 여백 1) | `1일차` · Size **9** · 색 `#E0E0E0` · Justification Left · 그림자 없음 | 글자 (850,10,29,11) |
| `DayNightIcon` | Overlay · Left · Top · Padding **(138, 24, 0, 0)** | Brush `T_UI_Icon_Day` (밤 `T_UI_Icon_Night` — 런타임 교체) · Image Size **18 × 17** · Tint `#FFFFFF` (텍스처 색 그대로 — 해 `#F0B008`) · NHT | (865,24,18,17) |
| `SB_Time` | Overlay · Left · Top · Padding **(188, 11, 0, 0)** | Width Override **90** (중심 = 188+45 = **233** = 화면 960) | 칸 중심 x 960 |
| `TimeText` | SizeBox 자식 (Center/Center) | `02:11` · Size **18** **⚠ 실측** (글자 높이 17 → 17×1.07; 레이아웃 표 22 는 글자 21px 로 잰 값) · 색 `#FFFFFF` · Justification **Center** · 그림자 없음 · 고정폭 숫자 (§6 ① 4 Min Desired Width) | 글자 (926,17,69,17) — `:` 는 x 959–960 |
| `HeartIcon` | Overlay · Left · Top · Padding **(312, 8, 0, 0)** | Brush 하트+∞ 아이콘 · Image Size **42 × 30** **(근사 — 하트 (1039,15,29,23) + ∞ (1055,8,13,13) 합친 상자)** · Tint `#FFFFFF` (텍스처 색 — 하트 `#80E820`) · NHT · **Collapsed** | (1039,8,42,30) (근사) |
| `TeamsRow` | Overlay · Left · Top · Padding **(401, 14, 0, 0)** | NHT | 행 x 1128–1240 |
| `AliveTeamsText` | HBox · VAlign **Bottom** · Padding 0 · Auto | `1` · Size **20** · 색 `#FFFFFF` · Left | (1128,20,11,19) |
| `Txt_TeamsLabel` | HBox · VAlign Bottom · Padding **(3, 0, 0, 2)** (근사) · Auto | `TEAMS` · Size **9** · 색 `#90B8E0` · NHT | (1142,28,39,8) |
| `Txt_TeamsSlash` | HBox · VAlign Bottom · Padding **(6, 0, 0, 0)** (근사) · Auto | `/` · Size **18** · 색 `#4A6A8A` **(근사 — 얇은 사선이라 배경 섞임)** · NHT | x ≈1186–1194 (근사) |
| `PersonIcon` | HBox · VAlign Center · Padding **(6, 0, 0, 0)** (근사) · Auto | Brush `T_UI_Icon_Team` · Image Size **14 × 16** · Tint `#A8D0F8` · NHT · **Collapsed** | (1201,21,14,16) |
| `UnknownText` | HBox · VAlign Bottom · Padding **(15, 0, 0, 0)** · Auto | `?` · Size **18** · 색 `#FFFFFF` · NHT · **Collapsed** | (1230,21,10,17) |
| `ClockRow` | Overlay · Left · Top · Padding **(211, 55, 0, 0)** | NHT · **Collapsed** | 행 y 59–74 |
| `ClockIcon` | HBox · VAlign Center · Padding 0 · Auto | Brush `T_UI_Icon_Clock` · Image Size **15 × 16** · Tint `#B0B0B0` | (938,59,15,16) |
| `ClockText` | HBox · VAlign Center · Padding **(9, 0, 0, 0)** · Auto | `20` · Size **15** · 색 **`#FFC6C6` ⚠ 실측** (§1.2 "주황" 이 아니라 연분홍) · Left | (962,59,20,14) |

- `PersonIcon` · `UnknownText` 는 HorizontalBox 의 **끝**이라 Collapsed 여도 앞 글자가 안 밀린다. 날씨 · 하트 · 시계 줄은 Overlay 직속이라 숨겨도 그대로
- `TimeText` 크기: 글자 높이 17px 기준 Size 18. 레이아웃 표의 "숫자 21px" 은 bbox 를 콜론 위쪽 여백까지 넣어 잰 것으로 보인다 → 겹쳐 보기로 18~22 사이에서 맞춘다 **(근사)**

---

### B. 처치 수 — `WBP_ER_KillScore`

| 요소 | 데이터 출처 | 상태 |
|---|---|---|
| `K` 내 처치 | `AERPlayerState::PlayerKills` (`Core/ERPlayerState.h:158`) — **"복제 안 함 (UI 는 F14)"** 주석 · 서버 전용 | ❌ 미구현 (복제 필요) |
| `TK` 팀 처치 | — | ❌ 미구현 |
| `A` 어시스트 | — (`Event_Kill_Dealt` 주석: "어시스트(관여)는 아직 막타만" `GAS/ERGameplayTags.h:128`) | ❌ 미구현 |

- 갱신: 처치 이벤트 때만 → `OnRep` + 델리게이트 (F14 에서)
- 최적화: Invalidation Box 안 · Volatile 아님
- 이미지 조각: 없음 (텍스트만). 폰트만 지정

**내부 구조** — 로컬 원점 = 원본 (1768, 18) (근사) · 크기 Size To Content (오른쪽 앵커라 숫자가 늘면 왼쪽으로 자란다)

```
WBP_ER_KillScore
InvalidationBox  KillInvalidation          (추가) 루트
└─ HorizontalBox  KillRow                  (추가)
   ├─ TextBlock  Txt_TKLabel  "TK"         (추가)
   ├─ TextBlock  **TeamKillText**  "0"     (추가)
   ├─ TextBlock  Txt_KLabel   "K"          (추가)
   ├─ TextBlock  **KillText**  "0"         (추가)
   ├─ TextBlock  Txt_ALabel   "A"          (추가)
   └─ TextBlock  **AssistText**  "0"       (추가)
```

| 위젯 | 부모 슬롯 | 위젯 자체 설정 | 실측 (원본 px) |
|---|---|---|---|
| `KillInvalidation` | 루트 | Can Cache ✔ · Visibility **NHT** | — |
| `KillRow` | InvalidationBox 자식 | — | 행 (1768,23,135,13) |
| `Txt_TKLabel` | HBox · VAlign Bottom · Padding 0 · Auto | `TK` · Size **14** **⚠ 실측** (글자 12~13px · 표의 15 보다 1 작음) · 색 `#888888` · 그림자 없음 | (1768,24,21,12) |
| `TeamKillText` | HBox · VAlign Bottom · Padding **(10, 0, 0, 0)** · Auto | `0` · Size 14 · 색 `#F0F0F0` | (1800,23,9,13) |
| `Txt_KLabel` | HBox · VAlign Bottom · Padding **(18, 0, 0, 0)** · Auto | `K` · Size 14 · 색 `#888888` | (1828,24,9,12) |
| `KillText` | HBox · VAlign Bottom · Padding **(10, 0, 0, 0)** · Auto | `0` · Size 14 · 색 `#F0F0F0` | (1848,23,9,13) |
| `Txt_ALabel` | HBox · VAlign Bottom · Padding **(16, 0, 0, 0)** · Auto | `A` · Size 14 · 색 `#888888` | (1874,24,10,12) |
| `AssistText` | HBox · VAlign Bottom · Padding **(10, 0, 0, 0)** · Auto | `0` · Size 14 · 색 `#F0F0F0` | (1894,23,9,13) |

- Padding 은 글자 사이 실측 간격(라벨→숫자 11 · 숫자→다음 라벨 19 / 17)에서 글자 옆 여백 ≈1 을 뺀 값 **(근사)**
- 배경 판 없음 — 월드 위에 글자만 있다 (`#1A2020` 근처 = 지붕 텍스처)

---

### C. 목표 장비 진행 — `WBP_ER_RouteGoals`

| 요소 | 데이터 출처 | 상태 |
|---|---|---|
| 목표 장비 5칸 | 목표 빌드(루트) 데이터 없음 | ❌ 미구현 |
| `1/4` | (미확인 §1.3). 보유 수 판정에 쓸 수 있는 것: `UERInventoryComponent::CountItem(FName)` (`Item/ERInventoryComponent.h:150` 부근 · "UI 회색 처리" 용도로 주석에 명시) | ❌ 미구현 (목표 데이터) |
| "P" 전환 버튼 | (미확인) | ❌ |

- 역기획서 §5 (제작 UI 재귀 분해)와 같은 계산을 쓴다 — **제작 UI(`03_아이템제작.md`)와 함께** 만든다. 이 문서에서는 **`WBP_ER_ItemSlot` 5칸 + `bShowRatio` 자리만** 만든다
- 갱신(나중): 가방 변경 이벤트(§N 과 같은 것) 때만 재계산 — 역기획서 §5.3 ②

**내부 구조** — 로컬 원점 = 원본 (1553, 57) · 크기 357 × 52

```
WBP_ER_RouteGoals
Overlay  RouteRoot                              (추가) 루트
├─ Button  **SwitchButton**                     (추가) 전환 버튼 (뜻 미확인 §1.3)
│  └─ Image  Img_SwitchArrows                   (추가) ⇆ 화살표
├─ Overlay  KeyBadge_P                          (추가) 검은 키 칸
│  ├─ Image      Img_KeyBg                      (추가)
│  └─ TextBlock  Txt_Key  "P"                   (추가)
├─ UniformGridPanel  GoalGrid                   (추가) 5열 × 1행
│  └─ WBP_ER_ItemSlot  **GoalSlot0** ~ **GoalSlot4**    (추가) 엔트리 내부는 §3.N
└─ UniformGridPanel  GoalRatioGrid              (추가) 5열 × 1행 — 칸 아래 글자 줄
   └─ TextBlock  **GoalRatio0** ~ **GoalRatio4**  "1 / 4"  (추가)
```

| 위젯 | 부모 슬롯 | 위젯 자체 설정 | 실측 (원본 px) |
|---|---|---|---|
| `RouteRoot` | 루트 | Visibility **Self Hit Test Invisible** (자식 버튼만 클릭) | (1553,57,357,52) |
| `SwitchButton` | Overlay · Left · Top · Padding **(6, 9, 0, 0)** | Style › Normal: Draw As **Rounded Box** · 반지름 8 (높이 16 의 반 — 알약 모양) · Tint `#386890` · Normal/Pressed Padding 0 · Is Focusable ✘ · 크기: 자식 Image 가 정한다(아래) | (1559,66,40,16) |
| `Img_SwitchArrows` | Button 슬롯 · HAlign Center · VAlign Center · Padding 0 | 화살표 아이콘 · Image Size **40 × 16** (판 크기를 이 Image 가 만든다 — 아이콘 자체는 ≈22×10 을 가운데 둔 텍스처) **(근사)** · Tint `#F8F8F8` · NHT | 흰 화살표 (≈1565–1587, 70–83) |
| `KeyBadge_P` | Overlay · Left · Top · Padding **(0, 22, 0, 0)** | NHT | (1553,79,13,13) |
| `Img_KeyBg` | Overlay · Fill · Fill | 단색 · Draw As **Image** · Image Size **13 × 13** · Tint `#000000` | 바탕 `#000000` · 안쪽 `#292929` 1px 테 (근사 — 무시 가능) |
| `Txt_Key` | Overlay · HAlign Center · VAlign Center | `P` · Size **9** · 색 `#CCCCCC` · Center | 글자 (1557,82,6,8) |
| `GoalGrid` | Overlay · Left · Top · Padding **(50, 0, 0, 0)** | **Min Desired Slot Width 56 · Height 35** · Slot Padding **(3, 0, 3, 0)** → 피치 62 | 칸 x 1606 / 1668 / 1730 / 1792 / 1854 (폭 56) · y 57–91 (높이 35) |
| `GoalSlot0~4` | Grid · Column 0~4 · Row 0 · HAlign **Fill** · VAlign **Fill** | `WBP_ER_ItemSlot` · `bShowRatio` **✘** · `bShowKeyLabel` ✘ · `bCompact` ✘ (아래 ⚠) | 칸 바탕 위→아래 그라데이션 `#3A2D43` → `#612C95` · 맨 아래 1px `#9B63C3` |
| `GoalRatioGrid` | Overlay · Left · Top · Padding **(50, 38, 0, 0)** (글자 위 41 − 여백 3) | Min Desired Slot Width **56** · Slot Padding (3, 0, 3, 0) — `GoalGrid` 와 같은 열 피치 · NHT | 글자 줄 y 98–108 |
| `GoalRatio0~4` | Grid · Column 0~4 · HAlign **Center** · VAlign Top | `1 / 4` (공백 포함 — 글자 사이 6~7px) · Size **12** **⚠ 실측** (숫자 11px · 표 11) · 색 `#E0E0E0` · Justification Center · 그림자 없음 | `1 / 4` (1620,98,29,11) — 중심 1634 = 칸 중심 1633.5 |

- ⚠ **§2.1 은 C 도 `bShowRatio` 로 적었지만** 실측에서 C 의 `n/4` 는 **칸 밖 아래 6px** 에 있다 → ItemSlot 의 Overlay 안 `RatioText` 로 넣으면 칸 그림과 겹친다. 그래서 C 는 `bShowRatio ✘` + 같은 열 피치의 **형제 Grid** 로 뺐다. D 는 글자가 칸 안 우하단이라 `bShowRatio` 그대로 (§3.D)
- 칸 크기는 SizeBox 없이 **UniformGridPanel 의 Min Desired Slot Width/Height** 로 박는다 (§2.1 "부모 슬롯 크기로" · 공통 §4.5). ItemSlot 자체 Desired Size 는 가장 작은 칸(G 34×22)보다 작게 둔다 — §3.N 엔트리 표
- 칸 바탕 보라색은 등급색으로 보인다(5칸 모두 같은 보라) — 등급 ↔ 색 대응 **(미확인 — §3.N 등급색)**
- `SwitchButton` Hovered: Tint `#4A80A8` · Pressed: `#2C5470` **(가안 — 레퍼런스에 없음)**

---

### D. 지역 재료 패널 — `WBP_ER_AreaMaterials`

| 요소 | 데이터 출처 | 상태 |
|---|---|---|
| "절" 지역 이름 | 지역 판정 없음 (F13 "개요만" `Docs/1_Task/00_기능목록_및_의존성.md:64`) | ❌ 미구현 (F13) |
| 탐색 재료 / 채집·사냥 목록 + `0/2` | 지역 → 재료 매핑 없음 | ❌ 미구현 (F13 · 제작 UI) |

- 이 문서에서는 **제목 텍스트 2줄 + `WBP_ER_ItemSlot`(bShowRatio) 가로 줄 2개** 레이아웃만
- 칸 수가 지역마다 다르다 → 나중에 C++ 가 칸을 **미리 최대 개수만큼 만들어 두고 Collapsed 로 숨긴다.** 매번 Add/Remove 하면 Child 무효화(비용 "High") — 공통 §4.1 표 ([Invalidation in Slate and UMG](https://dev.epicgames.com/documentation/unreal-engine/invalidation-in-slate-and-umg-for-unreal-engine))
- 이미지 조각: `T_UI_SectionHeader_Corner` (제목 앞 노란 삼각 꺾쇠) · 반투명 판 9-slice `T_UI_Panel_Dark`

**내부 구조** — 로컬 원점 = 원본 (1655, 632) · 크기 264 × 165

```
WBP_ER_AreaMaterials
InvalidationBox  AreaInvalidation                     (추가) 루트
└─ Overlay  AreaRoot                                  (추가)
   ├─ Image      Img_ExploreCorner                    (추가) 노란 삼각 꺾쇠
   ├─ HorizontalBox  ExploreTitleRow                  (추가)
   │  ├─ TextBlock  **AreaNameText**  "절"            (추가) 하늘색 지역 이름
   │  └─ TextBlock  Txt_ExploreTitle  "탐색 재료"      (추가)
   ├─ Button     AlertToggleButton                    (추가) 우상단 빨간 사선 아이콘 — Collapsed (뜻 미확인)
   │  └─ Image  Img_AlertToggle                       (추가)
   ├─ Overlay    ExploreRow0                          (추가) 줄 1
   │  ├─ Image   Img_ExploreRow0Bg                    (추가) 줄 바탕 띠
   │  └─ UniformGridPanel  ExploreGrid0               (추가) 6열 × 1행
   │     └─ WBP_ER_ItemSlot  **ExploreSlot0** ~ **ExploreSlot5**   (추가)
   ├─ Overlay    ExploreRow1                          (추가) 줄 2 — 구조 같음 · **ExploreSlot6** ~ **ExploreSlot11**
   ├─ Image      Img_GatherCorner                     (추가)
   ├─ TextBlock  Txt_GatherTitle  "채집/사냥"          (추가)
   └─ Overlay    GatherRow0                           (추가) 줄 3 — 구조 같음 · **GatherSlot0** ~ **GatherSlot5**
```

| 위젯 | 부모 슬롯 | 위젯 자체 설정 | 실측 (원본 px) |
|---|---|---|---|
| `AreaRoot` | InvalidationBox 자식 | Visibility **Self Hit Test Invisible** (칸 · 버튼만 클릭) | (1655,632,264,165) |
| `Img_ExploreCorner` | Overlay · Left · Top · Padding **(0, 6, 0, 0)** | Brush `T_UI_SectionHeader_Corner` (왼쪽 위 직각 삼각형) · Image Size **11 × 11** · Tint `#F8D058` · NHT | (1654,638,11,11) — 판 왼쪽보다 1px 밖 (무시) |
| `ExploreTitleRow` | Overlay · Left · Top · Padding **(11, 7, 0, 0)** (글자 위 9 − 여백 2) | NHT | 행 y 641–652 |
| `AreaNameText` | HBox · VAlign Bottom · Padding 0 · Auto | `절` · Size **10** · 색 `#50B8D8` · Left | (1666,641,10,12) |
| `Txt_ExploreTitle` | HBox · VAlign Bottom · Padding **(5, 0, 0, 0)** · Auto | `탐색 재료` · Size **10** · 색 `#D8D8D8` | (1682,641,54,12) |
| `AlertToggleButton` | Overlay · Left · Top · Padding **(235, 8, 0, 0)** | Style Normal/Hovered/Pressed Draw As **None** · Padding 0 · **Collapsed** (§8) | (1890,640,17,13) |
| `Img_AlertToggle` | Button 슬롯 · Center · Center | 흰 아이콘 + 빨간 사선 한 장 · Image Size **17 × 13** · Tint `#FFFFFF` (흰 `#F8F8F8` · 사선 `#F80000`) | 위 |
| `ExploreRow0` | Overlay · Left · Top · Padding **(0, 27, 0, 0)** | — | 띠 (1655,659,258,30) |
| `ExploreRow1` | Overlay · Left · Top · Padding **(0, 61, 0, 0)** | — · 줄이 비면 C++ 가 **줄째 Collapsed** | 띠 (1655,693,258,30) — 줄 사이 4px 는 월드 |
| `Img_*Row*Bg` | 줄 Overlay · HAlign Fill · VAlign Fill | Brush `T_UI_Panel_Dark` · Draw As **Box** · Margin 0.25 · Image Size 258 × 30 · Tint `#1D262D` · 알파 **0.85 (근사 — 월드가 살짝 비침)** · NHT | 띠 색 `#1A222B` / `#1D262D` / `#1F2831` (줄마다 월드 영향으로 ±3) |
| `ExploreGrid0` 등 | 줄 Overlay · HAlign Left · VAlign Fill · Padding 0 | **Min Desired Slot Width 41 · Height 30** · Slot Padding **(1, 0, 1, 0)** → 피치 43 · 6열 × 43 = 258 = 띠 폭 | 칸 x 1656 / 1699 / 1742 / 1785 / 1828 / 1871 |
| `ExploreSlot*` · `GatherSlot*` | Grid · Column 0~5 · Row 0 · HAlign **Fill** · VAlign **Fill** | `WBP_ER_ItemSlot` · `bShowRatio` **✔** · `bShowGradeBg` **✘** (§3.N 엔트리 (추가) 옵션 — 칸 바탕 없이 아이콘만) · 노란 밑줄 · 체크는 엔트리의 `UnderlineImage` · `CheckImage` (§3.N) | 칸 **41 × 30 ⚠ 실측** (표의 28 은 밑줄 위까지) |
| `Img_GatherCorner` | Overlay · Left · Top · Padding **(0, 103, 0, 0)** | `Img_ExploreCorner` 와 같음 | (1654,735,11,11) |
| `Txt_GatherTitle` | Overlay · Left · Top · Padding **(11, 104, 0, 0)** | `채집/사냥` · Size **10** · 색 `#D8D8D8` · NHT | (1666,738,56,12) |
| `GatherRow0` | Overlay · Left · Top · Padding **(0, 125, 0, 0)** | 구조 = `ExploreRow0` | 띠 (1655,757,258,30) |

**칸 안 (D 에서 쓰는 ItemSlot 표시)** — 엔트리 트리 · 이름은 §3.N
| 부분 | 설정 | 실측 |
|---|---|---|
| 아이콘 | 칸을 거의 채움 (위아래 2~3px 여백) | 예: 분홍 광석 (1714–1739, 664–686) |
| `RatioText` `0/2` | 칸 Overlay · HAlign **Right** · VAlign **Bottom** · Padding **(0, 0, 1, 3)** · Size **9** **⚠ 실측** (숫자 8px · 표 10) · 색 `#D9D9D9` · 그림자 Offset (1,1) · 색 `#000000` 알파 0.5 **(근사 — 글자 위아래 1px 가 바탕보다 어둡다)** | 1번 칸 글자 (1680,677,16,8) · 아래 끝 684 |
| `UnderlineImage` (노란 밑줄) | 칸 Overlay · HAlign Fill · VAlign **Bottom** · Padding 0 · 단색 Image Size (1 × **2**) · Tint `#FFD247` · 뜻 **(미확인 — 1줄 5칸 · 채집 1칸에만 있다)** | y 687–688 · 폭 41 |
| `CheckImage` (녹색 체크) | 칸 Overlay · HAlign Right · VAlign Bottom · Padding **(0, 0, 0, 3)** · Image Size **11 × 8** · Tint `#C0F010` · 뜻 **(미확인 — 다 모음?)** | (1729,712,11,8) — 2줄 2번째 칸 |

- 칸 수는 지역마다 다르다 → 줄 3개 × 6칸 = **18칸을 미리** 만들고 남는 칸 · 빈 줄은 Collapsed (§3.D 위 메모). 줄당 최대 6칸은 이미지 기준 · 지역 최대 개수는 **(미확인)**
- 칸 Hover/Pressed 표시는 엔트리 공통 (§3.N 엔트리 표 마지막 줄)

---

### E. 미니맵 — `WBP_ER_Minimap`

| 요소 | 데이터 출처 | 상태 |
|---|---|---|
| 지도 이미지 · 지역 경계 | 맵 · 지역 데이터 없음 | ❌ 미구현 (F13) |
| 자기 위치 | 로컬 폰 `GetActorLocation()` (엔진) | 값 ✅ / 좌표 변환 ❌ |
| 마커 (상자 · 야생동물 · 지역명) | — | ❌ 미구현 |
| 톱니 등 버튼 4개 | (미확인) | ❌ |

- ⭐ **HUD 에서 매 프레임 갱신이 정당한 유일한 위젯** — 역기획서 §3 구현 메모: *"`NativeTick`을 켜는 위젯은 미니맵 하나로 제한한다"*. 공통 §4.8 의 `DisableNativeTick` 기본값에서 **예외**로 표시한다
- 자기·팀원 아이콘 위젯만 **Volatile** (공통 §4.2). 지도 이미지는 Volatile 아님
- 역기획서 §4.3 구현 메모: SceneCapture 금지 · **지도 텍스처 1장 + 마커 UMG** · `WorldToMapUV` 함수 하나 · 마커 풀링
- Retainer Box 는 쓰지 않는다 — Epic: *"you should try using Invalidation Boxes first"* ([Invalidation in Slate and UMG](https://dev.epicgames.com/documentation/unreal-engine/invalidation-in-slate-and-umg-for-unreal-engine)). 마커 수백 개에서 느리면 그때 측정 후 결정 (공통 §4.3)
- 이 문서에서는 **테두리 + 지역명 텍스트 + 빈 지도 Image + 버튼 4칸 자리**만. 지도 텍스처는 F13 뒤
- 이미지 조각: `T_UI_Minimap_Frame` (9-slice) · `T_UI_Minimap_Self` (자기 위치 원) · 버튼 아이콘 4개 (미확인 — 뜻 확정 후)

**내부 구조** — 로컬 원점 = 원본 (1656, 811) · 크기 263 × 268

```
WBP_ER_Minimap                                   (HUD 에서 유일하게 Tick 허용 — §3.E 위)
Overlay  MinimapRoot                             (추가) 루트
├─ Image      Img_HeaderBg                       (추가) 머리줄 판
├─ TextBlock  **RegionNameText**  "절"           (추가)
├─ HorizontalBox  MinimapButtons                 (추가) 버튼 4개 — 뜻 미확인
│  ├─ Button  MapBtn0  └─ Image  Img_MapBtn0     (추가) 원형 차트 모양
│  ├─ Button  MapBtn1  └─ Image  Img_MapBtn1     (추가) 지도 핀
│  ├─ Button  MapBtn2  └─ Image  Img_MapBtn2     (추가) 사선 그은 핀
│  └─ Button  MapBtn3  └─ Image  Img_MapBtn3     (추가) 톱니
├─ Overlay  MapViewport                          (추가) 지도 창 — 잘라내기
│  ├─ Image  Img_MapBg                           (추가) 검정 바탕
│  ├─ Image  **MapImage**                        (추가) 지도 텍스처 1장 (F13 뒤 · 지금은 빈 Image)
│  ├─ Overlay  **MarkerLayer**                   (추가) 마커 풀 (지역명 · 상자 · 야생동물 · 팀원)
│  └─ Image  **SelfIcon**                        (추가) 자기 초상 + 녹색 고리
└─ Image  Img_MinimapFrame                       (추가) 바깥 테 (9-slice) — 맨 위에 그린다
```

| 위젯 | 부모 슬롯 | 위젯 자체 설정 | 실측 (원본 px) |
|---|---|---|---|
| `MinimapRoot` | 루트 | Visibility **Self Hit Test Invisible** | (1656,811,263,268) |
| `Img_HeaderBg` | Overlay · HAlign Fill · VAlign **Top** · Padding 0 | 단색 · Draw As Image · Image Size (263 × **28**) · Tint `#313937` (반투명 — 알파 **(근사 0.9)**) · NHT | y 811–838 · 색 `#313937` · 아래 1px `#2B3230` |
| `RegionNameText` | Overlay · Left · Top · Padding **(10, 5, 0, 0)** (글자 위 7 − 여백 2) | `절` · Size **10** · 색 `#D8D8D8` · Left · NHT | (1667,818,10,12) |
| `MinimapButtons` | Overlay · HAlign **Right** · VAlign Top · Padding **(0, 4, 13, 0)** | — | 아이콘 x 1808–1905 · y 815–832 |
| `MapBtn0~3` | HBox · VAlign Center · Padding **(0,0,7,0)** (마지막은 0) · Auto | Style Normal/Hovered/Pressed Draw As **None** · Padding 0 · Is Focusable ✘ · **Collapsed** 가 원칙(§8 · 뜻 미확인)이나 오른쪽 정렬이라 숨겨도 다른 위젯 안 밀림 | 피치 ≈27 · 중심 x 1816 / 1843 / 1871 / 1897 |
| `Img_MapBtn0~3` | Button 슬롯 · Center · Center | Image Size **20 × 18** (아이콘 실폭 17 / 20 / 16 / 17) · Tint `#B0B0B0` · Hover 가안 `#FFFFFF` · Pressed 가안 `#808080` **(미확인)** | (1808,815,17,18) / (1833,815,20,18) / (1863,815,16,18) / (1889,815,17,18) |
| `MapViewport` | Overlay · Left · Top · Padding **(1, 28, 3, 5)** | **Clipping = Clip to Bounds** (지도가 창 밖으로 안 나가게) · NHT | 창 x 1657–1915 (259) · y 839–1073 (**235 ⚠ 실측** · 표의 ≈240 은 아래 테 포함) |
| `Img_MapBg` | Overlay · Fill · Fill | 단색 · Tint `#000000` | `#000000` |
| `MapImage` | Overlay · HAlign **Center** · VAlign **Center** | 지도 텍스처 (F13 뒤) · Image Size = 지도 텍스처 크기 · **위치는 Render Transform › Translation** 으로 옮긴다 (슬롯 Padding 을 바꾸면 Layout 무효화) · NHT | 길 바닥 `#343636` 근처 · 건물 `#151616` |
| `MarkerLayer` | Overlay · Fill · Fill | 마커 위젯을 **미리 N개** 넣고 Collapsed (풀 — §3.E 위) · 각 마커 슬롯 Center/Center + Render Translation · NHT | 지역명 칸: 글자 Size **10** · 색 `#686868` · 뒤 판 `#141414` 반투명 (예: `골목길` 글자 (1669,845,34,12)) |
| `SelfIcon` | Overlay · HAlign **Center** · VAlign **Center** · Padding 0 | Brush `T_UI_Minimap_Self` (초상 원 + 녹색 고리 — 초상은 H 와 같은 원형 머티리얼) · Image Size **30 × 30** · 고리 색 `#40C818` | (1771,941,30,30) — 중심 (1786,956) = **지도 창 정중앙** |
| `Img_MinimapFrame` | Overlay · Fill · Fill | Brush `T_UI_Minimap_Frame` · Draw As **Border** (테만 그림) · Margin **(1, 28, 3, 5)** px 에 해당하는 비율 · Tint `#323837` · NHT | 오른쪽 테 x 1916–1918 `#323837` · 아래 테 y 1074–1078 `#1C1E1E` · 왼쪽 1px `#2C3235` |

- ⚠ **실측: 자기 아이콘이 지도 창 정중앙이다** (창 중심 (1786, 956) = 아이콘 중심) → 원작은 **지도가 움직이고 내가 가운데** 있는 방식으로 보인다 (스크린샷 1장 근거). 그러면 매 프레임 바뀌는 것은 `MapImage` · `MarkerLayer` 의 Render Translation 이고 `SelfIcon` 은 고정이다. §6 ① 6 의 "자기 위치 Image → Is Volatile" 은 지도 고정식 가정이다 — 어느 쪽인지 **(미확인 — 결정 필요)**. 이 절은 §6 을 바꾸지 않는다
- 지도 창 크기 259 × 235 는 화면 비율(16:9)과 다르다 — 지도 축척은 F13 데이터로 정한다 **(미확인)**

---

### F. 스탯 패널 — `WBP_ER_StatPanel` / `WBP_ER_StatRow` ×8

이미지 아이콘 모양으로 대응을 추정했다. **아이콘 ↔ 스탯 대응은 (미확인)** — 원작 툴팁으로 확인 필요.

| 칸 | 이미지 값 | 추정 스탯 | 데이터 출처 (`GAS/ERAttributeSet.h`) | 상태 |
|---|---|---|---|---|
| 1 | `58` (검) | 공격력 | `AttackPower` `:192-194` | ✅ |
| 2 | `0` (보라) | 스킬 증폭 | `SkillAmp` `:246-248` | ✅ |
| 3 | `0 \| 0%` | 방어 관통 (고정 \| %) (미확인) | `DefPenFlat` `:259-261` · `DefPenPercent` `:255-257` | ✅ (2개 구독) |
| 4 | `61` (방패) | 방어력 | `Defense` `:196-198` | ✅ |
| 5 | `0.6` | 공격 속도 | `AttackSpeed` `:200-202` | ✅ |
| 6 | `0 (0%)` (모래시계) | 스킬 가속 (쿨감 %) (미확인) | `SkillHaste` `:280-282` · % 환산식은 (미확인) | ✅ (값) |
| 7 | `0%` | 치명타 확률 | `CritChance` `:238-240` | ✅ |
| 8 | `3.73` | 이동 속도 | `MoveSpeed` `:204-206` | ✅ |

**갱신 방식**
- 칸마다 `ASC->GetGameplayAttributeValueChangeDelegate(어트리뷰트)` 구독 (`AbilitySystemComponent.h:534` · 공통 §2.2). ASC 는 PlayerState 에 있다 (`Core/ERPlayerState.cpp:38`)
- 어트리뷰트는 전원 복제 `COND_None` + `REPNOTIFY_Always` (`GAS/ERAttributeSet.cpp:296`) → 클라에서 이벤트가 온다
- `WBP_ER_StatRow` 는 **어트리뷰트 1개(또는 2개)를 디자이너 변수로 받는다** → 8칸이 같은 C++ 코드. 표시 형식(정수 / 소수 2자리 / %)도 디자이너 변수 (Enum)

**최적화**
- 패널 전체 Invalidation Box. 스탯은 장비 · 레벨업 · 버프 때만 바뀐다 → Volatile 아님
- 표시 전용 → `Not Hit-Testable (Self & All Children)` (공통 §4.7). ⚠ 원작은 마우스 올리면 툴팁이 있을 수 있음 (미확인) — 그러면 `Self Hit Test Invisible` 로 바꾼다

**이미지 조각:** 스탯 아이콘 8장 `T_UI_Stat_AttackPower` · `_SkillAmp` · `_DefPen` · `_Defense` · `_AttackSpeed` · `_SkillHaste` · `_CritChance` · `_MoveSpeed` (32px 정도 · 정확한 크기 (미확인))

**내부 구조** — 로컬 원점 = 원본 (300, 962) · 크기 160 × 117. 판 바탕은 `BottomPanelBg` 가 그린다 (이 위젯은 바탕 없음)

```
WBP_ER_StatPanel
InvalidationBox  StatInvalidation                 (추가) 루트
└─ UniformGridPanel  StatGrid                     (추가) 2열 × 4행
   ├─ WBP_ER_StatRow  **StatAttackPower**   (0,0) (추가)  58
   ├─ WBP_ER_StatRow  **StatSkillAmp**      (1,0) (추가)  0
   ├─ WBP_ER_StatRow  **StatDefPen**        (0,1) (추가)  0 | 0%
   ├─ WBP_ER_StatRow  **StatDefense**       (1,1) (추가)  61
   ├─ WBP_ER_StatRow  **StatAttackSpeed**   (0,2) (추가)  0.6
   ├─ WBP_ER_StatRow  **StatSkillHaste**    (1,2) (추가)  0 (0%)
   ├─ WBP_ER_StatRow  **StatCritChance**    (0,3) (추가)  0%
   └─ WBP_ER_StatRow  **StatMoveSpeed**     (1,3) (추가)  3.73
                                    (Column, Row) — 칸 ↔ 스탯 대응은 위 표 (미확인 그대로)

WBP_ER_StatRow (엔트리 — §6 ① 3 의 트리)
HorizontalBox  StatRowRoot                        (추가 이름 — 루트)
├─ Image      **StatIcon**
└─ TextBlock  **StatValue**
```

| 위젯 | 부모 슬롯 | 위젯 자체 설정 | 실측 (원본 px) |
|---|---|---|---|
| `StatInvalidation` | 루트 · (BottomPanel HBox 슬롯은 §2 트리 — Padding 0 · Auto) | Can Cache ✔ · Visibility **NHT** (§3.F 최적화) | — |
| `StatGrid` | InvalidationBox 자식 | **Min Desired Slot Width 77 · Height 29.5** · Slot Padding **0** (왼쪽 여백 13 은 `StatIcon` Padding 으로 준다) | 아이콘 1열 x 313 · 2열 x 390 (열 피치 **77 ⚠ 실측** · 표 78) · 행 중심 y 977 / 1006.5 / 1036 / 1065.5 (피치 29.5) |
| `Stat*` (8개) | Grid · Column/Row 위 트리 · HAlign **Left** · VAlign **Center** | `WBP_ER_StatRow` 인스턴스 · 디자이너 변수(어트리뷰트 · 표시 형식 — §3.F 갱신 방식)만 다르게 · 아이콘 Brush 는 인스턴스 변수 (Instance Editable · 표시 옵션) | 2열 → 열 왼쪽 = 77 |
| `StatRowRoot` | (엔트리 루트) | NHT | — |
| `StatIcon` | HBox · VAlign Center · Padding **(13, 0, 0, 0)** · Auto | Brush `T_UI_Stat_*` · Image Size **16 × 16** · Tint `#FFFFFF` (색은 텍스처 — 검 `#F0C060` · 증폭 `#BC8BC2` · 관통 `#C04880` · 방패 `#7CAFCB` · 공속 `#C8B888` · 모래시계 `#D0D0D0` · 치명 `#F05060` · 이속 `#D8D8D8` (근사)) · NHT | 1열 (313,970,16,15) (314,999,14,16) (314,1029,15,15) (313,1058,16,15) · 2열 (390,970,17,15) (392,999,14,16) (393,1028,12,15) (391,1058,15,15) |
| `StatValue` | HBox · VAlign Center · Padding **(9, 0, 0, 0)** · Auto | 기본 텍스트 `58` 등 · Size **11** · 색 `#E8E8E8` · Justification Left · 그림자 없음 · NHT | 1열 글자 x 338–339 · 2열 x 417 · 숫자 높이 10 (973–982) |

- 검산: 2열 아이콘 x = 칸 왼쪽 77 + `StatIcon` Padding 13 = 90 = 원본 390 − 300 ✓ · 1열 = 0 + 13 = 13 = 313 − 300 ✓
- 판 바탕 `#212931` · 스탯 영역과 장비 칸 사이 세로선 x 460 1px `#262A2A` → `BottomPanelBg` 텍스처에 포함 (G 의 `EquipDivider` 로 따로 그려도 된다 — §3.G)
- Hover 툴팁은 (미확인) — 위 최적화 메모 그대로

---

### G. 장비 5칸 — `WBP_ER_ItemSlot` ×5 (bCompact)

| 요소 | 데이터 출처 | 상태 |
|---|---|---|
| 칸 5개 (무기 · 머리 · 옷 · 팔 · 다리) | `EEREquipSlot` (`Item/ERItemTypes.h:33-50`) · `UERInventoryComponent::GetEquippedItem(Slot)` (`Item/ERInventoryComponent.h:144`) · 복제 `Equipped` 전원 (`:189-190` · `ERInventoryComponent.cpp:48`) | 값 ✅ |
| 갱신 이벤트 | `OnEquippedChanged` (`:72`) — ⚠ **클라에서는 무기 칸만** Broadcast (`ERInventoryComponent.cpp:69-78` "다른 칸의 클라 반응(UI)은 아직 없다") | ⚠ 이벤트 부족 |
| 아이콘 · 등급색 | `FERItemRow::Icon` (`TSoftObjectPtr<UTexture2D>` `Item/ERItemData.h:56`) · `Grade` (`:36`) | ✅ (필드) |

- 아이콘은 **소프트 포인터** → 슬롯이 비동기 로드한다. 로드 전에는 빈 칸 이미지
- 장착 · 해제 입력(나중): `ServerEquipFromBag` · `ServerUnequip` (`ERInventoryComponent.h:95,99`) — 위젯은 RPC 를 부르기만 (역기획서 §1)
- ⚠ 이미지에서 칸 순서(위→아래)가 무기·머리·옷·팔·다리인지 **(미확인)**. 일단 `EEREquipSlot` 순서로 둔다

#### 하단 패널 컨테이너 — `WBP_ER_BottomPanel` 내부 구조 (F~M 을 담는 그릇)

로컬 원점 = 원본 (300, 962) · 크기 **783 × 117 ⚠** (레이아웃 표 루트 줄의 1145 는 가방 분리 전 값 — 2026-10-12 결정 메모대로 판 783 × 117)

```
WBP_ER_BottomPanel
Overlay  BottomRoot                          (추가 이름 — §2 의 Overlay)
├─ Image  BottomPanelBg                      판 1장
├─ Image  Img_PillarBg                       (추가) G · V 세로 기둥 (파란 알약)
├─ Image  Img_PillarDivider                  (추가) 기둥 가운데 가로선
└─ HorizontalBox  BottomRow                  (추가 이름 — §2 의 HorizontalBox)
   ├─ WBP_ER_StatPanel  StatPanel            [F]
   ├─ VerticalBox       EquipColumn          [G]
   ├─ WBP_ER_Portrait   Portrait             [H]
   └─ VerticalBox       RightColumn          (추가 이름 — §2 의 VerticalBox)
      ├─ WBP_ER_SkillBar   SkillBar          [I]
      └─ HorizontalBox     VitalRow          (추가 이름) 
         ├─ WBP_ER_VitalBar   VitalBar       [L]
         └─ WBP_ER_CreditBox  CreditBox      [M]
```

| 위젯 | 부모 슬롯 | 위젯 자체 설정 | 실측 (원본 px) |
|---|---|---|---|
| `BottomRoot` | 루트 | Visibility **Self Hit Test Invisible** · **Clipping = Clip to Bounds** (초상 원이 판 위로 안 튀어나오게 — §레이아웃 표 "판 위에서 잘린다") | (300,962,783,117) |
| `BottomPanelBg` | Overlay · Fill · Fill · Padding 0 | Brush `T_UI_BottomPanel_Bg` · Draw As **Box** · Margin 0.1 (근사) · Tint `#212931` · NHT | 판 `#212931` (스탯 쪽) ~ `#2B3135` (스킬 쪽 — 오른쪽으로 갈수록 살짝 밝음 **(근사)**) · 스탯/장비 사이 세로선 x 460 1px `#262A2A` (텍스처에 넣거나 무시) |
| `Img_PillarBg` | Overlay · Left · Top · Padding **(738, 7, 0, 0)** | Brush 단색 · Draw As **Rounded Box** · 반지름 **19** (폭의 반) · Image Size **38 × 97** · Tint `#2F4B6B` · NHT | (1038,969,38,97) **⚠ 실측** (표의 (1040,≈962,38,106) 근사값 대체) |
| `Img_PillarDivider` | Overlay · Left · Top · Padding **(740, 70, 0, 0)** | 단색 · Image Size **34 × 1** · Tint `#1C3245` · NHT | y 1032 · x 1040–1073 |
| `BottomRow` | Overlay · HAlign Left · VAlign Fill · Padding 0 | — | — |
| `StatPanel` | HBox · VAlign Top · Padding **(0, 0, 9, 0)** · Auto | (§3.F) — 폭 154 (2열 × 77) | 장비 칸 시작 x 463 = 154 + 9 |
| `EquipColumn` | HBox · VAlign Top · Padding **(0, 0, 12, 0)** · Auto | (§3.G) — 폭 34 | 초상 시작 509 = 463 + 34 + 12 |
| `Portrait` | HBox · VAlign **Fill** · Padding **(0, 0, 15, 0)** · Auto | (§3.H) — 폭 136 | 스킬바 시작 660 = 509 + 136 + 15 |
| `RightColumn` | HBox · VAlign Top · Padding 0 · Auto | — | — |
| `SkillBar` | VBox · HAlign Left · Padding **(0, 8, 0, 0)** · Auto | (§3.I) — 높이 52 | y 970–1021 |
| `VitalRow` | VBox · HAlign Left · Padding **(0, 11, 0, 0)** · Auto | — | 행 위 y 1033 (= 1021 + 1 + 11) |
| `VitalBar` | HBox · VAlign Top · Padding **(0, 6, 0, 0)** · Auto | (§3.L) — 270 × 16 | (660,1039,270,16) |
| `CreditBox` | HBox · VAlign Top · Padding **(10, 0, 0, 0)** · Auto | (§3.M) | 크레딧 판 x 940 = 930 + 10 |

- 기둥(G 슬롯 + `0/15` + V)은 스킬바 줄과 체력 줄에 **걸쳐 있다** → 기둥 바탕만 컨테이너 Overlay 에 따로 깔고, 위쪽 G 는 `SkillBar` 의 `SlotG`, 아래쪽 V 는 `CreditBox` 안 버튼으로 그 위에 놓는다
- `SlotF` · `SlotG` 가 Collapsed 여도 기둥 바탕은 남는다 (레퍼런스 모양 유지). G 를 영영 안 쓰면 `Img_PillarBg` 높이를 아래 반(32)으로 줄인다 **(결정 필요 — 미확인)**

**내부 구조 (G)** — 로컬 원점 = 원본 (463, 962) · 크기 34 × 116

```
VerticalBox  EquipColumn                        (§2 · 하단 패널 HBox 안)
├─ WBP_ER_ItemSlot  **EquipWeapon**             칸 1
├─ WBP_ER_ItemSlot  **EquipChest**              칸 2  ⚠ 순서 실측 (아래)
├─ WBP_ER_ItemSlot  **EquipHead**               칸 3
├─ WBP_ER_ItemSlot  **EquipArm**                칸 4
└─ WBP_ER_ItemSlot  **EquipLeg**                칸 5
```

| 위젯 | 부모 슬롯 | 위젯 자체 설정 | 실측 (원본 px) |
|---|---|---|---|
| `EquipColumn` | (위 컨테이너 표) | Visibility **Self Hit Test Invisible** (칸은 클릭) | x 463–496 |
| `Equip*` (5개) | VBox · HAlign Fill · VAlign Top · Padding **(0, 0, 0, 1.5)** (마지막 0) · Auto | `WBP_ER_ItemSlot` · **`bCompact` ✔** (→ 엔트리 Desired 34 × 22 — §3.N 엔트리 표) · `bShowKeyLabel` ✘ · `bShowRatio` ✘ · `bShowGradeBg` ✔ | 칸 위 y 962 / 986 / 1010 / 1033 / 1056 (피치 ≈23.5) · 폭 34 |
| 칸 바탕 (찬 칸) | 엔트리 `GradeBg` | 위→아래 그라데이션 `#353435` → `#535353` + 맨 아래 1px `#797979` (일반 등급 회색으로 보임 — 등급색 (미확인)) | 칸 1 · 2 · 5 |
| 칸 바탕 (빈 칸) | 엔트리 `GradeBg` | 단색 `#313A40` (그라데이션 없음) | 칸 3 · 4 |
| 빈 칸 실루엣 | 엔트리 `IconImage` | 칸 종류별 회색 실루엣 (칸 3 = 모자 · 칸 4 = 장갑) · Tint `#8A8A8A` · 이미지 조각 `T_UI_EquipEmpty_Weapon/Chest/Head/Arm/Leg` (추가 · 5장) — 비었을 때 C++ 가 넣는다 | 칸 3 회색 모자 (≈472–488, 1015–1024) |

- ⚠ **실측: 빈 칸 실루엣이 칸 3 = 모자(머리) · 칸 4 = 장갑(팔)** 이다 → 위→아래 순서는 **무기 · 옷 · 머리 · 팔 · 다리** 로 보인다 (칸 2 의 아이템도 옷 모양). 위 "`EEREquipSlot` 순서로" 메모와 다를 수 있다 — VerticalBox 자식 **순서만** 이렇게 두고 이름은 바꾸지 않는다. `EEREquipSlot` 의 실제 열거 순서와의 대조는 **(미확인 — 소스 미대조)**
- Hover/Pressed: 엔트리 공통 (§3.N)

---

### H. 초상 — `WBP_ER_Portrait`

| 요소 | 데이터 출처 | 상태 |
|---|---|---|
| 얼굴 이미지 | `UERCharacterData` (`Character/ERCharacterData.h:31`) 에 **초상 필드 없음** (grep `Portrait` 0건) | ❌ 미구현 (필드 추가) |
| 레벨 "1" | `UERGrowthComponent::Level` (`Growth/ERGrowthComponent.h:82-83` · 전원 복제 `ERGrowthComponent.cpp:38`) · `GetLevel()` `:34` · `OnRep_Level` 은 **로그만** (`ERGrowthComponent.cpp:474-478` "F17 HUD 훅 자리") · `OnLevelUp` (`:31-32`) 은 서버 경로 | ⚠ 클라 이벤트 없음 |
| 경험치 진행 (원 테두리?) | `Exp` (`:86-87` · OwnerOnly `ERGrowthComponent.cpp:39`) / 필요량 `FindLevelRow` (`:37`). 이미지에서 진행 링이 보이는지 **(미확인)** | ⚠ 이벤트 없음 |
| 파란 육각 배지 | (미확인 §1.3) | ❌ |

- 레벨 원 · 경험치는 이벤트 때만 → Volatile 아님
- 이미지 조각: `T_UI_Portrait_Mask` (원형 마스크 — 머티리얼로 원형 자르기 `M_UI_PortraitCircle`) · `T_UI_Portrait_Frame` · `T_UI_Level_Badge` (레벨 원)
  - 원형 자르기를 머티리얼로 하면 이미지 1장으로 모든 실험체를 쓴다 — 공통 §4.9 "값 하나로 그리는 것은 머티리얼"

**내부 구조** — 로컬 원점 = 원본 (509, 962) · 크기 136 × 117 (원은 위로 18px 잘린다)

```
WBP_ER_Portrait
Overlay  PortraitRoot                         (추가) 루트
├─ Image  Img_PortraitRing                    (추가) 바깥 어두운 원 테 (지름 136)
├─ Image  **ExpRing**                         (추가) 경험치 고리 — 오른쪽 옅은 파란 호 (미확인)
├─ Image  **PortraitImage**                   (추가) 얼굴 (원형 머티리얼)
├─ Overlay  LevelBadge                        (추가) 왼쪽 아래 레벨 원
│  ├─ Image      Img_LevelBadgeBg             (추가)
│  └─ TextBlock  **LevelText**  "1"           (추가)
└─ Image  **HexBadge**                        (추가) 오른쪽 위 파란 육각 — Collapsed (뜻 미확인 §1.3)
```

| 위젯 | 부모 슬롯 | 위젯 자체 설정 | 실측 (원본 px) |
|---|---|---|---|
| `PortraitRoot` | 루트 | Visibility **NHT** (§6 ① 6) · Clipping 은 컨테이너 `BottomRoot` 가 한다 | (509,962,136,117) |
| `Img_PortraitRing` | Overlay · Left · Top · Padding **(0, −18, 0, −1)** (음수 Padding — 위로 18 올려 판 위에서 잘리게. 아래 −1 은 높이 136 을 다 받게 하는 값) | Brush `T_UI_Portrait_Frame` (원 테 한 장) · Image Size **136 × 136** · Tint `#FFFFFF` (텍스처: 바깥 테 4px `#171718` · 테 안쪽 판 `#212931`) | 원 x 509/510–645 · 중심 (577, 1012) |
| `ExpRing` | Overlay · Left · Top · Padding **(0, −18, 0, −1)** | `M_UI_CooldownRadial` 의 MID 재사용 (스칼라 `Progress` = 경험치 비율) **(가안)** · Image Size 136 × 136 · 색 `#496073` 근처 · NHT | 오른쪽 호 x 643–645 (y 1012 에서) — 진행 링인지 **(미확인)** |
| `PortraitImage` | Overlay · Left · Top · Padding **(13, −5, 0, 0)** | Brush = `M_UI_PortraitCircle` MID (텍스처 파라미터 = 실험체 초상 · 데이터 필드 생긴 뒤 §7.2) · Image Size **110 × 110 (근사)** · NHT | 얼굴 원 x ≈522–632 (지름 ≈110 · 표의 ≈109) |
| `LevelBadge` | Overlay · Left · Top · Padding **(5, 87, 0, 0)** | NHT | (514,1049,30,30) |
| `Img_LevelBadgeBg` | Overlay · Fill · Fill | Brush `T_UI_Level_Badge` (고리 + 안쪽 판 한 장) · Image Size **30 × 30** · 텍스처 색: 고리 `#40C818` (두께 ≈3) · 안 `#021601` | 위 |
| `LevelText` | Overlay · HAlign Center · VAlign Center · Padding (0, 0, 0, 1) (근사) | `1` · Size **17** · 색 `#F8F8F8` · Justification Center · 그림자 없음 | 숫자 높이 ≈16 |
| `HexBadge` | Overlay · Left · Top · Padding **(115, 3, 0, 0)** | 파란 육각 + 흰 아이콘 한 장 · Image Size **24 × 27** · Tint `#FFFFFF` (텍스처: 육각 `#1060C8` · 아이콘 `#F8F8F8` 14×17) · **Collapsed** | (624,965,24,27) |

- 원 왼쪽 아래 테 위에 옅은 세로 글자 `ETERNAL RETURN 2023` 이 있다 — 장식이라 `T_UI_Portrait_Frame` 텍스처에 넣는다 (따로 위젯 안 만든다)
- 음수 Padding 이 디자이너에서 어색하면 `Img_PortraitRing` · `ExpRing` 만 Render Transform › Translation Y −18 로 대신해도 된다 — 단 그 경우 슬롯 높이 117 에 눌려 원이 136 이 안 될 수 있다 (Overlay 는 비 Fill 자식 크기를 남은 공간으로 자른다) **(근사 — 디자이너에서 확인)**

---

### I. 스킬바 — `WBP_ER_SkillBar` / `WBP_ER_SkillSlot` ×8

| 요소 | 데이터 출처 | 상태 |
|---|---|---|
| 슬롯 Q W E R P D | `ERTags::Ability_Slot_Q/W/E/R/P/D` (`GAS/ERGameplayTags.h:140-146`) — 스펙의 DynamicAbilityTags 로 찾는다 (`GAS/ERSkillData.h:559` 주석) | ✅ |
| 슬롯 F · G | 태그 없음. ~~역기획서 §9: F(전술) 컷~~ → **F = 전술 스킬 추가 예정** (사용자 2026-10-12 컷 철회) · G 뜻 (미확인) | ❌ 미구현 — 그때까지 Collapsed |
| 스킬 아이콘 | `UERSkillData` 에 **아이콘 필드 없음** (grep `Icon` → `ERItemData.h:56` 1건뿐) | ❌ 미구현 (필드 추가) |
| 쿨다운 | 쿨다운 GE 의 태그 `Cooldown_Slot_Q/W/E/R/P/D` (`ERGameplayTags.h:168-173`) · D 는 무기별 `Cooldown_Weapon_*` 로 덮어쓸 수 있다 (`:177-185` · `UERSkillData.CooldownTagOverride`) | ✅ |
| 스킬 레벨 눈금 `— — —` | 현재 레벨 = 그 스펙의 Level (ASC · 서버가 바꾸고 소유자에게 복제 `Core/ERPlayerState.h:71-72` 주석) · 최대 `UERSkillData::MaxLevel` (`ERSkillData.h:477`) | 값 ✅ / **클라 이벤트 없음** ⚠ |
| 키 글자 Q/W/E/R/D | 지금은 고정 텍스트. 키 재설정 반영은 (미확인 — 입력 설정 `Character/ERInputConfig.h:77-78`) | — |
| 잠김(빈사 · 침묵 등) 어둡게 | `State_Block_Skill` (`:97`) · `State_Downed` (`:72`) · `State_Unarmed` (`:106`) | ✅ (태그 이벤트) |

**쿨다운 갱신 방식** (공통 §4.4 를 이 화면에 적용)
1. 슬롯마다 `ASC->RegisterGameplayTagEvent(Cooldown 태그, NewOrRemoved)` 구독 (`AbilitySystemComponent.h:744`)
2. **태그가 붙는 순간** `ASC->GetActiveEffectsTimeRemainingAndDuration(FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(태그))` 로 남은 시간 · 전체 시간을 **한 번** 얻는다 (`AbilitySystemComponent.h:836` · `GameplayEffect.h:1476`). 스킬 가속은 서버가 GE 지속시간에 이미 반영했다 (`ERGameplayTags.h:223` "ApplyCooldown 이 계산해서 넣는다") → UI 는 다시 계산하지 않는다
   - 플레이어 ASC 는 `Mixed` (`Core/ERPlayerState.cpp:46`) → GE 는 **소유자에게** 복제된다 (`AbilitySystemComponent.h:87` 부근 복제 모드 설명). 내 쿨다운은 보이고 남의 쿨다운은 안 온다 — 정상
3. 그 순간부터 슬롯에 **`FTimerManager` 주기 타이머**(0.1초 — 역기획서 §3 #2 "숫자 카운트다운만 로컬 0.1초 타이머")를 건다. 매 주기: 원형 게이지 머티리얼의 스칼라 `Progress` + 숫자 텍스트
4. **태그가 빠지면** 타이머 해제 · 게이지 숨김
   - 머티리얼 파라미터 변경은 **Paint 무효화**(비용 Low) — Epic: Paint 는 *"if you change non-layout parameters, like color or materials"* ([Invalidation in Slate and UMG](https://dev.epicgames.com/documentation/unreal-engine/invalidation-in-slate-and-umg-for-unreal-engine))
   - 대안 — 시작 시각 · 지속시간만 머티리얼에 넣고 GPU 가 `Time` 으로 돌리기: Epic *"Material-only animations incur no CPU cost"* ([Optimization Guidelines for UMG](https://dev.epicgames.com/documentation/unreal-engine/optimization-guidelines-for-umg-in-unreal-engine)). 그러나 UI 머티리얼 `Time` 과 월드 시간을 맞추는 방법이 **(미확인)** (공통 §4.4-3) → **기본은 3번 타이머**, 확인되면 바꾼다
   - **Active Timer 는 쓰지 않는다** — 게임 UI 용이 아니고 *"if a single active timer needs to execute, all of Slate will be ticked"* ([Slate UI Sleeping and Active Timers](https://dev.epicgames.com/documentation/unreal-engine/slate-ui-sleeping-and-active-timers-in-unreal-engine))

**최적화**
- `SkillBar` 전체 Invalidation Box
- **쿨다운 중일 때만** 그 슬롯의 `CooldownOverlay`(게이지 Image) 와 `CooldownText` 를 Volatile 로 — 공통 §4.2 "쿨다운 진행 중 숫자·게이지". Volatile 은 **이 두 위젯만**, 슬롯 전체에 걸지 않는다 (공통 §4.2 ⚠). 런타임에 Volatile 을 껐다 켤 수 있는지 **(미확인 — `SetVolatile` 류 API 미대조)**. 못 하면 상시 Volatile 로 두되 그 두 위젯으로 한정
- 아이콘 교체(무기 바꿔 D 가 바뀜)는 이벤트 때만

**`WBP_ER_SkillSlot` 내부 (BindWidget 이름)**
```
Overlay  SlotRoot
├─ Image          IconImage          스킬 아이콘
├─ Image          CooldownOverlay    M_UI_CooldownRadial 의 MID (Progress 0~1)
├─ Image          DisabledOverlay    잠김 시 어둡게 (Collapsed 기본)
├─ TextBlock      CooldownText       남은 초 (Collapsed 기본)
├─ TextBlock      KeyText            Q/W/E/R/D (좌하단)
└─ HorizontalBox  LevelPips          레벨 눈금 — Image × MaxLevel(최대 5 · ERSkillData.h:477) 미리 생성
```

**이미지 조각**
- `T_UI_SkillSlot_Frame` (9-slice) · `T_UI_SkillSlot_Frame_D` (D 슬롯 붉은 테두리 — 이미지에서 D/F/G 는 붉은 계열)
- `T_UI_SkillPip_On` · `T_UI_SkillPip_Off` (레벨 눈금)
- `M_UI_CooldownRadial` (Material Domain = User Interface · 스칼라 `Progress` · 원형 와이프) — 텍스처 없이 머티리얼 수식으로 가능
- 스킬 아이콘 — **실험체 × 5 + 무기 D × 8** 만큼. 원본 이미지 확보 경로 **(미확인)**

**내부 구조 (보강)** — 위 `WBP_ER_SkillSlot` 트리의 이름은 그대로 두고 잎 · 설정을 채운다. 로컬 원점 = 원본 (660, 970) · 크기 ≈416 × 53

```
WBP_ER_SkillBar
InvalidationBox  SkillInvalidation                 (추가) 루트
└─ HorizontalBox  SkillRow                         (추가 이름 — §2 의 HorizontalBox)
   ├─ SizeBox SB_SlotQ  └─ WBP_ER_SkillSlot **SlotQ**     (SizeBox 추가 — 슬롯마다 모양 · 크기가 달라서)
   ├─ SizeBox SB_SlotW  └─ **SlotW**
   ├─ SizeBox SB_SlotE  └─ **SlotE**
   ├─ SizeBox SB_SlotR  └─ **SlotR**
   ├─ SizeBox SB_SlotP  └─ **SlotP**               작은 네모 · 키 글자 없음
   ├─ SizeBox SB_SlotD  └─ **SlotD**               원형
   ├─ SizeBox SB_SlotF  └─ **SlotF**               원형 · Collapsed (전술 스킬 추가 예정)
   └─ SizeBox SB_SlotG  └─ **SlotG**               원형 · 기둥 안 · Collapsed (뜻 미확인)

WBP_ER_SkillSlot (엔트리 — 위 트리 + 잎 보강)
Overlay  SlotRoot
├─ Image          SlotFrame                (추가) 네모 테 1px — 원형 · P 는 Collapsed
├─ Image          IconImage
├─ Image          CooldownOverlay
├─ Image          DisabledOverlay
├─ TextBlock      CooldownText
├─ Border         KeyBadge                 (추가) 키 글자 뒤 검은 칸
│  └─ TextBlock   KeyText
├─ Border         PipStrip                 (추가) 레벨 눈금 줄 바탕 (검정)
│  └─ HorizontalBox  LevelPips
│     └─ Image  Pip0 ~ Pip4                (추가) 눈금 5개 미리 — MaxLevel 넘는 것은 Collapsed
└─ HorizontalBox  ChargeBox                (추가) G 의 `0/15` — 기본 Collapsed
   ├─ TextBlock  ChargeCurText  "0"        (추가)
   └─ TextBlock  ChargeMaxText  "/15"      (추가)
```

**`SkillBar` 배치**

| 위젯 | 부모 슬롯 (HorizontalBox) | SizeBox Override (W × H) | 실측 (원본 px) |
|---|---|---|---|
| `SkillInvalidation` | 루트 | Can Cache ✔ · Visibility **Self Hit Test Invisible** | — |
| `SB_SlotQ` | VAlign Top · Padding **0** · Auto | **48 × 52** (아이콘 48 + 눈금 줄 4) | (660,970) 테 · 눈금 줄 y 1018–1021 |
| `SB_SlotW` · `_E` · `_R` | VAlign Top · Padding **(11, 0, 0, 0)** · Auto | 48 × 52 | x 719 / 778 / 837 (피치 **59 ⚠ 실측** · 표 59.5) |
| `SB_SlotP` | VAlign Top · Padding **(11, 0, 0, 0)** · Auto | **33 × 37** | (896,970,33,37) |
| `SB_SlotD` | VAlign Top · Padding **(13, 0, 0, 0)** · Auto | **39 × 37** | 원 x 943–980 · 키 칸 x 942 |
| `SB_SlotF` | VAlign Top · Padding **(7, 0, 0, 0)** · Auto | **38 × 37** | x 988–1025 |
| `SB_SlotG` | VAlign Top · Padding **(7, 0, 0, 0)** · Auto | **42 × 53** (근사) | 키 칸 x 1033 · 원 (1041,974,32,32) · `0/15` 아래 끝 y 1022 |
| `SlotQ` ~ `SlotG` | SizeBox 자식 (채움) | 인스턴스 변수: 슬롯 태그 (§3.I 위) · **`bCompactSlot`** (추가 · Boolean · Instance Editable) — Q·W·E·R ✘ / P·D·F·G ✔ · `KeyLabel` (Text — Q/W/E/R/D/F/G · P 는 빈 값 → KeyBadge Collapsed) | — |

**엔트리 `WBP_ER_SkillSlot` — 위젯 설정** (값이 둘이면 `네모(✘) / 작은·원형(✔)` = `bCompactSlot`. Pre Construct 에서 **Padding · Visibility 만** 바꾼다 — 표시 옵션, 로직 아님 · §2.1 과 같은 성격)

| 위젯 | 부모 슬롯 (Overlay) | 위젯 자체 설정 | 실측 (원본 px) |
|---|---|---|---|
| `SlotRoot` | 루트 | Visibility **Visible** (클릭 · 툴팁 대비) | — |
| `SlotFrame` | Fill · Fill · Padding **(0,0,0,4) / Collapsed** | 단색 테 · Draw As **Border** · Margin 1px 해당 · Tint `#344048` (위쪽 `#404F58`) · NHT | Q 테 x 660 · 707 · y 970 · 1017 |
| `IconImage` | Fill · Fill · Padding **(1,1,1,5) / (0,0,0,0)** | 네모: 스킬 아이콘 Draw As Image · Tint `#FFFFFF` / 원형(D·F·G): Brush = `M_UI_PortraitCircle` MID (원형 자르기 — H 와 같은 머티리얼) · P: 네모 그대로 · NHT | Q 아이콘 46 × 46 (661–706, 971–1016) · D 원 지름 38 · G 원 32 |
| `CooldownOverlay` | `IconImage` 와 같은 Padding | `M_UI_CooldownRadial` MID · 어둡게 칠할 색 `#000000` 알파 0.6 **(가안 — 레퍼런스에 쿨다운 중 슬롯 없음)** · 기본 **Collapsed** · NHT · Volatile (§3.I 최적화) | — |
| `DisabledOverlay` | `IconImage` 와 같은 Padding | 단색 `#000000` 알파 0.5 **(가안)** · **Collapsed** · NHT | — |
| `CooldownText` | HAlign Center · VAlign Center · Padding (0,0,0,4) / 0 | `3` · Size **16 (가안)** · 색 `#FFFFFF` · 그림자 Offset (1,1) `#000000` 알파 0.8 (가안) · Center · **Collapsed** · NHT · Volatile | — |
| `KeyBadge` | HAlign **Left** · VAlign **Bottom** · Padding **(1,0,0,5) / (0,0,0,0)** | Border · Brush 단색 Tint `#000000` · Padding 0 · `KeyLabel` 이 비면 Collapsed · NHT | Q (661,1003,14,14) · D (942,993,14,14) · G (1033,999,14,14) |
| `KeyText` | Border 자식 · HAlign Center · VAlign Center | `Q` · Size **9** **⚠ 실측** (글자 7~8px · 표 10) · 색 `#E0E0E0` · Center · Min Desired Width **14** (칸 폭) | Q 글자 (665,1006,6,8) |
| `PipStrip` | HAlign **Fill** · VAlign **Bottom** · Padding **(1,0,1,0) / (14,0,1,0)** — P 는 `(1,0,1,0)` (키 칸이 없어서) | Border · Brush 단색 Tint `#000000` · Padding **(4, 1, 4, 1) / (2, 1, 2, 1)** · 높이 = 자식 2 + 1 + 1 = **4** · NHT | Q 줄 (661,1018,46,4) · D 줄 x 955–979 · P 줄 x 897–928 |
| `LevelPips` | Border 자식 · Fill · Fill | — | — |
| `Pip0~4` | HBox · VAlign Fill · Padding **(0,0,3,0)** (마지막 보이는 것 0) · Size **Fill 1.0** | 단색 Image Size (1 × **2**) · Off Tint `#4F4F4F` · On Tint 슬롯마다 다름: P `#E7B040` (주황) · F `#6BC2E2` (하늘) — Q·W·E·R·D 의 On 색은 **(미확인 — 레퍼런스가 전부 0레벨)** · MaxLevel 보다 큰 번호는 Collapsed | Q 5칸: x 665 / 673 / 681 / 690 / 698 (폭 5) · R 3칸: 842–852 / 856–865 / 869–879 (폭 10) — Fill 이라 개수에 맞춰 늘어난다 |
| `ChargeBox` | HAlign Center · VAlign Bottom · Padding (0,0,0,0) | **Collapsed** (SlotG 에서만 Visible) · NHT | (1049,1014,24,10) |
| `ChargeCurText` | HBox · Auto | `0` · Size **9** · 색 `#C84040` | (1049,1015,3,8) |
| `ChargeMaxText` | HBox · Auto | `/15` · Size **9** · 색 `#E0E0E0` | (1055,1014,18,10) |

- Hover/Pressed: 레퍼런스에 없음 **(미확인)** — 가안: Hover 때 `SlotFrame` Tint `#344048` → `#7A93A6` (색만). 원형 슬롯은 `IconImage` Tint `#FFFFFF` → `#E8E8E8`
- `SB_SlotF` · `SB_SlotG` 를 Collapsed 하면 HorizontalBox 가 폭을 당긴다 — 2026-10-12 결정대로 괜찮다 (오른쪽 끝이라 다른 슬롯은 안 밀린다)
- SizeBox 8개는 Invalidation Box 안이라 슬롯이 바뀔 때만 레이아웃을 다시 잰다 (공통 §4.5 의 "SizeBox 는 여러 패스" 비용은 캐시 뒤로 숨는다 **(근사 — 측정 전)**). SizeBox 없이 하려면 `bCompactSlot` Pre Construct 에서 `IconImage` Image Size 를 넣는 방법도 있다

---

### J. 스킬 강화 알림 — `WBP_ER_SkillUpPrompt`

| 요소 | 데이터 출처 | 상태 |
|---|---|---|
| "스킬 강화 포인트 +1" | `AERPlayerState::SkillPoints` (`Core/ERPlayerState.h:75-76` · OwnerOnly `ERPlayerState.cpp:192`) · `OnRep_SkillPoints` **로그만** (`ERPlayerState.cpp:469-473` "UI 가 생기면 여기서 갱신한다") | ⚠ 이벤트 없음 |
| 마름모 밝음/어두움 (올릴 수 있는 슬롯) | 서버 판정 조건: 포인트>0 · 슬롯 존재 · `bUsesSkillPoints` · Level<MaxLevel · `MinCharacterLevel` (`ERPlayerState.h:79-82` 주석 · `GAS/ERSkillData.h:584`). **클라 표시용 선판정 함수 없음** | ❌ 미구현 (표시용 판정) |
| 마름모 클릭 → 레벨업 | `AERPlayerState::ServerLevelUpSkill(FGameplayTag SlotTag)` (`:86-87` · Server Reliable WithValidation) | ✅ (RPC) |

- 클라 선판정은 **표시용일 뿐** — 서버가 다시 검증한다 (역기획서 §1 "표시용 선판정")
- "포인트가 남았는데 전부 만렙" 은 정상 (`ERPlayerState.h:82`) → 전부 어두우면 알림 텍스트도 숨긴다 (자체 결정값)
- 노란 하이라이트 3줄은 **BP 위젯 애니**(공통 §4.5 짧은 연출 = 머티리얼 > BP 트윈 > Sequencer). 반짝임이 반복이면 **머티리얼 애니**로 — Epic: *"Material-only animations incur no CPU cost"*
- 최적화: 포인트 없으면 **Collapsed** (공통 §4.7 오래 숨기는 것)
- 이미지 조각: `T_UI_SkillUp_Diamond` (마름모 · 밝음/어두움은 같은 텍스처 + Tint) · `T_UI_SkillUp_Glow` (세로 그라데이션 하이라이트)

**내부 구조** — 로컬 원점 = 원본 (658, 863) · 크기 272 × 100 · 아래 끝이 하단 패널 위(962)에 닿는다

```
WBP_ER_SkillUpPrompt
Overlay  SkillUpRoot                              (추가) 루트
├─ HorizontalBox  GlowRow                         (추가) 노란 세로 기둥 — 슬롯 피치 59
│  ├─ Image  GlowQ   (추가)
│  ├─ Image  GlowW   (추가)
│  ├─ Image  GlowE   (추가)
│  ├─ Image  GlowR   (추가)  ← 이 화면에선 Collapsed (R 은 못 올림)
│  └─ Image  GlowP   (추가)  ← 크기 (미확인)
├─ TextBlock  **SkillUpText**  "스킬 강화 포인트 +1"   (추가)
├─ Button  **LevelUpBtnQ**  └─ Image  Img_DiamondQ    (추가)
├─ Button  **LevelUpBtnW**  └─ Image  Img_DiamondW    (추가)
├─ Button  **LevelUpBtnE**  └─ Image  Img_DiamondE    (추가)
├─ Button  **LevelUpBtnR**  └─ Image  Img_DiamondR    (추가)
└─ Button  **LevelUpBtnP**  └─ Image  Img_DiamondP    (추가)
```

| 위젯 | 부모 슬롯 | 위젯 자체 설정 | 실측 (원본 px) |
|---|---|---|---|
| `SkillUpRoot` | 루트 | Visibility **Self Hit Test Invisible** (마름모만 클릭) · 포인트 0 이면 위젯째 Collapsed (§3.J 위) | (658,863,272,100) |
| `GlowRow` | Overlay · Left · **Bottom** · Padding **(0, 0, 0, 0)** | NHT | 기둥 아래 끝 y 961 |
| `GlowQ` · `W` · `E` · `R` | HBox · VAlign Bottom · Padding **(0,0,5,0)** · Auto | Brush `T_UI_SkillUp_Glow` (아래 진하고 위로 투명해지는 세로 그라데이션 흰 판) · Draw As Image · Image Size **54 × 99** · Tint `#E8B33A` · 알파 **0.7 (근사 — 월드와 섞여 위 `#5D6665` → 아래 `#9A8C65` 로 보임)** · NHT · 올릴 수 없는 슬롯은 Collapsed 가 아니라 **Hidden** (자리 유지) | x 657–710 / 716–769 / 775–828 (폭 54 · 피치 59) — 슬롯 테보다 좌우 3px 넓다 |
| `GlowP` | HBox · VAlign Bottom · Padding 0 · Auto | Image Size **39 × 99 (가안 — P 슬롯 33 + 6)** · Hidden | 이 화면에선 안 켜짐 **(미확인)** |
| `SkillUpText` | Overlay · Left · Top · Padding **(120, 16, 0, 0)** (글자 위 18 − 여백 2) | `스킬 강화 포인트 +1` · Size **12** **⚠ 실측** (한글 14px · 표 13) · 색 `#E0C068` · Left · **Outline Size 1 · 색 `#000000`** (글자 테두리가 검게 보인다 — 그림자인지 외곽선인지 (근사)) · NHT | (778,881,133,14) |
| `LevelUpBtnQ` | Overlay · Left · Top · Padding **(4, 42, 0, 0)** | Style Normal/Hovered/Pressed: Draw As **None** (모양은 자식 Image) · Padding 0 · Is Focusable ✘ | 마름모 중심 x 685 |
| `LevelUpBtnW` | 〃 Padding **(63, 42, 0, 0)** | 〃 | 중심 744 |
| `LevelUpBtnE` | 〃 Padding **(122, 42, 0, 0)** | 〃 | 중심 803 |
| `LevelUpBtnR` | 〃 Padding **(183, 42, 0, 0)** | 〃 · 올릴 수 없으면 **Is Enabled ✘** (Hidden 아님 — 어두운 마름모가 보인다) | 중심 864 |
| `LevelUpBtnP` | 〃 Padding **(232, 42, 0, 0)** | 〃 | 중심 913 |
| `Img_Diamond*` | Button 슬롯 · Center · Center · Padding 0 | Brush `T_UI_SkillUp_Diamond` (밝은 노란 마름모 + 위 꺾쇠 + 아래 점) · Image Size **46 × 45** · 밝음 Tint `#FFFFFF` (텍스처 색 `#E0B038` ~ `#FFFBC8`) · 어두움 Tint `#444444` 알파 0.7 **(근사 — 결과색 `#423D38`)** · NHT | 밝음 (662,~906,44,45) · 어두움 (891,907,44,44) |

- 마름모 x 위치는 슬롯 중심을 따르지만 간격이 일정하지 않다 (59 / 59 / 61 / 49) — R · P 슬롯 중심을 따라가서다. 그래서 HorizontalBox 대신 **Overlay + Padding 개별 지정**으로 뒀다
- Hover/Pressed **(미확인)** — 가안: Button Style 은 그대로 두고 Hover 때 `Img_Diamond*` Tint `#FFFFFF` 유지 + 크기 변화 없음, Pressed 때 Tint `#C8C8C8` (색만). Button 의 Hovered/Pressed Brush 를 Draw As None 으로 둔 이유: 모양이 자식 Image 라 Button 바탕이 겹치면 사각형이 보인다
- 기둥 반짝임 애니는 §3.J 위 메모 (머티리얼 애니 우선)

---

### K. 버프 칸 — `WBP_ER_BuffBar` / `WBP_ER_BuffIcon`

| 요소 | 데이터 출처 | 상태 |
|---|---|---|
| 버프 목록 | `ASC->OnActiveGameplayEffectAddedDelegateToSelf` — 엔진 주석 *"Called on both client and server whenever a duration based GE is added"* (`AbilitySystemComponent.h:518-519`) · 제거는 `OnAnyGameplayEffectRemovedDelegate` (미확인 — 이름 미대조) | 값 ✅ |
| 아이콘 · 이름 | GE 에 **UI 데이터 없음** (grep `Icon` 0건) | ❌ 미구현 |
| 남은 시간 · 중첩 | GE 지속시간 · 스택 (엔진) | ✅ |

- 이미지에는 **1칸만** 보인다. 원작 위치 · 중첩 표시 규칙은 역기획서 미확인 목록 "버프 바의 정확한 화면 위치" — 이 스크린샷이 위치 근거 1건이 된다 (스킬바 위 좌측)
- 칸은 **미리 N개 만들어 Collapsed** — 추가/삭제마다 Child 무효화를 피한다 (공통 §4.1). N 은 **(미확인)** → 일단 8 (자체 결정값)
- 남은 시간 표시는 I 의 쿨다운과 같은 방식 (진행 중일 때만 타이머)
- 이미지 조각: `T_UI_Buff_Frame` (버프/디버프 테두리 색 2종 — Tint 로 구분)

**내부 구조** — 로컬 원점 = 원본 (657, 830) **⚠ 실측** (표는 658 — 테 2px 를 넣으면 657) · 크기 252 × 30 (8칸 · 근사)

```
WBP_ER_BuffBar
InvalidationBox  BuffInvalidation               (추가) 루트
└─ HorizontalBox  BuffRow                       (추가)
   └─ WBP_ER_BuffIcon  **Buff0** ~ **Buff7**    (추가) 8칸 미리 · 전부 Collapsed (§6 ① 4)

WBP_ER_BuffIcon (엔트리 — §6 ① 3 의 트리 + 테)
Overlay  BuffRoot                               (추가 이름)
├─ Image      BuffFrame                         (추가) 2px 테 (버프/디버프 색)
├─ Image      **IconImage**
├─ TextBlock  **TimeText**                      남은 초 — 아래 가운데
└─ TextBlock  **StackText**                     중첩 수 — 우상단
```

| 위젯 | 부모 슬롯 | 위젯 자체 설정 | 실측 (원본 px) |
|---|---|---|---|
| `BuffInvalidation` | 루트 | Can Cache ✔ · Visibility **Self Hit Test Invisible** (툴팁 대비 — 툴팁 없으면 NHT) | — |
| `BuffRow` | InvalidationBox 자식 | — | — |
| `Buff0~7` | HBox · VAlign Top · Padding **(0, 0, 2, 0)** (자체 결정값 — 간격 (미확인) · 표의 "4" 는 28 칸 기준 → 30 칸이면 2 로 같은 피치 32) · Auto | 인스턴스 · 기본 **Collapsed** · Collapsed 라 앞에서부터 채우면 빈자리 없이 왼쪽 정렬 | 보이는 1칸 (657,830,30,30) |
| `BuffRoot` | (엔트리 루트) | — | — |
| `BuffFrame` | Overlay · Fill · Fill | Brush `T_UI_Buff_Frame` · Draw As **Border** · Margin 2px 해당 · Image Size **30 × 30** (엔트리 크기를 이 값이 정한다) · 버프 Tint `#46B3D0` (아래 · 오른쪽 변은 텍스처 음영으로 `#2980A1`) · 디버프 Tint **(미확인 — 레퍼런스에 없음)** · NHT | 테 위 · 왼쪽 `#46B3D0` · 아래 · 오른쪽 `#2980A1` · 두께 2 |
| `IconImage` | Overlay · Fill · Fill · Padding **(2, 2, 2, 2)** | GE 아이콘 (§7.2 — 데이터 생긴 뒤) · Image Size 26 × 26 · Tint `#FFFFFF` · NHT | 안쪽 (659,832,26,26) — 분홍 · 보라 그림 |
| `TimeText` | Overlay · HAlign Center · VAlign Bottom · Padding (0, 0, 0, 1) | `5` · Size **9 (가안)** · 색 `#FFFFFF` · Outline 1 `#000000` (가안) · Center · **Collapsed** 기본 · NHT · Volatile (§4 표 "남은 시간 숫자만") | 레퍼런스에 남은 시간 표시 없음 **(미확인)** |
| `StackText` | Overlay · HAlign **Right** · VAlign **Top** · Padding (0, 1, 2, 0) | `2` · Size **9 (가안)** · 색 `#FFFFFF` · Outline 1 `#000000` · Right · **Collapsed** · NHT | 중첩 표시 위치 = 역기획서 §3 #11 "우측 상단" · 크기 · 색 **(미확인)** |

- Hover: 툴팁(효과 이름 · 설명)을 붙일지 **(미확인)** — 붙인다면 `BuffRoot` 를 Visible 로 바꾸고 `ToolTipWidget` 에 연결. 색 변화는 두지 않는다 (자체 결정값)

---

### L. 체력 바 — `WBP_ER_VitalBar`

| 요소 | 데이터 출처 | 상태 |
|---|---|---|
| 체력 / 최대 `890/890` | `UERAttributeSet::HP` · `MaxHP` (`GAS/ERAttributeSet.h:164-170`) · `OnRep_HP` · `OnRep_MaxHP` (`:373-374`) → `GetGameplayAttributeValueChangeDelegate` | ✅ |
| 앞의 `+` | (미확인) 체력 재생 표시? `HPRegen` (`:172-174`) | (미확인) |
| 보호막 | `Shield` (`:338-340`) — 이미지에는 안 보인다 (보호막 0 상태) | ✅ (값) |
| 기력 바 | `VP` · `MaxVP` (`:180-186`) — **이미지에 없음** §1.2 | ✅ (값) · 자리만 Collapsed |

**갱신 방식** — HP · MaxHP 두 델리게이트. 둘 중 하나가 오면 비율과 텍스트를 다시 계산. 구독 직후 현재 값으로 초기화 (공통 §2.3)
- 피격 시 "줄어드는 잔상" 연출은 머티리얼 파라미터 (공통 §4.9) — 이미지에 없으므로 **선택 사항**

**최적화**
- `ProgressBar` 의 Percent 변경 = Paint (Low) — 레이아웃 불변. Volatile 아님 (공통 §4.2 표: "체력바(피격 시)")
- 텍스트 `890 / 890` 은 자릿수가 바뀔 수 있다 → A 와 같이 고정폭 숫자

**이미지 조각**
- `T_UI_HPBar_Fill` (녹색 · 그라데이션) · `T_UI_HPBar_Bg` · 9-slice 또는 Progress Bar 의 `Fill Image` Draw As = `Box`
- `T_UI_VPBar_Fill` (자리만 — 색 (미확인))

**내부 구조** — 로컬 원점 = 원본 (660, 1039) · 크기 270 × 16 (검은 테 1px 포함)

```
WBP_ER_VitalBar
VerticalBox  VitalRoot                           (추가) 루트 — HP 줄 아래 VP 줄 자리
├─ Overlay  HPOverlay                            (추가)
│  ├─ ProgressBar  **HPBar**                     (§6 ① 4 이름)
│  └─ HorizontalBox  HPTextRow                   (추가) 가운데 글자 줄
│     ├─ Image      HPRegenIcon                  (추가) 녹색 `+` (뜻 미확인 §3.L)
│     ├─ TextBlock  **HPText**  "890 / 890"      (§6 ① 4 이름)
│     └─ Spacer     HPTextBalance                (추가) 아이콘 폭만큼 — 숫자를 바 정중앙에 두려고
└─ ProgressBar  **VPBar**                        (§6 ① 4 이름) Collapsed
```

| 위젯 | 부모 슬롯 | 위젯 자체 설정 | 실측 (원본 px) |
|---|---|---|---|
| `VitalRoot` | 루트 | Visibility **NHT** (§6 ① 6) | — |
| `HPOverlay` | VBox · HAlign Fill · Padding 0 · Auto | — | (660,1039,270,16) |
| `HPBar` | Overlay · Fill · Fill · Padding 0 | **Style › Background Image**: 단색 Draw As **Box** · Tint `#000000` (테 색) / **Fill Image**: `T_UI_HPBar_Fill` (가로 그라데이션을 구운 텍스처) Draw As **Image** · Tint `#FFFFFF` / **Fill Color and Opacity** `#FFFFFF` / **Border Padding (1, 1)** (채움이 테 안쪽 1px 로 들어간다) / **Bar Fill Type = Left to Right** / Percent 1.0 (미리보기) · 크기: SizeBox 없이 → 위 컨테이너 `VitalRow` 가 Auto 라서 **Desired 를 정해야 한다** → Background Image 의 Image Size **270 × 16** | 테 x 660 · 929 · y 1039 · 1054 = `#000000` · 채움 왼쪽 `#A5DF39` → 가운데 `#78C929` → 오른쪽 `#5DAD20` · 세로로는 거의 같음 (`#8ED42E` 근처 · 근사) |
| `HPTextRow` | Overlay · HAlign **Center** · VAlign **Center** · Padding 0 | NHT | 글자 줄 x 756–821 |
| `HPRegenIcon` | HBox · VAlign Center · Padding **(0, 0, 4, 0)** · Auto | `+` 아이콘 · Image Size **8 × 8** · Tint `#B8F848` (가장자리 어두운 테는 텍스처) · Collapsed 원칙(§8 · 뜻 미확인)이지만 **Hidden** 으로 — Collapsed 면 숫자가 6px 오른쪽으로 밀린다 | (756,1043,8,8) |
| `HPText` | HBox · VAlign Center · Padding 0 · Auto | `890 / 890` · Size **10** **⚠ 실측** (숫자 9px · 표 12) · 색 `#E8E8E8` · Justification Center · **Outline Size 1 · 색 `#000000` 알파 0.5 (근사 — 글자 둘레가 바 색보다 어둡다 `#466335`)** · 고정폭 숫자 (§6 ① 4 Min Desired Width) | (768,1043,54,9) — 중심 x 794.5 = 바 중심 794.5 ✓ |
| `HPTextBalance` | HBox · Auto | Spacer Size **12 × 1** (= 아이콘 8 + 간격 4) | — |
| `VPBar` | VBox · HAlign Fill · Padding (0, 2, 0, 0) (가안) · Auto | 스타일은 `HPBar` 와 같게 · Fill Image `T_UI_VPBar_Fill` (색 **(미확인)**) · Background Image Size 270 × **6 (가안)** · **Collapsed** | 레퍼런스에 없음 (§1.2) |

- 보호막(`Shield`) 은 레퍼런스에 없다 — 표시 방법 **(미확인)**. 넣는다면 `HPOverlay` 에 ProgressBar 하나를 `HPBar` 위에 겹치는 방식 (자리만 메모 · 지금 만들지 않는다)
- Hover 없음 (표시 전용)

---

### M. 크레딧 · V 버튼 — `WBP_ER_CreditBox`

| 요소 | 데이터 출처 | 상태 |
|---|---|---|
| 크레딧 `20` | 크레딧 값 없음 (grep `Credit` → 야생동물 데이터 2파일뿐 `Wildlife/ERWildlifeData.h`) | ❌ 미구현 (F15) |
| `B` 키 | (미확인) 원격 드론 상점 / 다른 것 | ❌ |
| `V` + 순환 아이콘 | (미확인) | ❌ |

- 자리 + 텍스트 + 아이콘만. 이미지 조각: `T_UI_Icon_Credit`

**내부 구조** — 로컬 원점 = 원본 (940, 1033) · 크기 ≈136 × 32 (크레딧 판 90 + 간격 8 + V 버튼 38). V 버튼 바탕은 컨테이너의 `Img_PillarBg` (위 G 앞 컨테이너 표)

```
WBP_ER_CreditBox
HorizontalBox  CreditRoot                        (추가) 루트
├─ Overlay  CreditPanel                          (추가) 파란 판
│  ├─ Image  Img_CreditBg                        (추가)
│  ├─ HorizontalBox  CreditValueRow              (추가)
│  │  ├─ Image      Img_CreditIcon               (추가) 노란 C 동전
│  │  └─ TextBlock  **CreditText**  "20"         (추가)
│  └─ Border  KeyBadge_B                         (추가) 왼쪽 아래 검은 키 칸 (판 밖으로 2px 튀어나옴)
│     └─ TextBlock  Txt_KeyB  "B"                (추가)
└─ Button  **VButton**                           (추가) 기둥 아래 반 (뜻 미확인)
   └─ Overlay  VButtonContent                    (추가)
      ├─ Image   Img_VButtonHitArea              (추가) 투명 38×32 — 버튼 크기용 (아래 메모)
      ├─ Image   Img_VIcon                       (추가) 초시계 + 바람 줄 아이콘
      └─ Border  KeyBadge_V                      (추가)
         └─ TextBlock  Txt_KeyV  "V"             (추가)
```

| 위젯 | 부모 슬롯 | 위젯 자체 설정 | 실측 (원본 px) |
|---|---|---|---|
| `CreditRoot` | 루트 (컨테이너 `VitalRow` 슬롯 Padding (10,0,0,0)) | Visibility **Self Hit Test Invisible** | — |
| `CreditPanel` | HBox · VAlign Top · Padding 0 · Auto | NHT (클릭 기능 (미확인) — `B` 키가 무엇을 여는지 정해지면 Button 으로 감싼다) | (940,1033,90,30) |
| `Img_CreditBg` | Overlay · Fill · Fill | 위→아래 그라데이션 판 · Draw As Image · Image Size **90 × 30** (판 크기를 이 값이 정한다) · 색 위 `#2E4867` → 아래 `#325575` (흰 그라데이션 텍스처 + Tint `#325575` 또는 단색 `#2F4D6C` 로 단순화) | 판 `#2F4D6C` 근처 |
| `CreditValueRow` | Overlay · Left · Top · Padding **(17, 8, 0, 0)** | NHT | — |
| `Img_CreditIcon` | HBox · VAlign Center · Padding 0 · Auto | `T_UI_Icon_Credit` (육각 안 `C`) · Image Size **13 × 14** · Tint `#FFFFFF` (텍스처 색 `#B8C850`) | (957,1041,13,14) |
| `CreditText` | HBox · VAlign Center · Padding **(14, 0, 0, 0)** · Auto | `20` · Size **13** **⚠ 실측** (숫자 12px · 표 14) · 색 `#F8F8F8` · Left · 그림자 없음 | (985,1042,19,12) |
| `KeyBadge_B` | Overlay · Left · Top · Padding **(−2, 19, 0, 0)** (음수 — 판 밖으로) | Border · 단색 Tint `#000000` · Padding 0 · Min 크기는 자식 `Txt_KeyB` 의 Min Desired Width 14 · 높이 14 | (938,1052,14,14) |
| `Txt_KeyB` | Border 자식 · Center · Center | `B` · Size **9** · 색 `#E0E0E0` · Center · Min Desired Width 14 | 글자 (942,1055,6,8) |
| `VButton` | HBox · VAlign Top · Padding **(8, 0, 0, 0)** · Auto | Style Normal: Draw As **None** (바탕은 기둥) · Hovered: Draw As **Rounded Box** 반지름 6 · Tint `#FFFFFF` 알파 0.1 **(가안)** · Pressed: 알파 0.2 **(가안)** · Padding 0 · Is Focusable ✘ | 기둥 아래 반 (1038,1033,38,32) |
| `VButtonContent` | Button 슬롯 · Fill · Fill | 크기 = 자식 `Img_VIcon` 로 정한다 | — |
| `Img_VIcon` | Overlay · Left · Top · Padding **(7, 6, 0, 0)** | 초시계 아이콘 · Image Size **22 × 24 (근사)** · Tint `#F0F0F0` · NHT | ≈(1045,1039,22,24) |
| `KeyBadge_V` | Overlay · Left · Top · Padding **(−4, 19, 0, 0)** (음수 — 기둥 밖으로 4px) | `KeyBadge_B` 와 같음 | (1034,1052,14,14) |
| `Txt_KeyV` | Border 자식 · Center · Center | `V` · Size 9 · 색 `#E0E0E0` | — |

- `VButton` 의 크기를 38 × 32 로 맞추려면 `Img_VIcon` 하나로는 모자란다 → `VButtonContent` 첫 자식으로 투명 Image(`Img_VButtonHitArea` · Image Size 38 × 32 · Tint 알파 0) 를 하나 더 두거나, 버튼에만 SizeBox(38 × 32)를 씌운다 **(둘 중 하나 — 자체 결정값: 투명 Image)**
- 음수 Padding 을 피하려면 `CreditBox` 원점을 2px 왼쪽(938)으로 옮기고(컨테이너 Padding 8) 판을 Padding 2 로 미는 방법도 같다

---

### N. 가방 10칸 — `WBP_ER_Bag` / `WBP_ER_ItemSlot` ×10

| 요소 | 데이터 출처 | 상태 |
|---|---|---|
| 칸 수 10 | `UERItemSettings::InventorySlots = 10` (`Item/ERItemSettings.h:37`) | ✅ |
| 칸 내용 (아이템 · 수량) | `UERInventoryComponent::Bag` (`TArray<FERItemInstance>` `Item/ERInventoryComponent.h:203-204` · **OwnerOnly** `ERInventoryComponent.cpp:50`) · `GetBag()` `:89` · `FERItemInstance{ItemId, Count}` `:19-30` | 값 ✅ |
| 갱신 이벤트 | `OnRep_Bag` **로그만** (`ERInventoryComponent.cpp:63-67` "UI 가 생기면 여기서 갱신") | ⚠ 이벤트 없음 |
| 아이콘 · 등급 · 최대 스택 | `FERItemRow::Icon` `:56` · `Grade` `:36` · `MaxStack` `:63` (`Item/ERItemData.h`) | ✅ |
| 키 숫자 `1~0` | 고정 텍스트 (`bShowKeyLabel`) | — |
| 1번 칸 녹색 `+` | (미확인) 사용 가능 / 새로 들어옴 표시 | ❌ |
| 칸 클릭 → 사용 · 장착 · 버리기 | 장착 `ServerEquipFromBag(int32)` `:95` · 버리기 `ServerDiscard(int32)` `:103` · **소비(먹기) RPC 없음** | 장착·버리기 ✅ / 사용 ❌ |

- **칸 수 고정 → ListView 아님.** 공통 §4.6 "가방처럼 칸 수가 고정이면 미리 만든 슬롯으로 충분"
- 갱신은 `OnRep_Bag` → 델리게이트 → **10칸 전부 다시 채우기**(10칸이라 차이 계산 불필요 · 자체 결정값)
- 최적화: Invalidation Box. 슬롯 내용 변경 = Paint/Layout(수량 텍스트). Volatile 아님
- ⚠ 남의 가방은 복제되지 않는다 (`ERInventoryComponent.h:200` · 공통 §6) — 관전 UI 에서 쓰지 않는다

**`WBP_ER_ItemSlot` 내부 (BindWidget 이름)**
```
Overlay  SlotRoot
├─ Image      GradeBg       등급 배경 (Tint = 등급색)
├─ Image      IconImage     아이템 아이콘 (소프트 → 비동기 로드)
├─ TextBlock  CountText     수량 (1 이면 Collapsed)
├─ TextBlock  RatioText     n/m  (bShowRatio 일 때만 · C · D)
└─ TextBlock  KeyText       1~0  (bShowKeyLabel 일 때만 · N)
```

**이미지 조각**
- `T_UI_ItemSlot_Empty` (빈 칸) · `T_UI_ItemSlot_GradeBg` (흰 판 1장 + Tint 6색 — 등급 6종 `Item/ERItemTypes.h:15-29`)
- ⚠ **등급별 색상 코드는 (미확인)** — 역기획서 미확인 목록 "아이템 등급별 색상 코드". 자체 결정값으로 넣고 표시한다
- 아이템 아이콘 — `FERItemRow::Icon` 에 지정 (데이터 원본은 CSV — `Docs/3_EditorTasks/Data/` · 사용자 메모리 "데이터 원본은 CSV")

**내부 구조 (보강)** — 가방 로컬 원점 = 원본 (1087, 977) (그리드 패딩 포함) · 칸 박스 (1089, 981, 356, 87)

```
WBP_ER_Bag                                          (루트 Canvas 직속 — 2026-10-12 결정)
InvalidationBox  BagInvalidation                    (추가) 루트
└─ UniformGridPanel  BagGrid                        (추가 이름 — §2 의 UniformGridPanel) 5열 × 2행
   ├─ WBP_ER_ItemSlot  **BagSlot0**  (Col 0, Row 0)  키 "1"
   ├─ …               **BagSlot4**  (Col 4, Row 0)  키 "5"
   ├─ WBP_ER_ItemSlot  **BagSlot5**  (Col 0, Row 1)  키 "6"
   └─ …               **BagSlot9**  (Col 4, Row 1)  키 "0"

WBP_ER_ItemSlot (엔트리 — 위 트리의 이름 유지 + 잎 보강 · C · D · G · N 공용)
Overlay  SlotRoot
├─ Image      EmptyBg               (추가) 빈 칸 바탕
├─ Image      GradeBg               등급 바탕 (아이템 있을 때만)
├─ Image      IconImage
├─ Image      UnderlineImage        (추가) D 의 노란 밑줄 — Collapsed
├─ Image      CheckImage            (추가) D 의 녹색 체크 — Collapsed
├─ Image      NewMarkImage          (추가) N 1번 칸의 녹색 `+` (뜻 미확인) — Collapsed
├─ TextBlock  CountText
├─ TextBlock  RatioText
└─ Border     KeyBadge              (추가) 검은 키 칸
   └─ TextBlock  KeyText
```

**`Bag` 배치**

| 위젯 | 부모 슬롯 | 위젯 자체 설정 | 실측 (원본 px) |
|---|---|---|---|
| `Bag` (루트 Canvas 슬롯) | 앵커 (0.5, 1) · Alignment (0, 1) · Position **(127, −7)** · Size **360 × 96** **⚠** — 레이아웃 표의 (129, −11) · 356 × 87 은 칸 바깥 박스, 그리드 Slot Padding(2,4,2,4)을 넣으면 사방 2 · 4 씩 커진다 | — | 칸 박스 (1089,981,356,87) |
| `BagInvalidation` | 루트 | Can Cache ✔ · Visibility **Self Hit Test Invisible** (칸은 클릭) | — |
| `BagGrid` | InvalidationBox 자식 | **Min Desired Slot Width 68 · Height 40** · Slot Padding **(2, 4, 2, 4)** → 가로 피치 72 · 세로 피치 48 | 열 x 1089 / 1161 / 1233 / 1305 / 1377 · 행 y 981 / 1029 |
| `BagSlot0~9` | Grid · Column/Row 위 트리 · HAlign **Fill** · VAlign **Fill** | `WBP_ER_ItemSlot` · **`bShowKeyLabel` ✔** · `bShowRatio` ✘ · `bCompact` ✘ · `bShowGradeBg` ✔ · 키 글자는 인스턴스 변수 `KeyLabel` (Text · 추가 · Instance Editable — "1"~"9","0") | — |

**엔트리 `WBP_ER_ItemSlot` — 위젯 설정**

디자이너 변수 (전부 Boolean · Instance Editable ✔ · Expose on Spawn ✘ · Pre Construct 에서 **Visibility · Image Size 만** 바꾼다 — §6 ① 3 과 같은 규칙):
`bShowKeyLabel` (§2.1) · `bShowRatio` (§2.1) · `bCompact` (§2.1 — ✔ 면 `EmptyBg` · `GradeBg` Image Size 34 × 22, ✘ 면 16 × 16) · **`bShowGradeBg` (추가)** (✘ 면 `EmptyBg` · `GradeBg` Collapsed — D 용) · **`KeyLabel` (추가 · Text)**

| 위젯 | 부모 슬롯 (Overlay) | 위젯 자체 설정 | 실측 (원본 px — N 기준) |
|---|---|---|---|
| `SlotRoot` | 루트 | Visibility **Visible** (클릭 · 드래그) · Desired 는 아래 Image Size (작게 — 실제 크기는 부모 Grid 의 Min Desired 가 정한다 · §8 "ItemSlot 에 SizeBox 금지") | 칸 68 × 40 |
| `EmptyBg` | Fill · Fill · Padding 0 | 단색 · Draw As Image · Image Size 16 × 16 (bCompact ✔ 34 × 22) · Tint N: `#1E262E` 알파 **0.35 (근사 — 칸 안이 월드보다 살짝 어둡고 푸르다)** / G: `#313A40` 알파 1 (G 는 판 위라 불투명) — 두 값은 C++ 또는 인스턴스에서 Tint 만 다르게 **(근사)** · NHT | 빈 칸 예: `#384141` (옆 틈 월드 `#474E34`) |
| `GradeBg` | Fill · Fill · Padding 0 | Brush `T_UI_ItemSlot_GradeBg` (흰 세로 그라데이션: 위 어둡게 → 아래 밝게 + 맨 아래 1~2px 밝은 줄을 구운 한 장) · Draw As Image · Image Size = `EmptyBg` 와 같음 · **Tint = 등급색** (C++ — 아이템 없으면 Collapsed) · NHT | N 1번 칸: 위 `#232C3D`/`#25324D` → 아래 `#1E489B` · 맨 아래 줄 `#5486BA` (파랑) · C: `#3A2D43` → `#612C95` · 줄 `#9B63C3` (보라) · G: `#353435` → `#535353` · 줄 `#797979` (회색) — 등급 ↔ 색 대응 **(미확인 — 위 ⚠)** |
| `IconImage` | Fill · Fill · Padding **(2, 2, 2, 2)** (근사 — 아이콘이 칸을 거의 채움) | 아이템 아이콘 (소프트 → 비동기 로드) · Draw As Image · Tint `#FFFFFF` · 비율이 칸과 달라도 늘려 채운다 (ScaleBox 안 씀 — 레이아웃 패스 절약) · NHT | 수박 (≈1093–1146, 983–1018) |
| `UnderlineImage` | HAlign Fill · VAlign **Bottom** · Padding 0 | 단색 Image Size 1 × **2** · Tint `#FFD247` · **Collapsed** · NHT | D (§3.D 표) |
| `CheckImage` | HAlign **Right** · VAlign **Bottom** · Padding (0, 0, 0, 3) | Image Size **11 × 8** · Tint `#C0F010` · **Collapsed** · NHT | D (§3.D 표) |
| `NewMarkImage` | HAlign **Right** · VAlign **Top** · Padding **(0, −2, −3, 0)** (음수 — 칸 밖으로 살짝) | `+` 아이콘 (L 의 `HPRegenIcon` 과 같은 조각) · Image Size **10 × 9** · Tint `#B8F848` · **Collapsed** · 뜻 **(미확인)** · NHT | (1150,979,10,9) — 칸 오른쪽 위 모서리에 걸침 |
| `CountText` | HAlign **Right** · VAlign **Bottom** · Padding **(0, 0, 3, 1)** (글자 아래 1016 · 칸 아래 1020 — 줄 여백 3 감안 근사) | `3` · Size **12** **⚠ 실측** (숫자 11px · 표 10) · 색 `#F0F0F0` · Right · Outline 1 `#000000` 알파 0.6 (근사) · 수량 1 이면 Collapsed · NHT | (1147,1006,7,11) |
| `RatioText` | HAlign Right · VAlign Bottom · Padding (0, 0, 1, 3) | D 설정 그대로 (§3.D 표) · `bShowRatio` ✘ 면 Collapsed · NHT | — |
| `KeyBadge` | HAlign **Left** · VAlign **Bottom** · Padding **0** | Border · 단색 Tint `#000000` · Padding 0 · `bShowKeyLabel` ✘ 면 Collapsed · NHT | (1089,1009,12,12) — 칸 왼쪽 아래 모서리에 붙음 · 2행도 (1089,1057) |
| `KeyText` | Border 자식 · Center · Center | `KeyLabel` 값 · Size **9** **⚠ 실측** (숫자 8~9px · 표 8) · 색 `#A8A8A8` · Center · Min Desired Width **12** | `1` (1093,1011,5,9) |

- **Hover/Pressed (공통 — C · D · G · N)**: 레퍼런스에 없음 **(미확인)**. 가안: Hover 때 `EmptyBg` Tint 알파 +0.15 · Pressed 때 `IconImage` Tint `#C8C8C8` — **색 · 알파만** 바꾼다 (Paint 무효화만 · 공통 §4.1). 드래그 미리보기 위젯은 이 문서 범위 밖
- 엔트리에 SizeBox 가 없으므로 C(56×35) · D(41×30) · G(34×22) · N(68×40) 크기는 **부모의 Min Desired Slot 크기**(C · D · N) 또는 `bCompact` Image Size(G — VerticalBox 라 Min 이 없음)로 정해진다

---

### O. 시스템 메시지 — `WBP_ER_SystemMessage`

| 요소 | 데이터 출처 | 상태 |
|---|---|---|
| 안내 문구 3줄 | 채팅 · 시스템 메시지 없음 | ❌ 미구현 |
| "번역" 버튼 · 접기 버튼 | — | ❌ |

- 이 문서에서는 **자리만**(VerticalBox + TextBlock 3줄). 색이 섞인 문장(노랑 + 하늘 + 보라 "미스릴")은 `Rich Text` 가 필요하다 — Epic: Rich Text 는 *"very expensive compared with standard text boxes"* (공통 §4.5). → 줄마다 색이 하나면 TextBlock, 한 줄 안에서 색이 바뀌는 줄만 Rich Text (자체 결정값)
- 메시지가 계속 쌓이는 채팅이면 ListView (공통 §4.6) — 채팅 기능 결정 후

**내부 구조** — 로컬 원점 = 원본 (0, 524) · 크기 514 × 263

```
WBP_ER_SystemMessage
Overlay  MessageRoot                              (추가) 루트
├─ Button  **CollapseButton**                     (추가) 접기 (^)
│  └─ Overlay  CollapseContent                    (추가)
│     ├─ Image  Img_CollapseInner                 (추가) 파란 네모
│     └─ Image  Img_CollapseArrow                 (추가) 흰 꺾쇠
├─ Button  **TranslateButton**                    (추가) "번역"
│  └─ HorizontalBox  TranslateRow                 (추가)
│     ├─ Image      Img_TranslateIcon             (추가) A文 아이콘
│     └─ TextBlock  Txt_Translate  "번역"          (추가)
├─ Image  Img_MessageBg                           (추가) 메시지 뒤 반투명 판
└─ VerticalBox  **MessageList**                   (추가) 메시지 줄
   ├─ TextBlock      **MsgLine0**                 (추가) 주황 1줄
   ├─ TextBlock      **MsgLine1**                 (추가) 주황 1줄
   └─ RichTextBlock  **MsgLine2**                 (추가) 하늘 + 보라 섞인 줄 (§3.O 위 결정 — 색이 섞인 줄만 Rich Text)
```

| 위젯 | 부모 슬롯 | 위젯 자체 설정 | 실측 (원본 px) |
|---|---|---|---|
| `MessageRoot` | 루트 | Visibility **Self Hit Test Invisible** (버튼만 클릭 · 월드 클릭 통과) | (0,524,514,263) |
| `CollapseButton` | Overlay · Left · Top · Padding **(0, 0, 0, 0)** | Style Normal: Draw As **Rounded Box** · 반지름 (0, 0, 6, 0) 오른쪽 아래만 **(근사)** · Tint `#1B1F28` · Outline 1px `#2E3645` / Hovered: Tint `#232A36` **(가안)** / Pressed: `#141820` **(가안)** · Normal/Pressed Padding 0 · Is Focusable ✘ | 바깥 판 (0,525,55,66) · 판 `#1B1F28` · 테 `#2E3645` |
| `CollapseContent` | Button 슬롯 · Fill · Fill | 크기 = `Img_CollapseInner` + Padding | — |
| `Img_CollapseInner` | Overlay · Left · Top · Padding **(11, 13, 10, 19)** (→ 버튼 바깥 55 × 66 을 이 Padding 이 만든다) | 위→아래 그라데이션 판 · Image Size **34 × 34** · 위 `#0E3056` → 아래 `#255584` · 테 1px `#5B8BBC` (텍스처에 구움) · NHT | (11,537,34,34) |
| `Img_CollapseArrow` | Overlay · Left · Top · Padding **(20, 25, 0, 0)** | 위 꺾쇠 `^` · Image Size **16 × 9** · Tint `#F8F8F8` · 접힌 상태에선 Render Transform Angle 180 **(가안)** · NHT | (20,549,16,9) |
| `TranslateButton` | Overlay · Left · Top · Padding **(23, 66, 0, 0)** | Style 세 상태 Draw As **None** · Padding 0 · Hover 때 `Txt_Translate` 색 `#B8B8B8` → `#FFFFFF` **(가안)** | (23,591,39,9) |
| `Img_TranslateIcon` | HBox · VAlign Center · Padding 0 · Auto | `A文` 아이콘 · Image Size **14 × 9** · Tint `#A8A8A8` · NHT | (23,591,14,9) |
| `Txt_Translate` | HBox · VAlign Center · Padding **(6, 0, 0, 0)** · Auto | `번역` · Size **8** **⚠ 실측** (한글 9px · 표 9) · 색 `#B8B8B8` · NHT | (43,591,19,9) |
| `Img_MessageBg` | Overlay · HAlign Fill · VAlign **Bottom** · Padding 0 | 아래 진하고 위로 투명해지는 세로 그라데이션 · Image Size 514 × **70 (근사)** · Tint `#000000` 알파 **0.25 (근사 — 위쪽 경계 (미확인))** · NHT | 판 아래 787 · 오른쪽 끝 514 |
| `MessageList` | Overlay · Left · Top · Padding **(21, 200, 0, 0)** (첫 글자 위 726 − 524 − 여백 2) | NHT | 줄 위 y 726 / 745 / 765 (피치 ≈19.5) |
| `MsgLine0` · `MsgLine1` | VBox · HAlign Left · Padding **(0, 0, 0, 1)** · Auto | `이터널 리턴은 공정한 게임의 가치를 중요하게 생각합니다.` / `부당한 티밍은 제재 대상이 될 수 있습니다.` · Size **12** · 색 **`#FEA500` ⚠ 실측** (§1.2 "노란" → 주황) · Left · Shadow Offset (1, 1) · `#000000` 알파 0.8 (근사 — 글자 아래 `#090A08`) · Auto Wrap ✘ | (21,726,362,14) · (21,745,264,14) |
| `MsgLine2` | VBox · HAlign Left · Padding 0 · Auto | RichTextBlock · **Text Style Set** = 새 DataTable `DT_ER_RichTextStyle` (Row `Default` 12 · `#00F0F0` / Row `Item` 12 · `#A038E8`) **(추가 · 가안)** · 텍스트 `안내 로봇-LUMI 시그니처 상품 : <Item>미스릴</>` · 그림자는 MsgLine0 과 같게 | 하늘 (21,765,202,15) `#00F0F0` · 보라 `미스릴` (230,766,38,13) `#A038E8` |

- 메시지 줄 수는 레퍼런스 3줄. 쌓이는 방식 · 사라지는 시간 **(미확인)** — 채팅이면 `MessageList` 를 ListView 로 바꾼다 (§3.O 위). 지금은 줄 3개 고정
- RichTextBlock 이 부담이면(공통 §4.5) 디자이너 미리보기만 `MsgLine2` 를 HorizontalBox[TextBlock 하늘 · TextBlock 보라] 로 둬도 된다 **(대안)**

---

### P. 시간 · 핑 — `WBP_ER_NetInfo`

| 요소 | 데이터 출처 | 상태 |
|---|---|---|
| `0ms` 핑 | 엔진 `APlayerState::GetPingInMilliseconds()` (미확인 — 엔진 소스 미대조) | 엔진 ✅ (미확인) |
| `00:09` | (미확인 §1.3) | ❌ |

- 1초 주기 타이머 (A 와 같음). 개발 중 디버그 가치가 있다
- 이미지 조각: 없음

**내부 구조** — 로컬 원점 = 원본 (16, ≈1054) (근사) · Size To Content

```
WBP_ER_NetInfo
HorizontalBox  NetInfoRow                    (추가) 루트
├─ TextBlock  **ElapsedText**  "00:09"       (추가) 뜻 미확인 §1.3
└─ TextBlock  **PingText**  "0ms"            (추가)
```

| 위젯 | 부모 슬롯 | 위젯 자체 설정 | 실측 (원본 px) |
|---|---|---|---|
| `NetInfoRow` | 루트 (RootCanvas 슬롯은 레이아웃 표 — 좌하단 · Position (16, −8)) | Visibility **NHT** | — |
| `ElapsedText` | HBox · VAlign Bottom · Padding 0 · Auto | `00:09` · Size **10** **⚠ 실측** (숫자 9px · 표 11) · 색 `#7898A0` · Left · 그림자 없음 · 고정폭 숫자 | (17,1057,33,9) — 표의 x 16 보다 1 오른쪽 (글자 옆 여백) |
| `PingText` | HBox · VAlign Bottom · Padding **(25, 0, 0, 0)** · Auto | `0ms` · Size **10** · 색 `#7898A0` (근사 — `#708890` ~ `#7898A0`) · Left | (76,1057,26,9) — 앞 글자 끝 49 → 간격 26 |

- `ElapsedText` 는 뜻이 정해지기 전까지 §8 원칙대로 Collapsed 가 맞지만, HorizontalBox **앞쪽**이라 숨기면 `PingText` 가 왼쪽으로 붙는다 → 자리를 지키려면 **Hidden** (자체 결정값)
- 버튼 없음 · Hover 없음

---

### Q. 네임플레이트 (월드) — `WBP_ER_Nameplate`

| 요소 | 데이터 출처 | 상태 |
|---|---|---|
| 이름 "Chomul" | `APlayerState::GetPlayerName()` (엔진) | ✅ |
| 레벨 "1" | `UERGrowthComponent::Level` — **전원 복제** (`ERGrowthComponent.cpp:38` · 헤더 주석 "시야 안 적의 레벨은 보인다" `ERGrowthComponent.h:18`) · 이벤트는 H 와 같이 ⚠ | ⚠ |
| 체력 바 (칸 구분) | `HP` · `MaxHP` — **전원 복제** `COND_None` (`ERAttributeSet.cpp:296`) → 남의 체력 바도 이벤트로 갱신 가능 | ✅ |
| 칸 구분선 | 원작 칸 1개 = 체력 몇인지 **(미확인)** | (미확인) |
| 아군/적 색 | `AERPlayerState::TeamId` (`Core/ERPlayerState.h:67-68` 전원 복제) — 직접 읽지 말고 `ERTeamStatics::GetTeamId()` (`:62-64` 주석) | ✅ |
| 빈사 표시 | `State_Downed` 태그 (`ERGameplayTags.h:72`) · `AERPlayerState::IsDowned()` (`ERPlayerState.h:162`) | ✅ (태그 이벤트) |

- 붙이는 법: 실험체 폰에 `UWidgetComponent` (Space = **Screen**) — 폰 클래스 수정이 필요 → C++ 작업 (§7)
- ⭐ 24명분이 동시에 존재한다 → **이 위젯은 절대 바인딩 · Tick 금지** (공통 §2.4 "24명 … 매 프레임 전부 돈다")
- 칸 구분선은 **머티리얼**로 (스칼라 `MaxHP` → 구분선 개수) — 위젯을 칸마다 만들면 MaxHP 가 바뀔 때 Child 무효화(High)
- 시야 밖 적은 숨겨야 한다 → **F16 팀 시야** 뒤 (`00_기능목록 :67`). 지금은 전부 보임
- 이미지 조각: `T_UI_Nameplate_Bg` · `T_UI_Nameplate_LevelBox` · `M_UI_SegmentedBar` (머티리얼)

**내부 구조** — 로컬 원점 = 원본 (883, 339) (근사) · 크기 ≈136 × 49 · `UWidgetComponent` Draw Size **136 × 49** · Pivot (0.5, 1) (가안 — 머리 위에 아래 가운데를 붙임)

```
WBP_ER_Nameplate                                 (월드 위젯 — Tick · Bind 금지 §3.Q)
HorizontalBox  NameplateRoot                     (추가) 루트
├─ Border  LevelBox                              (추가) 왼쪽 레벨 칸
│  └─ TextBlock  **LevelText**  "1"              (추가)
└─ VerticalBox  BarColumn                        (추가)
   ├─ TextBlock  **NameText**  "Chomul"          (추가)
   ├─ HorizontalBox  **SegmentStrip**            (추가) 바 위 회색 5칸 — 뜻 미확인
   │  └─ Image  Seg0 ~ Seg4                      (추가)
   └─ Overlay  HPFrame                           (추가)
      ├─ Image        Img_HPFrameBg              (추가) 검정 판 + 밝은 1px 테
      ├─ ProgressBar  **HPBar**                  (추가) 녹색 채움 (위 10px 줄)
      └─ Image        **HPTicks**                (추가) 100 단위 눈금 — M_UI_SegmentedBar
```

| 위젯 | 부모 슬롯 | 위젯 자체 설정 | 실측 (원본 px) |
|---|---|---|---|
| `NameplateRoot` | 루트 | Visibility **NHT** (월드 클릭 통과) | — |
| `LevelBox` | HBox · VAlign **Bottom** · Padding 0 · Auto | Border · Brush `T_UI_Nameplate_LevelBox` · Draw As **Box** · Margin 2px 해당 (테 2px `#8C8C8C`/`#585858` · 안 `#121D1E`) · Padding 0 · 크기: 자식 Min Desired Width 29 + 높이 → **Min Desired Height 없음** → `LevelText` 에 Min Desired Width 29 · Border Padding (0, 7, 0, 7) 로 29 높이를 만든다 (근사) | (883,359,29,29) — 아래 끝 387 (바 테보다 2px 아래) |
| `LevelText` | Border 자식 · Center · Center | `1` · Size **11** · 색 `#E0E0E0` · Center · Min Desired Width 29 | 글자 (895,370,6,10) |
| `BarColumn` | HBox · VAlign Bottom · Padding **(−1, 0, 0, 2)** (레벨 칸 오른쪽 테와 1px 겹침 · 아래 2px 위) · Auto | — | 열 x 910–1018 (109) |
| `NameText` | VBox · HAlign **Center** · Padding **(0, 0, 0, 3)** · Auto | `Chomul` · Size **11** · 색 `#E8E8E8` · Center · Shadow Offset (1, 1) `#000000` 알파 0.5 **(근사)** | (940,342,49,10) — 중심 964.5 = 바 중심 964 |
| `SegmentStrip` | VBox · HAlign Fill · Padding **(2, 0, 4, 3)** · Auto · **Collapsed** (§8 — 뜻 미확인) | — | 줄 (912,357,103,3) |
| `Seg0~4` | HBox · Padding (0,0,2,0) (마지막 0) · Size **Fill 1.0** | 단색 Image Size (1 × **3**) · 켜짐 Tint `#9B9B9B` · 꺼짐 `#555555` | 913–931 · 935–953 · 955–973 · 975–993 · 997–1014 (켜짐 · 켜짐 · 꺼짐 · 꺼짐 · 켜짐) |
| `HPFrame` | VBox · HAlign Fill · Padding 0 · Auto | — | (910,363,109,23) |
| `Img_HPFrameBg` | Overlay · Fill · Fill | Brush 단색 · Draw As **Box** · Image Size **109 × 23** (프레임 크기를 이 값이 정한다) · Tint `#000000` · Outline 1px `#CACACA` (Rounded Box 의 Outline 또는 테 구운 텍스처) | 테 y 363 · 385 `#CACACA` · 안 `#000000` |
| `HPBar` | Overlay · HAlign Fill · VAlign **Top** · Padding **(2, 3, 3, 0)** | Background 없음(Draw As None) · Fill Image 단색 · **Fill Color `#32D700`** (위 5줄 `#30CF00` — 무시 가능) · Bar Fill Type Left to Right · 높이: Fill 브러시 Image Size (1 × **10**) | 채움 (912,366,104,10) |
| `HPTicks` | Overlay · HAlign Fill · VAlign Top · Padding (2, 3, 3, 0) | `M_UI_SegmentedBar` MID (스칼라 `MaxHP` · `HPPerTick` = 100 (근사)) · Image Size (1 × 4) · 눈금 색 `#000000` · 폭 1 · NHT | 눈금 x 924 / 936 / 948 / 959 / 971 / 983 / 994 / 1006 = **8개 → 칸 9개** ≈ 890 / 100 (근사 — 1칸 = 100 HP 로 보인다) · 눈금은 녹색 위쪽 ≈4px |

- 녹색 아래 8px 검은 줄 (y 376–383) 은 비어 있는 두 번째 바 자리로 보인다 (보호막 · VP 중 무엇인지 **(미확인)**). 지금은 `Img_HPFrameBg` 의 검정이 그대로 보이게 둔다
- 색: 이 화면은 **자기 캐릭터 = 녹색**. 아군 · 적 색은 레퍼런스에 없다 **(미확인)** — `ERTeamStatics::GetTeamId()` 로 고를 Fill Color 3개를 C++ 에 둔다 (§3.Q 위)

### R. 야생동물 재생성 (월드) — `WBP_ER_WildlifeRespawn`

| 요소 | 데이터 출처 | 상태 |
|---|---|---|
| 동물 아이콘 | 야생동물 데이터에 아이콘 없음 (grep `Icon` 0건) | ❌ |
| "132s" | `UERWildlifeSpawnSubsystem` 의 `RespawnAt` (`Wildlife/ERWildlifeSpawnSubsystem.h:54`) — 서버 서브시스템 · **복제 안 됨** | ❌ 미구현 (복제 경로) |

- 남은 시간 표시는 A 와 같은 "종료 시각 1회 복제 → 로컬 계산" 패턴이 맞다 (역기획서 §3 구현 메모)
- 이 문서에서는 **WBP 껍데기만**

**내부 구조** — 로컬 원점 = 원본 (230, 311) (근사) · 크기 ≈66 × 144 · `UWidgetComponent` Space **Screen** · Draw Size 66 × 144 · Pivot (0.5, 1) (가안 — 막대 끝 점이 땅의 재생성 자리)

```
WBP_ER_WildlifeRespawn                          (월드 위젯)
VerticalBox  RespawnRoot                        (추가) 루트
├─ TextBlock  **RespawnTimeText**  "132s"       (추가)
├─ Overlay  RespawnIcon                         (추가) 원형 아이콘 묶음
│  ├─ Image  Img_RespawnRingBg                  (추가) 검은 원판 + 안쪽 청회색 고리
│  ├─ Image  **RespawnProgress**                (추가) 위쪽 녹색 진행 호 (M_UI_CooldownRadial)
│  └─ Image  **AnimalIcon**                     (추가) 동물 얼굴 (원형 머티리얼)
└─ Image  Img_RespawnStem                       (추가) 아래로 내린 막대 + 끝 점
```

| 위젯 | 부모 슬롯 | 위젯 자체 설정 | 실측 (원본 px) |
|---|---|---|---|
| `RespawnRoot` | 루트 | Visibility **NHT** | — |
| `RespawnTimeText` | VBox · HAlign **Center** · Padding **(0, 0, 0, 7)** · Auto | `132s` · Size **12** (숫자 11px) · 색 `#F0F0F0` · Center · Shadow (1,1) `#000000` 알파 0.5 **(근사)** · 1초 타이머로만 갱신 (A 와 같은 패턴) | (246,313,33,11) — 중심 262.5 = 원 중심 |
| `RespawnIcon` | VBox · HAlign Center · Padding 0 · Auto | — | 원 (231,335,64,64) (근사) |
| `Img_RespawnRingBg` | Overlay · Fill · Fill | 원판 텍스처 · Image Size **64 × 64** · 바깥 검정 `#050606` (반투명 알파 ≈0.85 근사) · 안쪽 고리 지름 ≈51 · 1px `#2F3D45` | 고리 x 237 · 287 (`#3D4F58`) |
| `RespawnProgress` | Overlay · Fill · Fill | `M_UI_CooldownRadial` MID (스칼라 `Progress` = 남은 비율 **(가안 — 녹색 호가 진행을 뜻하는지 (미확인))**) · 색 `#00FE00` · 호 두께 ≈5 · 시작 각 = 12시 · NHT | 녹색 (262,335,12,5) — 원 맨 위 |
| `AnimalIcon` | Overlay · Center · Center · Padding 0 | `M_UI_PortraitCircle` MID (야생동물 아이콘 — 데이터 없음 §3.R 위) · Image Size **46 × 46 (근사)** · 좌우 주황 점 장식(`#985B16`)은 아이콘 테 텍스처에 포함 **(미확인)** | 곰 얼굴 ≈ (240,345,46,46) |
| `Img_RespawnStem` | VBox · HAlign Center · Padding 0 · Auto | 세로 막대 + 아래 점 한 장 · Image Size **8 × 56** · 막대 폭 ≈2 · 점 지름 ≈7 · Tint `#000000` 알파 **0.5 (근사 — 월드 위 `#181A16`)** | 막대 x 262 · y 399–446 · 점 (259,447,7,8) |

- 동물 종류별 아이콘 · 재생성 대기 중이 아닐 때 숨기는 규칙 **(미확인)**

### S. 상자 마커 (월드) — `WBP_ER_LootMarker`

| 요소 | 데이터 출처 | 상태 |
|---|---|---|
| 상자 위치 | `AERItemDropActor` (`Item/ERItemDropActor.h`) — 액터 자체가 복제 | ✅ |
| 열렸는지 / 비었는지 | `Items` 복제 (`ERItemDropActor.h:117-118`). "누가 열었나" `OpenedBy` 는 서버 전용 (`:107-108`) → **"내가 연 상자" 표시는 클라가 모른다** | ⚠ |
| 아이콘 모양 | 상자 종류(DT_Loot) 별 아이콘 (미확인) | ❌ |

- 원작은 열린 상자 마커가 달라지는지 **(미확인)**
- 상자 수가 많다 → 위젯 컴포넌트 수십 개. **보이는 거리에서만** 켜는 방식은 (미확인 — 측정 후)

**내부 구조** — 예: 가운데 위 상자 · 로컬 원점 = 원본 (915, 120) (근사) · 크기 ≈66 × 124 · `UWidgetComponent` Space **Screen** · Draw Size 66 × 124 · Pivot (0.5, 1) (가안 — 막대 끝 점이 상자 위치)

```
WBP_ER_LootMarker                               (월드 위젯 — 수십 개 · Tick · Bind 금지)
VerticalBox  LootRoot                           (추가) 루트
├─ Overlay  LootIconGroup                       (추가)
│  ├─ Image  Img_LootRing                       (추가) 회색 고리 + 반투명 어두운 안쪽
│  └─ Image  **LootIcon**                       (추가) 상자 아이콘 (종류별 — 미확인)
└─ Image  Img_LootStem                          (추가) 아래로 내린 막대 + 끝 점 (R 과 같은 조각)
```

| 위젯 | 부모 슬롯 | 위젯 자체 설정 | 실측 (원본 px) |
|---|---|---|---|
| `LootRoot` | 루트 | Visibility **NHT** (월드 클릭 통과 — 상자 클릭은 월드 액터가 받는다) | — |
| `LootIconGroup` | VBox · HAlign Center · Padding 0 · Auto | — | 고리 (916,121,64,64) |
| `Img_LootRing` | Overlay · Fill · Fill | 고리 텍스처 · Image Size **64 × 64** · 고리 두께 ≈3 · 고리 색 `#6A6A6A` · 안쪽 `#000000` 알파 **0.4 (근사 — 월드가 비침 `#141715`)** | 고리 x 917 · 979 / y 121 · 184 |
| `LootIcon` | Overlay · Center · Center · Padding (0, 0, 0, 4) (근사 — 아이콘이 살짝 위) | 상자 아이콘 · Image Size **38 × 34 (근사)** · Tint `#FFFFFF` · 열린/빈 상자 표시 **(미확인 §3.S 위)** | 상자 ≈ (929,136,38,34) |
| `Img_LootStem` | VBox · HAlign Center · Padding 0 · Auto | R 의 `Img_RespawnStem` 과 같은 조각 · Image Size **8 × 58 (근사)** · Tint `#000000` 알파 0.5 | 막대 x 947 · y 185–233 · 점 ≈ (944,234,7,8) |

- 레퍼런스의 상자 마커 8개는 모두 같은 크기 · 모양이다 (거리에 따른 크기 변화 없음 — Screen Space 라서). 고리 · 막대 색이 상자 종류나 열림에 따라 바뀌는지 **(미확인)**

---

## 4. 갱신 방식 요약 (한눈에)

| 영역 | 이벤트 원천 | 시간 따라 변함 | Volatile | Invalidation Box |
|---|---|---|---|---|
| A 매치 정보 | `OnPhaseChanged` | 남은 시간 — 1초 `FTimerManager` | ✘ | ○ |
| B 처치 | ❌ | ✘ | ✘ | ○ |
| E 미니맵 | 폰 위치 (매 프레임 · 예외) | ○ | 자기·팀원 아이콘만 | ✘ (아이콘이 매 프레임) |
| F 스탯 | 어트리뷰트 델리게이트 ×9 | ✘ | ✘ | ○ |
| G 장비 | `OnEquippedChanged` ⚠ | ✘ | ✘ | ○ (하단 패널) |
| H 초상 | `OnRep_Level` ⚠ | ✘ | ✘ | ○ |
| I 스킬바 | `RegisterGameplayTagEvent(Cooldown.*)` · State 태그 | 쿨다운 중만 0.1초 타이머 | 게이지·숫자만 | ○ |
| J 스킬 강화 | `OnRep_SkillPoints` ⚠ | ✘ | ✘ | ○ |
| K 버프 | `OnActiveGameplayEffectAddedDelegateToSelf` | 남은 시간 타이머 | 남은 시간 숫자만 | ○ |
| L 체력 | 어트리뷰트 델리게이트 | ✘ | ✘ | ○ |
| N 가방 | `OnRep_Bag` ⚠ | ✘ | ✘ | ○ |
| Q 네임플레이트 | 어트리뷰트 델리게이트 (남의 ASC) | ✘ | ✘ | (미확인 — 월드 위젯) |

⭐ **HUD 전체에서 Tick 을 켜는 위젯은 미니맵 하나** (역기획서 §3 구현 메모 · 공통 §4.8).
⭐ **UMG 프로퍼티 바인딩(`Bind` 드롭다운)은 어느 위젯에도 쓰지 않는다** — Epic *"Never use raw attribute binding."* ([Optimization Guidelines for UMG](https://dev.epicgames.com/documentation/unreal-engine/optimization-guidelines-for-umg-in-unreal-engine) · 공통 §2.4)

---

## 5. 필요한 이미지 조각 — 전체 목록

임포트 설정은 공통 §5 (Texture Group `UI` · NoMipmaps · `UserInterface2D`). 저장 위치 `/Game/UI/Textures/HUD/` **(폴더 미확인 — 공통 §1.3)**.

| 이름 | 쓰는 곳 | 형태 | 지금 만들까 |
|---|---|---|---|
| `T_UI_BottomPanel_Bg` | 하단 패널 판 | 9-slice | ○ |
| `T_UI_Panel_Dark` | D · O 반투명 판 | 9-slice | ○ |
| `T_UI_MatchBar_Bg` | A | 3-slice(가로) (미확인) | ○ |
| `T_UI_SkillSlot_Frame` · `_Frame_D` | I | 9-slice | ○ |
| `T_UI_SkillPip_On` · `_Off` | I 레벨 눈금 | 작은 아이콘 | ○ |
| `T_UI_SkillUp_Diamond` · `T_UI_SkillUp_Glow` | J | 아이콘 · 그라데이션 | ○ |
| `T_UI_ItemSlot_Empty` · `T_UI_ItemSlot_GradeBg` | G · N · C · D | 9-slice / 흰 판 | ○ |
| `T_UI_HPBar_Fill` · `T_UI_HPBar_Bg` | L | Box | ○ |
| `T_UI_Portrait_Frame` · `T_UI_Level_Badge` | H | 아이콘 | ○ |
| `T_UI_Stat_*` 8장 | F | 아이콘 | ○ |
| `T_UI_Icon_Day` · `_Night` · `_Team` · `_Clock` · `_Credit` | A · M | 아이콘 | ○ |
| `T_UI_Buff_Frame` | K | 9-slice | ○ |
| `T_UI_Minimap_Frame` · `T_UI_Minimap_Self` | E | 9-slice · 아이콘 | ○ (지도 자체는 F13 뒤) |
| `T_UI_SectionHeader_Corner` | D | 아이콘 | ○ |
| `T_UI_Nameplate_Bg` · `_LevelBox` | Q | 9-slice | ○ |
| 날씨 · 하트 · B/V 버튼 · 미니맵 버튼 4개 아이콘 | A · M · E | 아이콘 | ✘ 뜻 확정 후 |
| 스킬 아이콘 · 초상 · 야생동물 아이콘 | I · H · R | 데이터 이미지 | ✘ 데이터 필드 생긴 뒤 |

**머티리얼** (`/Game/UI/Materials/` 미확인): `M_UI_CooldownRadial` (I · K) · `M_UI_PortraitCircle` (H) · `M_UI_SegmentedBar` (Q). 전부 **Material Domain = User Interface** (공통 §4.9).

⚠ 원본 이미지 출처: 이 스크린샷에서 잘라 쓰면 해상도가 낮고 다른 UI 가 겹쳐 있다. **원본 리소스 확보 경로는 (미확인)** — 사용자 결정.

---

## 6. 에디터 작업

### ① 에디터 작업 — 지금 할 수 있는 것 (C++ 없이)

⚠ 순서대로. 1~6 은 **부모 = `UserWidget`** 으로 만든다.

- [ ] **1. 폴더 확인** — Content Browser 에서 `/Game/UI/` 가 있는지 본다. 없으면 `/Game/UI/Widgets/HUD/` · `/Game/UI/Textures/HUD/` · `/Game/UI/Materials/` 생성 (공통 §1.3 — 기존 폴더 규칙이 있으면 그걸 따르고 이 문서를 고쳐 달라)
- [ ] **2. 텍스처 임포트** — §5 표의 "지금 만들까 ○" 조각을 공통 §5 설정으로 임포트. 9-slice 는 공통 §5 "9-slice" 절의 Margin 계산
- [ ] **3. 재사용 위젯 먼저** (`/Game/UI/Widgets/HUD/`) — `Add > User Interface > Widget Blueprint` · 부모 `UserWidget`
  - [ ] `WBP_ER_ItemSlot` — §3.N 의 트리 · 이름 그대로. 디자이너 변수 `bShowKeyLabel` · `bShowRatio` · `bCompact` (Boolean · **Instance Editable** ✔ · Expose on Spawn ✘). `Event Pre Construct` 에서 이 셋으로 `KeyText` · `RatioText` Visibility 만 바꾼다 — **표시 옵션만, 데이터 로직 금지**
  - [ ] `WBP_ER_SkillSlot` — §3.I 의 트리 · 이름 그대로. `CooldownOverlay` 의 Brush 에 `M_UI_CooldownRadial` (아직 없으면 빈 Image). `CooldownText` · `DisabledOverlay` = **Collapsed**. `LevelPips` 안에 Image 5개
  - [ ] `WBP_ER_StatRow` — HorizontalBox [Image `StatIcon` · TextBlock `StatValue`]
  - [ ] `WBP_ER_BuffIcon` — Overlay [Image `IconImage` · TextBlock `TimeText` · TextBlock `StackText`(우상단 — 원작 12.0 중첩 수 우측 상단 · 역기획서 §3 #11)]
- [ ] **4. 영역 위젯** — §2 트리대로. 각 위젯 이름(BindWidget 대상)은 §2 · §3 의 **굵은/영문 이름 그대로**
  - [ ] `WBP_ER_BottomPanel` — Overlay + `BottomPanelBg` + HorizontalBox. 안에 `WBP_ER_StatPanel` · `EquipColumn`(ItemSlot ×5 · bCompact ✔ · 이름 `EquipWeapon` `EquipHead` `EquipChest` `EquipArm` `EquipLeg`) · `WBP_ER_Portrait` · `WBP_ER_SkillBar`(SkillSlot ×8 · 이름 `SlotQ` `SlotW` `SlotE` `SlotR` `SlotP` `SlotD` `SlotF` `SlotG` · **`SlotF` · `SlotG` 는 Collapsed**) · `WBP_ER_VitalBar`(ProgressBar `HPBar` · TextBlock `HPText` · ProgressBar `VPBar` **Collapsed**) · `WBP_ER_CreditBox` · `WBP_ER_Bag`(ItemSlot ×10 · bShowKeyLabel ✔ · 이름 `BagSlot0`~`BagSlot9`)
  - [ ] `WBP_ER_MatchInfo` · `WBP_ER_KillScore` · `WBP_ER_RouteGoals` · `WBP_ER_AreaMaterials` · `WBP_ER_Minimap` · `WBP_ER_SystemMessage` · `WBP_ER_NetInfo` · `WBP_ER_SkillUpPrompt` · `WBP_ER_BuffBar`(BuffIcon ×8 · 전부 Collapsed)
  - [ ] 고정폭 숫자: A `02:11` · L `890 / 890` · I 쿨다운 숫자 TextBlock 에 **Min Desired Width** 를 준다 (값은 폰트 크기에 맞춰 — 자릿수 최대 기준)
- [ ] **5. 루트** `WBP_ER_HUD` — Canvas Panel `RootCanvas` 하나 · 영역 위젯을 §1.1 위치에 **앵커** 지정 (A 상단 중앙 · B/C 우상단 · D 우측 · E 우하단 · O 좌측 · P 좌하단 · J/K/BottomPanel 하단 중앙). Canvas 는 여기에만 (공통 §4.5)
  - 앵커 · Alignment · Position · Size · ZOrder 숫자는 [레이아웃 표 (1920×1080 기준)](#레이아웃-표-19201080-기준) 를 그대로 넣는다
- [ ] **6. 최적화 지정** (각 WBP Details)
  - [ ] Invalidation Box 로 감싸기: `MatchInfo` · `KillScore` · `StatPanel` · `SkillBar` · `Bag` · `EquipColumn` · `AreaMaterials` (§4 표 ○)
  - [ ] `WBP_ER_SkillSlot` 의 `CooldownOverlay` · `CooldownText` → **Performance > Is Volatile ✔** (§3.I — 슬롯 전체 X)
  - [ ] `WBP_ER_Minimap` 의 자기 위치 Image → Is Volatile ✔
  - [ ] 표시 전용 위젯 Visibility = `Not Hit-Testable (Self & All Children)` — StatPanel · MatchInfo · KillScore · NetInfo · VitalBar · Portrait (공통 §4.7). 클릭할 것(Bag · EquipColumn · SkillUpPrompt 마름모)은 `Visible` 유지
  - [ ] 모든 WBP: Event Graph 에 **Event Tick 없음 · Bind 드롭다운 없음** (공통 §5 "위젯 BP 공통")
- [ ] **7. (C++ 베이스가 생긴 뒤)** 각 WBP `File > Reparent Blueprint` → §7 의 `UER~Widget`. 컴파일 에러 *"required widget binding"* 가 나면 이름 불일치 (공통 §5 ② 표)
- [ ] **8. (C++ 뒤)** 월드 위젯 — 실험체 BP 의 `UWidgetComponent` 에 `WBP_ER_Nameplate` 지정 · Space = **Screen** (§3.Q)

**지금 넣을 가짜 값:** 디자이너 미리보기용 텍스트(`02:11` · `890 / 890` 등)는 TextBlock 의 기본 Text 에 넣어도 된다 — C++ 가 덮어쓴다. **BP 그래프에서 값을 넣지 않는다.**

### ② 테스트

**지금 (C++ 없음) — 디자이너 미리보기만**
- [ ] `WBP_ER_HUD` Designer 탭 상단 해상도를 **1920×1080** 으로 → 레퍼런스 이미지와 겹쳐 비교. 성공: §1.1 배치도의 영역이 같은 자리에 있다
- [ ] 해상도를 **1280×720** · **2560×1440** 으로 바꿔도 앵커 덕에 영역이 화면 밖으로 안 나간다. 실패 = 앵커 잘못
- [ ] `WBP_ER_ItemSlot` 의 `bShowKeyLabel` 을 껐다 켜면 Designer 에서 키 글자가 사라졌다 나타난다
- [ ] 모든 WBP **Compile** 경고 0

**C++ 뒤 (PIE) — 공통 §5 ② 를 따른다** (`Play As Client` · Number of Players: 2)

| 확인 | 성공 판정 | 실패 시 증상 → 고칠 곳 |
|---|---|---|
| 체력 | 콘솔로 서버에서 피해 → 클라 L 바가 줄고 숫자 일치 | 0/빈칸 → 구독 직후 초기화 누락 (공통 §2.3) |
| 쿨다운 | Q 시전 → 게이지가 돌고 숫자가 줄다가 끝나면 사라짐. 남의 화면에는 내 쿨다운 없음 | 안 돎 → `Cooldown.Slot.Q` 태그 이벤트 구독 확인 · 끝나도 남음 → 태그 제거 때 타이머 해제 누락 |
| 페이즈 | 클라 2개의 `02:11` 이 ±1초 안 (역기획서 "구현 우선순위" 2순위 기준 ±0.5초) | 어긋남 → `GetServerWorldTimeSeconds` 대신 로컬 시간을 씀 |
| 가방 | 서버가 아이템 지급 → 소유 클라만 칸 갱신 · 다른 클라 HUD 는 그대로 | 다른 클라에도 보임 → 남의 PlayerState 인벤토리를 읽음 |
| 스킬 포인트 | 레벨업 → J 표시 · 마름모 클릭 → 서버 로그 레벨업 · J 사라짐 | 클릭해도 무반응 → `ServerLevelUpSkill` 호출 확인 |
| Tick | `stat slate` — 아무것도 안 할 때 UI 시간이 튀지 않음 | 기준 수치 (미확인 — 이 HUD 가 첫 측정 · 공통 §7) |

---

## 7. C++ 가 나중에 할 일 (이 문서는 소스를 바꾸지 않는다)

⚠ 전부 **CLAUDE.md §1.1 합의 후** 착수. 방안이 둘 이상인 것은 §7.3 에 적고 `Docs/4_Argument/` 로 넘긴다.

### 7.1 소스에 **없는 이벤트**를 추가 (⚠ 표시 항목)

| 할 일 | 위치 | 이유 |
|---|---|---|
| 가방 변경 델리게이트 Broadcast | `UERInventoryComponent::OnRep_Bag` (`Item/ERInventoryComponent.cpp:63-67`) + 서버 `AddItem` · `RemoveItem` 경로 | N · C — 지금 로그만 |
| 장착 칸 **전부** Broadcast (클라) | `UERInventoryComponent::OnRep_Equipped` (`ERInventoryComponent.cpp:69-78`) | G — 지금 무기 칸만 |
| 스킬 포인트 델리게이트 | `AERPlayerState::OnRep_SkillPoints` (`Core/ERPlayerState.cpp:469-473`) | J |
| 레벨 · 경험치 클라 델리게이트 | `UERGrowthComponent::OnRep_Level` (`Growth/ERGrowthComponent.cpp:474-478` "F17 HUD 훅 자리") · `Exp` 는 `Replicated` 만 (`.h:86`) → `ReplicatedUsing` 필요 | H · Q |
| 스킬 레벨 변경 알림 (클라) | 스펙 레벨은 ASC 스펙 복제로만 온다. 클라 알림 경로 **(미확인)** — 같은 서버 동작에서 `SkillPoints` 도 바뀌므로 `OnRep_SkillPoints` 에서 다시 읽는 방식이 가능한지 확인 필요 | I 레벨 눈금 |
| **클라 ASC 준비 완료** 알림 | `AERCharacterBase::InitAbilityActorInfo` (`Character/ERCharacterBase.h:121` · 클라는 `OnRep_PlayerState` `ERCharacterBase.cpp:157-162`) | HUD 가 ASC 를 언제 구독할지 — 공통 §2.3 "(미확인 — 01_인게임HUD 에서 정한다)" |

### 7.2 소스에 **없는 데이터** (❌)

| 데이터 | 기능 | 영역 |
|---|---|---|
| 스킬 아이콘 필드 (`UERSkillData`) | F17 (데이터) | I |
| 초상 필드 (`UERCharacterData`) | F17 (데이터) | H |
| GE UI 데이터 (아이콘 · 버프/디버프 구분) | F17 | K |
| 처치 · 팀 처치 · 어시스트 복제 (`PlayerKills` 는 서버 전용 `ERPlayerState.h:158`) | F14 | B |
| 생존 팀 수 | F14 | A |
| 크레딧 | F15 | M |
| 지역 판정 · 지도 · 지역 재료 | F13 | D · E |
| 야생동물 재생성 시각 복제 | F12 후속 | R |
| 소비 아이템 사용 RPC | F15 (미확인) | N |
| 클라 표시용 "스킬 올릴 수 있나" 판정 | F17 | J |
| 목표 장비(루트) · 제작 트리 | F17 (`03_아이템제작.md`) | C |

### 7.3 C++ 위젯 베이스 (Build.cs 에 `UMG` `Slate` `SlateCore` 추가 후 — 공통 §0)

| C++ 베이스 | WBP | 책임 |
|---|---|---|
| `UERHUDWidget` | `WBP_ER_HUD` | ASC · PlayerState · GameState 를 찾아 자식 위젯에 넘김 |
| `UERSkillSlotWidget` | `WBP_ER_SkillSlot` | 슬롯 태그(디자이너 변수) · 쿨다운 태그 이벤트 · 0.1초 타이머 · 레벨 눈금 |
| `UERItemSlotWidget` | `WBP_ER_ItemSlot` | 아이템 ID → 아이콘 비동기 로드 · 등급색 · 수량 |
| `UERStatRowWidget` | `WBP_ER_StatRow` | 어트리뷰트 1~2개 구독 · 표시 형식 |
| `UERVitalBarWidget` | `WBP_ER_VitalBar` | HP · MaxHP (· VP) 구독 |
| `UERMatchInfoWidget` | `WBP_ER_MatchInfo` | `OnPhaseChanged` · 1초 타이머 |
| `UERNameplateWidget` | `WBP_ER_Nameplate` | 대상 폰의 ASC 구독 (남의 ASC) |

- 나머지 영역(Bag · StatPanel · SkillBar · Portrait …)은 **자식을 BindWidget 으로 들고 데이터를 나눠 주는 얇은 베이스**가 필요한지, `UERHUDWidget` 하나가 직접 자식 슬롯을 채울지 → **CLAUDE.md §2 "한 곳에서만 쓰는 코드에 추상화 금지"** 로 보면 후자. 착수 때 결정 (미확인)
- 모든 베이스 `UCLASS(meta=(DisableNativeTick))` — 미니맵만 예외 (공통 §4.8)
- 입력(마름모 클릭 · 가방 칸 클릭)은 위젯이 **RPC 만** 부른다 (`ServerLevelUpSkill` · `ServerEquipFromBag` · `ServerDiscard`)

**결정이 필요한 것 — `Docs/4_Argument/` 로 (CLAUDE.md §1.2)**

| 결정 | 후보 | 비고 |
|---|---|---|
| HUD 를 누가 만드나 | `AHUD` 파생이 생성 / `AERPlayerController::BeginPlay` 의 로컬 분기(`ERPlayerController.cpp:41`)에서 생성 | 공통 §1.1 이 01 에 넘긴 미확인. Lyra 는 `ALyraHUD` + CommonUI (`Samples/Games/Lyra/Source/LyraGame/UI/LyraHUD.h`) — 우리는 CommonUI 미도입 |
| ASC 준비를 HUD 가 아는 법 | 폰/PlayerState 델리게이트 추가 / HUD 가 폰 Possess 이벤트에서 재시도 | §7.1 마지막 줄 |
| 쿨다운 게이지 진행 | 0.1초 타이머 + MID 파라미터 (기본) / 머티리얼 `Time` (GPU) | §3.I — `Time` 동기화 확인이 선행 |

- Lyra `Source/LyraGame/UI/` 에는 쿨다운 위젯 C++ 가 **없다** (grep `Cooldown` 0건) → 쿨다운 표시는 Lyra 대조 대상이 없다. HUD 구조 결정 때는 `Docs/6_Lyra참조/` 에 대조 기록을 남긴다 (CLAUDE.md §11)

---

## 8. 주의점

- **BindWidget 이름이 곧 계약이다.** WBP 에서 이름을 바꾸면 C++ 연결 후 컴파일 에러. 반대로 C++ 에서 이름을 바꾸면 WBP 가 깨진다 — 바꾸면 이 문서 §2 · §3 를 같이 고친다
- `SlotF` · `SlotG` · VP 바 · 날씨 아이콘 등 **뜻이 확정되지 않은 자리**는 Collapsed 로 둔다. 지우지 않는다 — 원작 확인 후 쓸 수 있다
- 가짜 미리보기 값을 **BP 그래프에서** 넣으면 C++ 값과 싸운다. Designer 기본 텍스트만 쓴다
- Listen Server 호스트 화면으로 테스트하지 않는다 — OnRep 가 안 온다 (공통 §2.3)
- `WBP_ER_ItemSlot` 에 `Size Box` 를 넣지 않는다 (공통 §4.5) — 크기는 부모 Grid 슬롯이 정한다

## 9. 못 하는 검증

| 검증 | 왜 못 하나 | 언제 |
|---|---|---|
| 실제 값이 HUD 에 들어오는지 | C++ 위젯 베이스 없음 · Build.cs 에 UMG 없음 | §7.3 작업 후 |
| 쿨다운 게이지 동작 | 스킬 아이콘 · 위젯 베이스 없음 | §7.3 후 |
| 미니맵 · 지역 패널 · 크레딧 · 처치 수 | 데이터 없음 (F13 · F14 · F15) | 각 기능 후 |
| `stat slate` 성능 기준 | HUD 가 처음이라 기준선 없음 | 첫 PIE 때 측정해 공통 §4 에 기록 |
| 이미지 비교 정확도 | 원본 UI 리소스 없음 — 스크린샷 기준 눈대중 | 리소스 확보 후 |

## 10. 끝나면

- F17 의 Task · Checklist 폴더가 **아직 없다** (`Docs/1_Task/` · `Docs/2_Checklist/` 에 F17 없음 · `00_기능목록 :69` ⬜ 미작성). 이 문서의 ① 1~6 이 끝나면 F17 작성 시 "인게임 HUD 위젯 껍데기" 항목을 ✅ 로 시작한다
- §1.3 · §3 의 (미확인) 은 원작 확인 후 이 문서를 고친다

---

## 출처

- 레퍼런스 이미지 `C:\Sung Unreal Project\Eternal Return\UI Image\인게임.png` (직접 열어 확인)
- [Slate UI Sleeping and Active Timers in Unreal Engine](https://dev.epicgames.com/documentation/unreal-engine/slate-ui-sleeping-and-active-timers-in-unreal-engine) — Active Timer 는 에디터 UI 용 · all-or-nothing (2026-10-12 읽음 · 5.8 표기판 · 5.4 차이 (미확인))
- [Optimization Guidelines for UMG in Unreal Engine](https://dev.epicgames.com/documentation/unreal-engine/optimization-guidelines-for-umg-in-unreal-engine) — 바인딩 금지 · 머티리얼 애니 · Rich Text · Canvas
- [Invalidation in Slate and UMG for Unreal Engine](https://dev.epicgames.com/documentation/unreal-engine/invalidation-in-slate-and-umg-for-unreal-engine) — Paint/Layout/Child 무효화 비용 · Volatile · Retainer 보다 Invalidation Box 먼저
- [`00_UI_공통원칙.md`](00_UI_공통원칙.md) §0 · §1 · §2 · §4 · §5
- [`../../0_GameDesign/Systems/UI_HUD_역기획서.md`](../../0_GameDesign/Systems/UI_HUD_역기획서.md) §1 · §2 · §3 · §4.3 · §5 · §9 · 미확인 항목
- `CLAUDE.md` §2 · §6 · §7 · §8 · §11
- 프로젝트 코드: `GAS/ERAttributeSet.h` · `GAS/ERAttributeSet.cpp:296` · `GAS/ERGameplayTags.h` · `GAS/ERSkillData.h` · `Item/ERInventoryComponent.h/.cpp` · `Item/ERItemData.h` · `Item/ERItemTypes.h` · `Item/ERItemSettings.h` · `Item/ERItemDropActor.h` · `Growth/ERGrowthComponent.h/.cpp` · `Core/ERGameState.h/.cpp` · `Core/ERPlayerState.h/.cpp` · `Character/ERCharacterBase.h/.cpp` · `Wildlife/ERWildlifeSpawnSubsystem.h` (각 라인은 본문)
- 엔진: `Engine/Plugins/Runtime/GameplayAbilities/Source/GameplayAbilities/Public/AbilitySystemComponent.h:518-519,534,744,836` · `GameplayEffect.h:1476`
- Lyra: `Samples/Games/Lyra/Source/LyraGame/UI/` (grep `Cooldown` 0건)
