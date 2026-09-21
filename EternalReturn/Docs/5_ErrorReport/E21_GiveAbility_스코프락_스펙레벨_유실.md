# E21 — 어빌리티 활성 중에 `GiveAbility` 하면 스펙이 목록에 아직 없다 (모드 평타 레벨 0)

> 발견 **2026-09-21** · F11-05 D 검증 로그 — 애셋의 CastTime 을 0 으로 지우자 되던 사격이 안 나갔다

## 증상

```
18:34:47 [스킬] … DA_Skill_D_Sniper_Stop (Ability.Slot.Attack) Lv.0/3
18:34:47 [모드] … 평타 교체 -> DA_Skill_D_Sniper_Stop Lv.1            ← 로그는 1 이라고 하는데
18:34:49 [입력] Ability.Slot.Attack -> 클라 선판정 실패 — DA_Skill_D_Sniper_Stop Lv.0 · 쿨다운 OK · 코스트 OK · 태그 OK   ← 실제 스펙은 0
```
18:15~18:22 세션(`DA_Skill_D_Sniper` 에 CastTime 0.15 가 남아 있던 때)에는 같은 코드로 3발이 다 나갔다.

## 원인

`AERPlayerState::SetModeAttack` 이 `GrantSkills`(GiveAbility) **뒤에** `FindAbilitySpecFromHandle(H)->Level = …` 로 레벨을 고쳤다.
`UAbilitySystemComponent::GiveAbility` 는 **어빌리티 목록이 잠겨 있으면**(`AbilityScopeLockCount > 0`) 스펙을 `AbilityPendingAdds` 에만 넣고 핸들을 돌려준다
(`AbilitySystemComponent_Abilities.cpp:284-289`). 그동안 `FindAbilitySpecFromHandle` 은 **null** → `if (Spec)` 이 조용히 건너뛰어 레벨이 InitialLevel(0) 으로 남는다.

- CastTime 0.15 일 때: EnterMode 가 `WaitDelay` 콜백(타이머)에서 불려 락 밖 → 됐다
- CastTime 0 일 때: `TriggerAbilityFromGameplayEvent → ActivateAbility → ExecuteAndRecover → EnterMode` 가 **한 호출 스택** 안 (`ABILITYLIST_SCOPE_LOCK` 중) → 안 됐다

로그의 `Lv.1` 은 계산값을 찍은 것이라 실제 스펙과 달랐다 — 로그가 사실을 보증하지 않았다.

## 수정

- `ERSkill::GrantSkills(…, int32 LevelOverride = -1)` — 레벨은 **스펙을 만들 때** 정한다 (`FGameplayAbilitySpec(Class, Level)`). 지연 추가돼도 그 값 그대로 들어간다
- `SetModeAttack` 은 부여 뒤 스펙을 만지지 않는다

## 규칙

**어빌리티 안에서 `GiveAbility`/`ClearAbility` 를 부르면 그 결과는 지연될 수 있다** — 돌려받은 핸들로 곧바로 `FindAbilitySpecFromHandle` 하지 않는다. 스펙에 넣을 값은 만들 때 넣는다.
`ClearAbility` 도 같은 이유로 지연되지만 (`AbilityPendingRemoves`) 우리는 결과를 안 읽어서 문제 없었다.

## 관련

- `Source/EternalReturn/GAS/ERSkillData.cpp` `GrantSkills` · `Core/ERPlayerState.cpp` `SetModeAttack`
- E12 (클라 선판정 CDO) · E19 (EndAbility 가 타이머를 지움) — 같은 계열: "GAS 가 조용히 다르게 동작하는" 경우
