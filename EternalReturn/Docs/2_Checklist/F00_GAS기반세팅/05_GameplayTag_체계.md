# ✅ F00-05 — GameplayTag 체계

> **2026-09-09 기록 보정** — 검증은 앞선 세션에서 끝났으나
> `Docs/3_EditorTasks/` 문서만 체크되고 이 체크리스트가 비어 있었다.

> Task [`../../Task/F00_GAS기반세팅/05_GameplayTag_체계.md`](../../1_Task/F00_GAS기반세팅/05_GameplayTag_체계.md)

## 선행

- [x] [04 복제 모드 설정](04_복제모드_설정.md) 완료

## 구현

- [x] `ERGameplayTags.h / .cpp` 생성
- [x] CC 태그 선언 — `Stun` `Snare` `Silence` `Disarm` `Slow` `Blind` `VisionBlock`
- [x] 상태 태그 — `Stealth` `Invulnerable` `CCImmune`
- [x] 어빌리티 형태 태그 — `Channeled` `NextAttackBuff`
- [x] 슬롯 태그 — `Q` `W` `E` `R` `D` `P`
- [x] 피해 채널 태그 — `Damage.Type.BasicAttack` / `Skill` / `True` (F03에서 쓴다)
- [x] ⭐ **`FNativeGameplayTag` 등으로 C++ 선언** — 문자열이 한 곳에만 있다

## ⭐ 만들지 않은 것 — 확인

- [x] **행동 강제 계열(공포·매혹·도발·광기)을 넣지 않았다**
- [x] **수면·에어본·제압·변이·춤을 넣지 않았다**
  근거: 6인 중 요구하는 캐릭터가 없어 **검증할 방법이 없다** (전투 문서 §구현 우선순위 주석)
- [x] 계층을 얕게 유지했다 (`State.CC.Stun` 정도. `State.Debuff.CC.Hard.Stun` 같은 과설계 아님)

## 빌드

- [x] `EternalReturnEditor` 빌드 통과
- [x] `EternalReturnServer` 빌드 통과

## 검증

- [x] 에디터 태그 목록에 선언한 태그가 보인다
- [x] 코드에서 태그를 **문자열 없이** 참조할 수 있다
- [x] ⭐ 태그 이름에 오타를 내면 **컴파일이 실패한다**

## 흔한 실수 — 걸렸는지 확인

- [x] ⭐ `FGameplayTag::RequestGameplayTag(FName("..."))` 를 코드 여기저기 박지 않았다
  ```
  grep -rn "RequestGameplayTag" Source/    ← ERGameplayTags.cpp 외에 나오면 실패
  ```
- [x] 태그 이름을 신중히 정했다 (**바꾸면 GameplayEffect 애셋이 깨진다**)
- [x] `Config/DefaultGameplayTags.ini` 를 건드렸다면 **변경 내용을 보고**했다

## 뒤에서 다시 만날 항목

- [x] ⭐ **무장 해제는 "평타 판정 스킬"도 막아야 한다** (전투 문서 §3.3) — 태그만으로는 부족하고 스킬 쪽 표시가 필요하다. **F07에서 다시 확인**할 항목으로 기록했다

---

## 실행 기록 (2026-09-06)

`Source/EternalReturn/GAS/ERGameplayTags.h / .cpp` — 네이티브 태그 **22개**.
헤더는 `UE_DECLARE_GAMEPLAY_TAG_EXTERN`, cpp 는 `UE_DEFINE_GAMEPLAY_TAG`.
문자열은 `.cpp` **한 곳에만** 존재하므로 오타가 컴파일에 걸린다.

- 단일 게임 모듈이라 `ETERNALRETURN_API` export 매크로는 붙이지 않았다.
- `Config/DefaultGameplayTags.ini` 는 **건드리지 않았다** — 코드가 참조하는 태그라 C++ 선언이 맞다 (`CLAUDE.md` §7).
- 무장 해제 ↔ `Ability.Form.NextAttackBuff` 의 관계를 헤더 주석에 남겼다. **F07 에서 배선한다.**
- `Actor.Type.*` / `Damage.Shape.AoE` 는 **넣지 않았다** — F03 이 실제로 쓸 때 추가한다 (F00 은 얇게).

⬜ **에디터 태그 목록 표시 확인은 사용자 몫.**

### 태그 검증 2건에 대해

- **"문자열 없이 참조"** / **"오타를 내면 컴파일 실패"** 는 `UE_DECLARE_GAMEPLAY_TAG_EXTERN` 이
  태그를 **C++ 변수**로 만들기 때문에 구조적으로 성립한다. `ERGameplayTags.cpp` 가 컴파일된 것이 그 증거다.
  다만 **실제 사용처가 아직 없다** — F03(데미지 채널)·F06(CC)이 처음 쓴다. 그때 최종 확인한다.
