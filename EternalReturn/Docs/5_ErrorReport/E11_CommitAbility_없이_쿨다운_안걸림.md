# E11 — `CommitAbility` 를 안 부르면 쿨다운이 영원히 안 걸린다

> 발견 **2026-09-13** · F07-02 · ⭐ **사용자 보고**

## 증상

사용자 보고: *"DA_Skill_Test_Q 에 쿨타임에 5 넣었는데 연속해서 눌려 쿨타임이 적용이 안 된 거 같아"*

- 부여 ✅ — `OnGiveAbility: ... SourceObject=DA_Skill_Test_Q | SlotTag=Ability.Slot.Q` (서버·클라 모두)
- 발동 ✅ — `GA_Test 발동` 이 0.3초 간격으로 계속 찍힌다
- ⚠ `[스킬] ... 쿨다운 N초` 로그가 **한 번도 없다**

## 진단

`ApplyCooldown` 진입에 진단 로그를 넣었다 → **진입 자체가 없다.** 조기 반환이 아니라 호출이 안 된다.

## ⭐ 원인

```cpp
// GameplayAbility.cpp:557-561
void UGameplayAbility::CommitExecute(...)
{
	ApplyCooldown(Handle, ActorInfo, ActivationInfo);
	ApplyCost(Handle, ActorInfo, ActivationInfo);
}
```

쿨다운·코스트는 **`CommitAbility` 시점**에만 걸린다. `GA_Test` 의 그래프가
`Event ActivateAbility → Print String → End Ability` 뿐이라 **Commit 을 거치지 않았다.**

⚠ GAS 는 이걸 경고하지 않는다. `CanActivateAbility` 는 쿨다운 태그가 **있을 때만** 막으므로,
태그를 심는 쪽(Commit)이 안 돌면 검사하는 쪽은 조용히 통과한다 — E09 · E10 과 같은 "조용히 건너뜀" 계열.

## 수정 — 에디터 (BP)

```
Event ActivateAbility
  → Commit Ability          ← ⭐ 추가. 쿨다운·코스트 검사 + 적용
     ├─ true  → Print String → End Ability
     └─ false → End Ability (was cancelled = true)
```

`Docs/3_EditorTasks/F07_스킬.md` 의 `GA_Test` 절을 고쳤다.

## ⚠ 재발 방지

- ⭐ **모든 어빌리티는 `ActivateAbility` 직후 `CommitAbility` 를 부른다.** F07-04 실행 파이프라인에서
  C++ 베이스가 이 순서를 강제할지 그때 결정한다 (Lyra 는 각 어빌리티 BP 가 직접 부른다).
- 테스트용 최소 BP 를 문서에 쓸 때도 **Commit 을 빼지 않는다.** "최소" 가 규약을 빼먹는 최소가 되면 안 된다.

- [x] 수정 후 재검증 — `쿨다운 5.00초` 로그 + 연타가 5초 간격으로만 통과 (2026-09-13)
- [x] `[쿨다운진단]` 임시 로그 제거 (코드에서 제거됨 · 빌드는 에디터 종료 후)
