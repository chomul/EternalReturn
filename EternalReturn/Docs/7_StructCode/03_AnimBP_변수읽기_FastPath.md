# AnimBP 가 C++ 변수를 읽는 구조 — Fast Path (2026-10-08)

> 사용자 질문 2026-10-08: "시퀀스 플레이어를 변수 바인드로 해도 문제 없어? Tick 마다 낭비 아님?" · "Start Position 핀이 뭐임?"

## 그림

```
게임 스레드                          워커 스레드                     AnimGraph (ABP_ERCharacter_Base)
─────────────                        ─────────────                   ─────────────────────────────
연출 컴포넌트                          
 (태그 · 동작표에서 애니 고름)          
   │ SetDead / SetDowned / SetInMode …  
   ▼                                  
UERAnimInstance::NativeUpdateAnimation   NativeThreadSafeUpdateAnimation
 pending → UPROPERTY 로 옮김              속도 → bIsMoving
 전이 조건 미리 조합                        
 (bDownOutro = !bDowned && End && !bDead)
   │                                                                  
   └──────────── 멤버 변수 ──────────────────────────────▶  핀에 **직접** 연결 (Fast Path)
                                                            Sequence ← DownIdleAnim
                                                            Start Position ← DeathStartPosition
                                                            전이 규칙 ← bDownOutro (변수 하나)
```

## Fast Path — 왜 매 틱 낭비가 아닌가

| 연결 방식 | 매 업데이트 비용 | 이 프로젝트 |
|---|---|---|
| 멤버 변수 → 핀 **직접** | 값 복사 (≈ 0) — 블루프린트 VM 안 돎 | ✅ 이렇게 쓴다 |
| 변수 → 함수 · Cast · 계산(AND · NOT) 노드 → 핀 | 블루프린트 VM 실행 | ❌ — 조합은 C++ 에서 (2026-10-05 전이 규칙 AND 로 컴파일 경고가 났던 일) |

- 무거운 계산(속도 · 전이 조합)은 C++ 에서 끝내고 AnimBP 는 결과만 읽는다 — `Source/EternalReturn/Presentation/ERAnimInstance.h` 머리 주석 ("AnimGraph 는 아래 변수를 직접 읽는다 (Fast Path)")
- 확인: 노드 오른쪽 위 **번개 아이콘** = Fast Path · Class Settings › **Warn About Blueprint Usage** 를 켜면 깨질 때 컴파일 경고
- 더 줄이는 법 (지금은 안 함): Anim Node Function (Sequence Player 의 On Become Relevant) 로 상태 진입 때 한 번만 애니 지정 — 지금 비용이 이미 무시할 수준 · 변수를 바꾸면 바로 반영되는 장점을 잃는다
- 출처: Epic 공식 문서 「Animation Optimization」(Fast Path · 멤버 변수 직접 접근) · `(링크 미기재 — 엔진 문서 사이트)`

## Start Position 핀

- Sequence Player 가 애니를 **몇 초 지점부터** 틀지 (기본 0)
- 사망(Dead 상태)에서만 쓴다 — C++ `DeathStartPosition`:
  - 눈앞에서 죽음 → `0` (쓰러지는 동작부터)
  - **늦게 보게 됨** (relevant 가 늦게 됨 · 나중 접속) → 애니 길이 = 마지막 프레임 (이미 누운 채로) — `UERAnimInstance::NativeUpdateAnimation` (`bPendingDeathSkipToEnd`)
- 안 꽂으면 늦게 본 화면에서 시체가 **다시 쓰러진다** (야생동물 시체 · F12.5-04 와 같은 이유)
- 꺼내기: Sequence Player 선택 → Details › Start Position ▼ → **Expose As Pin** → `Death Start Position` 연결
