# E32 — 산 야생동물을 봤던 클라가 돌아오면 시체가 그제야 쓰러진다

> 발견: 2026-09-30 F12.5-06 B (늦게 relevant 된 클라 — 누운 시체) · 관련 `Wildlife/ERWildlifeCharacter.cpp` OnRep_Dead · HandleOutOfHealth · SetBodyActive (DormantAll)

## 증상

- 서버 처치 15:45:03 · 15:45:11 (`사망 포즈 쓰러짐 (서버)`) → 창 B 가 26초 뒤 다가가자 클라에서 `사망 포즈 쓰러짐 (클라)` + 사망음
- 기대는 `사망 포즈 누운 채로 (늦은 relevant) (클라)` — 이미 죽은 지 오래인데 B 화면에서 쓰러지는 과정을 다시 튼다

## 원인

- 판정이 `!HasActorBegunPlay()` 뿐이었다 — **이 클라에 액터가 막 생겼을 때**만 "늦었다" 로 본다 (새 액터는 OnRep 이 PostNetInit 전에 불린다 — `DataChannel.cpp:3331 → 3345`)
- 로그에 `쓰러짐 (클라)` = BeginPlay 가 이미 끝난 액터 = **B 에 산 닭이 남아 있었다** (PIE 시작 때 150m 로 받은 것)
- 대기 야생동물은 `DORM_DormantAll` (`SetBodyActive`). 도먼시로 닫힌 채널은 클라가 액터를 **지우지 않는다** → 멀어져도 산 모습으로 남고, 다시 relevant 되면 그때 `bDead` 가 처음 와서 쓰러짐을 튼다
- 150m 기본값에서도 같다 — 산 걸 보고 떠났다가 다른 팀이 잡은 뒤 돌아오면 늦게 쓰러진다

## 해결

- 복제 값 `DeathServerTime` 추가 (`HandleOutOfHealth` 에서 `GameState->GetServerWorldTimeSeconds()`) · `bDead` 와 같은 묶음
- `OnRep_Dead`: **BeginPlay 전 또는 죽은 지 1초 넘음** → 누운 채로 (사망음 없음). 1초 = 서버 시각 오차(핑 · GameState 동기 주기) 여유
- 진단 줄: `[야생동물] … 사망 수신 — 죽은 지 N초 · BeginPlay 끝남/전`
- 빌드 에러 0 (Editor · Server · 2026-09-30)

## 재발 방지

- "처음 받는 액터인가" 로 "사건을 봤는가" 를 판정하지 않는다 — 도먼시 · relevancy 로 액터가 클라에 **남아 있을 수 있다**. 사건 판정은 사건 시각을 복제해서 한다
- F12.6 N1 (AI 상태 복제 → 등장 · 전투 시작 연출) 도 같은 함정 — 상태가 바뀐 시각을 같이 보낸다

## 재검증

- [x] 06 B 절차 (NetCull 30 · 50m) — 2026-09-30 01:13 로그: `죽은 지 5.78초 · BeginPlay 끝남` → `누운 채로 (늦은 relevant) (클라)` · `8.11초` 도 같음 (고치기 전 같은 상황 = `쓰러짐 (클라)`)
- [ ] 가까이서 처치 → `죽은 지 0.0x초` + `쓰러짐 (클라)` + 사망음
