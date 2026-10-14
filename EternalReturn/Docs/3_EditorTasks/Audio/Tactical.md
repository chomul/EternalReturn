# 전술 스킬 (F) — 소리 배치표

> 규칙 [00_규칙](00_규칙.md) · 방식 [Argument 82](../../4_Argument/82_전술스킬_소리.md) (추천 A — 소리 조각 애셋 칸) · 원본 `Data/Skills/Tactical.json` · 파일 `/Game/ER/Audio/SFX/Skill/TacticalSkill_*` (31개 · `ER_Asset/03_Log/SFX/전술_스킬.md`)
> 캐릭터와 무관 — 어느 실험체가 써도 같은 소리. 스킨 없음.

**`어디서` 칸 뜻 (이 표 전용)**
- `실행` = F 가 나가는 순간 · 시전자 자리 · 모든 클라
- `적중` = 맞은 대상마다 그 자리 (판정 뒤)
- `Lv2 실행` = Lv2 일 때만 실행 순간에 하나 더
- `반복` = 버프 · 장판이 있는 동안 계속 — 이번엔 ⏸ (액터 갈래)
- 무기 공통 시전음 · 타격음(`skillOneHandSword` 등)은 F 에선 **끈다**

⚠ = 파일 이름으로만 고른 제안 — **들어보고 `어디서` 칸을 고쳐 달라**

| # | 스킬 | 소리 | 어디서 (제안) | 이유 | 상태 |
|---|---|---|---|---|---|
| 1 | 블링크 | `TacticalSkill_Blink` | 실행 | 이름 그대로 | ✅ PIE 2026-10-14 |
| 2 | 블링크 | `TacticalSkill_Blink_Swift` | Lv2 실행 | Lv2 이속 버프 (swift) ⚠ | ✅ 코드 · JSON (빌드 2026-10-14) |
| 3 | 퀘이크 | `TacticalSkill_Quake` | 실행 | 이름 그대로 | ✅ PIE 2026-10-14 |
| 4 | 퀘이크 | `TacticalSkill_Quake_Cyclone` | Lv2 실행 (장판 시작 한 번) | Lv2 6초 회오리 장판 ⚠ — 반복이면 ⏸ | ✅ PIE 2026-10-14 |
| 5 | 퀘이크 | `TacticalSkill_Quake_CycloneHit` | 적중 — 장판 펄스 (`Quake_Pulse`) | 회오리 적중 | ✅ PIE 2026-10-14 |
| 6 | 진실의 칼날 | `TacticalSkill_TruthBladeSlash` | 실행 (두 번째 칼날은 적중음만 — 2차 판정은 실행 훅이 없다) | 이름 그대로 | ✅ PIE 2026-10-14 |
| 7 | 진실의 칼날 | `TacticalSkill_TruthBladeHit` | 적중 (두 번째 칼날도) | 이름 그대로 | ✅ PIE 2026-10-14 |
| 8 | 붉은 폭풍 | `TacticalSkill_RedStorm_Move` | 실행 | 2.5m 이동 | ✅ PIE 2026-10-14 |
| 9 | 붉은 폭풍 | `TacticalSkill_RedStorm_Buff` | 실행 (Move 와 같이) | 사거리 버프 — 이동이 0.1초라 같은 순간 ⚠ | ✅ PIE 2026-10-14 |
| 10 | 초월 | `TacticalSkill_TranscendenceOn` | 실행 | 보호막 켜짐 | ✅ PIE 2026-10-14 |
| 11 | 아티팩트 | `TacticalSkill_Respite` | 실행 | **아티팩트 이름 파일이 없다.** Respite(잠시 멈춤) + `_end` 가 2.5초 경직과 맞아 보임 ⚠ | ✅ PIE 2026-10-14 |
| 12 | 아티팩트 | `TacticalSkill_Respite_end` | 경직 끝 (2.5초 뒤) | ⚠ — 끝 순간 훅이 없어 ⏸ | ⏸ |
| 13 | 플라즈마 대시 | `TacticalSkill_Repulsor_Spray_fire` | 실행 | **플라즈마 이름 파일이 없다.** Spray(부채꼴로 뿌림) fire · hit · end 가 5발 부채꼴과 맞아 보임 ⚠ | ✅ PIE 2026-10-14 |
| 14 | 플라즈마 대시 | `TacticalSkill_Repulsor_Spray_hit` | 적중 | ⚠ | ✅ 코드 · JSON (빌드 2026-10-14) |
| 15 | 플라즈마 대시 | `TacticalSkill_Repulsor_Spray_end` | 안 씀 | 뜻 모름 ⚠ | — |
| 16 | 무효화 | `TacticalSkill_Nullification` | 실행 | 무효화 = Nullification | ✅ PIE 2026-10-14 |
| 17 | 무효화 | `TacticalSkill_Nullification_CcImmunity` | Lv2 실행 | Lv2 CC 저항 ⚠ | ✅ 코드 · JSON (빌드 2026-10-14) |
| 18 | 무효화 | `TacticalSkill_Neutralize_Start` | 안 씀 | 같은 뜻 다른 파일 — 옛 버전? ⚠ 들어보고 16 대신 쓸지 | — |
| 19 | 라이트 윙 | `TacticalSkill_LightWing_Start` | 실행 | 이름 그대로 | ✅ PIE 2026-10-14 |
| 20 | 라이트 윙 | `TacticalSkill_LightWing_Ing` | 반복 (7초 버프 동안) | 진행 중 | ⏸ |
| 21 | 리펄서 미사일 | `TacticalSkill_Repulsor_Missile_start` | 실행 | 3m 이동 시작 | ✅ PIE 2026-10-14 |
| 22 | 리펄서 미사일 | `TacticalSkill_Repulsor_Missile_fire` | 미사일 발마다 | ⚠ — 조각은 발 순간을 모른다 → 우선 ⏸ (필요하면 순차 사격 큐에 애셋 싣기) | ⏸ |
| 23 | 리펄서 미사일 | `TacticalSkill_Repulsor_Missile_hit` | 적중 (발마다) | 이름 그대로 | ✅ PIE 2026-10-14 |
| 24 | 스트라이더 | `TacticalSkill_TheChase_Start` | 실행 | **스트라이더 이름 파일이 없다.** The Chase(추격) 가 "적 쪽으로 이동 시 이속" 과 맞음 ⚠ | ✅ PIE 2026-10-14 |
| 25 | 스트라이더 | `TacticalSkill_TheChase_On` | 적 쪽 이속이 붙을 때 | ⚠ — 0.2초마다 켜졌다 꺼져서 자주 남 → ⏸ | ⏸ |
| 26 | 스트라이더 | `TacticalSkill_TheChase_Attack` | 적중 — 추가 피해 (`Strider_Hit`) | 추격 공격 | ✅ 코드 · JSON (빌드 2026-10-14) |
| — | 우리에 없는 전술 스킬 | `BlasterRounds_Active · _Impact` · `FalseOath` · `HealingWind` · `StrongBond` | 안 씀 | 그 스킬이 없다 (Argument 80 S2 11종 밖) | — |

## 구현 (A · 사용자 2026-10-14 · 빌드 Editor · Server ✅)
- 소리 조각 `UERSkillFragment_Sfx` 에 `ExecuteSound` · `HitSound` (애셋) → `SendSoundAssetCue` · Lv2 줄은 `MinLevel 2` 조각 하나 더
- 시전 · 타격 큐가 `Ability.Slot.F` 면 무기 공통 소리 건너뜀 (`ERPresentationComponent::HandlePresCue`) — 애니는 그대로
- 확인 로그: `[연출] … 소리 TacticalSkill_Quake ← … [물체 행]` · F 에서 `skillOneHandSword` 줄이 안 나와야 함
