# E05 — `PostAttributeChange` 에서 클라가 자기 어트리뷰트를 고치고 있었다

| | |
|---|---|
| 발견 | Lyra 소스 대조 중 (사용자 지적 아님) |
| 시점 | F02-04 구현 직후, PIE 검증 전 |
| 상태 | ✅ 구현 단계에서 수정 |
| 분류 | ⭐ **빌드는 통과하는데 동작이 틀린** 종류 |

## 증상

⚠ **런타임에 목격한 증상은 없다.** 빌드는 통과했다.

증상이 났다면 이렇게 보였을 것이다:

- 최대 체력 버프가 빠지는 순간 **클라 화면에서만** 체력바가 잠깐 튄다
- 서버 값이 다음 복제에 덮어쓰므로 **금방 정상으로 돌아온다** → 재현이 어렵고 원인을 못 찾는다

## 원인

`PostAttributeChange` 를 **서버 전용 함수로 착각**했다. 클라에서도 불린다.

```cpp
// 고치기 전 — 권위 검사가 없다
void UERAttributeSet::PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue)
{
    Super::PostAttributeChange(Attribute, OldValue, NewValue);

    if (Attribute == GetMaxHPAttribute() && NewValue < OldValue)
    {
        if (GetHP() > NewValue) { SetHP(NewValue); }   // ← 클라에서도 실행된다
    }
    ...
}
```

### 근거 ① — 복제가 이 함수까지 도달한다

```
OnRep_MaxHP
  -> GAMEPLAYATTRIBUTE_REPNOTIFY            AttributeSet.h:401-405
  -> SetBaseAttributeValueFromReplication   GameplayEffect.cpp:3471
  -> OnAttributeAggregatorDirty             GameplayEffect.cpp:3228  (Owner->IsNetSimulating() 처리)
  -> InternalUpdateNumericalAttribute       GameplayEffect.cpp:3671
  -> SetNumericValueChecked                 AttributeSet.cpp:102
  -> PostAttributeChange                                              ← 클라에서 여기까지
```

엔진 자신의 로그가 이걸 못박는다:

```cpp
// GameplayEffect.cpp:3491
UE_LOG(LogGameplayEffects, Log, TEXT("SetBaseAttributeValueFromReplication [%s]: ..."),
    OwnerIsNetAuthority ? TEXT("Authority") : TEXT("Client"), ...);
```

**`Client` 를 찍는 분기가 있다는 것 자체가 클라에서 돈다는 증거다.**

### 근거 ② — `SetHP()` 에는 권위 검사가 없다

```cpp
// AttributeSet.h:440-448
#define GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
    FORCEINLINE void Set##PropertyName(float NewVal) \
    { \
        UAbilitySystemComponent* AbilityComp = GetOwningAbilitySystemComponent(); \
        if (ensure(AbilityComp)) \
        { \
            AbilityComp->SetNumericAttributeBase(Get##PropertyName##Attribute(), NewVal); \
        }; \
    }
```

`ensure(AbilityComp)` 만 있고 **권위는 안 본다.** 반면 Lyra 가 쓰는 쪽은 본다:

```cpp
// AbilitySystemComponent.cpp:435-443
void UAbilitySystemComponent::ApplyModToAttribute(...)
{
    // We can only apply loose mods on the authority.
    if (IsOwnerActorAuthoritative())
    {
        ActiveGameplayEffects.ApplyModToAttribute(Attribute, ModifierOp, ModifierMagnitude);
    }
}
```

Lyra 가 같은 자리에서 `SetHealth` 대신 `ApplyModToAttribute` 를 쓴 이유가 이거였다
(`LyraHealthSet.cpp:204-211`).

## 해결

`PostAttributeChange` 앞머리에 권위 가드를 넣었다.

```cpp
// ⚠ 이 함수는 **클라에서도 불린다.** (호출 경로는 위 근거 ①)
// SetHP() 는 SetNumericAttributeBase 로 직행해서 권위 검사가 없다 (AttributeSet.h:446).
// 가드가 없으면 클라가 자기 베이스 값을 멋대로 고친다. 클라는 복제로 받기만 한다.
const UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent();
if (!ASC || !ASC->IsOwnerActorAuthoritative())
{
    return;
}
```

`ApplyModToAttribute` 로 바꾸는 방법도 있었지만 **가드 쪽을 골랐다.**
이 함수의 나머지(`bOutOfHealth` 해제)도 전부 서버 전용 상태라, 함수 전체를 막는 게 맞다.

⚠ `#include "AbilitySystemComponent.h"` 를 추가해야 했다 — `IsOwnerActorAuthoritative()` 호출에 완전 타입이 필요하다.

## 재발 방지

**`Set<어트리뷰트>()` 를 쓰기 전에 "이 함수가 서버에서만 불리는가"를 먼저 확인한다.**

| 함수 | 서버 전용? | 근거 |
|---|---|---|
| `PostGameplayEffectExecute` | ✅ 서버 전용 | GE 실행은 서버 권위 |
| `PreGameplayEffectExecute` | ✅ 서버 전용 | 위와 같음 |
| `PostAttributeChange` | ❌ **클라에서도** | `GameplayEffect.cpp:3491` |
| `PreAttributeChange` | ❌ **클라에서도** | 같은 경로 (`AttributeSet.cpp:87`) |
| `OnRep_*` | ❌ **클라 전용** | 정의상 |

⭐ **`PreAttributeChange` 도 클라에서 불린다.** 우리 `ClampAttribute` 는 넘어온 값만 자르고
다른 어트리뷰트를 쓰지 않으므로 문제가 없다 — **클램프 로직에 `Set` 을 넣으면 같은 버그가 난다.**

## 남은 검증

⬜ **PIE 로 확인 못 했다.** `MaxHP` 를 바꾸는 GE 가 아직 없다 (F02-05 이후).
`Docs/3_EditorTasks/F02_어트리뷰트셋.md` 검증 항목:
**클라에서 최대 체력 버프를 걸었다 빼고, 체력바가 튀지 않는지.**
