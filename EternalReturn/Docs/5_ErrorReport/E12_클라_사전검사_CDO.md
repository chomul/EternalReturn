# E12 — 클라의 `ServerInitiated` 사전 검사는 인스턴스가 아니라 **CDO** 에서 돈다

> 발견 **2026-09-13** · F07-03 · 사용자 로그에서 발견

## 증상

F07-03 테스트 중 클라 창에서 Q 를 누르자:

```
Ensure condition failed: IsInstantiated() [GameplayAbility.cpp:1637]
LogAbilitySystem: Error: /Game/GAS/Ability/GA_Test.Default__GA_Test_C: GetCurrentAbilitySpec cannot be called on a non-instanced ability.
LogEternalReturn: Error: [스킬] Default__GA_Test_C 에 SkillData 가 없다.
```

`Default__GA_Test_C` — **CDO** 다. 우리 어빌리티는 `InstancedPerActor` 인데 왜 CDO 에서 도나?

## ⭐ 원인

```cpp
// AbilitySystemComponent_Abilities.cpp:1586-1591  (UAbilitySystemComponent::TryActivateAbility)
if (NetMode != ROLE_Authority && (... == ServerOnly || ... == ServerInitiated))
{
	if (bAllowRemoteActivation)
	{
		if (Ability->CanActivateAbility(AbilityToActivate, ActorInfo, nullptr, nullptr, &FailureTags))
		//  ^^^^^^^ Spec->Ability = CDO. 인스턴스(Spec->GetPrimaryInstance())가 아니다
```

**클라**가 `ServerInitiated` 어빌리티를 `TryActivateAbility` 하면 서버로 보내기 전 사전 검사를 **CDO** 에서 한다.
서버의 `InternalTryActivateAbility` 는 인스턴스를 쓴다 (`:1723-1743` `AbilitySource = InstancedAbility ? InstancedAbility : Ability`).
**같은 함수가 서버에선 인스턴스, 클라에선 CDO 에서 불린다.**

F07-03 에서 `CheckCost` 를 오버라이드하며 `GetSkillData()` → `GetCurrentSourceObject()` → `GetCurrentAbilitySpec()` 을 불렀는데,
그 함수는 `ENSURE_ABILITY_IS_INSTANTIATED_OR_RETURN` 이다 (`GameplayAbility.cpp:1637`). CDO 에서 ensure.

⚠ F07-02 에서 안 보인 이유: `CheckCooldown` 은 `GetCooldownTags()` 만 부르고 그건 멤버를 돌려줄 뿐이라 CDO 에서도 안 터진다.
(대신 CDO 의 `CooldownTags` 는 비어 있어서 **클라 사전 쿨다운 검사는 항상 통과**한다 — 서버가 거부하므로 결과는 맞지만 RPC 한 번이 헛돈다. 아래 "남은 것".)

## 수정

`Handle` 을 받는 오버라이드(`CheckCost` · `ApplyCost` · `ApplyCooldown`)는 **핸들로 스펙을 찾는** `GetSkillData(Handle, ActorInfo)` 를 쓴다:

```cpp
const FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromHandle(Handle);
return Cast<UERSkillData>(Spec->SourceObject.Get());   // CDO 에서도 된다
```

`GetCostGameplayEffect()` 오버라이드는 **제거** — 핸들이 없는 시그니처라 CDO 에서 데이터를 못 찾는다.
`ApplyCost` 가 `CostType` 으로 GE 클래스를 직접 고른다.

무인자 `GetSkillData()` 는 남긴다 — `ActivateAbility` 안처럼 **인스턴스가 확실한 곳** 전용이라고 주석에 못 박았다.

## ⚠ 재발 방지

- ⭐ `UGameplayAbility` 의 `virtual ... (Handle, ActorInfo, ...)` 오버라이드 안에서는 **`GetCurrent*()` 계열을 쓰지 않는다.**
  `CurrentActorInfo` · `CurrentSpecHandle` · `GetCurrentSourceObject` 전부 인스턴스 멤버다. 인자로 받은 `Handle` · `ActorInfo` 로 찾는다.
  Lyra 도 `CheckCost(Handle, ActorInfo, ...)` 안에서 인자를 넘긴다 (`LyraGameplayAbility.cpp:201-220`).
- E09 · E10 · E11 과 같은 계열: **GAS 는 "어느 객체에서 불리는지" 를 이름에 담지 않는다.** 오버라이드하기 전에 호출부를 grep 한다.

## 남은 것

- [ ] CDO 의 `GetCooldownTags()` 가 비어 있어 클라 사전 쿨다운 검사가 통과한다 → 서버가 거부하니 동작은 맞음. UI 가 쿨다운 중 입력을 막으면 없어지는 문제라 **UI 작업 때** 본다
- [x] 수정 후 재검증 — 2026-09-13 클라 Q 연타 · 쿨다운 거부까지 ensure 없음
