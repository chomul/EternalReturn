# ✅ F06-01 — CC GE 기반 · 차단 배선 ⭐

> Task [`../../1_Task/F06_상태이상_CC/01_CC_GE기반_차단배선.md`](../../1_Task/F06_상태이상_CC/01_CC_GE기반_차단배선.md)

## 선행

- [ ] F00 · F02 완료
- [ ] ⚠ CC GE 를 애셋/네이티브 중 무엇으로 할지 `Docs/4_Argument/` 에서 **먼저** 정했다

## 구현

- [ ] CC 를 `HasDuration` GE 로 표현한다
- [ ] GE 의 `GrantedTags` 에 `State.CC.*` 를 넣는다
- [ ] 어빌리티의 `ActivationBlockedTags` 에 그 태그를 넣는다
- [ ] ⭐ **수동 해제 코드가 없다** — GE 만료 시 태그가 같이 사라진다
- [ ] 태그를 `ERGameplayTags.h` 에서 참조한다 (문자열 X)

## 빌드

- [ ] `EternalReturnEditor` 빌드 통과
- [ ] `EternalReturnServer` 빌드 통과

## 검증 — `Play As Client, Number of Players: 2`

- [ ] ⭐ 서버에서 기절 GE 부여 → 클라의 이동 · 평타 · 스킬이 **전부** 막힌다
- [ ] ⭐ **만료되면 자동 해제**된다
- [ ] 클라에서 `showdebug abilitysystem` 으로 태그가 보인다
- [ ] GE 를 수동 제거해도 태그가 같이 사라진다

## ⭐ 가장 위험한 실수

- [ ] ⭐ **자체 상태 enum 을 만들지 않았다** (`CLAUDE.md` §8)
- [ ] ⭐ **`FTimerManager` 로 지속시간을 재지 않았다**
- [ ] ⚠ CC 부여가 **서버 권위**다 — 클라가 자기에게 거는 경로가 없다

## 판단 기록

- [ ] CC GE 를 애셋/네이티브 중 무엇으로 할지 정하고 근거를 남겼다
