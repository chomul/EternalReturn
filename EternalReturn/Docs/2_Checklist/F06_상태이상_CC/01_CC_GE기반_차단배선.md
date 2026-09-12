# ✅ F06-01 — CC GE 기반 · 차단 배선 ⭐

> Task [`../../1_Task/F06_상태이상_CC/01_CC_GE기반_차단배선.md`](../../1_Task/F06_상태이상_CC/01_CC_GE기반_차단배선.md)

## 선행

- [x] F00 · F02 완료
- [x] ⚠ CC GE 를 애셋/네이티브 중 무엇으로 할지 `Docs/4_Argument/` 에서 **먼저** 정했다

## 구현

- [x] CC 를 `HasDuration` GE 로 표현한다  → ✅ GE_CC_Stun · Snare 확인. 코드가 HasDuration 을 **강제**한다 (ApplyCC ④)
- [x] GE 의 `GrantedTags` 에 `State.CC.*` 를 넣는다  → ✅ ER.CC.List 로 State.CC.Stun · State.CC.Snare 확인
- [ ] 어빌리티의 `ActivationBlockedTags` 에 그 태그를 넣는다  → ⏸ **F07** — 어빌리티가 0개라 걸 대상이 없다 · 태그(State.Block.Skill)는 준비됨
- [x] ⭐ **수동 해제 코드가 없다** — GE 만료 시 태그가 같이 사라진다
- [x] 태그를 `ERGameplayTags.h` 에서 참조한다 (문자열 X)

## 빌드

- [x] `EternalReturnEditor` 빌드 통과
- [x] `EternalReturnServer` 빌드 통과

## 검증 — `Play As Client, Number of Players: 2`

- [ ] ⭐ 서버에서 기절 GE 부여 → 클라의 이동 · 평타 · 스킬이 **전부** 막힌다  → ⚠ **이동만 확인됨** (Listen Server 2인). 평타·스킬은 ⏸ F07 (어빌리티 0개)
- [x] ⭐ **만료되면 자동 해제**된다  → ✅ 2초 뒤 태그가 사라지는 것을 확인
- [x] 클라에서 태그가 보인다  → ✅ Listen Server 2인에서 복제 확인
- [ ] GE 를 수동 제거해도 태그가 같이 사라진다  → ⚠ **확인 수단이 없다** — ER.CC.Clear 는 Loose 태그만 뗀다. ⏸ F07 에서 정화(F06-05)와 함께

## ⭐ 가장 위험한 실수

- [x] ⭐ **자체 상태 enum 을 만들지 않았다** (`CLAUDE.md` §8)
- [ ] ⭐ **`FTimerManager` 로 지속시간을 재지 않았다**  → ⚠ 진짜 CC 경로는 **0개**. 단 ERCCDebug.cpp 의 Loose 태그 해제에 1개 (임시 파일, F07 에서 삭제)
- [x] ⚠ CC 부여가 **서버 권위**다 — 클라가 자기에게 거는 경로가 없다

## 판단 기록

- [x] CC GE 를 애셋/네이티브 중 무엇으로 할지 정하고 근거를 남겼다
