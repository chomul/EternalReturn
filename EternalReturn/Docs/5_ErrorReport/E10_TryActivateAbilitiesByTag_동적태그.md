# E10 — `TryActivateAbilitiesByTag` 가 동적 태그를 못 찾는다

> 발견 **2026-09-13** · F07-01 · ⭐ **사용자 보고**
> 같은 계열: [`E09`](E09_둔화_태그조회_애셋태그.md) — 둘 다 "API 이름이 어떤 태그를 보는지 말해주지 않는다"

## 증상

사용자 보고: *"`[스킬] <- DA_Skill_Test_Q` 까진 나왔는데 Q 를 눌렀는데 print string 한 게 안 나와"*

- 스킬 부여 ✅ — 로그에 찍힌다
- 입력 ✅ — 진단 로그로 확인
- 슬롯 태그 ✅ — `동적태그=Ability.Slot.Q` 로 심겨 있다
- `CanActivateAbility` ✅ — **true**
- ⚠ 그런데 **`TryActivateAbilitiesByTag` 가 false**

## ⭐ 진단 과정 — 한 단계씩 좁혔다

| 단계 | 로그 | 결과 |
|---|---|---|
| A. 바인딩 | `[입력진단] 바인딩: Ability.Slot.Q -> IA_Skill_Q` | ✅ |
| B. 키 입력 | `[입력진단] 키 눌림: Ability.Slot.Q` | ✅ |
| C. 부여·태그 | `동적태그=Ability.Slot.Q \| 인스턴스=GA_Test_C_0` | ✅ |
| D. 발동 가능 | `CanActivateAbility=true \| 실패태그=(없음)` | ✅ |
| E. 발동 요청 | `TryActivateAbilitiesByTag -> false` | ❌ |

⭐ **D 가 true 인데 E 가 false** — 어빌리티는 발동 가능한데 **찾는 단계**에서 걸린다.

---

## ⭐⭐ 원인

```cpp
// AbilitySystemComponent_Abilities.cpp:1475
// GetActivatableGameplayAbilitySpecsByAllMatchingTags
if (Spec.Ability && Spec.Ability->AbilityTags.HasAll(GameplayTagContainer))
```

`TryActivateAbilitiesByTag` 는 **어빌리티 클래스의 `AbilityTags`** 를 본다.
**스펙의 `DynamicAbilityTags` 는 보지 않는다.**

우리는 `ERSkill::GrantSkills` 에서 슬롯 태그를 `DynamicAbilityTags` 에 심었다 —
그래야 같은 어빌리티 클래스를 여러 슬롯이 공유할 수 있기 때문이다
(방안 B, [`15_스킬데이터_위치.md`](../4_Argument/15_스킬데이터_위치.md)).
그런데 찾는 함수가 다른 곳을 봤다.

### ⚠ Lyra 를 반만 봤다

`LyraAbilitySet.cpp:98` 이 `DynamicAbilityTags.AddTag(InputTag)` 로 심는 것은 봤다.
**그걸 어떻게 찾는지는 안 봤다.** Lyra 는 `TryActivateAbilitiesByTag` 를 안 쓴다:

```cpp
// LyraAbilitySystemComponent.cpp:198-200
for (const FGameplayAbilitySpec& AbilitySpec : ActivatableAbilities.Items)
{
    if (AbilitySpec.Ability && (AbilitySpec.DynamicAbilityTags.HasTagExact(InputTag)))
```

⭐ **직접 순회한다.** 심는 쪽과 찾는 쪽이 짝이다. 한쪽만 보고 다른 쪽을 엔진 편의 함수로 대체하면 어긋난다.

---

## 수정

```cpp
// ❌ 클래스 태그만 본다
ASC->TryActivateAbilitiesByTag(SlotTags);

// ⭐ Lyra 처럼 동적 태그를 직접 순회
for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
{
    if (Spec.Ability && Spec.DynamicAbilityTags.HasTagExact(SlotTag))
    {
        bActivated |= ASC->TryActivateAbility(Spec.Handle);
    }
}
```

`Core/ERPlayerController.cpp` — `OnSkillSlotPressed`.

### 왜 `AbilityTags` 에 넣는 쪽으로 안 갔나

`GA_Test` 의 Class Defaults `Ability Tags` 에 `Ability.Slot.Q` 를 넣으면 `TryActivateAbilitiesByTag` 가 그대로 동작한다.
⚠ 하지만 그러면 **같은 로직 클래스를 여러 슬롯이 못 쓴다** — `GA_Projectile` 이 카티야 Q 이자 다니엘 W 일 때
클래스 태그에 Q 와 W 를 둘 다 박으면 Q 를 눌러도 W 가 나간다.
방안 B(로직 : 데이터 = 1 : N)의 전제가 무너진다. **동적 태그가 맞고, 찾는 쪽을 고치는 게 맞다.**

---

## ⚠ 재발 방지 — E09 와 같은 교훈

| | E09 | E10 |
|---|---|---|
| 함수 | `GetActiveEffectsWithAllTags` | `TryActivateAbilitiesByTag` |
| 이름이 시사하는 것 | "태그로 GE 찾기" | "태그로 어빌리티 발동" |
| 실제로 보는 것 | GE **애셋** 태그만 | 어빌리티 **클래스** 태그만 |
| 우리가 넣은 곳 | **부여** 태그 | **동적** 태그 |

⭐ **GAS 의 태그 조회 API 는 "어떤 태그를 보는지" 를 이름에 담지 않는다.**
쓰기 전에 **엔진 소스에서 무엇을 매칭하는지 확인**한다. 두 번 겪었으니 세 번째는 없어야 한다.

⭐ 그리고 **Lyra 를 볼 때 "심는 쪽" 과 "찾는 쪽" 을 함께 본다.** 한쪽만 베끼면 어긋난다.

## ✅ 검증 완료 — 2026-09-13

수정 후 Q 를 두 번 눌러 두 번 다 발동했다:

```
[입력진단] ⭐ ActivateAbility 진입: GA_Test_C_0 (Ability.Slot.Q, 권위=서버)
LogBlueprintUserMessages: [GA_Test_C_0] GA_Test 발동
[입력진단] TryActivateAbility(Ability.Slot.Q) -> true
```

두 번째 발동 전에 `활성=X` 로 돌아온 것도 확인 — `End Ability` 가 정상이다.

- [x] 수정 후 재검증
- [x] 임시 진단 로그 제거 (`[입력진단]` 전부 + `ActivateAbility` 오버라이드 + 진단용 include)
