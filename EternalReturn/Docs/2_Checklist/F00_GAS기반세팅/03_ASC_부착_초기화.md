# ✅ F00-03 — ASC 부착 · 초기화

> Task [`../../Task/F00_GAS기반세팅/03_ASC_부착_초기화.md`](../../1_Task/F00_GAS기반세팅/03_ASC_부착_초기화.md)

## 선행

- [x] [02 ASC 배치 결정](02_ASC_배치결정.md) 완료
- [x] 클라 2대로 PIE를 띄울 수 있다

## 구현

- [x] `AERPlayerState` 가 `IAbilitySystemInterface` 를 구현한다
- [x] ASC를 **생성자에서** `CreateDefaultSubobject` 로 만든다
- [x] ⭐ ASC 멤버에 **`UPROPERTY()`** 가 붙어 있다
- [x] `SetIsReplicated(true)`
- [x] `GetAbilitySystemComponent()` 오버라이드
- [x] ⭐ **서버**: `APawn::PossessedBy()` 에서 `InitAbilityActorInfo(PlayerState, this)`
- [x] ⭐ **클라**: `APawn::OnRep_PlayerState()` 에서 같은 호출
- [x] 부활로 Pawn이 새로 생기면 **다시 부른다**
- [x] ⭐ **`InitGlobalData()` 를 호출하지 않았다**

## 빌드

- [x] `EternalReturnEditor` 빌드 통과
- [x] `EternalReturnServer` 빌드 통과

## 검증 — `Play As Client, Number of Players: 2`

- [x] ⭐ **서버**에서 `GetAbilitySystemComponent()` 가 유효
- [x] ⭐ **클라**에서도 유효 — *"서버에선 되는데 클라에선 안 된다"* 가 없다
- [x] `AbilityActorInfo->AvatarActor` 가 캐릭터를 가리킨다 (양쪽)
- [x] `AbilityActorInfo->OwnerActor` 가 PlayerState를 가리킨다 (양쪽)
- [ ] 부활 후 Avatar가 **새 Pawn** 을 가리킨다

## ⭐ 가장 흔한 GAS 버그 — 걸렸는지 확인

- [x] **`OnRep_PlayerState` 를 빠뜨리지 않았다** — 빠뜨리면 클라에서 어빌리티가 조용히 안 된다
- [x] **`PossessedBy` 를 빠뜨리지 않았다** — 빠뜨리면 서버에서 안 된다
- [x] `InitAbilityActorInfo` 의 인자 순서가 `(Owner=PlayerState, Avatar=Pawn)` 이다

## 흔한 실수 — 걸렸는지 확인

- [x] `UPROPERTY()` 누락으로 GC dangling 위험이 없다
- [x] 생성자에 게임 로직·월드 접근이 없다
- [x] `GetAbilitySystemComponent()` 가 `nullptr` 을 반환할 수 있는 시점이 있음을 호출부가 감안한다
- [x] 야생동물도 **같은 인터페이스**를 구현하도록 계획했다 (F12)

## 막혔을 때

| 증상 | 확인할 것 |
|---|---|
| 클라에서 어빌리티가 안 나감 | `OnRep_PlayerState` 에서 `InitAbilityActorInfo` 를 부르는가 |
| ASC가 `nullptr` | 초기화 전 호출. 시점을 늦춘다 |
| 부활 후 어빌리티가 안 먹음 | Avatar가 옛 Pawn을 가리킨다. 재초기화 필요 |

---

## 실행 기록 (2026-09-06)

**만든 파일**

```
Source/EternalReturn/Core/ERPlayerState.h / .cpp        (ASC 소유 + 인터페이스)
Source/EternalReturn/Character/ERCharacterBase.h / .cpp (InitAbilityActorInfo 서버·클라)
Source/EternalReturn/Core/ERGameMode.h / .cpp           (검증용 배선)
```

- `AERCharacterBase` **도** `IAbilitySystemInterface` 를 구현해 PlayerState 의 ASC 를 넘긴다.
  호출부가 `Cast<AERPlayerState>` 를 하지 않아도 되고, 야생동물(Pawn 에 ASC)이 같은 모양으로 붙는다.
- `InitAbilityActorInfo()` 는 PlayerState/ASC 가 아직 없으면 **조용히 반환**한다.
  `PossessedBy` 시점에 PlayerState 가 없을 수 있고, 그 경우 `OnRep_PlayerState` 에서 다시 불린다.
- 서버/클라 어느 쪽에서 불렸는지 `UE_LOG(LogEternalReturn)` 으로 남긴다 → PIE 검증에 쓴다.
- `InitGlobalData()` 호출 없음 — `grep -rn "InitGlobalData" Source/` 결과 0건.

⬜ **PIE 검증 4건은 아직 못 했다.** `Config/DefaultEngine.ini:4` 의 `GlobalDefaultGameMode` 가
부모 클래스가 삭제된 `BP_RGameMode` 를 가리켜서, 그대로 PIE 를 켜면 `AERPlayerState` 가 안 쓰인다.
→ `Docs/3_EditorTasks/` 작업 후 확인한다.

---

## PIE 검증 결과 (2026-09-06 14:34, `Saved/Logs/EternalReturn.log`)

```
Server / Owner=ERPlayerState_0 / Avatar=BP_ERCharacterBase_C_0
Client / Owner=ERPlayerState_0 / Avatar=BP_ERCharacterBase_C_0
Server / Owner=ERPlayerState_1 / Avatar=BP_ERCharacterBase_C_1
Client / Owner=ERPlayerState_1 / Avatar=BP_ERCharacterBase_C_1
Client / Owner=ERPlayerState_0 / Avatar=BP_ERCharacterBase_C_1
Client / Owner=ERPlayerState_1 / Avatar=BP_ERCharacterBase_C_0
```

✅ **Server 2줄 + Client 4줄.** 서버·클라 양쪽 경로가 모두 탄다 — 가장 흔한 GAS 버그를 피했다.
✅ **12줄 전부 `Owner` = PlayerState, `Avatar` = Character.** 인자 순서가 맞다.

**Client 4줄인 이유**: `Play As Client / Players 2` 는 서버 월드 1 + 클라 월드 2 를 만든다.
각 클라 월드에 **자기 폰 + 상대 폰**이 있고 둘 다 `OnRep_PlayerState` 를 타므로 2 × 2 = 4 다.

⚠ 마지막 2줄이 교차(`PS_0 ↔ C_1`)로 보이는 것은 **PIE 월드마다 오브젝트 이름을 독립적으로 매기기 때문**으로 판단된다.
같은 월드 안에서 짝이 어긋난 게 아니다. 다만 로그에 월드 식별자가 없어 **단정하지 않는다** — 필요하면 월드 이름을 로그에 추가해 재확인한다.

⬜ **부활 후 Avatar 재지정은 아직 검증 못 했다** — 부활 시스템(F14)이 없어 폰이 재생성되지 않는다.
