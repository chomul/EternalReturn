# F00-03 — ASC 부착 · 초기화

> 기능 [F00 GAS 기반 세팅](00_개요.md) · 선행 [02 ASC 배치 결정](02_ASC_배치결정.md)
> 체크리스트 [`03_ASC_부착_초기화.md`](../../2_Checklist/F00_GAS기반세팅/03_ASC_부착_초기화.md)

## 할 일

`AERPlayerState` 에 ASC를 만들고, `IAbilitySystemInterface` 를 구현하고, **`InitAbilityActorInfo()` 를 올바른 시점에** 부른다.

```cpp
// AERPlayerState.h
class AERPlayerState : public APlayerState, public IAbilitySystemInterface
{
    UPROPERTY(VisibleAnywhere, Category="GAS")
    TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
};
```

## ⭐ `InitAbilityActorInfo` 를 두 번 불러야 한다

```cpp
InitAbilityActorInfo(AActor* InOwnerActor, AActor* InAvatarActor);   // AbilitySystemComponent.h:1547
```

- **Owner** = `AERPlayerState` (ASC를 소유한 액터)
- **Avatar** = `AERCharacterBase` (실제로 월드에 서 있는 액터)

문제는 **서버와 클라의 초기화 시점이 다르다**는 것이다.

| 어디 | 언제 부르나 |
|---|---|
| **서버** | `APawn::PossessedBy()` — 컨트롤러가 폰을 잡은 직후 |
| **클라** | `APawn::OnRep_PlayerState()` — PlayerState가 복제되어 도착한 뒤 |

⭐ **이게 GAS에서 가장 흔한 버그의 원인이다.** 한쪽만 부르면 그쪽에서만 어빌리티가 동작한다.
클라에서 `OnRep_PlayerState` 를 빠뜨리면 **"서버에선 되는데 클라에선 안 된다"** 가 된다.

부활로 Pawn이 새로 생기면 **Avatar가 바뀌므로 다시 불러야 한다.**

## ⭐ `InitGlobalData()` 를 호출하지 마라

인터넷 예제 대부분이 `GameInstance::Init()` 에서 이걸 부르라고 한다. **UE 5.4에서는 불필요하다.**

> `AbilitySystemGlobals.cpp:68` — *"Make sure the user didn't try to initialize the system again (**we call InitGlobalData automatically in UE5.3+**)."*

호출해도 `IsAbilitySystemGlobalsInitialized()` 검사에 걸려 조용히 반환되지만, **없어도 되는 코드를 넣지 않는다** (`CLAUDE.md` §2).

## 만질 파일

```
Source/EternalReturn/Core/ERPlayerState.h / .cpp        (ASC 생성 + 인터페이스)
Source/EternalReturn/Character/ERCharacterBase.h / .cpp  (PossessedBy / OnRep_PlayerState)
```

> ⚠ F01-01에서 만든 빈 껍데기에 얹는다. **F01보다 이 Task가 먼저면 클래스를 여기서 만든다.**

## 검증 — `Play As Client, Number of Players: 2`

- **빌드 통과** (Editor + Server)
- 서버에서 `GetAbilitySystemComponent()` 가 유효한 포인터를 반환
- ⭐ **클라에서도** 유효한 포인터를 반환
- `ASC->AbilityActorInfo->AvatarActor` 가 캐릭터를 가리킨다 (서버·클라 양쪽)
- 부활(Pawn 재생성) 후에도 Avatar가 새 Pawn을 가리킨다

## 주의점

- ⭐ **`TObjectPtr<UAbilitySystemComponent>` 에 `UPROPERTY()` 를 반드시 붙인다.** 안 붙이면 GC로 dangling된다 (`CLAUDE.md` §5).
- ASC는 `CreateDefaultSubobject` 로 **생성자에서** 만든다.
- ASC 자체의 복제를 켠다 (`SetIsReplicated(true)`).
- `GetAbilitySystemComponent()` 가 `nullptr` 을 반환할 수 있는 시점이 있다 (초기화 전). **호출부가 항상 검사**하게 만든다.
- 야생동물(F12)은 Pawn에 ASC를 붙이고 **같은 인터페이스**를 구현한다. 그래야 호출부가 배치를 몰라도 된다.

## 미확인

| 항목 | 상태 |
|---|---|
| 부활 시 어트리뷰트 초기화 범위 | **(미확인)** — 체력만 회복인지 전체 리셋인지. F14(매치 진행)와 함께 정한다 |
| 관전 상태의 ASC 처리 | **(미확인)** |
