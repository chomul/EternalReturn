# 시셀라 — 소리 배치표 (2026-10-06 사용자 결정 · 코드 빌드 ✅)

> 경로 `/Game/ER/Audio/SFX/Character_FX/sissela/s000/` (S000 기본 스킨 · 26개) · 규칙 [00_규칙](00_규칙.md)
> **`어디서` 칸을 고치면 그대로 적용한다.** 다른 스킨은 `s00x/` 에 같은 파일명이면 자동으로 덮는다 (s002 · s003 · s004 · s005).
> 2026-10-06 사용자 결정 전부 반영 · 코드 빌드 ✅ (Editor · Server). ✅ PIE 로그 2026-10-06 22:1x — 19줄 전부 서버 · 클라에서 남.

## 지금 상태

- 평타 · D 는 **무기 공용** 소리가 이미 난다 (`attackThrow` · `skillShuriken` …) — 표에 안 넣는다
- 시셀라 스킬 소리는 아직 **하나도 안 난다** (Fill 이 이 폴더를 아직 안 돌렸다)
- 이름 규칙 `<캐릭터>_SkillNN_<사건>` 은 Fill 기본 규칙과 대부분 맞는다 — `Fire` · `Hit` · `Start` · `End` 는 기존 키로, 시셀라만의 사건(합침 · 착지 폭발 · 끌기 · 카운트)은 새 키

## 표

| # | 소리 | 어디서 | 이유 | 상태 |
|---|---|---|---|---|
| 1 | `Sissela_Passive_Union` | 코드 (윌슨과 **합칠 때** · `Pres.Sfx.Join` — 줍기 · 거리 · E · W) | Union = 하나가 됨 | ✅ 코드 · ✅ PIE |
| 2 | `Sissela_Passive_Buff` | 코드 (강화 평타 **장전** · `EnhanceReady`) | (사용자 2026-10-06) | ✅ 명시 표 |
| 3 | `Sissela_Passive_Hit` | 코드 (강화 평타 **적중** · `HitEnhanced`) | | ✅ 명시 표 |
| 4 | `Sissela_Skill01_Fire` | 코드 (Q 시전 큐 · `SkillCast.Q`) | 윌슨 던짐 | ✅ 명시 표 |
| 5 | `Sissela_Skill01_Hit` | 코드 (Q **길** 적중 · `SkillHit.Q`) | | ✅ 명시 표 |
| 6 | `Sissela_Skill01_Hit2` | 코드 (Q **착지** 폭발 · `SkillLand.Q` · 맞힌 사람 없어도 · 착지 대상마다 타격음은 안 낸다) | 두 번째 피해 | ✅ 코드 |
| 7 | `Sissela_Skill01_Move` | 코드 (윌슨이 **날기 시작할 때 한 번** · `SkillMove.Q` · 윌슨이 떨어져 있으면 그 자리) | 움직이는 동안 한 번 (사용자 2026-10-06) | ✅ 코드 |
| 8 | `Sissela_Skill02_Start` | 코드 (W 시전 · `SkillCast.W`) | | ✅ 명시 표 |
| 9 | `Sissela_Skill02_End` | 코드 (W **터짐** · 1.5초 뒤 · `SkillBurst.W` · 맞힌 사람 없어도) | | ✅ 코드 |
| 10 | `Sissela_Skill02_Hit` | 코드 (W 터짐 **적중** · `SkillHit.W`) | | ✅ 명시 표 |
| 11 | `Sissela_Skill03_Fire` | 코드 (E 시전 큐 · `SkillCast.E`) | 몸 뻗기 | ✅ 명시 표 |
| 12 | `Sissela_Skill03_Hit` | 코드 (E **적** 적중 · `SkillHit.E`) | | ✅ 명시 표 |
| 13 | `Sissela_Skill03_Stun` | 코드 (E 적 적중 — 기절 · `SkillStun.E` · 12 와 **같이** 난다) | 겹쳐도 됨 (사용자 2026-10-06) | ✅ 코드 |
| 14 | `Sissela_Skill03_Take` | 코드 (E **끌어올 때** — 적 · 시셀라 · `SkillPull.E`) | Take = 끌어감 | ✅ 코드 |
| 15 | `Sissela_Skill03_Shield` | 코드 (E **시셀라 적중** — 보호막 · `SkillShield.E`) | | ✅ 코드 |
| 16 | `Sissela_Skill04_Start` | 코드 (R **누를 때** · 소리 조각 `CastStartSfx` · R 시전 큐는 끔) | | ✅ 코드 · 데이터 |
| 17 | `Sissela_Skill04_Count` | 코드 (R 집중(1초)이 끝나 카운트가 시작될 때 **한 번** · `SkillCount.R`) | 한 번만 (사용자 2026-10-06) — ⚠ 누를 때로 옮길지 들어보고 | ✅ 코드 · 데이터 |
| 18 | `Sissela_Skill04_Explosion` | 코드 (R **판정 순간** · 늦춘 판정 착지 큐 → `SkillLand.R`) | | ✅ 코드 |
| 19 | `Sissela_Skill04_Hit` | 코드 (R 적중 · 대상마다 · `SkillHit.R`) | | ✅ 명시 표 |
| 20 | `Sissela_Wilson_Death` | 안 씀 | 무시 (사용자 2026-10-06) | — |
| 21 | `Sissela_AirdropOpen_1` | 보류 | 보급 상자 — 맵 · 상자 만들 때 | ⏸ |
| 22 | `Sissela_AirdropOpen_2` | 보류 | 같음 | ⏸ |
| 23 | `Sissela_AirdropOpen_3` | 보류 | 같음 | ⏸ |
| 24 | `Sissela_AirdropOpen_4` | 보류 | 같음 | ⏸ |
| 25 | `Sissela_selected_animation` | 보류 | 로비 캐릭터 선택 | ⏸ |
| 26 | `Sissela_VictoryCutscene_sfx` | 보류 | 승리 연출 | ⏸ |

- 🆕 = 코드 · Fill 작업 필요 · ⚠ = 들어보고 결정 · 📝 = 노티파이 찍기 (사용자) · ⏸ = 보류
- 음성(`Audio/Voice/Sissela`)은 [00_규칙](00_규칙.md) 음성 절 — 나중에

## 코드 작업 — ✅ 2026-10-06 (빌드 통과)
- **소리 큐** `GameplayCue.Pres.Sfx` — 소리 키 하나를 서버에서 바로 (`UERPresentationComponent::SendSfxCue`) · 판정 큐로 못 잡는 순간 (합침 · 날기 시작 · 착지 · 터짐 · 기절 · 끌기 · 보호막)
- 키 9개: `Pres.Sfx.Join` · `SkillMove.Q` · `SkillLand.Q` · `SkillBurst.W` · `SkillStun.E` · `SkillPull.E` · `SkillShield.E` · `SkillCount.R` · `SkillLand.R`
- 늦춘 판정 착지 큐의 키를 **슬롯별로** — R 이면 `SkillLand.R` (재키 E 는 그대로 `SkillLand.E`)
- 조각 `소리` (`ERSkillFragment_Sfx`) — 선딜 시작 · 실행 순간 소리 키 (R 시작 · 카운트) · R 데이터 `bNoCastCue`
- Fill 명시 표 시셀라 20줄
