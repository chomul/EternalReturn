# 매그너스 — 소리 배치표 (2026-10-04 사용자 결정)

> 경로 `/Game/ER/Audio/SFX/Character_FX/magnus/s000/` (S000 기본 스킨) · 규칙 [00_규칙](00_규칙.md)
> **`어디서` 칸을 고치면 그대로 적용한다.** 다른 스킨은 `s00x/` 에 같은 파일명이면 자동으로 덮는다 (s002 · s003 · s005 에 같은 이름 있음).
> 2026-10-04 사용자가 1~24 를 정했다 (13 = 시동). 코드 빌드 ✅ (Editor · Server) — `ER.Pres.Fill Magnus Force` · PIE 대기.

## 지금 상태 (로그 2026-10-04)

- 매그너스 소리는 이름이 `magnus_Skill0N_<사건>` 이다(무기 토큰 없음 · 발사가 `Shot` 이 아니라 `Attack`). 지금 Fill 규칙(`<캐릭터>_<무기>_Skill0N_Shot`)에 안 맞아 **전부 규칙 밖**이다
- 그래서 지금 Q · E · W 는 **망치 공용 소리**가 난다: 시전 `skillHammer` · 적중 `hitSkillHammer` (`[무기공통]` · 로그 `소리 skillHammer_r1 [무기공통] ← … Pres.Sfx.SkillCast`)
- 평타(`attackHammer` · `hitHammer` · 방망이도)는 무기 공용으로 이미 난다 ✅ — 이 표에는 안 넣는다

## 표

| # | 소리 | 어디서 | 이유 | 상태 |
|---|---|---|---|---|
| 1 | `magnus_Skill01_Attack` | 코드 (Q 공격 큐) | 던지는 순간 (사용자 2026-10-04) · 망치 공용 `skillHammer` **대신** | ✅ 코드 (`SkillCast.Q`) · ⏳ Fill · PIE |
| 2 | `magnus_Skill01_Arrival` | 보류 | 뜻 (미확인) (사용자 2026-10-04 "잘 모르겠음") | ⏸ |
| 3 | `magnus_Skill01_Hit` | 코드 (Q 타격 큐) | 적이 맞았을 때 (사용자 2026-10-04) · 망치 공용 `hitSkillHammer` **대신** | ✅ 코드 (`SkillHit.Q`) · ⏳ Fill · PIE |
| 4 | `magnus_Skill01_Impact` | 코드 (Q 타격 큐 · Hit 뒤 **조금 늦게**) | 맞고 난 직후 (사용자 2026-10-04 "Hit 보다 조금 느리게") — Impact · r1 · r2 중 무작위 · 늦춤 [자체] 0.15초 | ✅ 코드 (`SkillHitLate.Q` · 0.15초) · ⏳ Fill · PIE |
| 5 | `magnus_Skill01_Impact_r1` | 4 와 같음 (무작위 변형) | | ✅ (4 와 같은 키) |
| 6 | `magnus_Skill01_Impact_r2` | 4 와 같음 (무작위 변형) | | ✅ (4 와 같은 키) |
| 7 | `magnus_Skill02_Attack` | 코드 (W **도는 동안** 반복 · `State.AnimHold` 동안) | 도는 동안 나는 소리 (사용자 2026-10-04) — 소리가 끝나면 다시 튼다 · 장판이 끝나면 멈춘다 | ✅ 코드 (`SkillLoop.W` · 끝나면 다시) · ⏳ Fill · PIE |
| 8 | `magnus_Skill02_Hit_r1` | 코드 (W 타격 큐 · 펄스마다 무작위) | 도는 동안 타격음 (사용자 2026-10-04) — r1~r3 중 무작위 | ✅ 코드 (`SkillHit.W`) · ⏳ Fill · PIE |
| 9 | `magnus_Skill02_Hit_r2` | 8 과 같음 | | ✅ (8 과 같은 키) |
| 10 | `magnus_Skill02_Hit_r3` | 8 과 같음 | | ✅ (8 과 같은 키) |
| 11 | `magnus_Skill03_Hit` | 코드 (E 타격 큐) | 망치로 친 결과 (사용자 2026-10-04 확인). E 시전 소리는 매그너스 전용이 없다 → 망치 공용 `skillHammer` 그대로 | ✅ 코드 (`SkillHit.E`) · ⏳ Fill · PIE |
| 12 | `magnus_Skill04_Activation` | 코드 (R 시전 큐) | 바이크에 타는 순간 (사용자 2026-10-04 확인) | ✅ 코드 (`SkillCast.R`) · ⏳ Fill · PIE |
| 13 | `magnus_Skill04_GoActive` | 코드 (R 탑승 순간 한 번 · 시동) | 시동 (사용자 2026-10-04) — `State.Riding` 이 붙을 때 반복 소리 시작 직전 | ✅ 코드 (`SkillLoopStart.R`) · ⏳ Fill · PIE |
| 14 | `magnus_Skill04_Drive` | 코드 (탄 동안 반복 · `State.Riding` 동안) | 엔진 소리 7초 내내 (사용자 2026-10-04 확인) — 끝나면 다시 튼다 · 내리면 멈춤 | ✅ 코드 (`SkillLoop.R`) · ⏳ Fill · PIE |
| 15 | `magnus_Skill04_Attack` | 코드 (R 재사용 = 바이크 발사 큐) | 바이크를 앞으로 보내는 순간 (사용자 2026-10-04 확인) | ✅ 코드 (`SkillRecast.R`) · ⏳ Fill · PIE |
| 16 | `magnus_Skill04_Hit` | 코드 (R 폭발 타격 큐) | 충돌 · 발사 바이크 폭발로 맞은 결과 (사용자 2026-10-04 확인) | ✅ 코드 (`SkillHit.R` · 폭발은 타격음만) · ⏳ Fill · PIE |
| 17 | `magnus_AirdropOpen` | 보류 | 보급 상자 여는 연출 — 기능 없음 — 맵 · 상자 만들 때 (사용자 2026-10-04) | ⏸ |
| 18 | `magnus_AirdropOpen_1` | 보류 | 같음 — 맵 · 상자 만들 때 (사용자 2026-10-04) | ⏸ |
| 19 | `magnus_AirdropOpen_2` | 보류 | 같음 — 맵 · 상자 만들 때 (사용자 2026-10-04) | ⏸ |
| 20 | `magnus_AirdropOpen_3` | 보류 | 같음 — 맵 · 상자 만들 때 (사용자 2026-10-04) | ⏸ |
| 21 | `magnus_AirdropOpen_4` | 보류 | 같음 — 맵 · 상자 만들 때 (사용자 2026-10-04) | ⏸ |
| 22 | `magnus_selected_animation` | 보류 | 로비 캐릭터 선택 — 맵 · 상자 만들 때 (사용자 2026-10-04) | ⏸ |
| 23 | `magnus_selected_animation2` | 보류 | 같음 — 맵 · 상자 만들 때 (사용자 2026-10-04) | ⏸ |
| 24 | `Magnus_VictoryCutscene_sfx` | 보류 | 승리 연출 — 맵 · 상자 만들 때 (사용자 2026-10-04) | ⏸ |

- 🆕 = 코드 · Fill 작업 필요 · ⚠ = 들어보고 결정 · ⏸ = 보류

## 코드 작업 — ✅ 2026-10-04 (빌드 통과)

1. **Fill 규칙 추가** — `<캐릭터>_Skill0N_<Attack|Hit>[_rK]` (무기 토큰 없음) → `SkillCast.<슬롯>` · `SkillHit.<슬롯>` · **캐릭터 기본 표**(무기 무관). `_rK` 는 무작위 변형으로 묶는다
   - 슬롯 키가 무기 공용 `SkillCast` · `SkillHit` 보다 좁으니 그쪽을 이긴다 (지금 조회 순서 그대로)
2. **R 시전 · 재사용 · 폭발** 큐 — 지금 R 은 시전 큐는 나간다(`SkillCast.R`). 재사용(발사) · 폭발이 큐를 보내는지 확인 · 없으면 추가
3. **탄 동안 반복 소리 (14)** — 소품처럼 `State.Riding` 태그로 켜고 끈다 (각 머신 로컬)
4. ⚠ 줄 결정에 따라: Q 빗나감 도착 큐 · Hit + Impact 겹쳐 내기(키 하나 더)

### 한 것 (2026-10-04)
- Fill 규칙 `<캐릭터>_Skill0N_<Attack|Activation|Hit|Impact|Drive>[_rK]` → **캐릭터 기본 표** (무기 무관). 예외 표(코드 · 이 배치표의 거울): `Skill02_Attack` = 반복 W · `Skill04_Attack` = 재사용 R · `Skill04_GoActive` = 시동 R
- 키 5개: `SkillHitLate.Q` · `SkillLoop.W` · `SkillLoop.R` · `SkillLoopStart.R` · `SkillRecast.R`
- 연출: Q 타격 뒤 0.15초 `SkillHitLate.Q` · 태그 동안 반복(`State.AnimHold` → W · `State.Riding` → R + 시동) · 재사용 공격음
- 어빌리티 큐: 슬롯 없는 하위 DA(`R_Explode` · `R_Launch` · `E_Wall`)는 그 어빌리티 슬롯으로 → 매그너스 소리가 난다 (전에는 망치 공용) · 재사용 발동이면 재사용 표시 · R 폭발은 타격음만
- ⚠ 벽에 부딪혀 아무도 안 맞은 폭발은 **소리가 없다** (16 = 맞은 결과). 폭발음이 따로 필요하면 알려 준다
- ⚠ E 벽 충돌 추가 피해(`E_Wall`)도 이제 `Skill03_Hit` 이 난다 (같은 E 슬롯)

### PIE 1차 (로그 2026-10-04 14:45 · 데디)
- ✅ Q `Skill01_Attack` · `Skill01_Hit` · 늦은 `Impact_r1/r2` · E `Skill03_Hit` · W 반복 시작 → 4초 뒤 멈춤 · R `Activation` · `Drive` 반복 → 내리면 멈춤 · 데디 서버에는 소리 줄 없음
- ✘ W 펄스 타격음 없음 — 장판이 타격음 큐를 안 보냈다 → 장판 칸 `bHitCuePerPulse` (W 만 켬 · 독가스 · 마름쇠는 그대로 꺼짐)
- ✘ R 바이크 발사음 없음 — 리캐스트 공격음이 막혔다 ([E39](../../5_ErrorReport/E39_리캐스트_발사에_공격음이_막혔다.md))
- `GoActive`(시동)는 로그 줄이 없어 확인 못 함 — 귀로 확인

### PIE 2차 (로그 2026-10-05 16:29–16:30 · 데디) — ✅
- W 펄스마다 `magnus_Skill02_Hit_r1~r3` 무작위 (`SkillHit.W`) · R 재사용 `magnus_Skill04_Attack` (`SkillRecast.R`) · 발사 바이크 적중 `magnus_Skill04_Hit` (`SkillHit.R`) · 탑승 `Activation` + `Drive` 반복
- 남은 확인: `GoActive`(시동) 귀로 · E 벽 충돌 `E_Wall` 이 타격음만 나는지 (E39 두 번째 줄)
