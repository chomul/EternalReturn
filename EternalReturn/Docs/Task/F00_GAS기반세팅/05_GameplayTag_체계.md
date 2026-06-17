# F00-05 — GameplayTag 체계

> 기능 [F00 GAS 기반 세팅](00_개요.md) · 선행 [04 복제 모드 설정](04_복제모드_설정.md)
> 체크리스트 [`05_GameplayTag_체계.md`](../../Checklist/F00_GAS기반세팅/05_GameplayTag_체계.md)

## 할 일

태그 이름 규칙을 정하고, **C++에서 태그를 문자열로 박지 않는 방법**을 세운다.

## 왜 지금 하나

GAS에서 태그는 **CC 차단·상태 표현·이펙트 분류**를 전부 담당한다.
F06(상태이상)·F07(스킬)이 시작되면 태그가 수십 개로 늘어난다.
**규칙 없이 늘어난 뒤에 정리하면 전부 이름을 바꿔야 하고, 데이터 애셋이 깨진다.**

## 이 프로젝트가 실제로 필요로 하는 태그

[전투판정 문서](../../GameDesign/Systems/전투판정_상태이상_역기획서.md) §3의 CC 목록과
[스킬 문서](../../GameDesign/Systems/스킬_프레임워크_역기획서.md) §8의 형태 목록에서 나온다.

```
State.CC.Stun                 기절 — 이동·평타·스킬 전부 차단
State.CC.Snare                속박
State.CC.Silence              침묵 — 스킬만 차단
State.CC.Disarm               무장 해제 — 평타 + "평타 판정 스킬"까지 차단
State.CC.Slow                 둔화
State.CC.Blind                실명
State.CC.VisionBlock          시야 차단
State.Stealth                 은신
State.Invulnerable            무적
State.CCImmune                이동 방해 면역 (매그너스 R)

Ability.Form.Channeled        채널링 — CC로 중단됨
Ability.Form.NextAttackBuff   다음 평타 강화 (4곳 공유)
Ability.Slot.Q / W / E / R / D / P
```

⚠ **행동 강제 계열(공포·매혹·도발·광기)과 수면·에어본은 넣지 않는다.**
6인 중 요구하는 캐릭터가 없어 **검증할 방법이 없다** (전투 문서 §구현 우선순위 주석).
7번째 캐릭터가 올 때 추가한다.

## ⭐ 태그를 코드에 문자열로 박지 마라

```cpp
// ❌ 이렇게 하지 않는다 — 오타가 컴파일에 안 걸리고, 이름을 바꾸면 조용히 깨진다
FGameplayTag::RequestGameplayTag(FName("State.CC.Stun"));
```

`FNativeGameplayTag` 로 **C++에 선언**하거나, 태그를 한 헤더에 모아 상수로 노출한다.
어느 방식이든 원칙은 하나 — **문자열이 한 곳에만 존재한다.**

## 태그는 어디에 정의하나

| 방식 | 언제 |
|---|---|
| `Config/DefaultGameplayTags.ini` | 디자이너가 편집. **`Config` 변경은 보고 대상** (`CLAUDE.md` §9) |
| C++ `FNativeGameplayTag` | 코드가 직접 참조하는 태그 |

⭐ **이 프로젝트는 로직이 C++에 있다** (`CLAUDE.md` §7). CC·슬롯처럼 **코드가 참조하는 태그는 C++ 선언**이 맞다.

## 만들 파일

```
Source/EternalReturn/GAS/ERGameplayTags.h / .cpp    (네이티브 태그 선언)
```

## 검증

- **빌드 통과** (Editor + Server)
- 에디터 태그 목록에 선언한 태그가 보인다
- 코드에서 태그를 **문자열 없이** 참조할 수 있다
- 오타를 내면 **컴파일이 실패한다** (문자열이면 런타임까지 안 걸린다)

## 주의점

- ⭐ **태그 이름을 나중에 바꾸면 데이터 애셋(GameplayEffect 등)이 깨진다.** 처음에 신중히 정한다 — `CLAUDE.md` §3의 "이름 변경은 애셋을 깨뜨린다"와 같은 문제다.
- 계층을 얕게 유지한다. `State.CC.Stun` 정도면 충분하고, `State.Debuff.CC.Hard.Stun` 같은 건 과설계다 (`CLAUDE.md` §2).
- ⭐ **무장 해제는 "평타 판정 스킬"도 막아야 한다** (전투 문서 §3.3). 태그 하나로 두 종류를 막으려면 스킬 쪽에 `Ability.Form.NextAttackBuff` 같은 표시가 필요하다 — F07에서 다시 만난다.
- `Config/DefaultGameplayTags.ini` 를 건드리면 **변경 내용을 보고**한다.

## 미확인

| 항목 | 상태 |
|---|---|
| CC 태그의 정확한 차단 범위 | 전투 문서 §3 기준. 세부는 **(미확인)** |
| 태그 계층 깊이 | **자체 결정값** |
| 디자이너가 편집할 태그가 있는지 | **(미확인)** — 있으면 `.ini` 병행 |
