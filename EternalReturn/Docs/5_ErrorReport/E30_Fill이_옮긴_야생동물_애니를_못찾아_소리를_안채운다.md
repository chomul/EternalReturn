# E30 — `ER.Pres.Fill Wild` 가 옮긴 야생동물 애니 폴더를 못 찾아 소리 줄을 하나도 안 채운다

> 발견: 2026-09-29 F12.5-05 PIE — "소리가 야생동물 소리가 안나오는거 같음" (사용자) · 관련 `Presentation/ERPresentationFill.cpp` FillWild · 같은 부류 [E26](E26_Fill이_옮긴_연출DA를_못찾고_사본을_만든다.md)

## 증상

- PIE 로그: 야생동물 공격 · 타격 전부 `소리 없음 [줄 없음] ← BP_ERWildlife … Pres.Sfx.Attack / Hit` (서버 · 클라 각 35회)
- `ER.Pres.Fill Wild` 로그: `DA_Wild_Wolf (Wolf_01 에 atk · death 애니 없음)` … **전 종** · `줄 채움 0 · 유지 0 · 규칙 밖 22`

## 원인

- FillWild 가 종 애니를 **정해진 경로** `/Game/ER/Monsters/<종 폴더>/Animations` 로만 찾았다 (`ERPresentationFill.cpp` 수정 전 592행)
- 사용자가 몬스터 폴더를 `/Game/ER/Wildlife/` 로 옮겼다 — 지금 `Content/ER/Wildlife/Wolf_01/Animations/Wolf_01_atk01.uasset` (`find Content` 2026-09-29)
- 애니가 0개면 `continue` 로 그 종을 **통째로 건너뛰는** 구조라, 새로 붙인 소리 줄(종 표)까지 안 들어갔다

## 고침 (빌드 에러 0 · Editor · Server)

- 종 애니: 전체 애니 중 **패키지 경로가 `/<종 폴더>/Animations` 로 끝나는 것** — 폴더 위치 무관
- 애니가 없어도 **소리는 채운다** — 애니 · 소리 둘 다 없을 때만 건너뛴다 (규칙 밖 문구 "소리만 채운다")

## 재검증

- ⏸ `ER.Pres.Fill Wild` → `줄 채움 > 0` · PIE `소리 wolfAttack` · `사망음 wolfDie`

## 재발 방지

- E26 교훈과 같다: **에디터 애셋은 옮겨진다 — Fill 은 경로가 아니라 이름 · 폴더 이름 끝으로 찾는다**
- ⚠ 아직 경로를 박은 곳 (`ERPresentationFill.cpp`): 실험체 애니 `/Game/ER/Characters/<Char>` · 소리 `/Game/ER/Audio/SFX/…`. 옮기면 같은 일이 난다 — 그때는 Fill 로그가 `애니 없음` · `줄 채움 0` 으로 알려 준다. 옮길 일이 생기면 같은 방식으로 바꾼다 (지금 바꾸지 않는다 — 옮긴 적 없음)
