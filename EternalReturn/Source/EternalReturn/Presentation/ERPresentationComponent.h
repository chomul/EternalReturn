// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "Item/ERItemTypes.h"

class UAbilitySystemComponent;
class UAnimInstance;
class UAnimMontage;
class UAnimSequenceBase;
class UERPresentationData;
class UERSkinData;
class UGameplayAbility;
class USoundBase;
struct FERPresentationEntry;
struct FERAttachPiece;
struct FERAttachProp;
class USceneComponent;
struct FGameplayCueParameters;
struct FStreamableHandle;

#include "ERPresentationComponent.generated.h"

/** 이 머신이 만든 부착 컴포넌트들 (Argument 64) — TMap 값으로 붙잡으려고 */
USTRUCT()
struct FERSpawnedAttach
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<TObjectPtr<USceneComponent>> Comps;
};

/** 대기 중 쉬는 자세 (F12.6-02 야생동물 경계 · 잠) — 상태라 AnimBP 상태머신이 튼다. */
UENUM()
enum class EERRestPose : uint8
{
	None,
	Beware,
	Sleep,
};

/**
 * 시전자 쪽 연출 해석기 (F12.5-01 · Argument 39). 실험체 · 야생동물이 하나씩 든다. **복제하지 않는다** — 각 머신이 같은 입력(스킨 · 무기)으로 같은 답을 낸다.
 *
 * 조회 순서 (좁은 것이 이긴다): 스킨(모드) > 스킨 > 모드 줄 > 무기 세트 > 캐릭터 기본 → 없으면 연출 없이 판정만. 모드 줄 = 모드 칸이 지금 모드와 같은 줄 (Argument 42 ⑤).
 *   ⏸ ⑤ 무기 공통(효과음)은 05 에서.
 *
 * ⭐ P2 — 로드한 스킨 · 무기 세트를 UPROPERTY 로 **붙잡는다.** 소프트 참조는 GC 를 막지 않는다.
 * ⭐ P3 — 스킨 · 무기가 바뀔 때만 해석해서 `Cache` 에 둔다. 재생은 해시 1회.
 */
UCLASS(ClassGroup = (ER))
class ETERNALRETURN_API UERPresentationComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UERPresentationComponent();

	/** 캐릭터 기본 표 (캐릭터 DA · 야생동물 DA 가 하드로 든 것). */
	void SetBase(UERPresentationData* InBase);

	/** 스킨 — 동기 로드 · 메시 · AnimBP 교체. 널이면 스킨 없음 (BP 기본). ⏸ P7 로딩 화면이 생기면 미리 로드. */
	void SetSkin(const TSoftObjectPtr<UERSkinData>& SkinRef);

	/** 장착 무기 계열 — 그 세트를 비동기 로드. 로드 전에는 ③ 이 비어 판정만 (기존 동작). */
	void SetWeapon(EERWeaponType InWeapon);

	/** 어빌리티 발동 애니. 서버(복제 원천)와 소유 클라가 부른다. AttackSpeed > 0 이면 평타 — 재생 속도 = max(1, 길이 × 공속). */
	void PlayAbilityAnim(UGameplayAbility* Ability, const FGameplayAbilityActivationInfo& ActivationInfo, FGameplayTag Key, float AttackSpeed, float ExpectedSeconds, float CastTime = 0.f);

	/**
	 * 판정 순간 — 선딜 구간을 맞추느라 바꾼 재생 속도를 1배속으로 (Argument 52 T2 개선: 후딜은 원래 속도). 어빌리티가 판정 때 부른다.
	 * 서버는 복제 원천(다른 클라로) · 소유 클라는 자기 것. 속도를 안 바꿨으면 아무것도 안 한다.
	 */
	void RestoreSkillAnimRate(UGameplayAbility* Ability);

	/**
	 * 이동이 수락됐다 — 액션 모션(평타 · 스킬)을 끊는다. 역기획서 §5.1 "후딜 중 이동 = 애니메이션 캔슬".
	 * ⭐ 어빌리티가 이미 끝났어도 모션은 남아 있을 수 있어서(판정과 모션은 따로 돈다 · Argument 36) 어빌리티 취소만으로는 부족하다.
	 * 부르는 곳: 소유 클라(로컬 재생분) · 서버(복제 원천 → 다른 클라). 이동 차단 중에는 수락 자체가 안 되니 부르지 않는다.
	 */
	void StopActionAnim();

	/**
	 * 모드 태그 구독 (Argument 42 S1) — ASC 의 `Mode` 부모 태그가 늘거나 줄면 활성 모드를 다시 고른다. 실험체만 (InitPresentation).
	 * 채집 태그(`State.Gathering` · F12.5-04)도 같이 구독한다 — 같은 ASC · 같은 복제 loose 태그 경로.
	 * 태그는 서버가 복제 loose 태그로 붙이므로 **각 머신이 스스로** 모드 자세 · 동작표를 바꾼다 (새 복제 없음).
	 */
	void BindModeTags(UAbilitySystemComponent* InASC);

	/**
	 * 사망 포즈 (04 · 야생동물). 서버는 사망 처리에서, 클라는 bDead OnRep 에서. 한 번뿐.
	 * @param bSkipToEnd 늦게 relevant 된 클라 — 쓰러지는 과정 없이 누운 채로 (OnRep 이 BeginPlay 보다 먼저 불린 경우 · DataChannel.cpp:3331 → 3345)
	 */
	void SetDead(bool bSkipToEnd);

	/**
	 * 복제 상태가 바뀐 사건 하나 — 몽타주(있으면) + 소리(있으면)를 **이 머신에서** 튼다 (F12.6-01 야생동물 등장 · 발견 · 전투 끝).
	 * 키가 없거나 동작표에 줄이 없으면 건너뛴다. 데디 서버는 아무것도 안 한다. 틀었는지 로그용 문자열을 돌려준다.
	 */
	FString PlayEventPres(FGameplayTag AnimKey, FGameplayTag SfxKey);

	/** 이 키 애니가 Section 섹션을 가진 몽타주면 그것 (스킬 통 몽타주 · K8). 아니면 nullptr. */
	UAnimMontage* FindSkillMontage(FGameplayTag Key, FName Section) const;

	/**
	 * 스킬 통 몽타주의 섹션 넘기기 (K8). 서버 = ASC 로 (다른 클라에 섹션 · 위치 복제) · 소유 클라 = 로컬
	 * (GAS 는 복제 몽타주를 본인에게 적용하지 않는다 — AbilitySystemComponent_Abilities.cpp:3147). 다른 클라는 부르지 않는다 (복제로 받는다).
	 * 그 몽타주가 재생 중이 아니면 false.
	 */
	bool JumpSkillSection(FGameplayTag Key, FName Section);

	/** 쉬는 자세 (F12.6-02) — AnimBP 에 자세 · 애니 6개를 넘긴다. bSkipIntro = 늦게 받음: 시작 동작 없이 반복부터. 같은 값이면 무시. */
	void SetRestPose(EERRestPose Pose, bool bSkipIntro);

	/**
	 * 연출 큐 (F12.5-05 · Argument 49) — 액터(IGameplayCueInterface)가 넘긴다. 클라에서만 온다 (데디 서버는 GAS 가 막는다).
	 * `GameplayCue.Pres.Attack` = 시전자 위치에 공격음 · `GameplayCue.Pres.Hit` = 타격 지점에 타격음. 소리는 **시전자**(Params.Instigator) 의 동작표에서 찾는다.
	 */
	void HandlePresCue(FGameplayTag CueTag, const FGameplayCueParameters& Params);

	/** [서버] 소리 키 하나를 Instigator 의 소리 표에서 골라 Location 에 (모든 클라 · GameplayCue.Pres.Sfx). 어빌리티 밖(윌슨 액터 · 투사체 · 지연 타이머)에서 쓴다 */
	static void SendSfxCue(AActor* Instigator, FGameplayTag SfxKey, const FVector& Location);

	/** [서버] 동작표 키 하나를 Instigator 에게 — 각 머신이 자기 화면에 (레니 R 같이 날아감 · GameplayCue.Pres.Anim) */
	static void SendAnimCue(AActor* Instigator, FGameplayTag AnimKey);

	/**
	 * 이 키 줄의 소리 하나 (각 클라 로컬 · 복제 안 함). 없으면 nullptr.
	 * ShotNumber > 0 (순차 사격 몇 번째 발 · 카티야 R) 이면 **그 순서의 소리** (`_Shot` → `_02` → `_03` · 모자라면 마지막) · 0 이면 무작위 (r1 · r2 변형).
	 */
	USoundBase* PickSound(FGameplayTag Key, int32 ShotNumber = 0) const;

	/** 모션 소리 노티파이 (Argument 59 N2) — 지금 스킨에 이 소리의 짝이 있으면 그것 · 없으면 그대로. */
	USoundBase* ResolveSound(USoundBase* Default) const;

	/** 지금 해석 결과에 이 키가 있나 (리캐스트 키 → 슬롯 키 대체 판단). */
	bool HasKey(FGameplayTag Key) const { return Cache.Contains(Key); }

	/** 해석 결과 전부 (ER.Pres.Show). */
	void DumpToLog() const;

	/** 지금 스킨의 소품 (발사 바이크가 쏜 사람의 것을 꺼낸다 · Argument 64 B1). 없으면 nullptr. */
	const FERAttachProp* FindProp(FGameplayTag Key) const;

	/**
	 * 조각들을 Parent 의 소켓에 만들어 붙인다 — 이 머신 화면에만 (복제 안 함). 충돌 없음. 만든 컴포넌트를 OutComps 에 더한다.
	 * 데디 서버에서는 아무것도 안 한다. 소켓이 없으면 Warning 후 원점에 붙인다.
	 */
	static void SpawnPieces(AActor* Owner, USceneComponent* Parent, const TArray<FERAttachPiece>& Pieces, TArray<TObjectPtr<USceneComponent>>& OutComps);

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void Rebuild();
	void OnWeaponSetLoaded(EERWeaponType LoadedWeapon);

	void OnModeTagChanged(const FGameplayTag Tag, int32 NewCount);
	/** 지금 붙은 모드 태그를 다시 읽는다. 바뀌면 Rebuild (모드 층 · AnimInstance bInMode). */
	void RefreshMode();
	/** 활성 모드 여부를 메인 AnimInstance(`UERAnimInstance`)에 — 진입 · 유지 · 해제는 상태머신이 (Argument 42 ④ MB). */
	void PushModeToAnim();

	/** 채집 태그가 붙으면 collect 를 **원래 속도로** 튼다 · 떨어지면 길고 부드러운 블렌드 아웃으로 일어선다 (Argument 47 F1). 이동하면 AnimInstance 가 끊는다. */
	void OnGatherTagChanged(const FGameplayTag Tag, int32 NewCount);
	/** 모션 유지 태그(State.AnimHold)가 빠지면 지금 몽타주를 End 섹션으로 — 서버(복제) · 소유 클라(로컬 재생분) (Argument 63 M1). */
	void OnAnimHoldTagChanged(const FGameplayTag Tag, int32 NewCount);
	/** 반복 소리 (F19-02 매그너스 W · R · Audio/Magnus.md) — 태그가 있는 동안 SkillLoop 키 소리를 몸에 붙여 반복 · 빠지면 멈춤. bPlayStart = 막 붙었다 (시작음) */
	void OnLoopSfxTagChanged(const FGameplayTag Tag, int32 NewCount);
	void RefreshLoopSfx(bool bPlayStart);
	UFUNCTION()
	void OnLoopAudioFinished();
	/** 손 무기를 지금 스킨 · 무기 종류로 다시 붙인다 (Argument 64 W1). SetSkin · SetWeapon 이 부른다. */
	void RefreshWeaponAttach();
	/** 지금 스킨 소품의 켜는 태그를 구독한다 (스킨 · ASC 가 바뀔 때 다시). */
	void BindPropTags();
	void UnbindPropTags(UAbilitySystemComponent* ASC);
	void OnPropTagChanged(const FGameplayTag Tag, int32 NewCount);
	/** 켜는 태그가 있는 소품만 몸에 붙어 있게 맞춘다 (Argument 64 B1). */
	void RefreshProps();

	/** 상태 포즈(사망)를 AnimInstance 에 (04). Rebuild 끝에서도 — 스킨 · 야생동물 AnimBP 교체 뒤 새 인스턴스에 다시 알린다. */
	void PushStateToAnim();

	/** 해석 결과에서 이 키의 첫 애니. */
	UAnimSequenceBase* FindFirstAnim(FGameplayTag Key) const;

	/** 무기 레이어를 메인 AnimBP 에 붙인다 (Argument 40 K2). Rebuild 끝에서 — 무기 · 세트 로드 · 스킨이 모두 여기를 지난다. */
	void ApplyWeaponLayer();
	/** @param bModePass false = 모드 칸이 빈 줄만 · true = 모드 칸 == 지금 모드인 줄만 (Argument 42 ⑤) */
	void AddLayer(const TArray<FERPresentationEntry>& Entries, const TCHAR* Source, bool bFilterWeapon, bool bRequireWeaponMatch, bool bModePass);

	UPROPERTY()
	TObjectPtr<UERPresentationData> Base;

	UPROPERTY()
	TObjectPtr<UERSkinData> Skin;

	/** P2 — 판 중에는 내리지 않는다 (바꾼 무기는 보통 계속 쓴다 · Argument 39 로드 시점). */
	UPROPERTY()
	TMap<EERWeaponType, TObjectPtr<UERPresentationData>> LoadedWeaponSets;

	/** 무기 공통 (DT_WeaponClass.Presentation · Argument 49) — 세트처럼 붙잡는다. 조회의 가장 넓은 층. */
	UPROPERTY()
	TMap<EERWeaponType, TObjectPtr<UERPresentationData>> LoadedWeaponCommon;

	EERWeaponType Weapon = EERWeaponType::None;

	struct FResolved
	{
		const FERPresentationEntry* Entry = nullptr;
		const TCHAR* Source = TEXT("");
	};
	/** P3 — 위 UPROPERTY 들이 붙잡은 DA 안의 항목을 가리킨다. Rebuild 가 전부 다시 만든다. */
	TMap<FGameplayTag, FResolved> Cache;

	/** 키별 다음 변형 (atk01 → atk02 → atk01). */
	TMap<FGameplayTag, int32> NextVariant;

	TSharedPtr<FStreamableHandle> PendingWeaponLoad;

	/** 활성 모드 (없으면 빈 태그) — 모드 칸이 이것과 같은 줄이 평소 줄을 덮는다 (Argument 42 ⑤) · AnimInstance bInMode (④ MB). */
	FGameplayTag ActiveMode;

	TWeakObjectPtr<UAbilitySystemComponent> ModeASC;
	FDelegateHandle ModeTagHandle;
	FDelegateHandle GatherTagHandle;
	FDelegateHandle AnimHoldTagHandle;
	/** 반복 소리 태그 → 구독 핸들 · 지금 도는 소리 (이 머신만) */
	TMap<FGameplayTag, FDelegateHandle> LoopSfxTagHandles;
	UPROPERTY()
	TMap<FGameplayTag, TObjectPtr<class UAudioComponent>> LoopAudio;
	/** 지금 트는 채집 몽타주 — 채집 끝에 이것만 멈춘다 (그 사이 스킬 몽타주가 덮었으면 이미 끝나 있다). */
	TWeakObjectPtr<UAnimMontage> GatherMontage;

	/** 상태 포즈 원천 — 컴포넌트가 들고 있다가 PushStateToAnim 이 넘긴다 (AnimInstance 가 바뀌어도 잃지 않게). */
	bool bDead = false;
	bool bDeathSkipToEnd = false;
	EERRestPose RestPose = EERRestPose::None;
	bool bRestSkipIntro = false;

	/** 지금 붙어 있는 무기 레이어 — 바뀔 때 이것을 Unlink 한다. */
	UPROPERTY()
	TSubclassOf<UAnimInstance> LinkedLayer;

	/** 손 무기 컴포넌트 (이 머신만) */
	UPROPERTY()
	TArray<TObjectPtr<USceneComponent>> WeaponComps;

	/** 켜져 있는 소품 — 키(Pres.Prop.*) → 컴포넌트 (이 머신만) */
	UPROPERTY()
	TMap<FGameplayTag, FERSpawnedAttach> ActiveProps;

	/** 소품 켜는 태그 → 구독 핸들 */
	TMap<FGameplayTag, FDelegateHandle> PropTagHandles;

	/** 재생 실패 Warning 은 액터당 한 번 (AnimBP 없는 야생동물이 평타마다 찍었다 — 2026-09-27 로그). */
	bool bWarnedPlayFail = false;
};
