# F00 — GAS 기반 세팅

> Task [`../Task/F00_GAS기반세팅/00_개요.md`](../1_Task/F00_GAS기반세팅/00_개요.md)
> 체크리스트 [`../Checklist/F00_GAS기반세팅/00_기능_완료판정.md`](../2_Checklist/F00_GAS기반세팅/00_기능_완료판정.md)
> 빌드 상태: `EternalReturnEditor` ✅ / `EternalReturnServer` ✅ (2026-09-06)
> **① 에디터 작업은 2026-09-06 완료됨** (사용자 수행). ② 테스트 일부가 남아 있다.

## 목적

GAS 를 동작하는 상태로 세우고, **ASC 가 서버·클라 양쪽에 붙는지** 확인한다.
C++ 는 끝났지만 애셋이 삭제된 옛 클래스를 부모로 참조하고 있어서, 그대로 PIE 를 켜면
새로 만든 `AERGameMode` / `AERPlayerState` / `AERCharacterBase` 가 쓰이지 않는다.

## 선행 조건

- [x] `EternalReturnEditor` 빌드 통과
- [x] 에디터를 **새로 실행**한다 (`UPROPERTY`·새 클래스가 추가됐다)
- [x] 첫 실행은 GAS 플러그인 모듈·셰이더 컴파일로 느리다 — 정상이다

---

# ① 에디터 작업

> 애셋·설정을 **실제로 바꾸는** 일. **2026-09-06 완료됨.**

## 1-1. GAS 플러그인 확인

- [x] **Edit > Plugins** → 검색 `Gameplay Abilities` → 체크박스가 **켜져 있다**

> `EternalReturn.uproject` 에 이미 넣었으므로 켜져 있어야 정상이다. **직접 켜거나 끄지 않는다.**

## 1-2. 게임모드 애셋 배선

옛 `BP_RGameMode` 는 부모 클래스가 삭제된 상태였다. 새 BP 로 교체했다.

- [x] `/Game/Core/BP_ERGameMode` 가 **`AERGameMode`** 를 부모로 한다
- [x] **Edit > Project Settings > Project > Maps & Modes > Default GameMode** 가 이 BP 를 가리킨다
- [x] 결과: `Config/DefaultEngine.ini:4` → `GlobalDefaultGameMode=/Game/Core/BP_ERGameMode.BP_ERGameMode_C`

## 1-3. 캐릭터 애셋 배선

- [x] `/Game/ERCharacter/Blueprints/BP_ERCharacterBase` 가 **`AERCharacterBase`** 를 부모로 한다
      (PIE 로그의 `Avatar=BP_ERCharacterBase_C_*` 로 확인됨)

## 1-4. ⛔ 하지 않은 것 — 데이터 테이블

아래 4개는 행 구조체가 삭제된 채 남아 있다. **지금 손대지 않는다.**
새 행 구조체가 아직 없어서, 지금 재지정하면 입력해둔 값이 날아간다.

| 애셋 | 옛 행 구조체 | 언제 |
|---|---|---|
| `/Game/DataTable/DT_CharStat` | `FCharStatRow` | F02 |
| `/Game/DataTable/DT_CharLevelStat` | `FCharLevelStatRow` | F10 |
| `/Game/DataTable/DT_WeaponLevelStat` | `FWeaponLevelStatRow` | F11 |
| `/Game/DataTable/DT_MonsterLevelStat` | `FMonsterLevelStatRow` | F12 |

---

# ② 테스트

> PIE 로 **확인만** 한다.

## 2-1. ASC 초기화 — 서버·클라 양쪽 (⭐ F00 의 핵심 판정)

맵: **`/Game/ERCharacter/Maps/TopDownMap`**

| 설정 | 값 |
|---|---|
| **Net Mode** | **Play As Client** |
| **Number of Players** | **2** |

- [x] **Window > Output Log** → `LogEternalReturn` 필터
- [x] 아래 두 종류가 **모두** 보인다

```
[GAS] InitAbilityActorInfo — Server / Owner=ERPlayerState_0 / Avatar=BP_ERCharacterBase_C_0
[GAS] InitAbilityActorInfo — Client / Owner=ERPlayerState_0 / Avatar=BP_ERCharacterBase_C_0
```

- [x] `Owner=` 는 **PlayerState**, `Avatar=` 는 **캐릭터**다 (반대면 인자 순서가 틀렸다)

| 보이는 것 | 뜻 |
|---|---|
| Server + Client 둘 다 | ✅ 정상 |
| **Server 만** | ❌ `OnRep_PlayerState` 문제 |
| **Client 만** | ❌ `PossessedBy` 문제 |
| **아무것도 없음** | ❌ 게임모드가 `AERGameMode` 가 아니다 (1-2 확인) |

> **2026-09-06 결과: 통과.** Server 2줄 + Client 4줄, 12줄 전부 `Owner`=PlayerState / `Avatar`=Character.
> Client 가 4줄인 것은 각 클라 월드에 자기 폰 + 상대 폰이 둘 다 있어서 정상이다.

## 2-2. 게임플레이 태그 목록

- [ ] **Edit > Project Settings > Project > GameplayTags > Manage Gameplay Tags**
- [ ] 아래 태그가 목록에 보인다 (C++ 네이티브 태그)

| 확인할 태그 |
|---|
| `State.CC.Stun` |
| `State.Stealth` |
| `Ability.Slot.Q` |
| `Damage.Type.BasicAttack` |

> ⚠ **여기서 태그를 직접 추가하지 않는다.** 코드가 소유한다. 에디터에서 만들면 이름이 두 곳에 생긴다.
> 안 보이면 빌드가 반영되지 않은 것 — 에디터를 닫고 재빌드한다.

## 2-3. GameplayEffect 애셋을 만들 수 있는지

- [ ] 콘텐츠 브라우저 우클릭 → **Blueprint Class** → 부모로 **`GameplayEffect`** 를 고를 수 있다
- [ ] ⛔ **실제로 만들지는 않는다.** 만드는 것은 F02 의 일이다

## 2-4. ⏸ 지금은 확인할 수 없는 것

기능이 아직 없어서 검증이 불가능하다. **해당 기능에서 확인한다.**

| 항목 | 언제 |
|---|---|
| 체력 변화가 즉시 반영 (`NetUpdateFrequency` 효과) | 어트리뷰트가 없다 → **F02** |
| `Mixed` 복제 모드 동작 (소유 클라 전체 / 타 클라 최소) | GE 가 없다 → **F02 이후** |
| 태그를 문자열 없이 코드에서 참조 | 사용처가 없다 → **F03 / F06** |
| 부활 후 Avatar 재지정 | 부활 시스템 → **F14** |
| `NetUpdateFrequency` 적정값 실측 | 봇 24명 필요 |

---

## 주의점

- ⚠ 부모 클래스를 재지정하면 그 BP 의 기존 노드가 끊어질 수 있다. 끊어진 노드는 지운다 — 옛 로직은 GAS 로 다시 만든다.
- ⚠ 데이터 테이블 4개는 건드리지 않는다 (1-4).
- ⚠ 게임플레이 태그를 에디터에서 추가하지 않는다.
- ⚠ 에디터가 열려 있으면 다음 빌드의 링크가 실패한다. 빌드 전에 **닫는다.**

## 끝나면

[`../Checklist/F00_GAS기반세팅/00_기능_완료판정.md`](../2_Checklist/F00_GAS기반세팅/00_기능_완료판정.md) 갱신.
**2-1 은 완료됨.** 남은 것은 2-2 / 2-3 뿐이다.
