// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Data/EnumBase.h"
#include "InputActionValue.h"
#include "GameFramework/Character.h"
#include "InputAction.h"
#include "AbilitySystemInterface.h"
#include "GameplayAbilitySpecHandle.h"
#include "TestCharacter.generated.h"

class AWeaponPickupActor;
class AWeaponBoxActor;
class UCombatAbilitySystemComponent;
class URangedTargetingComponent;
class UGameplayAbility;
struct FGameplayEffectContextHandle;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPlayerKilled);

UCLASS()
class TEAMPOTATO_API ATestCharacter : public ACharacter, public IAbilitySystemInterface
{
    GENERATED_BODY()

public:
    // Sets default values for this character's properties
    ATestCharacter();
    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
    virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;
    virtual void PossessedBy(AController* NewController) override;

    // Activates the skill configured on this character BP; GA handles cost/cooldown and effects.
    UFUNCTION(BlueprintCallable, Category = "Combat|Skill")
    bool TryUseCharacterSkill();

    UFUNCTION(BlueprintPure, Category = "Combat|Lock On")
    URangedTargetingComponent* GetRangedTargeting() const { return RangedTargeting; }

protected:
    virtual void PostInitializeComponents() override;
    // Called when the game starts or when spawned
    virtual void BeginPlay() override;

    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
    // 입력과 이벤트 바인딩
    virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

    //무적 활성화
    void InvincibleActivate();
    //무적 비활성화
    void InvincibleDeactivate();

    //애니메이션만 재생하는 함수
    //검 상태 회피
    void PlayDodgeMontage_Front_Sword();
    void PlayDodgeMontage_Right_Sword();
    void PlayDodgeMontage_Left_Sword();
    void PlayDodgeMontage_Back_Sword();

    //총 상태 회피
    void PlayDodgeMontage_Front_Gun();
    void PlayDodgeMontage_Back_Gun();
    void PlayDodgeMontage_Right_Gun();
    void PlayDodgeMontage_Left_Gun();

    //검 공격 몽타주 재생
    void PlaySwordAttackMontage();

    void PlaySwordAttackMontage_Combo();

    //총 발사 몽타주 재생
    void PlayGunShootingMontage();

    virtual void NotifyActorBeginOverlap(AActor* OtherActor) override;
    //주울 아이템과 오버랩이 끝났을때 호출
    virtual void NotifyActorEndOverlap(AActor* OtherActor) override;

    //데미지 받는 델리게이트
    UFUNCTION(BlueprintCallable, Category = "Damage")
    void TakeAnyDamage(AActor* DamagedActor, float InDamage, const UDamageType* DamageType, AController* InstigatedBy, AActor* DamageCauser);

    //사망 처리
    UFUNCTION(BlueprintCallable, Category = "Kill")
    void KillPlayer();

    //대각선 방향 애니메이션 재생을 위해 몸 돌리기
    UFUNCTION()
    void RotatePlayer(EMovingDirection TurnDirection);

    //총 상태에서 회피 애니메이션 재생시 원활하게 움직이도록 AnimInstance의 RootMotionMode::IgnoreRootMotion 설정
    UFUNCTION()
    void SetAnimRootMotionIgnore();

    //RootMotionMode::RootMotionFromMontagesOnly으로 원상복구
    UFUNCTION()
    void SetAnimRootMotionFromMontage();

    //스태미너 사용 가능 여부 반환 및 스태미너 사용
    UFUNCTION()
    bool UseEnergy(float InEnergyUseAmount);
    //스태미너 충전
    UFUNCTION(BlueprintCallable, Category = "Resource|Energy")
    void FillEnergy(float InEnergy);

    //체력회복
    UFUNCTION(BlueprintCallable, Category = "Resource|Health")
    void Heal(float InHeal);

    //공격력 증가
    UFUNCTION(BlueprintCallable, Category = "Resource|Power")
    void AddPower(float InPower);
    //최대체력 증가
    UFUNCTION(BlueprintCallable, Category = "Resource|Health")
    void AddMaxHealth(float InMaxHealth);
    //최대 스태미나 증가
    UFUNCTION(BlueprintCallable, Category = "Resource|Energy")
    void AddMaxEnergy(float InMaxEnergy);
    // 이동 속도 증가
    void AddMoveSpeed(float InMoveSpeed);

    //Getter
    //현재 활성화된 무기 확인
    UFUNCTION(BlueprintCallable, Category = "Weapon")
    inline EWeaponType GetPlayerActivatedWeapon() { return ActivatedWeapon; }

    UFUNCTION()
    inline UWeaponComponent* GetWeaponComponent() { return WeaponComponent; }

    UFUNCTION()
    inline UPlayerResource* GetResource() { return ResourceManager; }

    UFUNCTION(BlueprintCallable, Category = "Sight")
    inline float GetSightDegree() { return SightDegree; }

    UFUNCTION(BlueprintCallable, Category = "Direction")
    inline FVector2D GetCurrentInput() { return CurrentInput; }

    EMovingDirection GetPlayerDirection();

    UFUNCTION(BlueprintCallable, Category = "Direction")
    inline bool IsDirectionChanged() { return bIsDirectionChanged; }

    //Setter
    //행동중인지 설정
    UFUNCTION()
    void SetOnActing(bool InActing);

    void SetOnAttacking(bool InAttacking);

	//활성화 된 무기 설정
	UFUNCTION(BlueprintCallable, Category = "Weapon")
    void SetPlayerActivatedWeapon(EWeaponType InActivatedWeapon);

    UFUNCTION()
    void PlayerAttack();

protected:

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Skill")
    TSubclassOf<UGameplayAbility> SkillAbilityClass;

    // 앞뒤양옆으로 움직이는 함수
    UFUNCTION()
    void OnMovementInput(const FInputActionValue& InValue);

    //앞뒤 움직임 입력
    UFUNCTION()
    void OnFrontMovementInput(const FInputActionValue& InValue);

    //앞뒤 움직임 입력 종료
    UFUNCTION()
    void OnFrontMovementComplete();

    //좌우 움직임 입력
    UFUNCTION()
    void OnSideMovementInput(const FInputActionValue& InValue);

    //좌우 움직임 입력 종료
    UFUNCTION()
    void OnSideMovementComplete();

    //고개 옆으로 돌리는 함수
    UFUNCTION()
    void OnHorizonSightInput(const FInputActionValue& InValue);

    //고개 위아래로 움직이는 함수
    UFUNCTION()
    void OnVerticalSightInput(const FInputActionValue& InValue);


    //구르기 함수
    UFUNCTION()
    void OnRollInput();

    //공격 함수
    UFUNCTION()
    void OnAttack(bool bIsAutoFiring = false);
    void OnAttackStarted();
    void OnAttackCompleted();

    UFUNCTION()
    void OnSkillInput();

    // 상호작용 함수
    void OnInteract();

    // 무기 교체 함수
    void OnWeaponSwap();


    // 캐릭터 스탯 초기화 함수
    UFUNCTION()
    void InitializeCharacterStat();

private:
    FGameplayAbilitySpecHandle SkillAbilityHandle;

    void HandleCombatDamage(float ActualDamage, const FGameplayEffectContextHandle& Context);
    void HandleAttackBlocked();
    //행동을 할 수 있는 상태인지(행동중이 아니고 무기를 들고 있음)
    UFUNCTION()
    bool IsActionAvailable();

    //피격무적
    UFUNCTION()
    void OnHitInvincible();

public:
    // 상호작용 대상
    UPROPERTY()
    AActor* CurrentInteractTarget = nullptr;

    //공격중에 콤보 공격을 입력했는지
    UPROPERTY()
    bool bIsOnComboInput = false;

    //콤보공격 입력을 받을 수 있는지
    UPROPERTY()
    bool bIsComboInputAvailable = false;

    UPROPERTY(BlueprintAssignable, Category = "Event")
    FOnPlayerKilled OnPlayerKilled;

protected:
	//IA
	//이동 입력
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "InputAction")
    TObjectPtr<UInputAction> IA_Move = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "InputAction")
	TObjectPtr<UInputAction> IA_FrontMove = nullptr;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "InputAction")
    TObjectPtr<UInputAction> IA_SideMove = nullptr;
    //횡방향 마우스 이동
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "InputAction")
    TObjectPtr<UInputAction> IA_HorizonSight = nullptr;
    //종방향 마우스 이동
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "InputAction")
    TObjectPtr<UInputAction> IA_VerticalSight = nullptr;
    //공격 입력
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "InputAction")
    TObjectPtr<UInputAction> IA_Attack = nullptr;
    //구르기 입력
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "InputAction")
    TObjectPtr<UInputAction> IA_Roll = nullptr;
    // 스킬 입력
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "InputAction")
    TObjectPtr<UInputAction> IA_Skill = nullptr;
    // 상호작용 입력
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "InputAction")
    TObjectPtr<UInputAction> IA_Interact = nullptr;
    // 무기 교체 입력
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "InputAction")
    TObjectPtr<UInputAction> IA_WeaponSwap = nullptr;

    //components
    //스프링 암 컴포넌트
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Component")
    TObjectPtr<class USpringArmComponent> SpringArm = nullptr;
    //카메라 컴포넌트
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Component")
    TObjectPtr<class UCameraComponent> CameraComponent = nullptr;
    //움직임 컴포넌트
    UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Component")
    TObjectPtr<class UCharacterMovementComponent> MovementComponent = nullptr;

    //스프링 암 길이
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Component|Offset")
    float SpringArmLenght = 300.0f;
    //스프링 암 끄트머리(카메라) 위치조정
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Component|Offset")
    FVector SpringArmOffset = FVector(0, 0, 200.0f);
    //카메라 각도 조정
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Component|Offset")
    FQuat CameraRotation = FQuat(FRotator(-20.0f, 0, 0));

    //Anim Montage
    // 검
    //회피(구르기)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation|Montage")
    TObjectPtr<class UAnimMontage> RollMontage_Sword = nullptr;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation|Montage")
    TObjectPtr<class UAnimMontage> BackStepMontage_Sword = nullptr;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation|Montage")
    TObjectPtr<class UAnimMontage> RightStepMontage_Sword = nullptr;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation|Montage")
    TObjectPtr<class UAnimMontage> LeftStepMontage_Sword = nullptr;
    //공격
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation|Montage")
    TObjectPtr<class UAnimMontage> AttackMontage_Sword_Combo1 = nullptr;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation|Montage")
    TObjectPtr<class UAnimMontage> AttackMontage_Sword_Combo2 = nullptr;

    //총
    //회피(구르기)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation|Montage")
    TObjectPtr<class UAnimMontage> RollMontage_Gun = nullptr;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation|Montage")
    TObjectPtr<class UAnimMontage> BackStepMontage_Gun = nullptr;
    //공격
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation|Montage")
    TObjectPtr<class UAnimMontage> AttackMontage_Gun = nullptr;


    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
    TObjectPtr<class UWeaponComponent> WeaponComponent = nullptr;

    // 무기 연사 타이머 핸들
    FTimerHandle AttackTimerHandle;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
    TSubclassOf<UUserWidget> CrosshairWidgetClass;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
    TObjectPtr<UUserWidget> CrosshairWidget = nullptr;

private:
    //가로방향 마우스 감도
    float HorizonMouseSensivility = 1.0f;
    //세로방향 마우스 감도
    float VerticalMouseSensivility = 0.7f;

    //위아래 시야 움직임 제한치
    float MaxSightAngle = 20.0f;

    //마우스 상하 반전을 위함
    float Reverse = -1.0f;

    //플레이어 위아래 시야 각도
    float PlayerVerticalDegree = 0.0f;

    //플레이어가 행동중인지 확인
    bool bIsOnAction = false;

    //플레이어가 공격중인지 확인
    bool bIsOnAttacking = false;

    //이동 입력의 방향이 바뀌었는지 확인
    bool bIsDirectionChanged = false;

	//시야각
	float SightDegree = 0.0f;

    //피격무적 시간
    float OnHitInvincibleTime = 1.0f;

    bool bDeathHandled = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UCombatAbilitySystemComponent> CombatAbilitySystem;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<URangedTargetingComponent> RangedTargeting;

    // 공격 속도(애니메이션 조절 및 마우스 클릭 연사 방지용)
    float AttackSpeed = 1.0f;

    // 마지막으로 공격을 실행한 절대 시간
    float LastAttackTime = 0.0f;

    //무기, 상황별로 구분해서 맞는 애니메이션 호출하는 함수
    UPROPERTY()
    TObjectPtr<class UPlayerAnimation> PlayerAnimation = nullptr;

    //ABP
    UPROPERTY()
    TWeakObjectPtr<class UAnimInstance> AnimInstance = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<class UPlayerResource> ResourceManager = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<class UPerkComponent> PerkComponent = nullptr;

    //가지고 있는 무기 정보
    UPROPERTY()
    EWeaponType ActivatedWeapon = EWeaponType::None;

    //마지막 입력 저장하는 함수
    /*float FrontBackMove = 0.0f;
    float SideMove = 0.0f;*/

    //움직이는 방향 입력값
    FVector2D CurrentInput = FVector2D(0.0f, 0.0f);

    //대각선 방향 애니메이션 재생을 위해 몸을 돌릴 각도
    float AnimRotateDegree = 45.0f;

    //회피 애니메이션 중 움직이지 않는 애니메이션 재생시 캐릭터를 움직일 정도
    float LaunchPlayerPower = 1000.0f;

    UPROPERTY()
    TObjectPtr<class UCharacterSubsystem> CharacterSubsystem = nullptr;
};
