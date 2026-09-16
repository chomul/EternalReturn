# E13 — 커서 트레이스(Visibility)가 캐릭터를 못 잡는다 — 캡슐은 Visibility 를 무시한다

> 발견 **2026-09-14** · F07-07 · ⭐ **사용자 보고** ("인식을 못한 거 같은데")

## 증상

평타(`SingleTarget`)를 상대 위에 커서를 두고 눌러도 8번 전부 `판정 SingleTarget: 적중 0`.
F07-05 ④ 진단 로그에서도 커서를 상대 쪽에 뒀는데 조준 액터가 늘 `StaticMeshActor_34`(바닥)였다 — 그때는 Projectile 이라 방향만 맞으면 됐고, 지금 SingleTarget 에서 드러났다.

## ⭐ 원인

`ERPlayerController::OnSkillSlotPressed` 가 `GetHitResultUnderCursor(ECC_Visibility)` 로 조준을 잡았다.
**캐릭터 캡슐의 `Pawn` 콜리전 프로파일은 `Visibility` 채널에 `Ignore`** 다 (엔진 기본, `BaseEngine.ini` `[/Script/Engine.CollisionProfile]` Pawn 프로파일).
보통은 스켈레탈 메시(`CharacterMesh` 프로파일, Visibility Block)가 대신 맞아 주는데, 이 프로젝트는 아직 메시가 없다
(로그 `Couldn't find file for package /Game/ER/Characters/Jackie/Jackie_SK`). 그래서 트레이스가 캐릭터를 뚫고 바닥에 닿았다.

조준 **지점**은 맞았고(바닥 좌표), 조준 **액터**만 틀렸다 → `FTargetQuery.DesignatedTarget = 바닥` → `QuerySingleTarget` 이 필터에서 거른다.

## 수정

```cpp
// ECC_Pawn 먼저 — 캡슐이 막는다. 바닥도 Pawn 채널을 막으니 캐릭터 위가 아니면 바닥 지점. 그것도 없으면 Visibility.
if (GetHitResultUnderCursor(ECC_Pawn, false, Hit) || GetHitResultUnderCursor(ECC_Visibility, false, Hit))
```

`Core/ERPlayerController.cpp` `OnSkillSlotPressed`. **메시가 생겨도 캡슐로 잡는다** — 판정을 연출 메시에 맡기지 않는다.

## ⚠ 재발 방지

- 커서 · 마우스 피킹은 **Visibility 가 아니라 대상의 콜리전 프로파일이 막는 채널**로 한다. 캐릭터 = `ECC_Pawn`.
- 이동(`OnMoveToCursor`)은 바닥을 찍는 것이라 Visibility 가 맞다 — 그대로 둔다.
- 나중에 "적 캐릭터를 클릭하면 오토 어택" 을 만들 때도 같은 채널.

- [x] 수정 후 재검증 — 캡슐 위 A → `적중 1` · `피해 1명 (기본 50 · 평타)` (2026-09-14)
- 후속: 캡슐만 보이는 상태라 클릭 면적이 좁아 `Shape.AimAssistRadius`(조준점 반경 안 가장 가까운 대상) 추가 — 대상 **선택** 보조, 판정은 그대로
