# ✅ F05-01 — EnhancedInput 컨텍스트 구조

> Task [`../../1_Task/F05_입력_카메라_이동/01_EnhancedInput_구조.md`](../../1_Task/F05_입력_카메라_이동/01_EnhancedInput_구조.md)

## 선행

- [ ] F01 완료 (`AERPlayerController` 가 있다)

## 구현

- [ ] `UERInputConfig : public UDataAsset` 정의
- [ ] 액션을 **데이터 애셋 참조**로 든다 (문자열 아님)
- [ ] 컨텍스트 4종 분할 (`Default` / `Dead` / `UI` / `Practice`)
- [ ] 컨텍스트 우선순위를 정하고 주석에 남겼다
- [ ] `AddMappingContext` 를 **로컬 컨트롤러에서만** 부른다

## 빌드

- [ ] `EternalReturnEditor` 빌드 통과
- [ ] `EternalReturnServer` 빌드 통과

## 검증

- [ ] 컨텍스트를 바꾸면 이전 입력이 **안 먹는다**
- [ ] ⚠ `IMC_Practice` 가 기본 상태에서 **꺼져 있다**
- [ ] 두 컨텍스트가 같은 키를 쓸 때 우선순위대로 동작한다

## ⭐ 가장 위험한 실수

- [ ] ⚠⚠ **`IMC_Practice` 를 연습 모드 밖에서 켜지 않는다** — 텔레포트 치트가 된다
- [ ] 서버에서 `AddMappingContext` 를 부르지 않는다 (로컬 전용)

## 에디터 작업

- [ ] `IMC_*` / `IA_*` 애셋을 직접 만들지 않았다
- [ ] `Docs/3_EditorTasks/` 로 넘겼다
