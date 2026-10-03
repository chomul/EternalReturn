# 카티야 — 소리 배치표

> 경로 `/Game/ER/Audio/SFX/Character_FX/katja/s000/` (S000 기본 스킨) · 규칙 [00_규칙](00_규칙.md)
> **`어디서` 칸을 고치면 그대로 적용한다.** 다른 스킨은 `s00x/` 에 같은 파일명이면 자동으로 덮는다.

| # | 소리 | 어디서 | 이유 | 상태 |
|---|---|---|---|---|
| 1 | `Katja_SniperRifle_Normal_Hit` | 코드 (평타 타격 큐) | 맞았는지는 서버만 앎 | ✅ 기존 규칙 연결됨 |
| 2 | `Katja_SniperRifle_Reinforce_Hit` | 코드 (평타 타격 큐 · 강화 시) | 강화 평타가 맞았을 때만 — 평소 `Normal_Hit` **대신** | ✅ K8 (`Pres.Sfx.HitEnhanced`) |
| 3 | `Katja_SniperRifle_Reinforce_Ready` | 코드 (강화 걸 때 큐) | Q · E 뒤 강화가 걸린 순간 — 동작 없는 게임 상태 | ✅ K8 (`GameplayCue.Pres.Ready` → `Pres.Sfx.EnhanceReady`) |
| 4 | `Katja_SniperRifle_Reinforce_Shot` | 코드 (평타 공격 큐 · 강화 시) | 이번 평타가 강화를 소비할 때만 평소 `Shot` **대신** | ✅ K8 (`Pres.Sfx.AttackEnhanced`) |
| 5 | `Katja_SniperRifle_Shot` | 코드 (평타 공격 큐) | 평타 발사음 — 판정 순간 = 쏘는 순간 | ✅ K8 Fill 규칙 (`Pres.Sfx.Attack`) |
| 6 | `Katja_SniperRifle_Skill01_Hit` | 코드 (Q 타격 큐) | 적중 결과 | ✅ K8 |
| 7 | `Katja_SniperRifle_Skill01_Shot` | 코드 (Q 공격 큐) | 판정 순간 = 발사 | ✅ K8 |
| 8 | `Katja_SniperRifle_Skill02_Detected` | 코드 (W 드러냄 결과) | 적이 드러났을 때만 | ⏸ W 보류 (F16) |
| 9 | `Katja_SniperRifle_Skill02_Explode` | 코드 (W 다트 도착 판정) | 다트 도착 = 파동 순간 | ⏸ W 보류 |
| 10 | `Katja_SniperRifle_Skill02_ProjectileSound` | 액터 (W 다트) | 다트가 날아가는 동안 | ⏸ W 보류 |
| 11 | `Katja_SniperRifle_Skill02_Shot` | 코드 또는 노티파이 (W 발사) | 다트 발사 순간 | ⏸ W 보류 |
| 12 | `Katja_SniperRifle_Skill03_Hit` | 코드 (E 타격 큐) | 적중 결과 | ✅ K8 |
| 13 | `Katja_SniperRifle_Skill03_Shot` | 코드 (E 공격 큐) | 판정 순간 = 발사 | ✅ K8 |
| 14 | `Katja_SniperRifle_Skill04_Aiming` | 코드 (조준 큐 · 1발) | 스캔 직후 Loop 시작과 함께 (사용자 2026-10-02 "Loop 시작할 때 Aiming") — 노티파이 아님 | ✅ K8 (`Pres.Sfx.SkillAim.R` 1번째) |
| 15 | `Katja_SniperRifle_Skill04_Aiming_02` | 코드 (조준 큐 · 2발) | 1발 Fire 끝 → Loop 조준 순간 (서버가 다음 발이 있을 때만 보냄) | ✅ K8 (`GameplayCue.Pres.Aim` → `Pres.Sfx.SkillAim.R` 2번째) |
| 16 | `Katja_SniperRifle_Skill04_Aiming_03` | 코드 (조준 큐 · 3발) | 2발 Fire 끝 → Loop 조준 순간 | ✅ K8 (3번째) |
| 17 | `Katja_SniperRifle_Skill04_Drone_Scan` | 노티파이 (`Katja_Snipe_Skill04_Start` 시작 프레임) | R 시전 시 범위 스캔 소리 (사용자 2026-10-02) — 채널 동안 드론이 훑는 동작 | 📝 노티파이 찍기 |
| 18 | `Katja_SniperRifle_Skill04_Hit` | 코드 (R 타격 큐 · 1발) | 적중 결과 · 발 번호 | ✅ K8 |
| 19 | `Katja_SniperRifle_Skill04_Hit_02` | 코드 (R 타격 큐 · 2발) | 같음 | ✅ K8 |
| 20 | `Katja_SniperRifle_Skill04_Hit_03` | 코드 (R 타격 큐 · 3발) | 같음 | ✅ K8 |
| 21 | `Katja_SniperRifle_Skill04_Shot` | 코드 (R 공격 큐 · 1발) | 판정 순간 · 발 번호 | ✅ K8 |
| 22 | `Katja_SniperRifle_Skill04_Shot_02` | 코드 (R 공격 큐 · 2발) | 같음 | ✅ K8 |
| 23 | `Katja_SniperRifle_Skill04_Shot_03` | 코드 (R 공격 큐 · 3발) | 같음 | ✅ K8 |
| 24 | `Katja_SniperRifle_Skill04_Target_Fire` | 보류 | 뜻 (미확인) (사용자 2026-10-02 "모르겠다") | ⏸ |
| 25 | `Katja_SniperRifle_Skill04_Target_Scan` | 보류 | 뜻 (미확인) (사용자 2026-10-02 "모르겠다") | ⏸ |

- ✅ K8 = 코드에 이미 연결 (에디터에서 `ER.Pres.Fill Katja Force` 만) · 🆕 = 코드 추가 필요 · ⚠ = 들어보고 결정 · 📝 = 노티파이 찍기 (사용자) · ⏸ = 보류

## R 흐름 (사용자 2026-10-02 "Loop 시작할 때 Aiming · 소리 끝나면 Fire 잠시 · 다시 Loop + Aiming_02")
```
Start ─ Loop(채널 0.8) ─┬ Loop+Aiming (1.2) ─ Execute(Fire 0.5) ─ Loop+Aiming_02 (1.2) ─ Execute ─ Loop+Aiming_03 (1.2) ─ Execute ─ End
  Drone_Scan(노티파이) 스캔
```
- 발마다 같은 순서: **조준 (Loop + `Aiming_0N`) `AimTime` **1.2초** (PIE 확인 2026-10-02 "원하는대로 나간다") → 사격 (Execute) `Interval` 0.5초** — `Katja.json` R
- 조준음 3개 모두 코드 (발 번호) · Start 시퀀스의 `Aiming` 노티파이는 **지운다** (두 번 난다)
- 몽타주 하나 `AM_Katja_Snipe_Skill04` (섹션 Start · Loop · Execute · End) — 서버가 섹션을 넘기고 복제 · 본인 화면은 큐로
