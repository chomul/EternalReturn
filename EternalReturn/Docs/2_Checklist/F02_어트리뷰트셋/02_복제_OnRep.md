# ✅ F02-02 — 복제 · OnRep

> Task [`../../Task/F02_어트리뷰트셋/02_복제_OnRep.md`](../../1_Task/F02_어트리뷰트셋/02_복제_OnRep.md)

## 선행

- [ ] [01 어트리뷰트 정의](01_어트리뷰트_정의.md) 완료
- [ ] 클라 2대로 PIE를 띄울 수 있다

## 구현

- [ ] 각 어트리뷰트에 `ReplicatedUsing = OnRep_XXX`
- [ ] `OnRep_XXX(const FGameplayAttributeData& OldValue)` 시그니처
- [ ] ⭐ **모든 `OnRep_` 안에서 `GAMEPLAYATTRIBUTE_REPNOTIFY` 호출**
- [ ] `GetLifetimeReplicatedProps` 오버라이드
- [ ] `Super::GetLifetimeReplicatedProps()` 를 먼저 호출
- [ ] ⭐ `DOREPLIFETIME_CONDITION_NOTIFY(..., COND_None, **REPNOTIFY_Always**)`
- [ ] ⭐ **`IncomingDamage`(Meta)를 `DOREPLIFETIME` 에 넣지 않았다**

## 빌드

- [ ] `EternalReturnEditor` 빌드 통과
- [ ] `EternalReturnServer` 빌드 통과

## 검증 — `Play As Client, Number of Players: 2`

- [ ] ⭐ 서버에서 GE로 체력을 바꾸면 **클라 값이 갱신된다**
- [ ] `OnRep_Health` 가 클라에서 호출된다
- [ ] ⭐ `IncomingDamage` 가 클라에 **오지 않는다**
- [ ] 클라에서 어트리뷰트를 직접 바꿔도 서버 값이 안 변한다

## ⭐ GAS 특유의 함정 — 이 Task에서 가장 위험한 둘

- [ ] **`GAMEPLAYATTRIBUTE_REPNOTIFY` 누락이 없다**
  일반 `OnRep_` 처럼 값만 대입하면 컴파일도 되고 값도 보이는데 **GAS 내부 캐시가 갱신되지 않는다.**
  예측·이펙트 스택이 조용히 어긋나고 **나중에 찾기 매우 어렵다.**
  ```
  grep -n "OnRep_" Source/EternalReturn/GAS/ERAttributeSet.cpp   ← 각 함수에 매크로가 있는지 눈으로 확인
  ```
- [ ] **`DOREPLIFETIME` 누락이 없다** — 어트리뷰트를 추가할 때마다 여기에도 추가

## 흔한 실수 — 걸렸는지 확인

- [ ] `REPNOTIFY_Always` 를 빠뜨리지 않았다 (같은 값 롤백을 클라가 못 알아챈다)
- [ ] `UFUNCTION()` 을 `OnRep_` 에 붙였다
- [ ] ASC 이펙트 복제 모드(F00-04)를 **여기서 또 건드리지 않았다**

## 막혔을 때

| 증상 | 확인할 것 |
|---|---|
| 클라 값이 안 옴 | `DOREPLIFETIME` / ASC `SetIsReplicated` / 소유 액터 복제 |
| 값은 오는데 이펙트 계산이 이상함 | ⭐ `GAMEPLAYATTRIBUTE_REPNOTIFY` 누락 |
| 롤백이 UI에 반영 안 됨 | `REPNOTIFY_Always` 누락 |

## 미확인으로 남긴 것

- [ ] 적 어트리뷰트 복제 범위를 `COND_None` 으로 두고 **(미확인)** 표기했다
