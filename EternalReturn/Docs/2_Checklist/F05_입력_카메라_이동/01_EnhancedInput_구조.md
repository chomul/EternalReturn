# ✅ F05-01 — EnhancedInput 컨텍스트 구조

> Task [`../../1_Task/F05_입력_카메라_이동/01_EnhancedInput_구조.md`](../../1_Task/F05_입력_카메라_이동/01_EnhancedInput_구조.md)

## 선행

- [x] F01 완료 (`AERPlayerController` 가 있다)

## 구현

- [x] `UERInputConfig : public UDataAsset` 정의
- [x] 액션을 **데이터 애셋 참조**로 든다 (문자열 아님)
- [x] 컨텍스트 분할 — ⚠ **2종만 만들었다** (`Default` / `Dead`)
  ⭐ `IMC_Practice` 는 **코드에서 켜는 경로를 아예 안 만들었다.**
  지금 만들어두면 텔레포트 치트 통로가 미리 열린다. 연습 모드를 만들 때 추가한다.
  `IMC_UI` 도 인벤토리·상점(F08/F17)이 생길 때 추가한다
- [x] 컨텍스트 우선순위를 정하고 주석에 남겼다
- [x] `AddMappingContext` 를 **로컬 컨트롤러에서만** 부른다

## 빌드

- [x] `EternalReturnEditor` 빌드 통과
- [x] `EternalReturnServer` 빌드 통과

## 검증

- [ ] 컨텍스트를 바꾸면 이전 입력이 **안 먹는다**  → ⏸ **F11(사망·부활)** 필요 — `Dead` 컨텍스트를 켤 경로가 아직 없다
- [x] ⚠ `IMC_Practice` 가 기본 상태에서 **꺼져 있다**
- [ ] 두 컨텍스트가 같은 키를 쓸 때 우선순위대로 동작한다  → ⏸ **F11(사망·부활)** 필요 — 위와 같은 이유

## ⭐ 가장 위험한 실수

- [x] ⚠⚠ **`IMC_Practice` 를 연습 모드 밖에서 켜지 않는다** — 텔레포트 치트가 된다
- [x] 서버에서 `AddMappingContext` 를 부르지 않는다 (로컬 전용)

## 에디터 작업

- [x] `IMC_*` / `IA_*` 애셋을 직접 만들지 않았다
- [x] `Docs/3_EditorTasks/` 로 넘겼다
