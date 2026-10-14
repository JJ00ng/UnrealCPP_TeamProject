// Fill out your copyright notice in the Description page of Project Settings.


#include "Subsystem/CharacterSubsystem.h"
#include "Common/MyGameSettings.h"
#include "Player/TestCharacter.h"
#include "Component/WeaponComponent.h"
#include "GameFramework/GameModeBase.h"

void UCharacterSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	//UE_LOG(LogTemp, Warning, TEXT("UCharacterSubsystem::Initialize"));

	LoadAndCacheCharacterData();

    GameModeInitializedHandle = FGameModeEvents::OnGameModeInitializedEvent().AddUObject(
        this, &ThisClass::HandleGameModeInitialized);

    if(CharacterDataCache.Num() > 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("UCharacterSubsystem::Initialize - Character data loaded and cached successfully. Total Characters: %d"), CharacterDataCache.Num());
        
        // 초기 선택된 캐릭터 타입 설정 (데이터 테이블의 첫 번째 캐릭터)
        PlayerSaveData.SelectedCharacterType = CharacterDataCache.CreateConstIterator().Key();
        PlayerSaveData.EquippedMainWeapon = CharacterDataCache.Find(PlayerSaveData.SelectedCharacterType)->DefaultWeaponDataAsset;
        PlayerSaveData.EquippedSubWeapon = nullptr;
        PlayerSaveData.PlayerIcon = CharacterDataCache.Find(PlayerSaveData.SelectedCharacterType)->CharacterIcon;
        PlayerSaveData.PlayerIllustration = CharacterDataCache.Find(PlayerSaveData.SelectedCharacterType)->CharacterIllustration;
        PlayerSaveData.MaxHealth = CharacterDataCache.Find(PlayerSaveData.SelectedCharacterType)->MaxHealth;
        PlayerSaveData.MaxEnergy = CharacterDataCache.Find(PlayerSaveData.SelectedCharacterType)->MaxEnergy;
        PlayerSaveData.WalkSpeed = CharacterDataCache.Find(PlayerSaveData.SelectedCharacterType)->WalkSpeed;
    }
}

void UCharacterSubsystem::Deinitialize()
{
	FGameModeEvents::OnGameModeInitializedEvent().Remove(GameModeInitializedHandle);
	GameModeInitializedHandle.Reset();
	//UE_LOG(LogTemp, Warning, TEXT("UCharacterSubsystem::Deinitialized"));

	CharacterDataTable = nullptr;
	CharacterDataCache.Empty();

	Super::Deinitialize();
}

void UCharacterSubsystem::HandleGameModeInitialized(AGameModeBase* GameMode)
{
    // The delegate is global: never change another PIE/game instance or menu/spectator modes.
    if (!IsValid(GameMode) || GameMode->GetGameInstance() != GetGameInstance() ||
        !GameMode->HasAuthority() || !GameMode->DefaultPawnClass ||
        !GameMode->DefaultPawnClass->IsChildOf(ATestCharacter::StaticClass()))
    {
        return;
    }

    const FCharacterDataTableRow* SelectedRow = CharacterDataCache.Find(PlayerSaveData.SelectedCharacterType);
    if (!SelectedRow || !SelectedRow->CharacterClass ||
        SelectedRow->CharacterClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
    {
        UE_LOG(LogTemp, Warning, TEXT("Character selection has no spawnable class for type %d; keeping GameMode default."),
            static_cast<int32>(PlayerSaveData.SelectedCharacterType));
        return;
    }

    // Change this world's GameMode instance, not its BP defaults or the selected player's saved stats.
    GameMode->DefaultPawnClass = SelectedRow->CharacterClass;
    UE_LOG(LogTemp, Log, TEXT("Selected character for %s: %s"),
        *GetNameSafe(GameMode), *GetNameSafe(SelectedRow->CharacterClass.Get()));
}

void UCharacterSubsystem::LoadAndCacheCharacterData()
{
	UMyGameSettings* GameSettings = UMyGameSettings::Get();
	if (!GameSettings)
	{
		UE_LOG(LogTemp, Error, TEXT("UCharacterSubsystem::GetCharacterData - GameSettings is null"));
		return;
	}

	if (GameSettings->CharacterDataTable.IsNull())
	{
		UE_LOG(LogTemp, Warning, TEXT("UCharacterSubsystem::GetCharacterData - CharacterDataTable is null"));
		return;
	}

	CharacterDataTable = GameSettings->CharacterDataTable.LoadSynchronous();

	if (!CharacterDataTable)
	{
		UE_LOG(LogTemp, Error, TEXT("UCharacterSubsystem::GetCharacterData - Failed to load CharacterDataTable"));
		return;
	}

	// 데이터 테이블을 TMap으로 캐싱
	TArray<FName> RowNames = CharacterDataTable->GetRowNames();

	for (const FName& RowName : RowNames)
	{
		FCharacterDataTableRow* RowData = CharacterDataTable->FindRow<FCharacterDataTableRow>(RowName, TEXT("UCharacterSubsystem::GetCharacterData"));
		if (RowData)
		{
			CharacterDataCache.Add(RowData->CharacterType, *RowData);
		}
	}
}

// =============================================================
// 데이터 테이블
// =============================================================

const FCharacterDataTableRow& UCharacterSubsystem::GetCharacterDataRowByType(ECharacterType InCharacterType) const
{
    const FCharacterDataTableRow* FoundRow = CharacterDataCache.Find(InCharacterType);

    if (FoundRow)
    {
        return *FoundRow;
    }
    else
    {
        // 기본값 반환 또는 예외 처리
        UE_LOG(LogTemp, Error, TEXT("UCharacterSubsystem::GetCharacterDataRowByType - Character type %d not found in cache"), static_cast<int32>(InCharacterType));
        static FCharacterDataTableRow DefaultRow;
        return DefaultRow;
    }
}

void UCharacterSubsystem::ResetPlayerDataToInitialState()
{
    PlayerSaveData.EquippedMainWeapon = CharacterDataCache.Find(PlayerSaveData.SelectedCharacterType)->DefaultWeaponDataAsset;
    PlayerSaveData.EquippedSubWeapon = nullptr;
    PlayerSaveData.PlayerIcon = CharacterDataCache.Find(PlayerSaveData.SelectedCharacterType)->CharacterIcon;
    PlayerSaveData.PlayerIllustration = CharacterDataCache.Find(PlayerSaveData.SelectedCharacterType)->CharacterIllustration;
    PlayerSaveData.MaxHealth = CharacterDataCache.Find(PlayerSaveData.SelectedCharacterType)->MaxHealth;
    PlayerSaveData.MaxEnergy = CharacterDataCache.Find(PlayerSaveData.SelectedCharacterType)->MaxEnergy;
    PlayerSaveData.WalkSpeed = CharacterDataCache.Find(PlayerSaveData.SelectedCharacterType)->WalkSpeed;
}

// =============================================================
// 선택된 캐릭터
// =============================================================

// 선택된 캐릭터 타입 설정
void UCharacterSubsystem::SetSelectedCharacterType(ECharacterType NewCharacterType)
{
    // 캐릭터 변경시에 기본 캐릭터 상태로 초기화
    PlayerSaveData.SelectedCharacterType = NewCharacterType;
    PlayerSaveData.EquippedMainWeapon = CharacterDataCache.Find(NewCharacterType)->DefaultWeaponDataAsset;
    PlayerSaveData.EquippedSubWeapon = nullptr;
    PlayerSaveData.PlayerIcon = CharacterDataCache.Find(NewCharacterType)->CharacterIcon;
    PlayerSaveData.PlayerIllustration = CharacterDataCache.Find(NewCharacterType)->CharacterIllustration;
    PlayerSaveData.MaxHealth = CharacterDataCache.Find(NewCharacterType)->MaxHealth;
    PlayerSaveData.MaxEnergy = CharacterDataCache.Find(NewCharacterType)->MaxEnergy;
    PlayerSaveData.WalkSpeed = CharacterDataCache.Find(NewCharacterType)->WalkSpeed;

    // 선택된 캐릭터 타입으로 빙의
    PossessSelectedCharacterType(PlayerSaveData.SelectedCharacterType);

	//UE_LOG(LogTemp, Log, TEXT("UCharacterSubsystem::SetSelectedCharacterType - Changed to %d"), static_cast<int32>(NewCharacterType));
}

void UCharacterSubsystem::SetEquippedMainWeapon(UWeaponDataAsset* NewMainWeapon)
{
    if (PlayerSaveData.EquippedMainWeapon != NewMainWeapon)
    {
        PlayerSaveData.EquippedMainWeapon = NewMainWeapon;
    }
}

void UCharacterSubsystem::SetEquippedSubWeapon(UWeaponDataAsset* NewSubWeapon)
{
    if (PlayerSaveData.EquippedSubWeapon != NewSubWeapon)
    {
        PlayerSaveData.EquippedSubWeapon = NewSubWeapon;
    }
}

// 선택된 캐릭터 타입으로 빙의
void UCharacterSubsystem::PossessSelectedCharacterType(ECharacterType InCharacterType)
{
    // 현재 플레이어 폰의 위치와 회전을 가져옴
    APawn* CurrentPawn = GetWorld()->GetFirstPlayerController()->GetPawn();

    FVector SpawnLocation = CurrentPawn->GetActorLocation();
    FRotator SpawnRotation = CurrentPawn->GetActorRotation();

    // 현재 플레이어 폰을 제거
    CurrentPawn->Destroy();

    // 새로운 캐릭터 타입에 해당하는 폰을 스폰
    TSubclassOf<ATestCharacter> SpawnCharacter = CharacterDataCache.Find(InCharacterType)->CharacterClass;

    ATestCharacter* NewPawn = GetWorld()->SpawnActor<ATestCharacter>(SpawnCharacter, SpawnLocation, SpawnRotation);
    if (NewPawn)
    {
        GetWorld()->GetFirstPlayerController()->Possess(NewPawn);
        // Keep later respawns in this world consistent with the new selection too.
        HandleGameModeInitialized(GetWorld()->GetAuthGameMode());
    }
}


