# 30. 야생동물 데이터 — DataTable 행 vs 종별 DataAsset

> F12-01 · 제기: 사용자 2026-09-21 — "왜 DT 로? 애니메이션 · 모델링 등 여러 요소가 있잖아"
> 관련 [`15_스킬데이터_위치.md`](15_스킬데이터_위치.md) (스킬 = DA) · `UERCharacterData` (실험체 = DA · F02) · 역기획서 몬스터 §6.1 ("행" 표현)

## ⭐ 결정 — 2026-09-21 **B** (사용자 "B로 진행하자"). 기존 DT 점검: 아이템 · 루트 · 경험치 표 · 무기 계열은 **표**라 DT 유지 — 기준 "표는 DT · 개체 정의는 DA"

## 결정할 것

야생동물 한 종의 정의를 **`DT_Wildlife` 의 행**으로 두나, **`UERWildlifeData` 애셋**으로 두나.

## 방안 A — DataTable 행 (지금 01 코드)

`FERWildlifeRow { Type · BaseStats · PerLevel · LootRow · ExpValue · Tier }` · 스폰은 행 이름.
- 장점: 17행이 한 화면 · CSV 로 수치 일괄 편집 · 균형 조정에 강함
- 단점
  - 메시 · 애님BP · 사운드 · **스킬(곰 기절기 · 보스 스킬 = `UERSkillData` 참조)** 을 행에 넣으면 `TSoftObjectPtr` 열이 줄줄이 늘고 CSV 로는 못 찍는다 → 결국 종별 BP 를 따로 만들게 되어 **데이터가 두 곳**(행 + BP)
  - 실험체(`UERCharacterData` DA)와 구조가 달라 "실험체 vs 야생동물" 코드가 갈라진다 (스탯 초기화 · 스킬 부여 · 메시 지정)

## 방안 B — 종별 `UERWildlifeData : UPrimaryDataAsset` ⭐ 추천

```cpp
UCLASS() class UERWildlifeData : public UPrimaryDataAsset
  EERWildlifeType Type · bool bMutant · bool bBoss · int32 Tier
  FERCharStats BaseStats · FERCharStatGrowth PerLevel            // 실험체와 같은 구조체
  TArray<TObjectPtr<UERSkillData>> Skills                          // 곰 기절기 · 보스 스킬 — 실험체와 같은 GrantSkills 경로
  FName LootRow · float ExpValue
  TSoftObjectPtr<USkeletalMesh> Mesh · TSoftClassPtr<UAnimInstance> AnimClass   // 연출 (지금은 비워 둠)
UERWildlifeSettings.Wildlife : TMap<FName, TSoftObjectPtr<UERWildlifeData>>     // "Bear" → DA_Wild_Bear (스포너 · 디버그가 이름으로)
AERWildlifeCharacter::Initialize(const UERWildlifeData*, Level)  // 스탯 GE · 태그 · 스킬 부여 · 메시 적용
```
- 장점
  - **실험체와 같은 모양** — `UERCharacterData` 와 필드가 거의 같아 스탯 초기화(`ApplyStatRow`) · 스킬 부여(`GrantSkills`) · 메시 지정을 **같은 코드**가 처리한다
  - 스킬 · 애셋 참조가 자연스럽다 (`TObjectPtr<UERSkillData>` — F11.5 조각 그대로)
  - BP 하나(`BP_ERWildlife`) + DA 17개. 종별 BP 불필요
- 단점
  - 수치 일괄 편집은 DT 보다 불편 (17개 애셋 열기). 균형 조정이 잦으면 DT 가 편했을 것 — 대신 값은 에디터 Property Matrix 로 한 번에 볼 수 있다
  - 지역 스폰표(03 · F13)는 여전히 DT 가 맞다 — 거기서 DA 를 참조하면 된다 (`TSoftObjectPtr<UERWildlifeData>` 열)

## 비교표

| | A DT 행 | B 종별 DA |
|---|---|---|
| 구현 난이도 | 끝남 | 01 재작업 반나절 (코드만 · 에디터 작업 전이라 애셋 손실 없음) |
| 확장성 | 스킬 · 연출 붙이면 두 곳 | 실험체와 같은 길 |
| 네트워크 | 같음 (행 이름 vs DA 경로 1회 복제) | 같음 |
| GAS 정합성 | 스킬을 행에 넣기 어색 | `GrantSkills(Skills)` 그대로 |
| 성능 | 같음 | 같음 (DA 는 로드 1회) |

## 추천

⭐ **B**. 지금 01 은 코드만 빌드된 상태라 갈아타는 비용이 가장 싸다. 스폰표(03)는 DT 로, 종 정의는 DA 로 — "표는 DT · 개체 정의는 DA" 가 실험체(DA) · 아이템(DT) · 스킬(DA)과 같은 기준이다.
