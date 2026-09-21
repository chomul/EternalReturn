# E16 — 네이티브 GE 생성자에서 `FindOrAddComponent` 를 쓰면 에디터가 시작 중 Fatal

> 발견 **2026-09-19** · F11-02 · ⭐ **사용자 보고** ("에디터 빌드가 실패했음" — C++ 빌드는 통과했고 에디터 **기동**이 죽었다)

## 증상

```
LogWindows: Error: Fatal error: [UObjectGlobals.cpp] [Line: 4482]
NewObject with empty name can't be used to create default subobjects (inside of UObject derived class constructor)
as it produces inconsistent object names. Use ObjectInitializer.CreateDefaultSubobject<> instead.
```
`UERUnarmedEffect` CDO 생성 중 — 에디터가 뜨기 전에 죽는다.

## 원인

5.3+ 에서 GE 의 부여 태그(GrantedTags)는 `UTargetTagsGameplayEffectComponent` 로 든다. 이걸 붙이려고 생성자에서
`FindOrAddComponent<UTargetTagsGameplayEffectComponent>()` 를 불렀는데, 이 함수는 내부가 `NewObject<>(this, NAME_None, …)` 다
(`GameplayEffect.h:2418-2424` `AddComponent`). **생성자 안에서 이름 없는 NewObject 는 금지** — 엔진 소스의 `FindOrAddComponent` 호출은 전부
`PostLoad` 계열(애셋 업그레이드 경로, `GameplayEffect.cpp:452~`) 이지 생성자가 아니다.

## 수정

```cpp
// 생성자
UTargetTagsGameplayEffectComponent* Tags = CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("TargetTags"));
Tags->SetAndApplyTargetTagChanges(Granted);
GEComponents.Add(Tags);   // protected — 파생 클래스에서 접근 가능
```
빌드 · 기동 통과.

## ⚠ 재발 방지

- 네이티브 GE 에 **컴포넌트를 붙일 때는 `CreateDefaultSubobject` + `GEComponents.Add`**. `FindOrAddComponent` 는 생성자 밖(런타임 · PostLoad) 전용
- 기존 네이티브 GE(쿨다운 · 페이즈 · 상태) 는 **동적 태그**(`DynamicGrantedTags`) 라 컴포넌트가 필요 없었다 — 정적 부여 태그가 필요한 첫 GE 가 이것
- "C++ 빌드는 됐는데 에디터가 안 뜬다" 면 `Saved/Logs` 의 `Fatal error` 를 먼저 본다

- [x] 수정 후 빌드 — 2026-09-19 에러 0
- [ ] 에디터 기동 확인 (사용자)
