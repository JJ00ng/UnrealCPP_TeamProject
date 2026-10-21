// Fill out your copyright notice in the Description page of Project Settings.


#include "Player/TestCharacter.h"
#include "Player/PlayerAnimation.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "EnhancedInputComponent.h"
#include "Item/Weapon/WeaponPickupActor.h"
#include "Data/WeaponDataAsset.h"
#include "Item/Weapon/WeaponBoxActor.h"
#include "Component/WeaponComponent.h"
#include "Component/PlayerResource.h"
#include "Components/CapsuleComponent.h"
#include "Blueprint/UserWidget.h"
#include "IntetFace/Interactable.h"
#include "Component/PerkComponent.h"
#include "Subsystem/CharacterSubsystem.h"
#include "Subsystem/GameStateSubsystem.h"
#include "Combat/CombatAbilitySystemComponent.h"
#include "Combat/CombatFunctionLibrary.h"
#include "Combat/RangedTargetingComponent.h"
#include "Abilities/GameplayAbility.h"
#include "Engine/DamageEvents.h"


// Sets default values
ATestCharacter::ATestCharacter()
{
    // Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
    PrimaryActorTick.bCanEverTick = false;

    //컨트롤러 방향으로 회전
    MovementComponent = GetCharacterMovement();
    MovementComponent->bUseControllerDesiredRotation = true;
    MovementComponent->bOrientRotationToMovement = false;
    bUseControllerRotationYaw = true;

    //스프링 암 생성
    SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
    SpringArm->SetupAttachment(RootComponent);
    SpringArm->TargetArmLength = SpringArmLenght;
    SpringArm->SocketOffset = SpringArmOffset;
    SpringArm->bUsePawnControlRotation = true;		//컨트롤러를 따라 회전하도록 설정

    //카메라 생성
    CameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
    CameraComponent->SetupAttachment(SpringArm);
    CameraComponent->SetRelativeRotation(CameraRotation);

    PlayerAnimation = CreateDefaultSubobject<UPlayerAnimation>(TEXT("PlayerAnimation"));

    ActivatedWeapon = EWeaponType::None;			//테스트용

    WeaponComponent = CreateDefaultSubobject<UWeaponComponent>(TEXT("WeaponComponent"));

    ResourceManager = CreateDefaultSubobject<UPlayerResource>(TEXT("ResourceManager"));

    PerkComponent = CreateDefaultSubobject<UPerkComponent>(TEXT("PerkComponent"));

    CombatAbilitySystem = CreateDefaultSubobject<UCombatAbilitySystemComponent>(TEXT("CombatAbilitySystem"));
    RangedTargeting = CreateDefaultSubobject<URangedTargetingComponent>(TEXT("RangedTargeting"));
}

UAbilitySystemComponent* ATestCharacter::GetAbilitySystemComponent() const
{
    return CombatAbilitySystem;
}

void ATestCharacter::PostInitializeComponents()
{
    Super::PostInitializeComponents();
    // Remove the old serialized binding: legacy ApplyDamage is bridged by TakeDamage below.
    OnTakeAnyDamage.RemoveDynamic(this, &ATestCharacter::TakeAnyDamage);
    CombatAbilitySystem->OnCombatDeath.AddUObject(this, &ATestCharacter::KillPlayer);
    CombatAbilitySystem->OnCombatDamageReceived.AddUObject(this, &ATestCharacter::HandleCombatDamage);
    CombatAbilitySystem->InitializeCombat(100.0f, OnHitInvincibleTime);
    CombatAbilitySystem->OnCombatAttackBlocked.AddUObject(this, &ThisClass::HandleAttackBlocked);

    if (HasAuthority() && SkillAbilityClass && !SkillAbilityHandle.IsValid())
    {
        SkillAbilityHandle = CombatAbilitySystem->GiveAbility(FGameplayAbilitySpec(SkillAbilityClass, 1, INDEX_NONE, this));
    }
}

void ATestCharacter::PossessedBy(AController* NewController)
{
    Super::PossessedBy(NewController);
    // Character switching spawns the pawn before possession; update GAS's cached controller.
    if (CombatAbilitySystem && CombatAbilitySystem->IsCombatInitialized())
    {
        CombatAbilitySystem->RefreshAbilityActorInfo();
    }
}

float ATestCharacter::TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
    return UCombatFunctionLibrary::ApplyCombatDamage(this, DamageAmount, DamageCauser, EventInstigator);
}

void ATestCharacter::HandleCombatDamage(float ActualDamage, const FGameplayEffectContextHandle& Context)
{
    // Notification only: the legacy native health-mutating delegate has been removed.
    const APawn* DamageInstigator = Cast<APawn>(Context.GetOriginalInstigator());
    AActor::TakeDamage(ActualDamage, FDamageEvent(), DamageInstigator ? DamageInstigator->GetController() : nullptr, Context.GetEffectCauser());
}

// Called when the game starts or when spawned
void ATestCharacter::BeginPlay()
{
    Super::BeginPlay();

    UE_LOG(LogTemp, Warning, TEXT("ATestCharacter::BeginPlay"));

    if (GetMesh())
    {
        AnimInstance = GetMesh()->GetAnimInstance();	// ABP 객체 가져오기
    }

    if (CrosshairWidgetClass)
    {
        CrosshairWidget = CreateWidget<UUserWidget>(
            GetWorld()->GetFirstPlayerController(),
            CrosshairWidgetClass
        );

        if (CrosshairWidget)
        {
            CrosshairWidget->AddToViewport();
            CrosshairWidget->SetVisibility(ESlateVisibility::Hidden);
        }
    }

    UGameStateSubsystem* GameStateSubsystem = GetGameInstance()->GetSubsystem<UGameStateSubsystem>();
    CharacterSubsystem = GetGameInstance()->GetSubsystem<UCharacterSubsystem>();

    InitializeCharacterStat();

    // 게임 상태가 로비일 때 캐릭터 초기화 작업 수행
    if (GameStateSubsystem && CharacterSubsystem)
    {
        if (GameStateSubsystem->GetCurrentGameState() == EGameState::Lobby)
        {
            // 타이밍을 맞출 수 있다면 캐릭터 서브시스템으로 이관하는 것이 좋음
            CharacterSubsystem->ResetPlayerDataToInitialState();

            // 캐릭터 변경을 알리는 델리게이트 브로드캐스트
            CharacterSubsystem->OnSelectedCharacterChanged.Broadcast();
            InitializeCharacterStat();
        }
    }

    // 캐릭터 서브시스템에서 무기 정보를 가져와서 무기 컴포넌트에 설정
    if (CharacterSubsystem)
    {
        CharacterSubsystem->SetWalkSpeed(CombatAbilitySystem->GetBaseMoveSpeed());
        WeaponComponent->InitializeBaseWeapon(CharacterSubsystem->GetEquippedMainWeapon());
        WeaponComponent->PickupWeapon(CharacterSubsystem->GetEquippedSubWeapon());
    }



    UE_LOG(LogTemp, Warning, TEXT("ATestCharacter::BeginPlay - Character initialized with equipped weapons."));
}

void ATestCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (HasAuthority() && CombatAbilitySystem && SkillAbilityHandle.IsValid())
    {
        // ClearAbility also ends active instances and their ability tasks on character removal.
        const FGameplayAbilitySpecHandle HandleToRemove = SkillAbilityHandle;
        SkillAbilityHandle = FGameplayAbilitySpecHandle();
        CombatAbilitySystem->ClearAbility(HandleToRemove);
    }
    Super::EndPlay(EndPlayReason);
}

// Called to bind functionality to input
void ATestCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* enhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (enhancedInputComponent)
	{
		/*enhancedInputComponent->BindAction(IA_FrontMove, ETriggerEvent::Triggered, this, &ATestCharacter::OnFrontMovementInput);
		enhancedInputComponent->BindAction(IA_FrontMove, ETriggerEvent::Completed, this, &ATestCharacter::OnFrontMovementComplete);
		enhancedInputComponent->BindAction(IA_SideMove, ETriggerEvent::Triggered, this, &ATestCharacter::OnSideMovementInput);
		enhancedInputComponent->BindAction(IA_SideMove, ETriggerEvent::Completed, this, &ATestCharacter::OnSideMovementComplete);*/
        enhancedInputComponent->BindAction(IA_Move, ETriggerEvent::Triggered, this, &ATestCharacter::OnMovementInput);
		enhancedInputComponent->BindAction(IA_HorizonSight, ETriggerEvent::Triggered, this, &ATestCharacter::OnHorizonSightInput);
		enhancedInputComponent->BindAction(IA_VerticalSight, ETriggerEvent::Triggered, this, &ATestCharacter::OnVerticalSightInput);
		enhancedInputComponent->BindAction(IA_Attack, ETriggerEvent::Started, this, &ATestCharacter::OnAttackStarted);
		enhancedInputComponent->BindAction(IA_Attack, ETriggerEvent::Completed, this, &ATestCharacter::OnAttackCompleted);
		enhancedInputComponent->BindAction(IA_Roll, ETriggerEvent::Started, this, &ATestCharacter::OnRollInput);
		enhancedInputComponent->BindAction(IA_Interact, ETriggerEvent::Started, this, &ATestCharacter::OnInteract);
		enhancedInputComponent->BindAction(IA_Skill, ETriggerEvent::Started, this, &ATestCharacter::OnSkillInput);
        enhancedInputComponent->BindAction(IA_WeaponSwap, ETriggerEvent::Started, this, &ATestCharacter::OnWeaponSwap);
    }
}

void ATestCharacter::InitializeCharacterStat()
{
    if (ResourceManager)
    {
        CharacterSubsystem = GetGameInstance()->GetSubsystem<UCharacterSubsystem>();
        if (CharacterSubsystem)
        {
            UE_LOG(LogTemp, Warning, TEXT("Initializing Character Stats"));
            FPlayerSaveData CurrentPlayerData = CharacterSubsystem->GetCurrentPlayerData();
            ResourceManager->SetMaxHealth(CurrentPlayerData.MaxHealth);
            ResourceManager->SetMaxEnergy(CurrentPlayerData.MaxEnergy);
            CombatAbilitySystem->SetBaseMoveSpeed(CurrentPlayerData.WalkSpeed);
        }
    }
}

void ATestCharacter::SetPlayerActivatedWeapon(EWeaponType InActivatedWeapon)
{
    ActivatedWeapon = InActivatedWeapon;

    if (!CrosshairWidget) return;

    if (InActivatedWeapon == EWeaponType::Gun)
        CrosshairWidget->SetVisibility(ESlateVisibility::Visible);
    else
        CrosshairWidget->SetVisibility(ESlateVisibility::Hidden);
}

void ATestCharacter::PlayerAttack()
{
}

void ATestCharacter::NotifyActorBeginOverlap(AActor* OtherActor)
{
    Super::NotifyActorBeginOverlap(OtherActor);

    if (OtherActor && OtherActor->GetClass()->ImplementsInterface(UInteractable::StaticClass()))
    {
        CurrentInteractTarget = OtherActor;
    }
}

void ATestCharacter::NotifyActorEndOverlap(AActor* OtherActor)
{
    Super::NotifyActorEndOverlap(OtherActor);

    if (OtherActor == CurrentInteractTarget)
    {
        CurrentInteractTarget = nullptr;
    }
}

void ATestCharacter::TakeAnyDamage(AActor* DamagedActor, float InDamage, const UDamageType* DamageType, AController* InstigatedBy, AActor* DamageCauser)
{
    // Retained for existing Blueprint calls, with no separate health subtraction.
    UCombatFunctionLibrary::ApplyCombatDamage(this, InDamage, DamageCauser, InstigatedBy);
}


void ATestCharacter::KillPlayer()
{
    if (bDeathHandled || !HasAuthority()) return;
    bDeathHandled = true;
    CombatAbilitySystem->MarkDead();
    OnAttackCompleted();
    //캐릭터 이동 중지
    GetCharacterMovement()->DisableMovement();
    GetCharacterMovement()->StopMovementImmediately();

    //캡슐 충돌 끄기 
    GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    //메시를 Ragdoll 프로파일로
    USkeletalMeshComponent* MeshComp = GetMesh();
    MeshComp->SetCollisionProfileName(TEXT("Ragdoll"));

    //물리 활성화
    MeshComp->SetSimulatePhysics(true);

    //캡슐에서 분리 
    MeshComp->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);

    //애니메이션 재생을 막기 위함
    bIsOnAction = true;
    
    if (OnPlayerKilled.IsBound())
    {
        OnPlayerKilled.Broadcast();
    }
}

void ATestCharacter::InvincibleActivate()
{
    CombatAbilitySystem->BeginDodgeInvincibility();
}

void ATestCharacter::InvincibleDeactivate()
{
    CombatAbilitySystem->EndDodgeInvincibility();
}

void ATestCharacter::PlaySwordAttackMontage()
{
    if (!CombatAbilitySystem->CanAttack()) return;
    PlayAnimMontage(AttackMontage_Sword_Combo1, AttackSpeed);
}

void ATestCharacter::PlaySwordAttackMontage_Combo()
{
    if (!CombatAbilitySystem->CanAttack()) return;
    if (!IsValid(WeaponComponent))
	{
		return;
	}

	AWeaponBase* ActiveWeapon = WeaponComponent->GetActivateWeapon();
	UWeaponDataAsset* WeaponData = IsValid(ActiveWeapon) ? ActiveWeapon->GetWeaponData() : nullptr;
	if (WeaponData && UseEnergy(WeaponData->AttackCost))
    {
        PlayAnimMontage(AttackMontage_Sword_Combo2, AttackSpeed);
        WeaponComponent->WeaponAttack();
    }
}

void ATestCharacter::PlayGunShootingMontage()
{
    if (!CombatAbilitySystem->CanAttack()) return;
    PlayAnimMontage(AttackMontage_Gun);
}

void ATestCharacter::PlayDodgeMontage_Front_Sword()
{
    PlayAnimMontage(RollMontage_Sword);
}

void ATestCharacter::PlayDodgeMontage_Right_Sword()
{
    PlayAnimMontage(RightStepMontage_Sword);
}

void ATestCharacter::PlayDodgeMontage_Left_Sword()
{
    PlayAnimMontage(LeftStepMontage_Sword);
}

void ATestCharacter::PlayDodgeMontage_Back_Sword()
{
    PlayAnimMontage(BackStepMontage_Sword);
}

void ATestCharacter::PlayDodgeMontage_Front_Gun()
{
    PlayAnimMontage(RollMontage_Gun);
}

void ATestCharacter::PlayDodgeMontage_Back_Gun()
{
    SetAnimRootMotionIgnore();
    FVector playerDirection = -GetActorForwardVector();                         //플레이어 기준 뒷쪽 방향 벡터
    LaunchCharacter(playerDirection * LaunchPlayerPower, false, false);         //플레이어를 뒷쪽으로 이동
    PlayAnimMontage(BackStepMontage_Gun);
}

void ATestCharacter::PlayDodgeMontage_Right_Gun()
{
    SetAnimRootMotionIgnore();
    FVector playerDirection = GetActorRightVector();                            //플레이어 기준 오른쪽 방향 벡터
    LaunchCharacter(playerDirection * LaunchPlayerPower, false, false);         //플레이어를 오른쪽으로 이동
    PlayAnimMontage(BackStepMontage_Gun);
}

void ATestCharacter::PlayDodgeMontage_Left_Gun()
{
    SetAnimRootMotionIgnore();
    FVector playerDirection = -GetActorRightVector();                           //플레이어 기준 왼쪽 방향 벡터
    LaunchCharacter(playerDirection * LaunchPlayerPower, false, false);         //플레이어를 왼쪽으로 이동
    PlayAnimMontage(BackStepMontage_Gun);
}


void ATestCharacter::OnMovementInput(const FInputActionValue& InValue)
{
    //FVector2D inputtedMovement = InValue.Get<FVector2D>();
    //FVector moveDirection(inputtedMovement.Y, inputtedMovement.X, 0.0f);
    //FQuat controlYawRotation = FQuat(FRotator(0, GetControlRotation().Yaw, 0));	// 컨트롤러의 Yaw회전을 따로 뽑아와서
    //moveDirection = controlYawRotation.RotateVector(moveDirection);	// 이동 방향에 적용


	//AddMovementInput(moveDirection);

    if (!bIsOnAction)
    {
        CurrentInput = InValue.Get<FVector2D>();

        if (Controller && !CurrentInput.IsZero())
        {
            FRotator controllerRotation = Controller->GetControlRotation();         //컨트롤러 회전 구하기
            FRotator controllerYawRotation(0, controllerRotation.Yaw, 0);           //컨트롤러 방향으로 플레이어 회전

            FVector Direction = FVector(CurrentInput.X, CurrentInput.Y, 0.0f);
            Direction = controllerYawRotation.RotateVector(Direction);
            AddMovementInput(Direction);
        }
    }
}

void ATestCharacter::OnFrontMovementInput(const FInputActionValue& InValue)
{
    /*if(!bIsOnAction)
    {
        FrontBackMove = InValue.Get<float>();
        if (Controller && (FrontBackMove != 0.0f))
        {
            FRotator controllerRotation = Controller->GetControlRotation();
            FRotator controllerYawRotation(0, controllerRotation.Yaw, 0);

            FVector Direction = FRotationMatrix(controllerYawRotation).GetUnitAxis(EAxis::X);
            AddMovementInput(Direction, FrontBackMove);
        }
    }*/
}


void ATestCharacter::OnSideMovementInput(const FInputActionValue& InValue)
{
    /*if (!bIsOnAction)
    {
        SideMove = InValue.Get<float>();
        if (Controller && (SideMove != 0.0f))
        {
            FRotator controllerRotation = Controller->GetControlRotation();
            FRotator controllerYawRotation(0, controllerRotation.Yaw, 0);

            FVector Direction = FRotationMatrix(controllerYawRotation).GetUnitAxis(EAxis::Y);
            AddMovementInput(Direction, SideMove);
        }
    }*/
}

void ATestCharacter::OnFrontMovementComplete()
{
    //FrontBackMove = 0.0f;
}

void ATestCharacter::OnSideMovementComplete()
{
    //SideMove = 0.0f;
}

void ATestCharacter::OnHorizonSightInput(const FInputActionValue& InValue)
{
    AddControllerYawInput((InValue.Get<float>()) * HorizonMouseSensivility);	        //마우스 움직이는 값 * 마우스 감도만큼 움직임
}

void ATestCharacter::OnVerticalSightInput(const FInputActionValue& InValue)
{
    SightDegree = ((InValue.Get<float>()) * VerticalMouseSensivility) * Reverse;		//마우스 움직이는 값 * 마우스 감도만큼 움직임
    PlayerVerticalDegree += SightDegree;

    if (PlayerVerticalDegree > MaxSightAngle)		//위로 넘어가지 않도록
    {
        PlayerVerticalDegree = MaxSightAngle;
        SightDegree = 0.0f;
    }
    else if (PlayerVerticalDegree < -MaxSightAngle)	//아래로 넘어가지 않도록
    {
        PlayerVerticalDegree = -MaxSightAngle;
        SightDegree = 0.0f;
    }

    AddControllerPitchInput(SightDegree);
}

void ATestCharacter::OnAttack(bool bIsAutoFiring)
{
    if (!CombatAbilitySystem->CanAttack()) { OnAttackCompleted(); return; }
	if (!IsValid(WeaponComponent) || !GetWorld())
	{
		return;
	}

	AWeaponBase* ActiveWeapon = WeaponComponent->GetActivateWeapon();
	UWeaponDataAsset* WeaponData = IsValid(ActiveWeapon) ? ActiveWeapon->GetWeaponData() : nullptr;
	if (!WeaponData)
	{
		return;
	}

    const float Cost = WeaponData->AttackCost;
    AttackSpeed = WeaponData->AttackSpeed;
    
    // 공격 속도에 따른 최소 시간 계산
    float MinInterval = (AttackSpeed > 0) ? (1.0f / AttackSpeed) : 1.0f;
    float CurrentTime = GetWorld()->GetTimeSeconds();

    // 마우스 연타시에만 쿨타임 체크
    if ((!bIsAutoFiring) && (ActivatedWeapon == EWeaponType::Gun))
    {
        if (CurrentTime - LastAttackTime < MinInterval)
        {
            return;
        }
    }
    
    if (IsActionAvailable())
    {
        if (UseEnergy(Cost))
        {
            LastAttackTime = CurrentTime;
			if (IsValid(PlayerAnimation))
			{
				PlayerAnimation->PlayAttackAnimation();
			}
            WeaponComponent->WeaponAttack();
        }
        else
        {
            // 에너지가 부족하면 연사 중단
            OnAttackCompleted();
            WeaponComponent->SwitchToBaseWeapon();
        }
    }
    else if (bIsOnAttacking && bIsComboInputAvailable)
    {
        // 총일 때는 콤보 입력을 무시하도록 설정
        if (WeaponComponent->GetCurrentWeaponType() == EWeaponType::Sword)
        {
               bIsOnComboInput = true;
        }
    }
}

void ATestCharacter::OnAttackStarted()
{
    if (!CombatAbilitySystem->CanAttack()) return;
    // 이미 공격 타이머가 작동 중이라면 중복 실행 방지
    if (GetWorldTimerManager().IsTimerActive(AttackTimerHandle)) return;

    // 누르면 공격 일단 실행
    OnAttack(false);

    // 무기 데이터에서 공격 속도 가져옴
	if (!IsValid(WeaponComponent))
	{
		return;
	}

	AWeaponBase* ActiveWeapon = WeaponComponent->GetActivateWeapon();
	UWeaponDataAsset* WeaponData = IsValid(ActiveWeapon) ? ActiveWeapon->GetWeaponData() : nullptr;
	if (!WeaponData)
	{
		return;
	}

    const float FireRate = WeaponData->AttackSpeed;
    if (FireRate > 0)
    {
        float interval = 1.0f / FireRate;

        // 연사 타이머에서 true를 넘겨 타이머 재생 방지
        GetWorldTimerManager().SetTimer(
            AttackTimerHandle,
            FTimerDelegate::CreateUObject(this, &ATestCharacter::OnAttack, true),
            interval,
            true
        );
    }
}

void ATestCharacter::OnAttackCompleted()
{
    GetWorldTimerManager().ClearTimer(AttackTimerHandle);
}

void ATestCharacter::HandleAttackBlocked()
{
    OnAttackCompleted();
    bIsOnComboInput = false;
    bIsComboInputAvailable = false;
    bIsOnAttacking = false;
    if (WeaponComponent)
        if (AWeaponBase* Weapon = WeaponComponent->GetActivateWeapon()) Weapon->CancelAttack();
    if (AttackMontage_Sword_Combo1) StopAnimMontage(AttackMontage_Sword_Combo1);
    if (AttackMontage_Sword_Combo2) StopAnimMontage(AttackMontage_Sword_Combo2);
    if (AttackMontage_Gun) StopAnimMontage(AttackMontage_Gun);
}

void ATestCharacter::OnSkillInput()
{
    TryUseCharacterSkill();
}

bool ATestCharacter::TryUseCharacterSkill()
{
    // The current combat system is authority-only (single player).
    if (!HasAuthority() || IsActorBeingDestroyed() || !CombatAbilitySystem ||
        !CombatAbilitySystem->IsCombatInitialized() || CombatAbilitySystem->IsDead() ||
        !SkillAbilityHandle.IsValid())
    {
        return false;
    }

    const FGameplayAbilitySpec* SkillSpec = CombatAbilitySystem->FindAbilitySpecFromHandle(SkillAbilityHandle);
    if (!SkillSpec || SkillSpec->IsActive()) return false;

    // Let each GA decide its own blocked tags, cooldown and cost; buffs need not require a weapon.
    return CombatAbilitySystem->TryActivateAbility(SkillAbilityHandle, false);
}

void ATestCharacter::OnInteract()
{
    if (!CurrentInteractTarget) return;

    if (CurrentInteractTarget->GetClass()->ImplementsInterface(UInteractable::StaticClass()))
    {
        IInteractable::Execute_Interact(CurrentInteractTarget, this);
        CurrentInteractTarget = nullptr;
    }
}

bool ATestCharacter::IsActionAvailable()
{
    if (!bIsOnAction && !bIsOnAttacking && (ActivatedWeapon != EWeaponType::None))
    {
        return true;
    }
    return false;
}

void ATestCharacter::OnHitInvincible()
{
    CombatAbilitySystem->ApplyHitInvincibility();
}

void ATestCharacter::OnWeaponSwap()
{
    if(IsActionAvailable())
    {
        WeaponComponent->SwapWeapon();
    }
}

void ATestCharacter::RotatePlayer(EMovingDirection TurnDirection)
{
    FRotator currentRotation = GetActorRotation();
    if (TurnDirection == EMovingDirection::Right)
    {
        currentRotation.Yaw += AnimRotateDegree;
    }
    else if (TurnDirection == EMovingDirection::Left)
    {
        currentRotation.Yaw -= AnimRotateDegree;
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("ATestCharacter::RotatePlayer | wrong direction"));
    }
    SetActorRotation(currentRotation);
    bIsOnAction = true;
}

void ATestCharacter::SetAnimRootMotionIgnore()
{
    AnimInstance->SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);
}

void ATestCharacter::SetAnimRootMotionFromMontage()
{
    AnimInstance->SetRootMotionMode(ERootMotionMode::RootMotionFromMontagesOnly);
}

bool ATestCharacter::UseEnergy(float InEnergyUseAmount)
{
    return ResourceManager->UseEnergy(InEnergyUseAmount);
}

void ATestCharacter::FillEnergy(float InEnergy)
{
    ResourceManager->FillEnergy(InEnergy);
}

void ATestCharacter::Heal(float InHeal)
{
    ResourceManager->Heal(InHeal);
}

void ATestCharacter::AddPower(float InPower)
{
    ResourceManager->AddPower(InPower);
}

void ATestCharacter::AddMaxHealth(float InMaxHealth)
{
    ResourceManager->AddMaxHealth(InMaxHealth);
}

void ATestCharacter::AddMaxEnergy(float InMaxEnergy)
{
    ResourceManager->AddMaxEnergy(InMaxEnergy);
}

void ATestCharacter::AddMoveSpeed(float InMoveSpeed)
{
    CombatAbilitySystem->SetBaseMoveSpeed(CombatAbilitySystem->GetBaseMoveSpeed() + InMoveSpeed);

    if (CharacterSubsystem)
    {
        CharacterSubsystem->SetWalkSpeed(CombatAbilitySystem->GetBaseMoveSpeed());
    }
}

EMovingDirection ATestCharacter::GetPlayerDirection()
{
    EMovingDirection playerDirection = EMovingDirection::None;

    if (CurrentInput.X > 0)
    {
        if (CurrentInput.Y > 0)
            playerDirection = EMovingDirection::FrontRight;
        else if (CurrentInput.Y < 0)
            playerDirection = EMovingDirection::FrontLeft;
        else
            playerDirection = EMovingDirection::Front;
    }
    else if (CurrentInput.X < 0)
    {
        if (CurrentInput.Y > 0)
            playerDirection = EMovingDirection::BackRight;
        else if (CurrentInput.Y < 0)
            playerDirection = EMovingDirection::BackLeft;
        else
            playerDirection = EMovingDirection::Back;
    }
    else
    {
        if (CurrentInput.Y > 0)
            playerDirection = EMovingDirection::Right;
        else if (CurrentInput.Y < 0)
            playerDirection = EMovingDirection::Left;
    }

    return playerDirection;
}

void ATestCharacter::SetOnActing(bool InActing)
{
    if (!InActing)
    {
        MovementComponent->bUseControllerDesiredRotation = true;
        MovementComponent->bOrientRotationToMovement = false;
        bUseControllerRotationYaw = true;

        if (Controller)
        {
            FRotator controllerRotation = Controller->GetControlRotation();
            SetActorRotation(controllerRotation);
        }
    }
    bIsOnAction = InActing;
}

void ATestCharacter::SetOnAttacking(bool InAttacking)
{
    bIsOnAttacking = InAttacking;
}

void ATestCharacter::OnRollInput()
{
    if (!bIsOnAction && (ActivatedWeapon != EWeaponType::None))
    {
        MovementComponent->bUseControllerDesiredRotation = false;
        MovementComponent->bOrientRotationToMovement = true;
        bUseControllerRotationYaw = false;
        PlayerAnimation->PlayRollMontage();
    }
}
