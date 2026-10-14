#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Combat/SummonedTurret.h"
#include "Combat/TurretProjectile.h"
#include "Combat/CombatAbilitySystemComponent.h"
#include "Combat/CombatAttributeSet.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Enemy/EnemyCharacter.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Player/TestCharacter.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include <limits>

namespace
{
    struct FTurretTestWorld
    {
        UWorld* World;
        uint64 InitialFrame = GFrameCounter;
        FTurretTestWorld()
        {
            World = UWorld::CreateWorld(EWorldType::GamePreview, false);
            GEngine->CreateNewWorldContext(EWorldType::GamePreview).SetCurrentWorld(World);
            World->InitializeActorsForPlay(FURL());
        }
        ~FTurretTestWorld()
        {
            World->DestroyWorld(false);
            GEngine->DestroyWorldContext(World);
            GFrameCounter = InitialFrame;
        }
        ASummonedTurret* Turret(FVector Location = FVector(0, 0, 100), APawn* Owner = nullptr,
            float Lifetime = 60.0f, float Interval = 1.0f)
        {
            const FTransform Transform(Location);
            auto* Turret = World->SpawnActorDeferred<ASummonedTurret>(ASummonedTurret::StaticClass(),
                Transform, Owner, Owner, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
            Turret->MaxLifetime = Lifetime;
            Turret->FireInterval = Interval;
            Turret->FinishSpawning(Transform);
            Turret->DispatchBeginPlay();
            return Turret;
        }
        AEnemyCharacter* Enemy(FVector Location)
        {
            FActorSpawnParameters Params;
            Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            return World->SpawnActor<AEnemyCharacter>(Location, FRotator::ZeroRotator, Params);
        }
        AActor* Wall(FVector Location)
        {
            AActor* Wall = World->SpawnActor<AActor>();
            auto* Box = NewObject<UBoxComponent>(Wall);
            Wall->AddInstanceComponent(Box);
            Wall->SetRootComponent(Box);
            Box->InitBoxExtent(FVector(20, 120, 200));
            Box->SetCollisionProfileName(TEXT("BlockAll"));
            Box->RegisterComponent();
            Wall->SetActorLocation(Location);
            return Wall;
        }
        void Advance(float Seconds)
        {
            for (float Time = 0; Time < Seconds; Time += 0.025f)
            {
                // Start only our self-contained actors; enemy/player BeginPlay requires real game subsystems.
                for (TActorIterator<ATurretProjectile> It(World); It; ++It)
                    if (!It->HasActorBegunPlay()) It->DispatchBeginPlay();
                ++GFrameCounter;
                World->Tick(LEVELTICK_All, 0.025f);
            }
        }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurretTargetTest, "TeamPotato.Combat.Turret.TargetingAndProjectileDamage",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTurretTargetTest::RunTest(const FString& Parameters)
{
    FTurretTestWorld Fixture;
    auto* Turret = Fixture.Turret();
    auto* Near = Fixture.Enemy(FVector(500, 0, 100));
    auto* Far = Fixture.Enemy(FVector(0, 800, 100));
    Fixture.Enemy(FVector(2000, 0, 100));
    TestTrue(TEXT("Nearest living enemy selected"), Turret->FindNearestEnemy() == Near);
    const float Before = Near->GetCombatHealth();
    const float FarBefore = Far->GetCombatHealth();
    auto* Shot = Turret->FireAtNearestEnemy();
    if (!TestNotNull(TEXT("Turret fires projectile"), Shot)) return false;
    TestEqual(TEXT("Firing alone does not damage target"), Near->GetCombatHealth(), Before);
    TestNull(TEXT("Manual calls cannot bypass fire interval"), Turret->FireAtNearestEnemy());
    Fixture.Advance(0.4f);
    TestEqual(TEXT("Moving projectile applies its own damage through GAS"), Near->GetCombatHealth(), Before - 10.0f);
    TestTrue(TEXT("Projectile is consumed on impact"), !IsValid(Shot) || Shot->IsActorBeingDestroyed());
    TestEqual(TEXT("Other enemy unharmed"), Far->GetCombatHealth(), FarBefore);
    auto* Wall = Fixture.Wall(FVector(250, 0, 100));
    TestTrue(TEXT("Wall occludes nearest enemy; visible alternative selected"), Turret->FindNearestEnemy() == Far);
    Far->SetActorHiddenInGame(true);
    TestNull(TEXT("No shooting at hidden, occluded or out of range enemies"), Turret->FindNearestEnemy());
    Wall->Destroy();
    auto* NearASC = CastChecked<UCombatAbilitySystemComponent>(Near->GetAbilitySystemComponent());
    NearASC->SetNumericAttributeBase(UCombatAttributeSet::GetHealthAttribute(), 0.0f);
    TestNull(TEXT("Dead enemies are not acquired"), Turret->FindNearestEnemy());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurretLifecycleTest, "TeamPotato.Combat.Turret.LifetimeReplacementAndHealth",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTurretLifecycleTest::RunTest(const FString& Parameters)
{
    FTurretTestWorld Fixture;
    auto* Summoner = Fixture.World->SpawnActor<APawn>();
    auto* OtherSummoner = Fixture.World->SpawnActor<APawn>();
    auto* Old = Fixture.Turret(FVector(0, 0, 100), Summoner);
    auto* Other = Fixture.Turret(FVector(0, 800, 100), OtherSummoner);
    auto* Replacement = Fixture.Turret(FVector(200, 0, 100), Summoner, 0.2f);
    TestTrue(TEXT("New turret replaces same summoner's old turret"), !IsValid(Old) || Old->IsActorBeingDestroyed());
    TestTrue(TEXT("Other summoner's turret retained"), IsValid(Other) && !Other->IsActorBeingDestroyed());
    TestEqual(TEXT("Initial health"), Replacement->CurrentHealth, 100.0f);
    TestEqual(TEXT("Damage is handled by turret"), UGameplayStatics::ApplyDamage(Replacement, 25, nullptr, nullptr, nullptr), 25.0f);
    TestEqual(TEXT("Health reduced"), Replacement->CurrentHealth, 75.0f);
    TestEqual(TEXT("Invalid damage rejected"), UGameplayStatics::ApplyDamage(Replacement,
        std::numeric_limits<float>::quiet_NaN(), nullptr, nullptr, nullptr), 0.0f);
    Fixture.Advance(0.4f);
    TestTrue(TEXT("Maximum lifetime destroys surviving turret"), !IsValid(Replacement) || Replacement->IsActorBeingDestroyed());
    TestEqual(TEXT("Overkill clamped to remaining health"), UGameplayStatics::ApplyDamage(Other, 999, nullptr, nullptr, nullptr), 100.0f);
    TestTrue(TEXT("Lethal damage destroys turret"), !IsValid(Other) || Other->IsActorBeingDestroyed());
    auto* Orphan = Fixture.Turret(FVector(500, 0, 100), Summoner);
    Summoner->Destroy();
    TestTrue(TEXT("Character destruction cleans up its turret"), !IsValid(Orphan) || Orphan->IsActorBeingDestroyed());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurretAutomaticFireTest, "TeamPotato.Combat.Turret.AutomaticFireAndSafety",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTurretAutomaticFireTest::RunTest(const FString& Parameters)
{
    FTurretTestWorld Fixture;
    Fixture.Turret(FVector(0, 0, 100), nullptr, 60.0f, 0.15f);
    auto* Enemy = Fixture.Enemy(FVector(400, 0, 100));
    const float Before = Enemy->GetCombatHealth();
    Fixture.Advance(0.5f);
    TestTrue(TEXT("Timer automatically fires and projectiles hit"), Enemy->GetCombatHealth() < Before);

    auto* Player = Fixture.World->SpawnActor<ATestCharacter>(FVector(0, 1000, 100), FRotator::ZeroRotator);
    auto* PlayerASC = CastChecked<UCombatAbilitySystemComponent>(Player->GetAbilitySystemComponent());
    auto* Shot = Fixture.World->SpawnActor<ATurretProjectile>(FVector(0, 1200, 100), FRotator::ZeroRotator);
    Shot->DispatchBeginPlay();
    TestFalse(TEXT("Zero direction rejected"), Shot->Launch(FVector::ZeroVector));
    TestTrue(TEXT("Valid projectile launch"), Shot->Launch(FVector::ForwardVector));
    FHitResult Hit;
    Shot->Collision->OnComponentHit.Broadcast(Shot->Collision, Player, nullptr, FVector::ZeroVector, Hit);
    TestEqual(TEXT("Projectile never damages a player"), PlayerASC->GetHealth(), 100.0f);
    TestTrue(TEXT("Friendly blocking impact consumes projectile"), !IsValid(Shot) || Shot->IsActorBeingDestroyed());

    auto* ShortLived = Fixture.World->SpawnActor<ATurretProjectile>(FVector(0, -1000, 100), FRotator::ZeroRotator);
    ShortLived->MaxLifetime = 0.1f;
    ShortLived->Launch(FVector::ForwardVector);
    ShortLived->DispatchBeginPlay();
    TestTrue(TEXT("Early-launched movement remains ticking after BeginPlay"), ShortLived->Movement->IsComponentTickEnabled());
    Fixture.Advance(0.2f);
    TestTrue(TEXT("Early-launched projectile keeps configured lifetime after BeginPlay"), !IsValid(ShortLived) || ShortLived->IsActorBeingDestroyed());
    return true;
}

#endif
