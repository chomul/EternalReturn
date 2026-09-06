# CLAUDE.md

Unreal Engine C++ 프로젝트용 작업 지침. LLM이 흔히 저지르는 실수를 줄이는 것이 목적이다.

**트레이드오프:** 속도보다 안정성을 우선한다. 사소한 작업에는 판단해서 적용한다.

---

## 0. 프로젝트 정보

| 항목 | 값 |
|---|---|
| 엔진 | `C:\UnrealEngine-5.4.4` (UE 5.4.4 소스 빌드) |
| 프로젝트 | `EternalReturn.uproject` |
| 모듈 | `EternalReturn` (Runtime) |
| 타겟 | Game / Editor / **Server** (데디케이티드 서버 있음) |
| 장르 | 탑다운 MOBA·배틀로얄 (이터널 리턴 유형) |
| 주요 의존 | `NavigationSystem`, `AIModule`, `Niagara`, `EnhancedInput` |

**폴더 구조** — `Source/EternalReturn/` 아래 기능별 분류. 새 파일은 기존 폴더 규칙을 따른다.
- `Character/` — 캐릭터, 입력 데이터 애셋
- `Component/` — 스탯 등 액터 컴포넌트
- `Core/` — GameInstance / GameMode / GameState / PlayerController / PlayerState

**네이밍**
- 프로젝트 클래스는 `ER` 프리픽스 + 언리얼 접두사: `AERCharacterBase`, `UERGameInstance`
- 컴포넌트는 `~Component` 접미사: `UCharacterStatComponent`
- 데이터 테이블 행 구조체는 `F~Row` + `FTableRowBase` 상속
- 주석과 커밋 메시지는 **한국어**로 작성한다 (기존 코드 스타일 유지)

---

## 1. 코딩 전에 생각하기

**추측하지 말 것. 혼란을 숨기지 말 것. 트레이드오프를 드러낼 것.**

구현 전에:
- 가정을 명시한다. 불확실하면 묻는다.
- 해석이 여러 개면 전부 제시한다 — 조용히 하나를 고르지 않는다.
- 더 단순한 방법이 있으면 말한다. 필요하면 반대 의견을 낸다.
- 불명확하면 멈춘다. 무엇이 헷갈리는지 이름 붙이고 묻는다.

**언리얼 추가 규칙**
- 직접 구현하기 전에 **엔진이 이미 제공하는지 먼저 확인한다.** (예: `CharacterMovementComponent`, `UGameplayStatics`, `FTimerManager`, `UNavigationSystemV1`, `AIPerception`)
- 게임플레이 기능은 **네트워크 설계부터 말한다**: 어디가 권위(서버)이고 무엇이 복제되는지. 이걸 정하지 않고 코드를 쓰지 않는다.

### 1.1 작업 시작 전 합의 — 예외 없다

**코드를 쓰기 전에 사용자와 구조·구현 방식을 합의한다.** 합의 없이 구현을 시작하지 않는다.

작업 시작 전에 아래 네 가지를 제시하고 승인을 받는다:

1. **구조** — 어떤 클래스·컴포넌트를 만들고 각각 무슨 책임을 지는지, 기존 코드 어디에 붙는지
2. **네트워크 설계** — 무엇이 서버 권위이고 무엇이 복제되는지 (§6)
3. **세부 구현 방식** — 자료구조, 데이터 테이블 스키마, 델리게이트·타이머 등 실제로 쓸 수단
4. **검증 방법** — 무엇이 되면 성공인지 (§4)

사소한 작업(오타 수정, 한 줄 버그 픽스, 로그 추가, 문서 오탈자)은 예외다. 판단해서 적용한다.

### 1.2 구현 방안이 여럿이면 — `Docs/Argument/` 에 비교 문서를 만든다

세부 구현에 **선택 가능한 방안이 2개 이상**이면 채팅으로 설명하지 말고, **문서를 만들어서 사용자가 고르게 한다.**

- 위치·이름: `Docs/Argument/N. 주제.md` — `N`은 1부터 증가하는 일련번호
  (예: `Docs/Argument/1. 스킬 시스템 구조 선택.md`)
- 작성 시점: **구현 시작 전.** 이미 짜놓고 사후 정당화하지 않는다.
- 채팅에는 **문서 경로 + 방안 이름 한 줄씩 + 내 추천 1개**만 남긴다. 긴 비교를 채팅에 복붙하지 않는다.

문서에 반드시 포함할 것:

1. **결정할 것** — 한 문장. 무엇을 고르는 건지.
2. **방안 목록** — 각 방안마다:
   - 어떤 방식인지 (필요하면 코드 스케치 5~15줄)
   - **장점** / **단점** 을 각각 불릿으로. 한쪽만 쓰지 않는다.
   - 이 프로젝트 기준 비용: 구현 난이도, 네트워크 비용, **GAS가 이미 제공하는 것을 다시 만드는 건 아닌지** (§8)
3. **비교표** — 방안 × 평가축. 축은 최소한 `구현 난이도 / 확장성 / 네트워크 / GAS 정합성 / 성능`
4. **추천** — 어느 것을 왜 추천하는지. **나열만 하고 끝내지 않는다.**
5. ⭐ **근거와 출처** — **모든 주장에 출처를 단다. 출처 없는 주장은 쓰지 않는다.**
   - 엔진 코드: `파일 경로:라인` (예: `Engine/Source/Runtime/Engine/Classes/GameFramework/Actor.h:1234`)
   - 공식 문서: URL + 문서 제목
   - 이 프로젝트 문서: `Docs/GameDesign/...` 상대 경로 + 섹션 번호
   - 이 프로젝트 코드: `Source/EternalReturn/...:라인`
   - 측정값이면 **어떻게 측정했는지**까지 적는다
   - 확인 못 한 것은 **`(미확인)`** 으로 남긴다. **추측을 근거처럼 쓰지 않는다.**

원칙: **사용자가 문서만 보고 고를 수 있어야 한다.** "상황에 따라 다릅니다"로 끝내지 않는다.

---

## 2. 단순함 우선

**문제를 푸는 최소한의 코드. 추측성 코드 금지.**

- 요청받지 않은 기능 추가 금지.
- 한 곳에서만 쓰는 코드에 추상화 금지. 인터페이스·베이스 클래스 남발 금지.
- 요청받지 않은 "유연성"·"확장성" 금지.
- 일어날 수 없는 상황에 대한 예외 처리 금지.
- 200줄 썼는데 50줄로 될 것 같으면 다시 쓴다.

**언리얼 추가 규칙**
- `Tick`은 정말 매 프레임 필요할 때만 켠다. 주기적 처리는 `FTimerManager`, 이벤트성은 델리게이트를 쓴다. 켤 거면 `PrimaryActorTick.TickInterval`을 검토한다.
- 컴포넌트를 하나 더 만들기 전에 기존 컴포넌트에 들어가는 게 맞는지 본다.

---

## 3. 수술적 변경

**꼭 필요한 것만 건드린다. 내가 만든 쓰레기만 치운다.**

- 인접 코드·주석·포맷을 "개선"하지 않는다.
- 망가지지 않은 것을 리팩터링하지 않는다.
- 다르게 짰을 코드라도 기존 스타일에 맞춘다.
- 관련 없는 죽은 코드를 발견하면 **말만 한다** — 지우지 않는다.
- 내 변경으로 생긴 미사용 include/변수/함수만 제거한다.

기준: 변경된 모든 줄이 사용자의 요청으로 직접 추적되어야 한다.

**언리얼 추가 규칙**
- 헤더에서는 **전방 선언**을 쓰고 include는 `.cpp`로 내린다. `.generated.h`는 항상 include 목록의 **마지막**.
- 기존 `UPROPERTY`의 순서·이름을 임의로 바꾸지 않는다 — 블루프린트/애셋 참조가 끊긴다.
- 클래스 이름 변경, 프로퍼티 이름 변경은 애셋을 깨뜨린다. 필요하면 먼저 알리고 승인받는다.

---

## 4. 목표 기반 실행 — 검증은 "빌드"다

언리얼에는 단위 테스트가 거의 없다. **컴파일 성공이 1차 검증이고, 이건 생략할 수 없다.**

작업을 검증 가능한 목표로 바꾼다:
- "스킬 추가" → "빌드 통과 + 서버에서 실행 시 클라에 복제되는 경로가 코드상 존재"
- "버그 수정" → "재현 조건을 먼저 말로 정의 → 수정 → 빌드 통과"

다단계 작업은 짧은 계획을 먼저 제시한다:
```
1. [단계] → 검증: [확인 방법]
2. [단계] → 검증: [확인 방법]
```

### 빌드 명령 (Claude가 직접 실행한다)

```bash
"C:/UnrealEngine-5.4.4/Engine/Build/BatchFiles/Build.bat" EternalReturnEditor Win64 Development \
  -Project="C:/Sung Unreal Project/Eternal Return/EternalReturn/EternalReturn.uproject" \
  -WaitMutex -NoHotReload
```

서버 타겟까지 확인이 필요할 때:
```bash
"C:/UnrealEngine-5.4.4/Engine/Build/BatchFiles/Build.bat" EternalReturnServer Win64 Development \
  -Project="C:/Sung Unreal Project/Eternal Return/EternalReturn/EternalReturn.uproject" -WaitMutex
```

빌드 규칙:
- C++ 파일을 수정했으면 **보고 전에 반드시 빌드한다.** "빌드는 안 해봤습니다"로 끝내지 않는다.
- 오래 걸리므로 백그라운드로 실행하고 결과를 기다린다.
- 백그라운드로 돌릴 때 출력을 `| tail` 같은 파이프로 넘기지 않는다. 파이프는 종료 전까지 아무것도 보여주지 않아 진행 상황을 알 수 없다. **로그 파일로 리다이렉트**하고, 진행률은 UBT 로그의 `[n/총개수]` 줄로 확인한다: `C:/UnrealEngine-5.4.4/Engine/Programs/UnrealBuildTool/Log.txt`
- 액션 수가 수천 개면(예: `[113/2453]`) 프로젝트가 아니라 **엔진 전체를 재빌드하는 중**이다. 몇 시간 걸리므로 사용자에게 알리고 계속할지 확인한다. 프로젝트 모듈만 바뀐 정상 빌드는 보통 수십 개 액션이다.
  - 흔한 원인: UBT 로그 앞부분의 `UnrealHeaderTool needs to run because ...` — UHT 실행 파일이 엔진 생성 코드보다 최신이면 전 모듈 UHT가 재실행되고 엔진 전체가 다시 컴파일된다.
- **에디터가 열려 있으면 링크가 실패한다.** `Cannot open ... UnrealEditor-EternalReturn.dll` 류의 에러가 나오면 사용자에게 **에디터를 닫아달라고 요청**하고 재시도한다. 임의로 프로세스를 죽이지 않는다.
- 빌드 실패 시 에러를 그대로 인용하고 고친다. 실패를 성공으로 보고하지 않는다.
- 헤더에 `UPROPERTY`/`UFUNCTION`을 추가하면 에디터 재시작이 필요할 수 있다 — 그럴 땐 사용자에게 알린다.

### 기능 작업이 끝나면 — `Docs/EditorTasks/` 문서를 **반드시** 만든다

C++ 빌드가 통과했다고 기능이 끝난 게 아니다. **에디터에서 손대야 할 것과 PIE 로 확인해야 할 것을 문서로 남긴다.**

- 위치·이름: **`Docs/EditorTasks/F##_기능명.md`** — Task 폴더와 **같은 이름**을 쓴다
  (예: `Docs/Task/F01_코어골격_팀ID/` → `Docs/EditorTasks/F01_코어골격_팀ID.md`)
  ⚠ 날짜를 파일명에 넣지 않는다. 기능이 갱신되면 같은 파일을 고친다.
- 작성 시점: 빌드가 통과한 **직후.** 빌드 실패 상태로 만들지 않는다.
- 채팅에는 문서 경로와 3줄 요약만. 긴 절차를 복붙하지 않는다.
- **에디터에서 할 게 하나도 없어도 문서는 만든다.** "없음"이라고 적는다 — 사용자가 확인할 게 없다는 것도 정보다.

#### ⭐ 두 섹션을 반드시 **분리**한다

섞어 쓰지 않는다. 사용자가 "지금 뭘 해야 하나"와 "뭘 확인해야 하나"를 구분할 수 있어야 한다.

**① 에디터 작업** — 애셋·설정을 실제로 **바꾸는** 일. 하고 나면 저장되고 남는다.
- 애셋 전체 경로 (`/Game/Core/BP_ERGameMode`)
- 메뉴·패널 위치 (`File > Reparent Blueprint`, `Details 패널 > Match 카테고리`)
- 프로퍼티 **에디터 표시 이름**과 넣을 값 (C++ 변수명이 아니다)
- 새로 만들 애셋은 타입·부모 클래스·이름·저장 위치까지
- 데이터 테이블은 컬럼명과 예시 행을 표로, 행 이름 규칙도

**② 테스트** — PIE 로 **확인만** 하고 끝나는 일. 아무것도 바꾸지 않는다.
- Net Mode / 플레이어 수를 명시 (`Play As Client, Number of Players: 4`)
- **성공 판정을 구체적으로.** 어떤 로그가 몇 줄 나와야 하는지, 무엇이 보이면 실패인지
- 실패 시 원인 후보를 표로

각 항목에 **체크박스(`- [ ]`)** 를 단다. 사용자가 그대로 체크하며 진행한다.

#### 그 밖에 포함할 것

- **목적** — 끝나면 무엇이 동작하는지 한 문단
- **선행 조건** — 에디터 재시작 필요 여부, 먼저 끝나 있어야 할 작업
- **주의점** — 흔한 실수, 값을 잘못 넣었을 때 나타나는 증상
- **끝나면** — 어느 체크리스트의 어느 항목이 채워지는지

원칙: **사용자가 문서만 보고 그대로 따라 하면 끝나야 한다.** "적당히 설정하세요"를 쓰지 않는다.
값을 모르면 추측해서 적지 말고 그 자리에서 묻는다.

---

## 5. 언리얼 필수 규칙

- **GC**: `UObject*` 멤버는 반드시 `UPROPERTY()`를 붙인다. 안 붙이면 가비지 컬렉션으로 dangling된다.
- **생성자**: `CreateDefaultSubobject`는 생성자에서만. 생성자에서 게임 로직·월드 접근 금지.
- **초기화 순서**: `BeginPlay`보다 이른 시점에 다른 액터를 참조하지 않는다. 필요하면 `PostInitializeComponents` 또는 지연 초기화.
- **애셋 참조**: 하드 포인터 대신 `TSoftObjectPtr` / `TSubclassOf` + `EditDefaultsOnly`를 우선 고려한다.
- **문자열**: 리터럴은 `TEXT("...")`. 로그는 `UE_LOG`, 임시 디버그는 `GEngine->AddOnScreenDebugMessage` 대신 `UE_LOG`를 기본으로.
- **검증**: 회복 불가능한 프로그래머 실수는 `check()`, 런타임에 발생 가능한 상황은 `ensure()` + 조기 반환. `nullptr` 체크 없이 역참조하지 않는다.
- **캐스팅**: `Cast<T>()` 결과는 항상 검사한다. `static_cast`로 UObject를 캐스팅하지 않는다.

---

## 6. 멀티플레이 / 리플리케이션

이 프로젝트는 데디케이티드 서버 타겟이 있고 스탯이 이미 `Replicated`다. **모든 게임플레이 코드는 네트워크를 전제로 작성한다.**

- **서버 권위**: 체력·데미지·스탯·상태이상 등 게임 상태 변경은 서버에서만. 클라이언트에서 직접 값을 바꾸지 않는다.
- 값 변경 전에 `HasAuthority()`로 가드한다.
- `UPROPERTY(Replicated)`를 추가하면 **반드시** `GetLifetimeReplicatedProps`에 `DOREPLIFETIME`을 추가한다. 빠뜨리면 조용히 복제되지 않는다.
- UI/연출 갱신이 필요한 값은 `ReplicatedUsing = OnRep_XXX`를 쓴다.
- RPC 규칙: 입력은 `Server` RPC(`WithValidation` 포함), 연출은 `NetMulticast`, 본인 전용 피드백은 `Client`. RPC에 무거운 로직을 넣지 않는다.
- **클라이언트 예측(prediction)은 요청받았을 때만 구현한다.** 먼저 서버 권위로 동작하게 만든다.

---

## 7. C++ / 블루프린트 경계

**로직은 C++, 블루프린트는 데이터와 연출만.**

- 게임플레이 로직·수치 계산·상태 전이·네트워크 코드는 전부 C++에 둔다.
- 블루프린트가 담당하는 것: 메시/머티리얼/애님BP 지정, 나이아가라·사운드·카메라 셰이크 연출, 위젯 바인딩, 데이터 테이블 지정.
- 노출 규칙:
  - 디자이너가 애셋·수치를 세팅 → `UPROPERTY(EditDefaultsOnly, Category="...")`
  - BP/위젯이 읽기만 → `BlueprintReadOnly`
  - `BlueprintReadWrite`는 기본적으로 쓰지 않는다 (BP가 상태를 바꾸게 되므로)
- `BlueprintImplementableEvent`는 **연출 훅**(이펙트 재생, 몽타주 등)에만 쓴다. 로직 분기를 BP로 넘기지 않는다.
- 새 `UFUNCTION`을 BP에 노출할 땐 왜 노출이 필요한지 한 줄로 근거를 댄다.

---

## 8. GAS 기반 개발

**이 프로젝트는 GAS(GameplayAbilitySystem) 위에서 만든다.** 2026-09-02 결정 — 근거는 `Docs/Argument/1. 스킬 시스템 구조 선택.md`.

> 이전 방침은 "자체 구현 후 나중에 전환"이었다. 코드가 한 줄도 없는 시점이라 **두 번 만들지 않기로** 했다.

**활성화 상태** — 이미 켜져 있다. 다시 켜지 않는다.
- `EternalReturn.uproject` → `Plugins` 에 `GameplayAbilities`
- `EternalReturn.Build.cs` → `GameplayAbilities`, `GameplayTags`, `GameplayTasks`

### 어디에 무엇을 쓰는가

| 하려는 것 | GAS에서 | 직접 만들지 마라 |
|---|---|---|
| 스탯(체력·공격력·방어력) | `UAttributeSet` | ❌ `UStatComponent` |
| 스킬 1개 | `UGameplayAbility` 파생 클래스 1개 | ❌ 자체 스킬 베이스 |
| 쿨다운 | Cooldown `UGameplayEffect` (Duration) | ❌ `FTimerManager` 직접 |
| 코스트(기력·체력) | Cost `UGameplayEffect` | ❌ 수동 차감 |
| 데미지 공식 | `UGameplayEffectExecutionCalculation` **하나** | ❌ 스킬마다 계산 |
| 상태이상(CC) | `UGameplayEffect` + `GameplayTag` | ❌ 자체 상태 enum |
| CC로 시전 차단 | `ActivationBlockedTags` | ❌ 수동 플래그 검사 |
| 채널링·몽타주 대기 | `UAbilityTask` | ❌ 자체 타이머 상태 머신 |
| 조준 데이터 전송 | `FGameplayAbilityTargetData` | ❌ 자체 RPC 구조체 |
| 이펙트·사운드 | `GameplayCue` | ❌ 스킬 안에서 직접 재생 |

**GAS가 이미 제공하는 것을 다시 만들지 않는다** (§1 언리얼 추가 규칙과 같은 원칙).

### 그래도 직접 만드는 것

GAS가 안 해주는 영역이다. 여기는 자체 구현이 맞다.
- **판정 형상 계산** (부채꼴·이중 반경·관통) — `TargetData` 는 결과를 담는 그릇일 뿐, 기하 계산은 직접 한다.
- **팀 시야(fog of war)** · 릴리번시
- **클릭 이동 · 카메라**
- **인벤토리 · 제작 · 매치 타임라인**

### 반드시 지킬 것

- **ASC 배치를 바꾸지 않는다.** 한 번 정하면 되돌리기 비용이 크다. 결정은 `Docs/Task/F00_GAS기반세팅/` 참조.
- **복제 모드**: 플레이어는 `Mixed`, AI·야생동물은 `Minimal`.
  ⚠ `Minimal` 은 **소유자가 있는 ASC에서 동작하지 않는다** — 엔진 주석: *"this does not work for Owned AbilitySystemComponents (Use Mixed instead)"* (`AbilitySystemComponent.h:87`).
- **`InitGlobalData()` 를 직접 호출하지 않는다.** UE 5.3+ 는 자동 호출한다 (`AbilitySystemGlobals.cpp:68` — *"we call InitGlobalData automatically in UE5.3+"*). 인터넷 예제 대부분이 5.2 이전 기준이라 이걸 시킨다. **따라 하지 마라.**
- **어트리뷰트 클램프는 `PreAttributeChange`** 에 넣는다. 엔진 주석: *"This function is meant to enforce things like Health = Clamp(Health, 0, MaxHealth) and NOT things like trigger this extra thing if damage is applied"* (`AttributeSet.h:210-212`). 베이스 값까지 자르려면 `PreAttributeBaseChange` 도 같이 (`:225`).
- **어빌리티 부여·발동은 서버 권위.** §6 규칙은 그대로 적용된다.
- **`GameplayTag` 를 코드에 문자열로 박지 않는다.** 태그는 데이터로 관리한다.

### 예측(prediction)

**GAS 예측을 지금 켜지 않는다.** 먼저 서버 권위로 정확히 동작시킨다 (§6과 동일). 예측은 별도 작업으로 요청받았을 때 한다.

---

## 9. 건드리지 말 것

- ❌ **`Content/` 하위 애셋 파일** (`.uasset`, `.umap`) — 생성·수정·삭제 절대 금지. 읽지도 않는다(바이너리). 에디터에서 해야 할 작업은 §4의 **에디터 작업 문서**로 넘긴다.
- ❌ **`git commit` / `git push`** — 사용자가 명시적으로 요청하기 전까지 하지 않는다. `git status`, `git diff` 같은 읽기 명령은 자유롭게 사용.
- ⚠️ `Config/*.ini`, `EternalReturn.uproject`, `*.Build.cs` — 수정은 가능하지만 **변경 내용을 요약해서 보고**한다. 플러그인 활성화나 모듈 의존성 추가는 이유를 함께 설명한다.
- `Binaries/`, `Intermediate/`, `Saved/`, `DerivedDataCache/` — 빌드 산출물. 수정하지 않는다.

---

## 10. 토큰 절약 규칙

### 엔진 소스 참조
- 엔진 소스를 통째로 읽지 않는다. 필요한 클래스·함수만 `grep`으로 찾아 해당 부분만 확인한다.
  ```bash
  grep -n "virtual void BeginPlay" "C:/UnrealEngine-5.4.4/Engine/Source/Runtime/Engine/Classes/GameFramework/Actor.h"
  ```
- 전체 헤더를 열기 전에, 함수 시그니처만으로 충분한지 먼저 판단한다.

### 빌드 에러 처리
- 빌드 로그 전체를 붙여넣지 않는다. **파일명 + 라인 번호 + 에러 코드**(`C2xxx`, `LNK2xxx` 등)만 추려서 전달한다.
- UHT 에러는 대개 마지막 5~10줄에 핵심 원인이 있다. 그 부분만 확인한다.

### 코드 작성
- 리플렉션 매크로(`UCLASS`/`UPROPERTY`/`UFUNCTION`)의 `meta` 옵션은 **실제로 필요한 것만** 쓴다. 옵션 나열 금지.
- `.h`/`.cpp`를 함께 수정할 때는 **변경된 부분만 diff로** 보여준다. 파일 전체 재출력 금지.

### 블루프린트 논의
- 블루프린트 JSON 직렬화 전체를 붙여넣지 않는다. 핵심 노드 흐름만 텍스트로 요약한다.
- 스크린샷보다 화살표 요약을 선호한다: `OnComponentHit → CastToPlayer → ApplyDamage`

### 컨텍스트 관리
- 작업이 여러 파일에 걸칠 때, 관련 없는 파일은 컨텍스트에 올리지 않는다.
- **한 세션에서 하나의 기능/버그에만 집중한다.** 여러 이슈를 동시에 처리하지 않는다.

---

**이 지침이 작동하고 있다는 신호:** diff에 불필요한 변경이 줄고, 과설계로 인한 재작성이 줄고, 실수 이후가 아니라 구현 이전에 질문이 나온다. 그리고 "빌드 안 해봤다"는 보고가 사라진다.
