#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Combat/RangedTargetingComponent.h"
#include "Combat/CombatAbilitySystemComponent.h"
#include "Combat/CombatAttributeSet.h"
#include "Combat/TurretProjectile.h"
#include "Components/BoxComponent.h"
#include "Enemy/EnemyCharacter.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Item/Weapon/GunWeaponActor.h"
#include "Item/Weapon/LaserGunWeaponActor.h"
#include "Item/Weapon/RangeSwordWeaponActor.h"
#include "Item/Weapon/BulletActor.h"
#include "Item/Weapon/SlashActor.h"
#include "Player/TestCharacter.h"
#include "UObject/UnrealType.h"
#include <limits>

namespace
{
    struct FTargetingWorld
    {
        UWorld* World;
        ATestCharacter* Player;
        FTargetingWorld()
        {
            World = UWorld::CreateWorld(EWorldType::GamePreview, false);
            GEngine->CreateNewWorldContext(EWorldType::GamePreview).SetCurrentWorld(World);
            World->InitializeActorsForPlay(FURL());
            Player = Spawn<ATestCharacter>(FVector(0, 0, 100));
        }
        ~FTargetingWorld()
        {
            World->DestroyWorld(false);
            GEngine->DestroyWorldContext(World);
        }
        template<class T> T* Spawn(FVector Location)
        {
            FActorSpawnParameters Params;
            Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            return World->SpawnActor<T>(Location, FRotator::ZeroRotator, Params);
        }
        AActor* Wall(FVector Location)
        {
            auto* Actor = Spawn<AActor>(Location);
            auto* Box = NewObject<UBoxComponent>(Actor);
            Actor->AddInstanceComponent(Box);
            Actor->SetRootComponent(Box);
            Box->InitBoxExtent(FVector(20, 60, 200));
            Box->SetCollisionProfileName(TEXT("BlockAll"));
            Box->RegisterComponent();
            Actor->SetActorLocation(Location);
            return Actor;
        }
        void ConfigureClass(UObject* Object, FName PropertyName, UClass* Value)
        {
            auto* Property = FindFProperty<FClassProperty>(Object->GetClass(), PropertyName);
            check(Property);
            Property->SetObjectPropertyValue_InContainer(Object, Value);
        }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRangedTargetSelectionTest, "TeamPotato.Combat.LockOn.TargetSelectionAndOcclusion",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRangedTargetSelectionTest::RunTest(const FString& Parameters)
{
    FTargetingWorld Fixture;
    auto* Targeting = Fixture.Player->GetRangedTargeting();
    const FVector Origin(50, 0, 130);
    auto* Near = Fixture.Spawn<AEnemyCharacter>(FVector(400, 200, 100));
    auto* Far = Fixture.Spawn<AEnemyCharacter>(FVector(900, -400, 180));
    Fixture.Spawn<AEnemyCharacter>(FVector(-150, 0, 100));
    Fixture.Spawn<AEnemyCharacter>(FVector(0, 150, 100));
    Fixture.Spawn<AEnemyCharacter>(FVector(1800, 0, 100));
    TestTrue(TEXT("Closest enemy in forward cone selected"), Targeting->FindLockOnTarget(Origin) == Near);
    Near->SetActorHiddenInGame(true);
    TestTrue(TEXT("Hidden pooled target excluded"), Targeting->FindLockOnTarget(Origin) == Far);
    Near->SetActorHiddenInGame(false);
    Near->SetCanBeDamaged(false);
    TestTrue(TEXT("Non-damageable target excluded"), Targeting->FindLockOnTarget(Origin) == Far);
    Near->SetCanBeDamaged(true);
    AActor* Wall = Fixture.Wall(FVector(200, 100, 100));
    TestTrue(TEXT("Wall excludes near target but allows visible alternative"), Targeting->FindLockOnTarget(Origin) == Far);
    // A muzzle moved past the wall must still not acquire enemies hidden from the caster.
    Far->SetActorHiddenInGame(true);
    TestNull(TEXT("Offset muzzle cannot acquire through caster-side wall"), Targeting->FindLockOnTarget(FVector(300, 150, 130)));
    Far->SetActorHiddenInGame(false);
    Wall->Destroy();
    auto* NearASC = CastChecked<UCombatAbilitySystemComponent>(Near->GetAbilitySystemComponent());
    NearASC->SetNumericAttributeBase(UCombatAttributeSet::GetHealthAttribute(), 0.0f);
    TestTrue(TEXT("Dead target excluded on next shot"), Targeting->FindLockOnTarget(Origin) == Far);
    Targeting->LockOnRange = 300.0f;
    TestNull(TEXT("Range, rear and cone filters reject remaining candidates"), Targeting->FindLockOnTarget(Origin));
    Targeting->LockOnRange = 1500.0f;
    Targeting->LockOnHalfAngle = 10.0f;
    TestNull(TEXT("Narrow cone rejects off-axis target"), Targeting->FindLockOnTarget(Origin));
    Targeting->LockOnHalfAngle = 60.0f;
    Targeting->bEnableLockOn = false;
    TestTrue(TEXT("Disabled assistance returns normalized original aim"),
        Targeting->ResolveFireDirection(Origin, FVector(0, 3, 0)).Equals(FVector::RightVector));
    Targeting->bEnableLockOn = true;
    Targeting->LockOnRange = std::numeric_limits<float>::quiet_NaN();
    TestNull(TEXT("Invalid configuration safely disables acquisition"), Targeting->FindLockOnTarget(Origin));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRangedWeaponLockOnTest, "TeamPotato.Combat.LockOn.GunLaserAndSlashIntegration",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRangedWeaponLockOnTest::RunTest(const FString& Parameters)
{
    FTargetingWorld Fixture;
    auto* Enemy = Fixture.Spawn<AEnemyCharacter>(FVector(500, 200, 180));
    const FVector Muzzle(100, 0, 130);
    const FVector Expected = (Enemy->GetActorLocation() - Muzzle).GetSafeNormal();
    auto* Gun = Fixture.Spawn<AGunWeaponActor>(Muzzle);
    Gun->SetOwner(Fixture.Player);
    Gun->SetOwnerComponent(Fixture.Player->GetWeaponComponent());
    Fixture.ConfigureClass(Gun, TEXT("BulletClass"), ABulletActor::StaticClass());
    Gun->Attack(Fixture.Player);
    ABulletActor* First = nullptr;
    for (TActorIterator<ABulletActor> It(Fixture.World); It; ++It) First = *It;
    if (!TestNotNull(TEXT("Gun fires without requiring viewport for lock-on"), First)) return false;
    TestTrue(TEXT("Gun bullet velocity points at enemy"),
        First->FindComponentByClass<UProjectileMovementComponent>()->Velocity.GetSafeNormal().Equals(Expected, 0.001f));
    Fixture.Player->GetRangedTargeting()->bEnableLockOn = false;
    Gun->Attack(Fixture.Player);
    ABulletActor* Fallback = nullptr;
    for (TActorIterator<ABulletActor> It(Fixture.World); It; ++It) if (*It != First) Fallback = *It;
    if (!TestNotNull(TEXT("Gun still fires without lock-on"), Fallback)) return false;
    TestTrue(TEXT("No-viewport fallback is character forward"),
        Fallback->FindComponentByClass<UProjectileMovementComponent>()->Velocity.GetSafeNormal().Equals(FVector::ForwardVector, 0.001f));
    Fixture.Player->GetRangedTargeting()->bEnableLockOn = true;
    auto* Laser = Fixture.Spawn<ALaserGunWeaponActor>(Muzzle);
    Laser->SetOwner(Fixture.Player);
    Laser->SetOwnerComponent(Fixture.Player->GetWeaponComponent());
    Fixture.ConfigureClass(Laser, TEXT("LaserClass"), ATurretProjectile::StaticClass());
    static_cast<AWeaponBase*>(Laser)->Attack(Fixture.Player);
    ATurretProjectile* LaserShot = nullptr;
    for (TActorIterator<ATurretProjectile> It(Fixture.World); It; ++It) LaserShot = *It;
    if (!TestNotNull(TEXT("Laser projectile spawned"), LaserShot)) return false;
    TestTrue(TEXT("Laser rotation uses shared lock-on"), LaserShot->GetActorForwardVector().Equals(Expected, 0.001f));
    TestTrue(TEXT("Laser retains caster attribution"), LaserShot->GetInstigator() == Fixture.Player);
    LaserShot->Destroy();
    auto* Sword = Fixture.Spawn<ARangeSwordWeaponActor>(Muzzle);
    Sword->SetOwner(Fixture.Player);
    Fixture.ConfigureClass(Sword, TEXT("SlashActorClass"), ASlashActor::StaticClass());
    Sword->EndAttack();
    ASlashActor* Slash = nullptr;
    for (TActorIterator<ASlashActor> It(Fixture.World); It; ++It) Slash = *It;
    if (!TestNotNull(TEXT("Ranged sword projectile spawned"), Slash)) return false;
    TestTrue(TEXT("Slash velocity also uses shared lock-on"), Slash->ProjectileMovementComponent->Velocity.GetSafeNormal().Equals(
        (Enemy->GetActorLocation() - Slash->GetActorLocation()).GetSafeNormal(), 0.001f));
    return true;
}

#endif
