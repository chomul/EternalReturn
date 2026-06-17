# F02-02 — 복제 · OnRep

> 기능 [F02 어트리뷰트셋](00_개요.md) · 선행 [01 어트리뷰트 정의](01_어트리뷰트_정의.md)
> 체크리스트 [`02_복제_OnRep.md`](../../Checklist/F02_어트리뷰트셋/02_복제_OnRep.md)

## 할 일

어트리뷰트를 복제하고, GAS 전용 `OnRep` 매크로로 예측 시스템과 동기화한다.

```cpp
// 헤더
UFUNCTION() void OnRep_Health(const FGameplayAttributeData& OldValue);
UFUNCTION() void OnRep_MaxHealth(const FGameplayAttributeData& OldValue);

// cpp
void UERAttributeSet::OnRep_Health(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UERAttributeSet, Health, OldValue);   // ⭐ 반드시 이 매크로
}

void UERAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const
{
    Super::GetLifetimeReplicatedProps(Out);
    DOREPLIFETIME_CONDITION_NOTIFY(UERAttributeSet, Health,    COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UERAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
    // ⭐ IncomingDamage(Meta)는 넣지 않는다
}
```

## ⭐ `GAMEPLAYATTRIBUTE_REPNOTIFY` 를 반드시 쓴다

매크로 정의는 `AttributeSet.h:401`, 사용 예시가 `:397` 에 있다.

일반 `OnRep_` 처럼 값만 대입하면 **GAS 내부 캐시가 갱신되지 않는다.**
클라 예측·이펙트 스택 계산이 어긋나고, **증상이 즉시 드러나지 않아 나중에 찾기 매우 어렵다.**

## ⭐ `REPNOTIFY_Always` 가 필요한 이유

기본 동작은 **값이 바뀌었을 때만** OnRep을 부른다.
그런데 어트리뷰트는 **같은 값으로 되돌아오는 경우**가 있다 — 예측이 틀려 서버 값으로 롤백될 때.
`REPNOTIFY_Always` 가 없으면 그 롤백을 클라가 못 알아챈다.

## ⭐ Meta Attribute는 복제하지 않는다

`IncomingDamage` 를 `DOREPLIFETIME` 에 넣으면 안 된다.
서버에서 한 번 쓰고 버리는 값이고, 클라에 보내면 **다른 플레이어가 받을 피해량이 노출**된다.

## 만질 파일

```
Source/EternalReturn/GAS/ERAttributeSet.h / .cpp
```

## 검증 — `Play As Client, Number of Players: 2`

- **빌드 통과** (Editor + Server)
- ⭐ 서버에서 GE로 체력을 바꾸면 **클라 값이 갱신된다**
- `OnRep_Health` 가 클라에서 호출된다
- `IncomingDamage` 가 클라에 **오지 않는다**
- 클라에서 어트리뷰트를 직접 바꿔도 서버 값이 안 변한다

## 주의점

- ⭐ **`GAMEPLAYATTRIBUTE_REPNOTIFY` 누락** — 이 Task에서 가장 위험한 실수. 컴파일도 되고 값도 보이는데 예측이 조용히 깨진다.
- ⭐ **`DOREPLIFETIME` 누락** — 어트리뷰트를 추가할 때마다 여기에도 추가한다. 빠뜨리면 에러 없이 복제만 안 된다 (`CLAUDE.md` §6).
- `Super::GetLifetimeReplicatedProps()` 를 먼저 부른다.
- ASC 자체의 이펙트 복제 모드는 F00-04에서 `Mixed` 로 설정했다. **여기서 또 건드리지 않는다.**
- 적의 어트리뷰트를 어디까지 보낼지는 `COND_` 로 조절할 수 있다 — 지금은 `COND_None` 으로 두고 **(미확인)** 으로 남긴다.

## 미확인

| 항목 | 상태 |
|---|---|
| 적 어트리뷰트 복제 범위 | **(미확인)** — 적 공격력을 알면 정보 우위. `COND_OwnerOnly` 검토 대상 |
| 복제 빈도의 실제 비용 | **(미측정)** — 24명 규모 |
