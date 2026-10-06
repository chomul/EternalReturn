# E40 — 어빌리티에 묶은 타이머가 EndAbility 에 지워져 시셀라 W 가 안 터졌다

> 발견: 2026-10-06 F19-04 시셀라 2단계 PIE · 관련 `Source/EternalReturn/GAS/Fragment/ERSkillFragment_Bubble.cpp`

## 증상 (로그 2026-10-06 07:2x)

```
[스킬] BP_Sissela_C_0 <- DA_Skill_Sissela_W 감싸기 1.5초 — 피해 면역 · 이속 ×1.15 (다시 누르면 터짐)
[스킬] ERPlayerState_0 <- DA_Skill_Sissela_W 리캐스트 윈도우 1.5초 (Recast.Slot.W)
```
- 1.5초 뒤 `감싸기 터짐` 줄이 **없다** — 주변 피해(W_Burst)가 한 번도 안 나갔다

## 원인

- 터짐 타이머를 `FTimerDelegate::CreateWeakLambda(A, …)` — **어빌리티 객체에 묶어** 걸었다
- 엔진 `UGameplayAbility::EndAbility` 는 그 어빌리티 객체의 타이머를 전부 지운다 —
  `MyWorld->GetTimerManager().ClearAllTimersForObject(this)` (`GameplayAbility.cpp:707` · CVar `AbilitySystem.ClearAbilityTimers` 기본 1)
- W 는 시전하자마자 끝나는 어빌리티라 1.5초 전에 EndAbility → 타이머가 지워졌다
- ⚠ 처음엔 "조각 상태가 비워진다" 고 짐작했는데 **틀렸다** — `FragmentStates` 는 EndAbility 에서 안 비운다 (grep 으로 확인)

## 해결

- 타이머를 객체에 묶지 않은 람다로 (`CreateLambda` + `TWeakObjectPtr` 로 어빌리티를 잡음) — 2차 판정 조각(`ERSkillFragment_FollowUp`)과 같은 방식
- 같은 김에 사용자 규칙 반영: W 는 **다시 눌러 터뜨리지 않는다** — 리캐스트 조각 · 조각 상태 제거
- 빌드 에러 0 (Editor · Server · 2026-10-06)

## 재발 방지

- 어빌리티가 끝난 **뒤** 에 일어나야 하는 일(지연 터짐 · 2차 판정)은 어빌리티 객체에 묶은 타이머로 걸지 않는다
- 다른 곳 확인: `ERSkillFragment_FollowUp` (CreateLambda ✅) · `AERWilson` 끌기 타이머 (윌슨 액터에 묶음 ✅) · `ERShield` 만료 (CreateLambda ✅)

## 재검증

- [x] W → 1.5초 뒤 `감싸기 터짐 → DA_Skill_Sissela_W_Burst` (로그 2026-10-06 07:49:32.8 → 34.3)
