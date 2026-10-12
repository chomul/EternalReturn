# E41 — 스킬 BP 가 부모의 차단 태그를 덮어 빈사 중 스킬이 나갔다

> 발견: 2026-10-08 F14-01 빈사 PIE (사용자 "빈사상태에서 스킬이 나감") · 관련 `Source/EternalReturn/GAS/ERGameplayAbility.cpp` 생성자 · `CanActivateAbility`

## 증상 (로그 2026-10-07 19:23~19:24 UTC)

```
19:23:53.717 [빈사] ERPlayerState_0 빈사 — 빈사로 만든 ERPlayerState_3 · 체력 400 …
19:24:05.239 [스킬] ERPlayerState_0 <- DA_Skill_Jackie_Q 쿨다운 12.00초 …          ← 빈사 중 Q 발동 (서버)
19:24:46.449 [빈사] ERPlayerState_0 소생 ← ERPlayerState_1 …
```

## 원인

- 빈사 · 사망 · 소생 차단은 C++ 생성자 `ActivationBlockedTags.AddTag(State_Downed …)` 로만 넣었다 (`ERGameplayAbility.cpp:80-82`)
- 스킬 어빌리티 클래스는 BP(`GA_Test` 등 — 스킬 DA 의 AbilityClass). F06 때 그 BP 의 `ActivationBlockedTags` 에 `State.Block.Skill` 을 넣어 **저장**했다 (`Docs/3_EditorTasks/F06_상태이상.md:612`)
- BP 가 저장한 컨테이너 칸은 **값 통째로** 직렬화된다 → 그 뒤 부모(C++) 생성자에 추가한 태그는 그 BP 에 내려오지 않는다
- 생성자 주석 "BP Class Defaults 는 여기에 **추가**된다" 가 틀렸다
- ⚠ 애셋(.uasset)은 열지 않아(CLAUDE.md §9) BP 칸 값을 직접 보지는 못했다 — 위 로그(태그가 붙은 상태에서 발동)와 F06 작업 기록으로 판단

## 해결

- `UERGameplayAbility::CanActivateAbility` 가 ASC 에 `State.Downed` · `State.Dead` · `State.Reviving` 이 있으면 **코드로** 거절 (애셋 값과 무관 · 클라 선판정도 같은 함수)
- 생성자 주석 고침 (덮어쓴다는 경고)
- 빌드 Editor · Server 에러 0 (2026-10-08)

## 재발 방지

- **꼭 막아야 하는 상태**는 차단 태그 칸이 아니라 `CanActivateAbility` 코드로 — 애셋이 칸을 저장했는지에 따라 조용히 빠진다
- ⚠ 같은 문제가 이미 있을 수 있다: 생성자에서 나중에 추가한 `State.Unarmed` (F11) · `State.Recovering` 이 그 BP 에 있는지 — `F11_무기.md:146` "Q 안 나감 (Unarmed)" 이 미체크로 남아 있다 → 무기 없이 Q 를 눌러 확인 필요

## 재검증

- [x] 빈사 중 Q · W · E · R · 평타 → 안 나간다 (로그에 `[스킬] … 쿨다운` 없음) — R 만 미측정 (Lv.0 · 같은 코드 검사)
  - 2026-10-12 PIE: **W · E 막힘 확인** — 빈사 18:23:16.877 ~ 소생 18:23:41.799 사이 W 2번 · E 2번 모두 `클라 선판정 실패 … 보유 태그 [State.Downed, …]` · 발동 로그 없음. 소생 뒤 Q · W · E · 평타 다시 나감 (18:23:45 ~ 56)
  - 2026-10-12 PIE 2차: **Q 막힘 확인** — 빈사 18:49:34.111 뒤 Q 확정 15번 · W 3번 · E 4번 모두 `클라 선판정 실패 … [State.Downed]` · 발동 로그 없음
  - **평타 막힘 확인** — 빈사 중 적 우클릭 → 공격 안 하고 기어가기만 함 (사용자 눈 확인 2026-10-12). 로그가 없는 이유: `ERPlayerController.cpp:656` 이 빈사 중 적 우클릭을 바닥 클릭(이동)으로 처리 — 대상 설정 로그 자체가 안 찍힌다
  - 미측정: R (Lv.0) — Q · W · E 와 같은 `CanActivateAbility` 검사
- [x] 무기 없이 Q → **나갔다** (2026-10-11 PIE · 같은 문제 확인):
  ```
  13:20:33.398 [스킬] ERPlayerState_0 <- DA_Skill_Jackie_Q 쿨다운 12.00초 …          ← 시작부터 무기 없음 (State.Unarmed)
  13:21:02.249 [무기] ERPlayerState_0 무기 없음 — 회수 2 · State.Unarmed
  13:21:08.532 [스킬] ERPlayerState_0 <- DA_Skill_Jackie_Q 쿨다운 … · 적중 1 · 리캐스트   ← 해제 뒤에도
  13:21:09.072 [입력] … 클라 선판정 실패 … 쿨다운 ✘ · 태그 OK · 보유 태그 [State.Unarmed, …]   ← Unarmed 를 들고도 태그 OK
  ```
  → `CanActivateAbility` 에 `State.Unarmed` 도 코드로 거절 추가. 단 **C++ 클래스 기본값이 Unarmed 를 막는 어빌리티만** (가장 가까운 네이티브 부모 CDO 의 차단 칸을 본다) — 패시브(`UERPassiveAbility` 는 칸을 비움)는 무기 없이 켜져야 해서. 빌드 Editor · Server
- [x] 수정 뒤 PIE: 무기 없이 Q · 해제 뒤 Q → `클라 선판정 실패` (발동 로그 없음) · 장착 중 Q 는 나간다 · 재키 패시브는 시작 때 켜진다 (2026-10-12 PIE 두 번):
  ```
  18:01:05.846 [패시브] ERPlayerState_0 <- DA_Skill_Jackie_P 켜짐                      ← 무기 없이 켜짐 (4명 모두)
  18:01:58.497 [입력] Ability.Slot.Q -> 클라 선판정 실패 … 보유 태그 [State.Unarmed]   ← 무기 없이 Q · 발동 로그 없음
  18:02:13.317 [스킬] ERPlayerState_0 <- DA_Skill_Jackie_Q 쿨다운 12.00초 …             ← 단검 장착 뒤 Q 나감
  18:02:17.264 [장비] ERPlayerState_0 -> dagger_t1 해제
  18:02:26.691 [입력] … 클라 선판정 실패 … 쿨다운 OK … 보유 태그 [State.Unarmed]       ← 쿨다운 끝난 뒤에도 안 나감
  ```
  선판정 줄의 `태그 OK` 는 엔진 태그 검사만 보여 준다 — 코드 검사는 안 보인다 (발동 로그 유무로 판단)
- ⚠ 남은 같은 부류: `State.Recovering` (생성자에 있음) — 측정 안 함. 해당 기능 테스트 때 같이 본다
