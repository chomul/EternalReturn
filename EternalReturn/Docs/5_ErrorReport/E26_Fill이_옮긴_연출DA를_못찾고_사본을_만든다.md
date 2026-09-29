# E26 — `ER.Pres.Fill` 이 옮긴 연출 DA 를 못 찾고 빈 사본을 만들어 캐릭터 DA 를 사본에 연결한다

> 발견: 2026-09-28 F12.5-02 PIE · 관련 `Presentation/ERPresentationFill.cpp` · 같은 계열 F12 `ER.Wild.ImportCSV` (17개 중복)

## 증상

- 무기 레이어가 안 붙음 → 새 Warning 으로 `AnimInstance 없음 (메시 Character_Source · AnimClass None)` — 스킨 S000 을 적용했는데 AnimClass 가 비었다
- 사용자는 `DA_Skin_Jackie_S000` 에 Anim Class 를 넣고 저장해 두었다 (사용자 확인 2026-09-28)
- 사용자가 연출 DA 폴더를 `/Game/ER/Presentation/Jackie` → `/Game/ERCharacter/CharData/Presentation/Jackie` 로 옮긴 뒤였다

## 원인

`FindOrCreate` 가 **정해진 경로**(`/Game/ER/Presentation/<Char>/<Name>`) 로만 찾았다.
- 2026-09-27 16:07 `ER.Pres.Fill Jackie` → `새 애셋 11` — 옮긴 원본을 못 찾고 원래 경로에 **빈 사본** 11개 (스킨 DA 사본은 AnimClass 가 비어 있다)
- 16:24 `ER.Pres.Fill Jackie Force` → `DA_Char_Jackie` 의 Presentation · Skins 를 **사본으로 재연결**
- → 게임은 AnimClass 가 빈 사본 스킨을 썼다. 사용자가 고친 원본은 참조되지 않았다
- 현재 디스크에는 원본 위치만 남아 있다 (사본은 저장되지 않았다 — `find Content -name "DA_Skin_Jackie_*"` 2026-09-28)

## 고침 (빌드 에러 0)

- `FindOrCreate` — 애셋 레지스트리에서 **이름으로** 찾는다 (폴더 무관). 둘 이상이면 Warning 후 첫 번째
- 새로 만들 무기 세트 · 스킨 DA 는 **기본 DA(`DA_Pres_<Char>`) 가 있는 폴더**에 — 옮긴 폴더를 따른다
- 임시로 사용자가 `BP_Jackie` 메시에 Anim Class 를 넣어 동작 확인 (2026-09-28 01:43 레이어 연결 로그)

## 재검증 (2026-09-28 02:11) ✅

- `ER.Pres.Fill Jackie` → `새 애셋 0 · 유지 18` (옮긴 위치의 DA 를 찾았다)
- 사용자가 `BP_Jackie` 메시 Anim Class 를 비움 → 스폰 시 `스킨 DA_Skin_Jackie_S000` 직후 `무기 레이어 None → ABPL_Jackie_Unarmed_C` (서버 · 클라 · 두 폰) — **스킨 DA 의 AnimClass 경로**로 동작

## 교훈

**애셋을 만드는 명령은 이름으로 찾는다 — 경로로 찾지 않는다.** 두 번째 같은 사고다 (F12 ImportCSV). 사용자는 애셋을 옮긴다.

## 검증

```
ER.Pres.Fill Jackie        → "새 애셋 0" · DA_Char_Jackie → Presentation DA_Pres_Jackie (옮긴 위치)
DA_Char_Jackie 열기        → Presentation · Skins 가 ERCharacter/CharData/Presentation/Jackie 쪽을 가리킨다
BP_Jackie 메시 Anim Class 비우고 PIE → 스킨 DA 의 AnimClass 로 "무기 레이어 None → ABPL_Jackie_Unarmed_C"
```
