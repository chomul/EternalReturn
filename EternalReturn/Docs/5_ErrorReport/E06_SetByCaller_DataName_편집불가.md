# E06 — SetByCaller 의 `Data Name` 이 에디터에서 `None` 에서 안 바뀜

| | |
|---|---|
| 발견 | ⭐ **사용자 지적** — 에디터 작업 중 |
| 시점 | F02-05 의 `GE_ERInitStats` 애셋을 만드는 중 |
| 상태 | ✅ 해결 |
| 분류 | **내가 쓴 에디터 작업 문서가 틀렸다** |

## 증상

사용자 보고:

> ⚠ Data Tag 가 아니라 Data Name 인데 Data 이름이 None 에서 안 바뀌는데 이건 어떻게해

`GE_ERInitStats` 의 모디파이어에서
`Modifier Magnitude > Magnitude Calculation Type = Set By Caller` 까지는 되는데,
그 아래 **`Data Name` 필드가 `None` 인 채 편집이 되지 않는다.**

`Docs/3_EditorTasks/F02_어트리뷰트셋.md` 가 *"`Data Tag` 가 아니라 `Data Name` 이다"* 라고
지시하고 있었다. **그 지시가 틀렸다.**

## 원인

`Data Name` 은 **읽기 전용 프로퍼티**다.

```cpp
// GameplayEffect.h:252-258
/** The Name the caller (code or blueprint) will use to set this magnitude by. */
UPROPERTY(VisibleDefaultsOnly, Category=SetByCaller)
FName	DataName;

UPROPERTY(EditDefaultsOnly, Category = SetByCaller, meta = (Categories = "SetByCaller"))
FGameplayTag DataTag;
```

- `DataName` → **`VisibleDefaultsOnly`** = 디테일 패널에 **보이지만 편집은 안 된다**
- `DataTag` → `EditDefaultsOnly` = 편집 가능

즉 **애셋에서 SetByCaller 키를 지정하는 방법은 태그뿐이다.**
`FName` 경로는 C++ 이나 블루프린트가 GE 를 코드로 만들 때만 쓸 수 있다.

### ⭐ 게다가 FName 경로는 deprecated 다

```cpp
// GameplayEffect.cpp:1120-1133
case EGameplayEffectMagnitudeCalculation::SetByCaller:
{
    if (SetByCallerMagnitude.DataTag.IsValid())
    {
        OutCalculatedMagnitude = InRelevantSpec.GetSetByCallerMagnitude(SetByCallerMagnitude.DataTag, ...);
    }
    else
    {
        PRAGMA_DISABLE_DEPRECATION_WARNINGS
        OutCalculatedMagnitude = InRelevantSpec.GetSetByCallerMagnitude(SetByCallerMagnitude.DataName, ...);
        PRAGMA_ENABLE_DEPRECATION_WARNINGS
    }
}
```

`DataTag` 가 유효하면 태그를 쓰고, 아니면 이름으로 떨어지는데
**그 이름 경로는 deprecation 경고를 끄고 부른다.**

### 왜 내가 틀렸나

`GameplayEffect.h:1063-1066` 에서 두 오버로드가 **둘 다 존재하고 둘 다 deprecated 표시가 없는 것**을 보고
"둘 다 쓸 수 있다"고 판단했다.

```cpp
void SetSetByCallerMagnitude(FName DataName, float Magnitude);
void SetSetByCallerMagnitude(FGameplayTag DataTag, float Magnitude);
```

⚠ **함수 시그니처만 보고 판단했고, 프로퍼티의 `UPROPERTY` 지정자를 확인하지 않았다.**
C++ 에서 호출 가능한 것과 **에디터에서 설정 가능한 것은 다른 문제**인데 그걸 구분하지 않았다.

## 해결

전부 `FGameplayTag` 경로로 바꿨다.

### ① 태그 33개를 네이티브로 선언 (`GAS/ERGameplayTags.h/.cpp`)

```cpp
UE_DEFINE_GAMEPLAY_TAG(SetByCaller_MaxHP, "SetByCaller.MaxHP");
// ... 33개
```

⚠ **접두사가 반드시 `SetByCaller.` 여야 한다.**
`DataTag` 의 `meta = (Categories = "SetByCaller")` 가
**에디터 태그 선택기를 그 아래로만 거르기** 때문이다 (`GameplayEffect.h:257`).
다른 이름으로 지으면 선택기에 아예 안 나타난다.

### ② 적용부 (`ERAttributeInit.cpp`)

```cpp
TArray<TPair<FGameplayTag, float>> Magnitudes;   // 이전: TPair<FName, float>
Add(ERTags::SetByCaller_MaxHP, Row.MaxHP);       // 이전: Add(TEXT("MaxHP"), ...)
```

⭐ 문자열이 사라져서 **오타가 컴파일 에러**가 된다. `CLAUDE.md` §8 규칙과도 맞는다.

### ③ 검사기 (`ERAttributeInit::ValidateInitEffect`)

```cpp
// 이전: GetSetByCallerDataNameIfPossible(DataName)
const FGameplayTag& DataTag = Mod.ModifierMagnitude.GetSetByCallerFloat().DataTag;
```

### ④ 에디터 작업 문서

`Docs/3_EditorTasks/F02_어트리뷰트셋.md` 의 33줄 표를 `Data Name` → **`Data Tag`** 로 교체.

## 재발 방지

⭐ **에디터에서 설정할 프로퍼티는 `UPROPERTY` 지정자를 반드시 확인한다.**

| 지정자 | 에디터에서 |
|---|---|
| `EditDefaultsOnly` / `EditAnywhere` | ✅ 편집 가능 |
| `VisibleDefaultsOnly` / `VisibleAnywhere` | ❌ **보이지만 편집 불가** |
| 지정자 없음 | ❌ 아예 안 보임 |

**C++ 에 세터가 있다는 것은 에디터에서 설정 가능하다는 뜻이 아니다.**
`Docs/3_EditorTasks/` 문서에 *"이 값을 넣으세요"* 라고 쓰기 전에
**그 프로퍼티가 정말 편집 가능한지 헤더에서 확인한다.**

그리고 `meta = (Categories = "...")` 가 붙은 태그 프로퍼티는
**그 접두사 아래 태그만 선택기에 뜬다.** 태그 이름을 지을 때 먼저 본다.

⚠ **이번엔 사용자가 막혀서 물어봤기 때문에 발견됐다.**
만약 `Data Name` 이 편집 가능했다면 값이 안 들어가도 검사기가 잡았겠지만,
**애초에 입력이 불가능한 절차를 문서에 적은 것**은 검사기로 못 막는다.
막는 방법은 위의 `UPROPERTY` 확인뿐이다.
