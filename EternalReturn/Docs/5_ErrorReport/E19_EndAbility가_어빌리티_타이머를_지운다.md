# E19 — `EndAbility` 가 어빌리티에 묶인 타이머를 전부 지운다 (투척 2차 판정 실종)

> 발견 **2026-09-20** · F11-05 C 검증 로그에서 (투척 D 재테스트 — 1차 적중은 있는데 `2차 판정` 로그가 두 번 다 없다)

## 증상

```
12:07:38.235 DA_Skill_D_Throw 선딜 0.15초 시작
12:07:38.389 쿨다운 25.00초 → GroundCircle 적중 1 → 피해 1명 (기본 80) → [CC] Slow 1.00초
(0.5초 뒤 기대: [스킬] … 2차 판정 (DA_Skill_D_Throw_2nd) · GroundCircle 적중 …)   ← 없음. 12:08:05 재시도도 같음
```

## 원인

`ExecuteAndRecover` 가 `SetTimer(Handle, this, &ExecuteFollowUp, 0.5)` 로 2차 판정을 예약한 직후 `EndAbility` 로 끝난다 (RecoveryTime 0).
`UGameplayAbility::EndAbility` 는 **`ClearAllTimersForObject(this)`** 를 부른다 (`GameplayAbility.cpp:707`, CVar `AbilitySystem.ClearAbilityTimers` 기본 1).
`this` 에 묶인 타이머라서 0.5초를 못 기다리고 지워졌다. 어빌리티 인스턴스(InstancedPerActor)가 살아 있는 것과는 별개다.

같은 파일의 다른 두 타이머(권총 지연 버프 · 방망이 벽 충돌 구독 해제)는 처음부터 `CreateLambda` 라 영향이 없었다 — 그래서 B 회차에서는 안 드러났다.

## 수정

- 2차 판정 타이머를 `FTimerDelegate::CreateLambda([WeakThis])` 로 — 오브젝트에 안 묶여 `ClearAllTimersForObject` 를 안 탄다. 실행 시 `WeakThis.Get()` 확인
- 의미상도 맞다: 이미 던진 연막이라 시전자가 기절해도 터진다 (어빌리티 취소와 무관)

## 규칙

**어빌리티 안에서 `EndAbility` 뒤에도 살아야 하는 타이머는 `this` 에 묶지 않는다** (`SetTimer(Handle, this, &Fn, …)` ✘ · `CreateWeakLambda(this, …)` ✘ · `CreateLambda` + `TWeakObjectPtr` ✔).
반대로 어빌리티와 같이 죽어야 하는 타이머(선딜 · 후딜)는 지금처럼 `this` 에 묶는 게 맞다 — 취소 시 자동 정리.

## 재발 (2026-10-02 · F19-01 카티야 R)

- 증상: `스캔 3명` 인데 **1발만** — 2 · 3발 줄 없음 (로그 18:42 · 18:43)
- 원인: 순차 사격 타이머를 `FTimerDelegate::CreateWeakLambda(this, …)` 로 — **WeakLambda 도 this 에 묶인다** (`ClearAllTimersForObject(this)` 대상). R 은 후딜 0 이라 1발째 직후 `EndAbility` → 2 · 3발 타이머 삭제
- 해결: `CreateLambda([WeakThis …])` + `WeakThis.Get()` (이 문서 해결과 같다) · `ERGameplayAbility.cpp` `FireSequentialShots` · 빌드 에러 0
- ⚠ 규칙 보강: **`CreateWeakLambda(this, …)` 도 ✘** — 이름에 Weak 가 있어도 객체에 묶인다

## 관련

- `Source/EternalReturn/GAS/ERGameplayAbility.cpp` `ExecuteAndRecover` · `ExecuteFollowUp`
- `Docs/3_EditorTasks/F11_무기.md` C ②
