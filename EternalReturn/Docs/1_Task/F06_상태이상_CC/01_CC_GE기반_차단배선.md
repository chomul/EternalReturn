# F06-01 — CC GE 기반 · 차단 배선 ⭐

> 기능 [F06](00_개요.md) · 근거 [`전투판정_상태이상_역기획서.md`](../../0_GameDesign/Systems/전투판정_상태이상_역기획서.md) §3 · 우선순위 1
> 체크리스트 [`../../2_Checklist/F06_상태이상_CC/01_CC_GE기반_차단배선.md`](../../2_Checklist/F06_상태이상_CC/01_CC_GE기반_차단배선.md)

## 할 일

CC 하나를 **GE 로 표현**하고, 그 GE 가 붙은 동안 **태그가 살아 있게** 하고,
그 태그가 **어빌리티를 막게** 한다.

## 구조

```
CC GE (HasDuration)
   └─ GrantedTags: State.CC.Stun        ← 걸려 있는 동안 태그가 붙는다
          ↓
   어빌리티의 ActivationBlockedTags 에 State.CC.Stun
          ↓
   기절 중에는 어빌리티가 발동하지 않는다
```

⭐ **만료 시 자동으로 풀린다.** GE 가 사라지면 태그도 사라진다.
**수동 해제 코드를 만들지 않는다** — 만들면 언젠가 해제를 빠뜨려 영구 기절이 생긴다.

## ⚠ 결정할 것 — CC GE 를 애셋으로 두나 네이티브로 두나

| | 방안 | |
|---|---|---|
| A | **애셋** (`GE_CC_Stun` 등) | 기획자가 지속시간을 조정한다. C-6 결정과 일관 |
| B | **네이티브 클래스** | 코드로 만들어 검증기가 불필요 |

⚠ CC 는 **지속시간이라는 조정 대상 수치**가 있다. `UERLifestealEffect`(값 0개)와 다르다.
→ **A 가 유력하지만 방안이 2개라 `Docs/4_Argument/` 문서를 먼저 쓴다** (`CLAUDE.md` §1.2).

## 검증 — `Play As Client, Number of Players: 2`

- **빌드 통과** (Editor + Server)
- ⭐ 서버에서 기절 GE 를 부여하면 **클라의 이동 · 평타 · 스킬이 전부 막힌다**
- ⭐ **만료되면 자동 해제**된다
- 클라에서 `showdebug abilitysystem` 으로 태그가 보인다
- GE 를 수동 제거해도 태그가 같이 사라진다

## 주의점

- ⭐ **자체 상태 enum 을 만들지 않는다** (`CLAUDE.md` §8)
- ⭐ **`FTimerManager` 로 지속시간을 재지 않는다** — GE 의 Duration 이 한다
- ⚠ CC 부여는 **서버 권위**다. 클라가 자기에게 거는 경로가 없어야 한다
- 태그를 문자열로 박지 않는다 — `ERGameplayTags.h` 사용
