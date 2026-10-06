# 레니 — 소리 배치표 (2026-10-07 사용자 결정 · 코드 빌드 ✅)

> 경로 `/Game/ER/Audio/SFX/Character_FX/leni/s000/` (S000 기본 스킨 · 22개) · 규칙 [00_규칙](00_규칙.md)
> **`어디서` 칸을 고치면 그대로 적용한다.** 다른 스킨은 `s00x/` 에 같은 파일명이면 자동으로 덮는다 (s001 · s002 · s003).
> 2026-10-07 사용자 결정 전부 반영 · 코드 빌드 ✅. 남은 일: `ER.Skill.ImportJson Leni` · `ER.Skill.ImportJson Common` · `ER.Pres.Fill Leni Force` · PIE.

## 지금 상태

- 레니는 **평타 소리도 캐릭터 전용**이 있다 (`Leni_Pistol_NormalAttack` · `_Hit`) — 무기 공용 권총 소리 **대신** 쓰는 것으로 제안
- 판정 큐(공격 · 타격)로 못 잡는 순간 (곰돌이 · 폭발 · 회복 · 벽 충돌 · 터짐)은 시셀라 때 만든 **소리 큐** (`GameplayCue.Pres.Sfx`)
- 적 · 아군이 갈리는 스킬 — 타격음은 **적에게만** (아군은 회복 · 보호막 소리)

## 표

| # | 소리 | 어디서 | 이유 | 상태 |
|---|---|---|---|---|
| 1 | `Leni_Pistol_NormalAttack` | 코드 (평타 공격 큐 · `Attack` — 권총 공용 대신) | 레니 전용 총성 | ✅ 명시 표 |
| 2 | `Leni_Pistol_NormalAttack_Hit` | 코드 (평타 타격 큐 · `Hit`) | | ✅ 명시 표 |
| 3 | `Leni_Pistol_Passive_BearAppear` | 코드 (곰돌이가 **붙을 때** · 아군 자리 · `BearAppear`) | | ✅ 코드 |
| 4 | `Leni_Pistol_Passive_BearShot` | 코드 (곰돌이가 **날아갈 때** · 아군 자리 · `BearShot`) | | ✅ 코드 |
| 5 | `Leni_Pistol_Passive_Hit` | 코드 (곰돌이 **적중** · 적 자리 · `SkillHit.P`) | | ✅ 코드 |
| 6 | `Leni_Pistol_Reload` | 코드 (D 가 **끝날 때** — `Mode.MovingReload` 해제 · `ModeEnd`) | (사용자 2026-10-07) | ✅ 코드 |
| 7 | `Leni_Pistol_Skill01_Shot` | 코드 (Q 시전 큐 · `SkillCast.Q`) | 당근 발사 | ✅ 명시 표 |
| 8 | `Leni_Pistol_Skill01_Boom` | 코드 (Q **폭발** · 맞힌 사람 없어도 · `SkillLand.Q`) | | ✅ 코드 |
| 9 | `Leni_Pistol_Skill01_Hit` | 코드 (Q **적** 적중만 · `SkillHit.Q`) | | ✅ 코드 (적만 거름) |
| 10 | `Leni_Pistol_Skill01_Recovery` | 코드 (Q **아군 회복** · 아군마다 · `SkillAlly.Q`) | | ✅ 코드 |
| 11 | `Leni_Pistol_Skill02_Jump` | 코드 (W 시전 · 뛰어오를 때 · `SkillCast.W`) | | ✅ 명시 표 |
| 12 | `Leni_Pistol_Skill02_Hit` | 코드 (W **적** 적중만 · `SkillHit.W`) | (사용자 2026-10-07) | ✅ 코드 (적만 거름) |
| 13 | `Leni_Pistol_Skill03_Shot` | 코드 (E 시전 큐 · `SkillCast.E`) | 에어 호른 · ⚠ 타격음(적 · 아군)과 같은 순간이라 묻힌다 — 단독은 들림 → **소리 크기 조절 때** (사용자 2026-10-07 · F17) | ✅ 명시 표 · PIE |
| 14 | `Leni_Pistol_Skill03_Hit` | 코드 (E 적중 — **적 · 아군 둘 다** · `SkillHit.E`) | (사용자 2026-10-07) | ✅ 코드 (`bAllyHitCue`) |
| 15 | `Leni_Pistol_Skill04_Whistle` | 코드 (R **떼는 순간** — 설치 · `SkillCast.R`) | (사용자 2026-10-07) | ✅ 명시 표 |
| 16 | `Leni_Pistol_Skill04_Spring` | 코드 (R **터질 때** · 판정 순간 · `SkillLand.R`) | 스프링 | ✅ 명시 표 |
| 17 | `Leni_Pistol_Skill04_Hit` | 코드 (R 적중 · `SkillHit.R`) | | ✅ 명시 표 |
| 18 | `Leni_Pistol_Skill04_WallHit` | 코드 (R **벽 충돌** · 대상 자리 · `SkillWall.R`) | | ✅ 코드 |
| 19 | `Leni_AirdropOpen_1` | 보류 | 보급 상자 | ⏸ |
| 20 | `Leni_AirdropOpen_2` | 보류 | 같음 | ⏸ |
| 21 | `Leni_selected_animation` | 보류 | 로비 캐릭터 선택 | ⏸ |
| 22 | `Leni_VictoryCutscene_sfx` | 보류 | 승리 연출 | ⏸ |

- 🆕 = 코드 · Fill 작업 필요 · ⚠ = 들어보고 결정 · 📝 = 노티파이 찍기 (사용자) · ⏸ = 보류
- 음성(`Audio/Voice/Leni`)은 [00_규칙](00_규칙.md) 음성 절 — 나중에
