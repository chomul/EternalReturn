# ✅ F02-02 — 복제 · OnRep

> Task [`../../Task/F02_어트리뷰트셋/02_복제_OnRep.md`](../../1_Task/F02_어트리뷰트셋/02_복제_OnRep.md)

## 선행

- [x] [01 어트리뷰트 정의](01_어트리뷰트_정의.md) 완료
- [ ] 클라 2대로 PIE를 띄울 수 있다

## 구현

- [x] 각 어트리뷰트에 `ReplicatedUsing = OnRep_XXX`
- [x] `OnRep_XXX(const FGameplayAttributeData& OldValue)` 시그니처
- [x] ⭐ **모든 `OnRep_` 안에서 `GAMEPLAYATTRIBUTE_REPNOTIFY` 호출**
- [x] `GetLifetimeReplicatedProps` 오버라이드
- [x] `Super::GetLifetimeReplicatedProps()` 를 먼저 호출
- [x] ⭐ `DOREPLIFETIME_CONDITION_NOTIFY(..., COND_None, **REPNOTIFY_Always**)`
- [x] ⭐ **`IncomingDamage`(Meta)를 `DOREPLIFETIME` 에 넣지 않았다**

## 빌드

- [x] `EternalReturnEditor` 빌드 통과
- [x] `EternalReturnServer` 빌드 통과

## 검증 — `Play As Client, Number of Players: 2`

- [ ] ⭐ 서버에서 GE로 체력을 바꾸면 **클라 값이 갱신된다**
- [ ] `OnRep_Health` 가 클라에서 호출된다
- [ ] ⭐ `IncomingDamage` 가 클라에 **오지 않는다**
- [ ] 클라에서 어트리뷰트를 직접 바꿔도 서버 값이 안 변한다

## ⭐ GAS 특유의 함정 — 이 Task에서 가장 위험한 둘

- [x] **`GAMEPLAYATTRIBUTE_REPNOTIFY` 누락이 없다**
  일반 `OnRep_` 처럼 값만 대입하면 컴파일도 되고 값도 보이는데 **GAS 내부 캐시가 갱신되지 않는다.**
  예측·이펙트 스택이 조용히 어긋나고 **나중에 찾기 매우 어렵다.**
  ```
  grep -n "OnRep_" Source/EternalReturn/GAS/ERAttributeSet.cpp   ← 각 함수에 매크로가 있는지 눈으로 확인
  ```
- [x] **`DOREPLIFETIME` 누락이 없다** — 어트리뷰트를 추가할 때마다 여기에도 추가

## 흔한 실수 — 걸렸는지 확인

- [x] `REPNOTIFY_Always` 를 빠뜨리지 않았다 (같은 값 롤백을 클라가 못 알아챈다)
- [x] `UFUNCTION()` 을 `OnRep_` 에 붙였다
- [x] ASC 이펙트 복제 모드(F00-04)를 **여기서 또 건드리지 않았다**

## 막혔을 때

| 증상 | 확인할 것 |
|---|---|
| 클라 값이 안 옴 | `DOREPLIFETIME` / ASC `SetIsReplicated` / 소유 액터 복제 |
| 값은 오는데 이펙트 계산이 이상함 | ⭐ `GAMEPLAYATTRIBUTE_REPNOTIFY` 누락 |
| 롤백이 UI에 반영 안 됨 | `REPNOTIFY_Always` 누락 |

## 미확인으로 남긴 것

- [x] 적 어트리뷰트 복제 범위를 `COND_None` 으로 두고 **(미확인)** 표기했다

---

## 실행 기록 (2026-09-07)

코드는 F02-01 에서 함께 들어갔다. 이 Task 는 **검증 위주**였다.

### 선언 수 대조 — 전부 일치

```
ReplicatedUsing = OnRep_   33
UFUNCTION() void OnRep_    33
ER_REP(...)                33
ER_ONREP(...)              33
ER_REP(IncomingDamage)      0   ← Meta 는 복제하지 않는다 ✅
```

⭐ 매크로로 묶은 덕에 **세로로 정렬돼 눈으로 대조된다.** 33개를 손으로 쓰면 하나씩 어긋난다.

### ⭐ 조사: AttributeSet 이 실제로 복제되는 경로를 확인했다

"`UPROPERTY(Replicated)` 를 붙였으니 되겠지"로 넘기지 않고 엔진에서 확인했다.

**1) ASC 가 AttributeSet 을 복제 서브오브젝트로 내보낸다**

```cpp
// AbilitySystemComponent.cpp - UAbilitySystemComponent::ReplicateSubobjects
for (const UAttributeSet* Set : GetSpawnedAttributes())
    if (IsValid(Set))
        WroteSomething |= Channel->ReplicateSubobject(const_cast<UAttributeSet*>(Set), *Bunch, *RepFlags);
```

**2) 생성자에서 만든 세트가 `SpawnedAttributes` 에 자동 등록된다**

```cpp
// AbilitySystemComponent_Abilities.cpp:56 - InitializeComponent()
AActor* Owner = GetOwner();
GetObjectsWithOuter(Owner, ChildObjects, ...);
for (UObject* Obj : ChildObjects)
    if (UAttributeSet* Set = Cast<UAttributeSet>(Obj))
        SpawnedAttributes.AddUnique(Set);
```

⚠ 엔진 주석의 조건: *"we are currently requiring all attribute sets to be **subobjects of the same owner**"*

우리 세트는 `AERPlayerState` 의 서브오브젝트이고 ASC 도 같은 Owner 다 → **조건 만족.**

⭐ **이 사실이 "고유 세트 병렬 추가" 결정을 뒷받침한다** — 에키온 VF 세트도
같은 PlayerState 에 만들면 자동으로 등록된다. 별도 배선이 필요 없다.

### 빌드

F02-01 과 같은 빌드. Editor / Server 둘 다 exit 0, 에러 0.

### ⬜ PIE 검증은 F02 전체가 끝난 뒤

지금은 어트리뷰트에 값을 넣을 GE 가 없어서 "서버에서 바꾸면 클라가 받는지"를 시험할 수단이 없다.
**F02-05(초기값 GE) 이후**에 EditorTasks 문서로 한 번에 확인한다.
