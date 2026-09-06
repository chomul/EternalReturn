# F01 — 코어 골격 · 팀 ID

> Task [`../Task/F01_코어골격_팀ID/00_개요.md`](../Task/F01_코어골격_팀ID/00_개요.md)
> 체크리스트 [`../Checklist/F01_코어골격_팀ID/00_기능_완료판정.md`](../Checklist/F01_코어골격_팀ID/00_기능_완료판정.md)
> 빌드 상태: `EternalReturnEditor` ✅ / `EternalReturnServer` ✅ (2026-09-06)

## 목적

접속한 플레이어가 **서버에서 팀에 배정되고, 그 팀 번호가 모든 클라이언트에 복제되는지** 확인한다.
끝나면 F01 통합 검증 5건이 채워지고, 이후 모든 기능(시야·아군 오사·UI)이 팀을 신뢰하고 쓸 수 있다.

## 선행 조건

- [x] ⭐ **에디터를 재시작한다.** 헤더에 `UPROPERTY` 를 새로 추가했다(`TeamId`, `TeamSize`, `TeamCount`).
      이전 세션이 떠 있으면 새 프로퍼티가 Details 패널에 안 보인다
- [x] [F00 에디터 작업](F00_GAS기반세팅.md)이 끝나 있다 — `GlobalDefaultGameMode` 가 `BP_ERGameMode` 를 가리켜야 한다

---

# ① 에디터 작업

> 애셋·설정을 **실제로 바꾸는** 일. 저장되고 남는다.

## 1-1. `BP_ERGameMode` 에 새 프로퍼티가 보이는지 확인

애셋: **`/Game/Core/BP_ERGameMode`**

- [x] 애셋을 열고 툴바 **Class Defaults** 클릭
- [x] Details 패널에 **`Match`** 카테고리가 새로 생겼는지 확인
- [x] 그 아래 두 프로퍼티가 보이는지 확인

| 에디터 표시 이름 | 기본값 | 뜻 |
|---|---|---|
| **Team Size** | `3` | 한 팀의 인원 |
| **Team Count** | `8` | 팀 개수 |

> 안 보이면 에디터를 닫고 재빌드 후 다시 연다. C++ 빌드가 반영되지 않은 것이다.

## 1-2. 테스트용으로 `Team Size` 를 2로 내린다

- [x] **Team Size** 를 **`3` → `2`** 로 변경
- [x] **Compile** → **Save**

> ⚠ **왜 바꾸나:** PIE 는 4명으로 테스트한다. `Team Size = 3` 이면 4명 중 3명이 팀 0,
> 1명이 팀 1에 들어가서 **팀이 고르게 갈리는지 확인할 수 없다.** 2로 두면 2팀 × 2명이 된다.
>
> ⚠ **테스트가 끝나면 3으로 되돌린다.** 실제 매치는 3인 × 8팀 = 24명이다.

## 1-3. 되돌리기 (테스트 후)

- [ ] 테스트를 마쳤으면 **Team Size** 를 **`3`** 으로 복구하고 저장

---

# ② 테스트

> PIE 로 **확인만** 한다. 아무것도 바꾸지 않는다.

## 2-1. PIE 설정

맵: **`/Game/ERCharacter/Maps/TopDownMap`**

- [x] 툴바 **Play** 옆 드롭다운 → **Advanced Settings**

| 설정 | 값 |
|---|---|
| **Net Mode** | **Play As Client** |
| **Number of Players** | **4** |

- [x] **Play** 실행
- [x] **Window > Output Log** 를 열고 `LogEternalReturn` 으로 필터

## 2-2. 팀 배정 — 로그 확인

- [x] 아래 **4줄이 전부** 보인다 (PlayerState 번호 순서는 달라도 된다)

```
[Team] ERPlayerState_0 -> Team 0 (1/2)
[Team] ERPlayerState_1 -> Team 0 (2/2)
[Team] ERPlayerState_2 -> Team 1 (1/2)
[Team] ERPlayerState_3 -> Team 1 (2/2)
```

- [x] **팀 0에 2명, 팀 1에 2명**이다 (괄호 안 `(n/2)` 로 확인)
- [x] `[Team] 정원 초과` 경고가 **없다**
- [x] `[Team] PostLogin — PlayerState/GameState 없음` 경고가 **없다**

| 보이는 것 | 뜻 |
|---|---|
| 4줄이 팀 0·1로 2명씩 | ✅ 정상 |
| **전원이 Team 0** | ❌ `Team Size` 를 2로 안 바꿨다 (1-2 확인) |
| **로그가 하나도 없음** | ❌ 게임모드가 `AERGameMode` 가 아니다. [F00 문서](F00_GAS기반세팅.md) 3-3 확인 |
| `정원 초과` 경고 | ❌ `Team Count` 가 1이거나 플레이어가 정원보다 많다 |

## 2-3. 데이터값이 실제로 동작하는지

- [x] PIE 를 끄고 **Team Size** 를 **`1`** 로 바꾼 뒤 다시 실행
- [x] **4팀(Team 0·1·2·3)** 으로 나뉜다 → 상수로 박히지 않았다는 증거
- [ ] 확인 후 **`2`** 로 되돌린다

## 2-4. 스폰 클래스 확인

- [x] Output Log 에서 `[GAS] InitAbilityActorInfo` 줄의 `Avatar=` 값이 **`BP_ERCharacterBase_C_*`** 다
- [x] 4명이 전부 스폰됐다 (Server 4줄 + Client 다수)

## 2-5. 서버 권위 확인

- [x] Output Log 에서 팀 배정 로그가 **서버에서만** 찍힌다
      (`[Team]` 로그는 `AERGameMode` 에서만 나오고, GameMode 는 서버에만 존재한다)

> ⚠ **"클라에서 `TeamId` 를 조작해도 서버 값이 안 변한다"** 는 지금 확인할 수단이 없다.
> 클라가 값을 바꾸는 코드 경로 자체가 없기 때문이다(치트 콘솔도 아직 없음).
> **코드상 경로가 없다는 것으로 대신하고, 체크리스트에는 미확인으로 남긴다.**

---

## 주의점

- ⚠ **`Team Size` 를 3으로 되돌리는 것을 잊지 않는다.** 테스트값이 그대로 커밋되면 실제 매치가 2인 팀이 된다.
- ⚠ 로그의 `ERPlayerState_N` 번호는 **PIE 월드마다 독립적으로 매겨진다.** 번호가 순서대로가 아니어도 정상이다.
  중요한 건 **팀당 인원수**다.
- ⚠ 팀 색 표시·아군 구분 UI 는 아직 없다. **로그로만 확인한다.** 화면에는 아무 변화가 없는 것이 정상이다.

## 끝나면

[`../Checklist/F01_코어골격_팀ID/00_기능_완료판정.md`](../Checklist/F01_코어골격_팀ID/00_기능_완료판정.md)
의 **「통합 검증」** 5건을 채운다.

---

## 실행 결과 (2026-09-06 15:42, `Saved/Logs/EternalReturn.log`)

**2-2 팀 배정** — `Team Size = 2`

```
[Team] ERPlayerState_0 → Team 0 (1/2)
[Team] ERPlayerState_1 → Team 0 (2/2)
[Team] ERPlayerState_2 → Team 1 (1/2)
[Team] ERPlayerState_3 → Team 1 (2/2)
```
팀당 정확히 2명. 경고 0건.

**2-3 데이터값 동작** — `Team Size = 1`

```
[Team] ERPlayerState_0 → Team 0 (1/1)
[Team] ERPlayerState_1 → Team 1 (1/1)
[Team] ERPlayerState_2 → Team 2 (1/1)
[Team] ERPlayerState_3 → Team 3 (1/1)
```
4팀으로 갈렸다 → **상수로 박히지 않았음이 증명됐다.**

**2-4 스폰 클래스** — `Avatar=BP_ERCharacterBase_C_0 ~ _3` 각 10줄.
Server 8줄 / Client 32줄 (4폰 × 4클라 × 2회 실행) — 수가 맞는다.

## ⚠ 아직 안 한 것

- [ ] **`Team Size` 를 `3` 으로 되돌린다** (1-3). 지금 `1` 또는 `2` 로 남아 있을 것이다
