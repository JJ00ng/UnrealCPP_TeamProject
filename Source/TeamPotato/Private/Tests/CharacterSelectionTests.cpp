#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Subsystem/CharacterSubsystem.h"
#include "Subsystems/SubsystemCollection.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/DefaultPawn.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "Player/TestCharacter.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "UObject/UnrealType.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
    struct FSelectionTestWorld
    {
        UWorld* World;

        explicit FSelectionTestWorld(UGameInstance* GameInstance)
        {
            World = UWorld::CreateWorld(EWorldType::GamePreview, false);
            GEngine->CreateNewWorldContext(EWorldType::GamePreview).SetCurrentWorld(World);
            World->SetGameInstance(GameInstance);
            World->InitializeActorsForPlay(FURL());
        }

        ~FSelectionTestWorld()
        {
            World->DestroyWorld(false);
            GEngine->DestroyWorldContext(World);
        }

        AGameModeBase* InitializeMode(UClass* DefaultClass, UClass* ModeClass = AGameModeBase::StaticClass())
        {
            auto* Mode = World->SpawnActor<AGameModeBase>(ModeClass);
            Mode->DefaultPawnClass = DefaultClass;
            FString Error;
            // Exercise the engine event used by OpenLevel, before player spawning.
            Mode->InitGame(TEXT("CharacterSelectionTest"), FString(), Error);
            return Mode;
        }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCharacterSelectionTravelTest, "TeamPotato.Character.SelectionAcrossMaps",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCharacterSelectionTravelTest::RunTest(const FString& Parameters)
{
    TStrongObjectPtr<UGameInstance> Instance(NewObject<UGameInstance>(GEngine));
    TStrongObjectPtr<UCharacterSubsystem> Selection(NewObject<UCharacterSubsystem>(Instance.Get()));
    FSubsystemCollection<UGameInstanceSubsystem> Collection;
    Selection->Initialize(Collection);

    // Read real BP defaults without BeginPlay/UI/level dependencies.
    UClass* Engineer = LoadClass<ATestCharacter>(nullptr,
        TEXT("/Game/BluePrints/Test/Player/BP_PlayerEngne.BP_PlayerEngne_C"));
    UClass* GameplayMode = LoadClass<AGameModeBase>(nullptr,
        TEXT("/Game/BluePrints/Test/Framework/GM_PlayerTest.GM_PlayerTest_C"));
    if (!TestNotNull(TEXT("Engineer character asset loads"), Engineer) ||
        !TestNotNull(TEXT("Actual gameplay GameMode asset loads"), GameplayMode))
    {
        Selection->Deinitialize();
        return false;
    }
    FCharacterDataTableRow Row;
    Row.CharacterType = ECharacterType::Gunner;
    Row.CharacterClass = Engineer;
    Selection->CharacterDataCache.Add(Row.CharacterType, Row);
    Selection->PlayerSaveData.SelectedCharacterType = Row.CharacterType;
    Selection->PlayerSaveData.MaxHealth = 237.0f;

    // Successive worlds share selection, just like lobby -> dungeon -> another chapter.
    for (int32 MapIndex = 0; MapIndex < 2; ++MapIndex)
    {
        FSelectionTestWorld Fixture(Instance.Get());
        AGameModeBase* Mode = Fixture.InitializeMode(
            GameplayMode->GetDefaultObject<AGameModeBase>()->DefaultPawnClass, GameplayMode);
        TestTrue(TEXT("New map uses selected character instead of fixed default"),
            Mode->DefaultPawnClass.Get() == Engineer);
        auto* Controller = Fixture.World->SpawnActor<APlayerController>();
        auto* Player = Cast<ATestCharacter>(Mode->SpawnDefaultPawnAtTransform(Controller, FTransform::Identity));
        if (TestNotNull(TEXT("GameMode spawns selected player"), Player))
        {
            TestTrue(TEXT("Spawned class is the selected BP"), Player->GetClass() == Engineer);
            auto* Property = FindFProperty<FClassProperty>(ATestCharacter::StaticClass(), TEXT("SkillAbilityClass"));
            auto* ExpectedSkill = Cast<UClass>(Property->GetObjectPropertyValue_InContainer(Engineer->GetDefaultObject()));
            if (TestNotNull(TEXT("Engineer BP has a configured skill"), ExpectedSkill))
            {
                TestNotNull(TEXT("New character receives its own configured ability"),
                    Player->GetAbilitySystemComponent()->FindAbilitySpecFromClass(ExpectedSkill));
            }
        }
        TestEqual(TEXT("Travel preserves saved stats"), Selection->PlayerSaveData.MaxHealth, 237.0f);
        TestTrue(TEXT("Menu default is left untouched"),
            Fixture.InitializeMode(ADefaultPawn::StaticClass())->DefaultPawnClass == ADefaultPawn::StaticClass());
        TestNull(TEXT("No-pawn menu is left untouched"), Fixture.InitializeMode(nullptr)->DefaultPawnClass.Get());
    }

    {
        TStrongObjectPtr<UGameInstance> OtherInstance(NewObject<UGameInstance>(GEngine));
        FSelectionTestWorld OtherWorld(OtherInstance.Get());
        TestTrue(TEXT("Global event cannot change a different game instance"),
            OtherWorld.InitializeMode(ATestCharacter::StaticClass())->DefaultPawnClass == ATestCharacter::StaticClass());
    }

    {
        FSelectionTestWorld Fixture(Instance.Get());
        Selection->CharacterDataCache[Row.CharacterType].CharacterClass = nullptr;
        AddExpectedError(TEXT("Character selection has no spawnable class"), EAutomationExpectedErrorFlags::Contains, 2);
        TestTrue(TEXT("Unset selected class preserves valid fallback"),
            Fixture.InitializeMode(ATestCharacter::StaticClass())->DefaultPawnClass == ATestCharacter::StaticClass());
        Selection->PlayerSaveData.SelectedCharacterType = ECharacterType::None;
        Selection->CharacterDataCache.Remove(ECharacterType::None);
        TestTrue(TEXT("Missing selected row preserves valid fallback"),
            Fixture.InitializeMode(ATestCharacter::StaticClass())->DefaultPawnClass == ATestCharacter::StaticClass());

        Selection->PlayerSaveData.SelectedCharacterType = Row.CharacterType;
        Selection->CharacterDataCache.Add(Row.CharacterType, Row);
        Selection->Deinitialize();
        TestFalse(TEXT("Shutdown clears delegate handle"), Selection->GameModeInitializedHandle.IsValid());
        TestTrue(TEXT("Shutdown unsubscribes from new map events"),
            Fixture.InitializeMode(ATestCharacter::StaticClass())->DefaultPawnClass == ATestCharacter::StaticClass());
    }

    TestTrue(TEXT("GameMode class defaults were not modified"),
        GetDefault<AGameModeBase>()->DefaultPawnClass == ADefaultPawn::StaticClass());
    return true;
}

#endif
