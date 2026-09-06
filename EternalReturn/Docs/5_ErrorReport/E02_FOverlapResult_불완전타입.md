# E02 — `FOverlapResult` 불완전 타입

> 발견 2026-09-06 · 관련 [F04 판정·타게팅](../1_Task/F04_판정_타게팅/00_개요.md)
> 상태: ✅ **해결**

## 증상

`ERTargeting.cpp` 첫 빌드에서 컴파일 실패.

```
ERTargeting.cpp(69): error C2027: 정의되지 않은 형식 'FOverlapResult'을(를) 사용했습니다.
```

해당 줄은 오버랩 결과를 순회하는 코드였다.

```cpp
for (const FOverlapResult& Overlap : Overlaps)
{
    AActor* Actor = Overlap.GetActor();   // ← 여기
    ...
}
```

`#include "Engine/World.h"` 는 이미 있었고, `World->OverlapMultiByChannel(...)` 선언도
`World.h` 에 있어서 **호출 자체는 컴파일됐다.**

## 원인

**UE5 에서 `Engine/World.h` 가 `FOverlapResult` 정의를 더 이상 끌어오지 않는다.**

`World.h` 는 `OverlapMultiByChannel` 시그니처에 `TArray<struct FOverlapResult>&` 를 쓰는데,
`struct FOverlapResult` **전방 선언만으로 충분**하다. 그래서 함수 선언은 통과하지만
**멤버에 접근하는 순간** 정의가 없어 터진다.

```cpp
// Engine/Classes/Engine/World.h:2092
bool OverlapMultiByChannel(TArray<struct FOverlapResult>& OutOverlaps, ...) const;
//                                ^^^^^^ 전방 선언
```

정의는 별도 헤더로 분리돼 있다 — `Engine/OverlapResult.h`.

> UE4 시절에는 `World.h` 가 이걸 포함하는 경로가 있어서 그냥 됐다.
> UE5 의 include 정리(IWYU) 과정에서 빠졌다.

## 해결

```cpp
#include "Engine/OverlapResult.h"
```

한 줄 추가로 해결. 다시 빌드 → exit 0.

## 재발 방지

- **`Sweep` / `Line` 계열도 같은 구조다.** `FHitResult` 는 `Engine/HitResult.h` 에 있다.
  지금은 다른 헤더 경로로 들어와 있지만, 같은 에러가 나면 **결과 타입의 전용 헤더**를 의심한다.
- ⚠ **막을 방법이 마땅치 않다.** 이건 "쓸 때 알게 되는" 종류다.
  다만 `error C2027` + 엔진 구조체 이름이면 **include 누락을 먼저 의심**하면 빠르게 끝난다.

## 배운 것

`C2027`(불완전 타입)은 대개 **"선언은 보이는데 정의가 안 보이는"** 상황이다.
함수 호출이 컴파일된다고 그 함수의 매개변수 타입 정의까지 있는 건 아니다.
