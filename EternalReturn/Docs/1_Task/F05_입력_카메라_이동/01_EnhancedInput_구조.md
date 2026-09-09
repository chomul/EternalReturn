# F05-01 — EnhancedInput 컨텍스트 구조

> 기능 [F05](00_개요.md) · 근거 [`입력_카메라_조작_역기획서.md`](../../0_GameDesign/Systems/입력_카메라_조작_역기획서.md) §8
> 체크리스트 [`../../2_Checklist/F05_입력_카메라_이동/01_EnhancedInput_구조.md`](../../2_Checklist/F05_입력_카메라_이동/01_EnhancedInput_구조.md)

## 할 일

입력 액션을 **컨텍스트로 분할**하고, 코드가 액션을 이름이 아니라 **데이터 애셋**으로 참조하게 한다.

## ⭐ 컨텍스트를 나누는 이유

한 컨텍스트에 전부 넣으면 **상황별로 입력을 막을 수 없다.**
사망 중에 이동 입력이 살아 있거나, 상점 UI 에서 스킬이 나가는 문제가 생긴다.

역기획서 §8.1 의 분할:

| 컨텍스트 | 켜지는 때 |
|---|---|
| `IMC_Default` | 평소 |
| `IMC_Dead` | 사망 · 관전 |
| `IMC_UI` | 인벤토리 · 상점 |
| `IMC_Practice` | ⚠ **연습 모드 전용** |

⚠⚠ **`IMC_Practice` 를 연습 모드 밖에서 켜면 안 된다.**
즉시 이동(`Ctrl+Alt+우클릭`)이 들어 있어 **텔레포트 치트가 된다** (§10).

## 액션을 데이터 애셋으로 묶는다

```cpp
UCLASS(BlueprintType, Const)
class UERInputConfig : public UDataAsset
{
    UPROPERTY(EditDefaultsOnly) TObjectPtr<UInputAction> Move;
    UPROPERTY(EditDefaultsOnly) TObjectPtr<UInputAction> CameraLock;
    // 스킬 슬롯은 태그로 찾는다 (F07 에서 연결)
};
```

⭐ Lyra 의 `ULyraInputConfig` 와 같은 형태다 (`UDataAsset`, `LyraInputConfig.h:39`).
근거: [`../../6_Lyra참조/03_데이터애셋_구성.md`](../../6_Lyra참조/03_데이터애셋_구성.md)

## 만들 파일

```
Source/EternalReturn/Character/ERInputConfig.h
```

⚠ `IMC_*` / `IA_*` **애셋 생성은 `Content/` 라 하지 않는다.** `Docs/3_EditorTasks/` 로 넘긴다.

## 검증

- **빌드 통과** (Editor + Server)
- 컨텍스트를 바꾸면 이전 컨텍스트의 입력이 **안 먹는다**
- `IMC_Practice` 가 기본 상태에서 **꺼져 있다**

## 주의점

- ⭐ 액션을 코드에서 **문자열로 찾지 않는다.** 데이터 애셋 참조로 한다
- 컨텍스트 우선순위(Priority)를 정하고 주석에 남긴다 — 겹치면 높은 쪽이 이긴다
- `AddMappingContext` / `RemoveMappingContext` 는 **로컬 컨트롤러에서만** 부른다
