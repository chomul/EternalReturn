# CLAUDE.md

Unreal Engine C++ 프로젝트용 작업 지침. LLM이 흔히 저지르는 실수를 줄이는 것이 목적이다.

**트레이드오프:** 속도보다 안정성을 우선한다. 사소한 작업에는 판단해서 적용한다.

---

## 0. 프로젝트 정보

| 항목 | 값 |
|---|---|
| 엔진 | `C:\UnrealEngine-5.4.4` (UE 5.4.4 소스 빌드) |
| 프로젝트 | `EternalReturn.uproject` · 모듈 `EternalReturn` (Runtime) |
| 타겟 | Game / Editor / **Server** (데디케이티드 서버 있음) |
| 장르 | 탑다운 MOBA·배틀로얄 (이터널 리턴 유형) · 3인 1팀 × 8팀 |
| 주요 의존 | `GameplayAbilities` `GameplayTags` `GameplayTasks` `NavigationSystem` `AIModule` `Niagara` `EnhancedInput` `DeveloperSettings` |

**폴더** — `Source/EternalReturn/` 아래 기능별 분류(`Character/` `Combat/` `Component/` `Core/` `GAS/`).
새 파일은 기존 폴더 규칙을 따른다.

**네이밍**
- `ER` 프리픽스 + 언리얼 접두사: `AERCharacterBase`, `UERGameInstance`
- 컴포넌트는 `~Component`, 데이터 테이블 행은 `F~Row` + `FTableRowBase` 상속
- **주석과 커밋 메시지는 한국어**

**문서** — `Docs/` 아래. 서식은 [`Docs/00_문서작성규칙.md`](Docs/00_문서작성규칙.md).

| 폴더 | 용도 |
|---|---|
| `0_GameDesign/` | 역기획서 |
| `1_Task/` · `2_Checklist/` | 기능별 작업·체크리스트 (`F##_기능명/`) |
| `3_EditorTasks/` | 에디터에서 할 일 + PIE 테스트 |
| `4_Argument/` | 구현 방안 비교·결정 |
| `5_ErrorReport/` | 원인을 찾은 문제 |
| `6_Lyra참조/` | Lyra 소스 대조 기록 |

---

## 1. 코딩 전에 생각하기

**추측하지 말 것. 혼란을 숨기지 말 것. 트레이드오프를 드러낼 것.**

- 가정을 명시한다. 불확실하면 묻는다.
- 해석이 여러 개면 전부 제시한다 — 조용히 하나를 고르지 않는다.
- 더 단순한 방법이 있으면 말한다. 필요하면 반대 의견을 낸다.
- 불명확하면 멈춘다. 무엇이 헷갈리는지 이름 붙이고 묻는다.
- 직접 구현하기 전에 **엔진이 이미 제공하는지 먼저 확인한다** (`CharacterMovementComponent`, `UGameplayStatics`, `FTimerManager`, `UNavigationSystemV1`, `AIPerception`).
- 게임플레이 기능은 **네트워크 설계부터 말한다**: 어디가 서버 권위이고 무엇이 복제되는지.

### 1.1 작업 시작 전 합의 — 예외 없다

**코드를 쓰기 전에 아래 넷을 제시하고 승인받는다.**

1. **구조** — 어떤 클래스·컴포넌트를 만들고 각각 무슨 책임인지, 기존 코드 어디에 붙는지
2. **네트워크 설계** — 무엇이 서버 권위이고 무엇이 복제되는지 (§6)
3. **세부 구현 방식** — 자료구조, 데이터 스키마, 델리게이트·타이머
4. **검증 방법** — 무엇이 되면 성공인지 (§4)

사소한 작업(오타, 한 줄 버그 픽스, 로그 추가)은 예외다.

### 1.2 방안이 2개 이상이면 문서로 만들어 고르게 한다

채팅으로 설명하지 않는다. `Docs/4_Argument/` 에 비교 문서를 만들고,
채팅에는 **경로 + 방안 이름 한 줄씩 + 내 추천 1개**만 남긴다.

서식: [`Docs/00_문서작성규칙.md`](Docs/00_문서작성규칙.md) §1

---

## 2. 단순함 우선

**문제를 푸는 최소한의 코드. 추측성 코드 금지.**

- 요청받지 않은 기능·"유연성"·"확장성" 추가 금지
- 한 곳에서만 쓰는 코드에 추상화 금지. 인터페이스·베이스 클래스 남발 금지
- 일어날 수 없는 상황에 대한 예외 처리 금지
- 200줄 썼는데 50줄로 될 것 같으면 다시 쓴다
- `Tick`은 정말 매 프레임 필요할 때만. 주기적 처리는 `FTimerManager`, 이벤트성은 델리게이트. 켤 거면 `TickInterval` 검토
- 컴포넌트를 하나 더 만들기 전에 기존 컴포넌트에 들어가는 게 맞는지 본다

---

## 3. 수술적 변경

**꼭 필요한 것만 건드린다. 내가 만든 쓰레기만 치운다.**

- 인접 코드·주석·포맷을 "개선"하지 않는다. 망가지지 않은 것을 리팩터링하지 않는다
- 다르게 짰을 코드라도 기존 스타일에 맞춘다
- 관련 없는 죽은 코드는 **말만 한다** — 지우지 않는다
- 내 변경으로 생긴 미사용 include/변수/함수만 제거한다

기준: **변경된 모든 줄이 사용자의 요청으로 직접 추적되어야 한다.**

- 헤더에서는 **전방 선언**, include는 `.cpp` 로. `.generated.h` 는 항상 **마지막**
- 기존 `UPROPERTY` 의 순서·이름을 임의로 바꾸지 않는다 — 블루프린트/애셋 참조가 끊긴다
- 클래스·프로퍼티 이름 변경은 애셋을 깨뜨린다. 먼저 알리고 승인받는다

---

## 4. 검증은 "빌드"다

언리얼에는 단위 테스트가 거의 없다. **컴파일 성공이 1차 검증이고 생략할 수 없다.**

작업을 검증 가능한 목표로 바꾼다:
- "스킬 추가" → "빌드 통과 + 서버에서 클라로 복제되는 경로가 코드상 존재"
- "버그 수정" → "재현 조건을 먼저 말로 정의 → 수정 → 빌드 통과"

다단계 작업은 `1. [단계] → 검증: [확인 방법]` 형식으로 계획을 먼저 제시한다.

### 빌드 명령 (Claude가 직접 실행한다)

```bash
"C:/UnrealEngine-5.4.4/Engine/Build/BatchFiles/Build.bat" EternalReturnEditor Win64 Development \
  -Project="C:/Sung Unreal Project/Eternal Return/EternalReturn/EternalReturn.uproject" \
  -WaitMutex -NoHotReload
```

서버 타겟은 `EternalReturnServer` 로 바꾸고 `-NoHotReload` 를 뺀다.

### 빌드 규칙

- C++ 를 수정했으면 **보고 전에 반드시 빌드한다.** "빌드는 안 해봤습니다"로 끝내지 않는다
- 오래 걸리므로 **백그라운드 + 로그 파일 리다이렉트.** 파이프(`| tail`)로 넘기지 않는다 — 종료 전까지 아무것도 안 보인다
- 진행률은 UBT 로그의 `[n/총개수]`: `C:/UnrealEngine-5.4.4/Engine/Programs/UnrealBuildTool/Log.txt`
- ⚠ **액션이 수천 개면**(`[113/2453]`) 프로젝트가 아니라 **엔진 전체를 재빌드 중**이다. 몇 시간 걸리므로 사용자에게 알리고 계속할지 확인한다. 정상 빌드는 수십 개다
  - 흔한 원인: UBT 로그 앞부분의 `UnrealHeaderTool needs to run because ...`
- ⚠ **에디터가 열려 있으면 링크가 실패한다.** `Cannot open ... UnrealEditor-EternalReturn.dll` 이 나오면 **에디터를 닫아달라고 요청**한다. 임의로 프로세스를 죽이지 않는다
- 실패 시 에러를 그대로 인용하고 고친다. **실패를 성공으로 보고하지 않는다**
- 헤더에 `UPROPERTY`/`UFUNCTION`/새 `USTRUCT` 를 추가하면 **에디터 재시작이 필요할 수 있다** — 사용자에게 알린다

### 빌드가 통과하면 남길 문서 — 둘 다 필수

| 언제 | 어디에 |
|---|---|
| 원인을 찾은 문제를 고쳤으면 | `Docs/5_ErrorReport/` |
| 기능 작업이 끝났으면 | `Docs/3_EditorTasks/` |

⭐ **C++ 빌드가 통과했다고 기능이 끝난 게 아니다.** 에디터에서 손댈 것과 PIE 로 확인할 것을 문서로 남긴다.
서식: [`Docs/00_문서작성규칙.md`](Docs/00_문서작성규칙.md) §2 · §3

그리고 진행한 내용을 **해당 `Docs/2_Checklist/` 항목에 체크**한다.

---

## 5. 언리얼 필수 규칙

- **GC**: `UObject*` 멤버는 반드시 `UPROPERTY()`. 안 붙이면 dangling 된다
- **생성자**: `CreateDefaultSubobject` 는 생성자에서만. 생성자에서 게임 로직·월드 접근 금지
- **초기화 순서**: `BeginPlay` 보다 이른 시점에 다른 액터를 참조하지 않는다
- **애셋 참조**: 하드 포인터 대신 `TSoftObjectPtr` / `TSubclassOf` + `EditDefaultsOnly`
- **문자열**: 리터럴은 `TEXT("...")`. 로그는 `UE_LOG` (`AddOnScreenDebugMessage` 말고)
- **검증**: 회복 불가능한 실수는 `check()`, 런타임에 가능한 상황은 `ensure()` + 조기 반환. `nullptr` 체크 없이 역참조하지 않는다
- **캐스팅**: `Cast<T>()` 결과는 항상 검사. `static_cast` 로 UObject 캐스팅 금지

---

## 6. 멀티플레이 / 리플리케이션

**모든 게임플레이 코드는 네트워크를 전제로 작성한다.**

- **서버 권위**: 체력·데미지·스탯·상태이상 변경은 서버에서만. 값 변경 전에 `HasAuthority()` 로 가드
- `UPROPERTY(Replicated)` 를 추가하면 **반드시** `GetLifetimeReplicatedProps` 에 `DOREPLIFETIME`. 빠뜨리면 조용히 복제되지 않는다
- UI/연출 갱신이 필요한 값은 `ReplicatedUsing = OnRep_XXX`
- RPC: 입력은 `Server`(`WithValidation`), 연출은 `NetMulticast`, 본인 전용은 `Client`. RPC 에 무거운 로직 금지
- ⚠ **`Set<어트리뷰트>()` 류는 권위를 안 본다.** 쓰기 전에 그 함수가 서버 전용인지 확인한다 (`Docs/5_ErrorReport/E05`)
- **클라이언트 예측은 요청받았을 때만.** 먼저 서버 권위로 동작하게 만든다

---

## 7. C++ / 블루프린트 경계

**로직은 C++, 블루프린트는 데이터와 연출만.**

- 게임플레이 로직·수치 계산·상태 전이·네트워크 코드는 전부 C++
- 블루프린트: 메시/머티리얼/애님BP 지정, 나이아가라·사운드·카메라 연출, 위젯 바인딩, 데이터 지정
- 노출 규칙: 디자이너가 세팅 → `EditDefaultsOnly` / BP가 읽기만 → `BlueprintReadOnly` / **`BlueprintReadWrite` 는 기본적으로 안 쓴다**
- `BlueprintImplementableEvent` 는 **연출 훅**에만. 로직 분기를 BP 로 넘기지 않는다
- 새 `UFUNCTION` 을 BP 에 노출할 땐 왜 필요한지 한 줄로 근거를 댄다

---

## 8. GAS 기반 개발

**이 프로젝트는 GAS 위에서 만든다.** 근거: `Docs/4_Argument/1_스킬시스템_구조선택.md`
플러그인·모듈은 **이미 켜져 있다. 다시 켜지 않는다.**

### GAS 가 제공하는 것을 다시 만들지 않는다

| 하려는 것 | GAS에서 | 직접 만들지 마라 |
|---|---|---|
| 스탯 | `UAttributeSet` | ❌ `UStatComponent` |
| 스킬 1개 | `UGameplayAbility` 파생 | ❌ 자체 스킬 베이스 |
| 쿨다운 | Cooldown `UGameplayEffect` | ❌ `FTimerManager` 직접 |
| 코스트 | Cost `UGameplayEffect` | ❌ 수동 차감 |
| 데미지 공식 | `UGameplayEffectExecutionCalculation` **하나** | ❌ 스킬마다 계산 |
| 상태이상(CC) | `UGameplayEffect` + `GameplayTag` | ❌ 자체 상태 enum |
| CC로 시전 차단 | `ActivationBlockedTags` | ❌ 수동 플래그 검사 |
| 채널링·몽타주 대기 | `UAbilityTask` | ❌ 자체 타이머 상태 머신 |
| 조준 데이터 전송 | `FGameplayAbilityTargetData` | ❌ 자체 RPC 구조체 |
| 이펙트·사운드 | `GameplayCue` | ❌ 스킬 안에서 직접 재생 |

**직접 만드는 것** (GAS 가 안 해주는 영역): 판정 형상 계산(부채꼴·이중 반경·관통), 팀 시야,
클릭 이동·카메라, 인벤토리·제작·매치 타임라인.

### ⚠ 반드시 지킬 것

- **ASC 배치를 바꾸지 않는다.** 플레이어는 PlayerState, 야생동물은 Pawn
- **복제 모드**: 플레이어 `Mixed`, AI·야생동물 `Minimal`
  ⚠ `Minimal` 은 **소유자가 있는 ASC 에서 동작하지 않는다** — *"this does not work for Owned AbilitySystemComponents (Use Mixed instead)"* (`AbilitySystemComponent.h:87`)
- **`InitGlobalData()` 를 직접 호출하지 않는다.** UE 5.3+ 는 자동 호출한다 (`AbilitySystemGlobals.cpp:68`).
  ⚠ 인터넷 예제 대부분이 5.2 이전 기준이라 이걸 시킨다. **따라 하지 마라**
- **어트리뷰트 클램프는 `PreAttributeChange`** 에 — *"meant to enforce things like Health = Clamp(Health, 0, MaxHealth) and NOT things like trigger this extra thing if damage is applied"* (`AttributeSet.h:210-212`). 베이스까지 자르려면 `PreAttributeBaseChange` 도 (`:225`)
- **어빌리티 부여·발동은 서버 권위.** §6 이 그대로 적용된다
- **`GameplayTag` 를 코드에 문자열로 박지 않는다** — `Source/EternalReturn/GAS/ERGameplayTags.h` 에 선언한다
- ⚠ **GAS 는 없는 것을 조용히 건너뛴다** — 없는 어트리뷰트의 모디파이어는 경고 없이 `continue` 된다 (`GameplayEffect.cpp:4238`). 애셋을 읽는 쪽이 **직접 검사하고 로그를 남긴다**
- **GAS 예측을 지금 켜지 않는다.** 먼저 서버 권위로 정확히 동작시킨다

---

## 9. 건드리지 말 것

- ❌ **`Content/` 하위 애셋** (`.uasset`, `.umap`) — 생성·수정·삭제 절대 금지. 읽지도 않는다(바이너리). 에디터 작업은 `Docs/3_EditorTasks/` 로 넘긴다
- ❌ **`git commit` / `git push`** — 명시적 요청 전까지 하지 않는다. `git status`, `git diff` 는 자유
- ⚠ `Config/*.ini`, `EternalReturn.uproject`, `*.Build.cs`, `CLAUDE.md` — 수정 가능하지만 **변경 내용을 요약해서 보고**한다. 플러그인·모듈 추가는 이유를 함께
- `Binaries/` `Intermediate/` `Saved/` `DerivedDataCache/` — 빌드 산출물. 수정하지 않는다

---

## 10. 토큰 절약

- 엔진 소스를 통째로 읽지 않는다. `grep` 으로 필요한 함수만. 전체 헤더를 열기 전에 시그니처만으로 충분한지 먼저 판단한다
- 빌드 로그 전체를 붙여넣지 않는다. **파일명 + 라인 + 에러 코드**(`C2xxx`, `LNK2xxx`)만. UHT 에러는 대개 마지막 5~10줄에 원인이 있다
- 리플렉션 매크로의 `meta` 옵션은 **실제로 필요한 것만**. 옵션 나열 금지
- `.h`/`.cpp` 를 함께 고칠 때는 **변경된 부분만** 보여준다. 파일 전체 재출력 금지
- 블루프린트는 핵심 노드 흐름만 텍스트로: `OnComponentHit → CastToPlayer → ApplyDamage`
- **한 세션에서 하나의 기능/버그에만 집중한다**

---

## 11. Lyra 를 먼저 본다

Epic 공식 샘플이 `C:/UnrealEngine-5.4.4/Samples/Games/Lyra` 에 있다 (소스만, `Content/` 없음).
GAS 를 실제 규모로 쓴 유일한 1st-party 레퍼런스다.

**GAS 관련 구조를 정하기 전에 Lyra 가 어떻게 했는지 먼저 확인하고, 읽은 내용은 `Docs/6_Lyra참조/` 에 남긴다.**

⚠ **그대로 베끼지 않는다.** Lyra 는 팀 슈터고 **캐릭터별 스탯 차이가 없다**(어트리뷰트 6개).
우리는 실험체마다 다르다(33개). **"Lyra 가 그렇게 했으니까"는 근거가 아니다** —
그 *이유*가 우리한테도 성립하는지 확인하고, 다르면 왜 다른지 적는다.

전제 차이 정리: [`Docs/6_Lyra참조/00_개요.md`](Docs/6_Lyra참조/00_개요.md)

---

**이 지침이 작동하고 있다는 신호:** diff에 불필요한 변경이 줄고, 과설계로 인한 재작성이 줄고,
실수 이후가 아니라 구현 이전에 질문이 나온다. 그리고 "빌드 안 해봤다"는 보고가 사라진다.
