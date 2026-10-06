# 재키 — 소리 배치표 (2026-10-06 사용자 결정 · 코드 빌드 ✅)

> 경로 `/Game/ER/Audio/SFX/Character_FX/jackie/s000/` (S000 기본 스킨 · 34개) · 규칙 [00_규칙](00_규칙.md)
> **`어디서` 칸을 고치면 그대로 적용한다.** 다른 스킨은 `s00x/` 에 같은 파일명이면 자동으로 덮는다 (s003 · s005 에 같은 이름 다수).
> 2026-10-06 사용자 결정 전부 반영 · 코드 빌드 ✅ (Editor · Server). 남은 일: `ER.Pres.Fill Jackie Force` · PIE · 5 소리 크기 (노티파이 없음 — 17 도 코드).

## 지금 상태

- 평타 · Q · D 는 **무기 공용** 소리가 이미 난다 (`attackOneHandSword` · `skillAxe` …) — 재키 전용 Q 소리 파일은 없다 → 표에 안 넣는다
- 이 폴더 소리는 대부분 **규칙 밖**이다: 이름이 Fill 규칙(`<캐릭터>_Skill0N_<Attack|Activation|Hit|Impact|Drive>`)과 다르고 (`skill02attack_Axe` · `Skill03_Jump` …), `_v1` · `_v2` 는 지금 규칙이 "변형" 으로 **건너뛴다** → 코드 · Fill 규칙 작업이 필요하다
- E 는 판정을 **착지 순간**으로 옮겼다 (`JudgeDelay`) → E 의 시전 큐도 착지 때 나간다. 점프하는 순간 소리는 큐가 아니라 **노티파이**가 맞다

## 표

| # | 소리 | 어디서 | 이유 | 상태 |
|---|---|---|---|---|
| 1 | `jackie_ChainSaw_Attack_v1` | 코드 (평타 공격 큐 · **전기톱 모드** · v1 · v2 무작위) | 전기톱 평타 휘두름 — 모드 중 평소 무기 소리 **대신** | ✅ 명시 표 (모드 줄) · ⏳ Fill · PIE |
| 2 | `jackie_ChainSaw_Attack_v2` | 1 과 같음 | | ✅ 명시 표 |
| 3 | `jackie_ChainSaw_Hit_v1` | 코드 (평타 타격 큐 · 전기톱 모드 · 무작위) | 전기톱 평타 맞음 | ✅ 명시 표 |
| 4 | `jackie_ChainSaw_Hit_v2` | 3 과 같음 | | ✅ 명시 표 |
| 5 | `jackie_Passive_Activation` | 코드 (**출혈을 걸 때** · 적중마다 · `Pres.Sfx.SkillHit.P` · 대상 자리) | 출혈 부여 (사용자 2026-10-06) · **소리 크기는 나중에** (자주 난다) | ✅ 코드 · ⏳ Fill · PIE |
| 6 | `jackie_Passive_MaxStart` | 코드 (아드레날린 시작 — `State.Adrenaline` 붙을 때 한 번) | 출혈 최대 → 아드레날린 (사용자 2026-10-06) | ✅ 명시 표 |
| 7 | `jackie_Passive_MaxLoop` | 코드 (아드레날린 5초 동안 반복 · `State.Adrenaline` 동안) | (사용자 2026-10-06) | ✅ 명시 표 |
| 8 | `jackie_Skill02_Activation` | 안 씀 | 무시 (사용자 2026-10-06) · Fill 규칙(`Activation`)이 W 시전으로 잡으므로 예외로 뺀다 | — |
| 9 | `jackie_Skill02_Start` | 코드 (W 시전 — 평타 강화가 걸릴 때 · 강화 걸림 큐 `EnhanceReady`) | 평타 강화할 때 나오는 소리 (사용자 2026-10-06) | ✅ 명시 표 |
| 10 | `jackie_skill02_attack_1Hand` | 코드 (평타 공격 큐 · **강화 시** · 단검) | W 강화 평타 휘두름 — 평소 평타 소리 **대신** (`Pres.Sfx.AttackEnhanced` · 카티야 P 와 같은 키) | ✅ 명시 표 (무기 세트) · ⏳ Fill · PIE |
| 11 | `jackie_skill02_attack_2Hand` | 10 과 같음 (양손검) | 양손검이 아직 없다 | ✅ 명시 표 (양손검이 생기면 난다) |
| 12 | `jackie_skill02attack_Axe` | 10 과 같음 (도끼) | 이름이 `attack_` 이 아니라 `attack` 붙여 씀 — 규칙이 둘 다 받게 | ✅ 명시 표 |
| 13 | `jackie_skill02attack_Dual` | 10 과 같음 (쌍검) | 쌍검이 아직 없다 | ✅ 명시 표 |
| 14 | `jackie_skill02attack_Saw` | 10 과 같음 (**전기톱 모드**) | 모드 중 강화 평타 | ✅ 명시 표 (모드 줄) |
| 15 | `jackie_skill02attack_Hit` | 코드 (평타 타격 큐 · 강화 시) | 강화 평타 맞음 (`Pres.Sfx.HitEnhanced`) | ✅ 명시 표 |
| 16 | `jackie_Skill03_Jump` | 안 씀 | 무시 (사용자 2026-10-06) | — |
| 17 | `jackie_Skill03_Jumping` | 코드 (E 시전 큐 — **E 누를 때** · 뛰어오르는 순간 · `SkillCast.E`) | E 실행할 때 — 노티파이 대신 코드로 (사용자 2026-10-06) | ✅ 코드 · ⏳ Fill · PIE |
| 18 | `jackie_Skill03_Bump` | 코드 (E **착지 큐** · 판정 순간 · `SkillLand.E` · 맞힌 사람 없어도) | E 착지할 때 (사용자 2026-10-06) | ✅ 코드 · ⏳ Fill · PIE |
| 19 | `jackie_Skill04_Activation_Start` | 안 씀 | 무시 (사용자 2026-10-06) | — |
| 20 | `jackie_Skill04_Activation_Loop` | 안 씀 | 무시 — 23 · 24 가 대신 (사용자 2026-10-06) | — |
| 21 | `jackie_Skill04_Activation_v1` | 코드 (R 시전 큐 · **전기톱 시동** · v1 · v2 무작위 · `SkillCast.R`) | R 누를 때 (사용자 2026-10-06) | ✅ 코드 · ⏳ Fill · PIE |
| 22 | `jackie_Skill04_Activation_v2` | 21 과 같음 | | ✅ |
| 23 | `jackie_Skill04_ChainSaw_v1` | 코드 (R 동안 반복 · `Mode.Chainsaw` 동안 · v1 · v2 무작위) | R 동안 — 20 대신 (사용자 2026-10-06) | ✅ 명시 표 |
| 24 | `jackie_Skill04_ChainSaw_v2` | 23 과 같음 | | ✅ 명시 표 |
| 25 | `jackie_Skill04_Finish_v1` | 코드 (R 재사용 = **학살** 공격 큐 · v1 · v2 무작위 · `SkillRecast.R`) | 학살 (사용자 2026-10-06) | ✅ 명시 표 |
| 26 | `jackie_Skill04_Finish_v2` | 25 와 같음 | | ✅ 명시 표 |
| 27 | `jackie_AirdropOpen` | 보류 | 보급 상자 — 맵 · 상자 만들 때 | ⏸ |
| 28 | `jackie_AirdropOpen_1` | 보류 | 같음 | ⏸ |
| 29 | `jackie_AirdropOpen_2` | 보류 | 같음 | ⏸ |
| 30 | `jackie_AirdropOpen_3` | 보류 | 같음 | ⏸ |
| 31 | `jackie_AirdropOpen_4` | 보류 | 같음 | ⏸ |
| 32 | `jackie_selected_animation` | 보류 | 로비 캐릭터 선택 | ⏸ |
| 33 | `jackie_selected_animation2` | 보류 | 같음 | ⏸ |
| 34 | `Jackie_VictoryCutscene_sfx` | 보류 | 승리 연출 | ⏸ |

- 🆕 = 코드 · Fill 작업 필요 · ⚠ = 들어보고 결정 · 📝 = 노티파이 찍기 (사용자) · ⏸ = 보류

## 코드 작업 — ✅ 2026-10-06 (빌드 통과)

1. **Fill 규칙** — `_vK` 를 무작위 변형으로 받기 (지금은 건너뜀 — 다른 캐릭터의 `_v` 뜻을 확인하고) · `skill02attack_<무기>` / `skill02_attack_<무기>` → 무기 세트 `AttackEnhanced` (Saw = 모드 줄) · `skill02attack_Hit` → `HitEnhanced` · `ChainSaw_Attack/Hit` → 모드 줄 평타 · `Skill03_Bump` → E 시전 · `Skill04_Activation_Start` → R 시전
2. **반복 소리** — 매그너스에서 만든 표(`LoopSfxTable`)에 `State.Adrenaline` (7) · `Mode.Chainsaw` (20) 두 줄
3. ⚠ 줄 결정에 따라: 아드레날린 시작 한 번 (6) · 출혈 걸 때 (5) · 학살 키 (25)

### 한 것 (2026-10-06)
- Fill **캐릭터 소리 명시 표** (`CharSoundRules` — 이 배치표의 거울 · 이름 정확히 → 무기 · 모드 · 키) — 이름이 규칙과 안 맞는 소리 · `_v1/_v2` 를 받는다. 스킨 폴더(s003 · s005)의 같은 이름도 스킨 줄로
- 키 3개: `Pres.Sfx.SkillHit.P` (출혈 걸 때) · `SkillLoop.P` · `SkillLoopStart.P` (아드레날린 동안 · 시작)
- 반복 소리 표에 2줄: `State.Adrenaline` → P · `Mode.Chainsaw` → R (매그너스 탑승과 같은 키 — 반복 소리를 **태그**로 들고 있게 바꿈) · 반복할 때마다 변형을 다시 고른다 (v1 · v2)
- 출혈 조각이 걸 때마다 P 타격 큐 · W 데이터 `bNoCastCue` (W 시전 때 무기 공용 스킬 소리 대신 9 강화 걸림 소리만)
- (추가) E 늦춘 판정: 뛰어오를 때 시전 큐(17) · 착지 때 **착지 큐** `GameplayCue.Pres.Land` → `Pres.Sfx.SkillLand.E` (18) — 판정 · 피해는 그대로 착지 순간
