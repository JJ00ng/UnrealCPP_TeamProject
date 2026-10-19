#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Combat/ProjectileSkillAbility.h"
#include "Combat/TurretProjectile.h"
#include "Combat/CombatAbilitySystemComponent.h"
#include "Combat/CombatEffects.h"
#include "Combat/CombatTags.h"
#include "Components/BoxComponent.h"
#include "Enemy/EnemyCharacter.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Player/TestCharacter.h"
#include "UObject/UnrealType.h"
#include <limits>

namespace
{
    struct FProjectileSkillWorld
    {
        UWorld* World;
        uint64 InitialFrame = GFrameCounter;
        ATestCharacter* Player;
        UCombatAbilitySystemComponent* ASC;
        FGameplayAbilitySpecHandle Handle;
        UProjectileSkillAbility* Skill;

        FProjectileSkillWorld()
        {
            World = UWorld::CreateWorld(EWorldType::GamePreview, false);
            GEngine->CreateNewWorldContext(EWorldType::GamePreview).SetCurrentWorld(World);
            World->InitializeActorsForPlay(FURL());
            Player = World->SpawnActor<ATestCharacter>(FVector(0, 0, 100), FRotator::ZeroRotator);
            ASC = CastChecked<UCombatAbilitySystemComponent>(Player->GetAbilitySystemComponent());
            Handle = ASC->GiveAbility(FGameplayAbilitySpec(UProjectileSkillAbility::StaticClass(), 1, INDEX_NONE, Player));
            Skill = CastChecked<UProjectileSkillAbility>(ASC->FindAbilitySpecFromHandle(Handle)->GetPrimaryInstance());
        }

        ~FProjectileSkillWorld()
        {
            World->DestroyWorld(false);
            GEngine->DestroyWorldContext(World);
            GFrameCounter = InitialFrame;
        }

        void UseTestCooldown()
        {
            // A real duration GE with granted tags exercises GAS's normal cooldown contract.
            auto* Property = FindFProperty<FClassProperty>(UGameplayAbility::StaticClass(), TEXT("CooldownGameplayEffectClass"));
            check(Property);
            Property->SetObjectPropertyValue_InContainer(Skill, UCombatHitInvincibilityEffect::StaticClass());
        }

        int32 ShotCount() const
        {
            int32 Count = 0;
            for (TActorIterator<ATurretProjectile> It(World); It; ++It)
                if (!It->IsActorBeingDestroyed()) ++Count;
            return Count;
        }

        ATurretProjectile* FirstShot() const
        {
            for (TActorIterator<ATurretProjectile> It(World); It; ++It)
                if (!It->IsActorBeingDestroyed()) return *It;
            return nullptr;
        }

        void Advance(float Seconds)
        {
            for (float Time = 0; Time < Seconds; Time += 0.025f)
            {
                for (TActorIterator<ATurretProjectile> It(World); It; ++It)
                    if (!It->HasActorBegunPlay()) It->DispatchBeginPlay();
                ++GFrameCounter;
                World->Tick(LEVELTICK_All, 0.025f);
            }
        }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectileSkillFireTest, "TeamPotato.Combat.ProjectileSkill.FireDamageAndCooldown",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FProjectileSkillFireTest::RunTest(const FString& Parameters)
{
    FProjectileSkillWorld Fixture;
    Fixture.UseTestCooldown();
    auto* Enemy = Fixture.World->SpawnActor<AEnemyCharacter>(FVector(500, 0, 130), FRotator::ZeroRotator);
    const float Before = Enemy->GetCombatHealth();
    TestTrue(TEXT("Skill activates through GAS"), Fixture.ASC->TryActivateAbility(Fixture.Handle, false));
    TestEqual(TEXT("One projectile spawned"), Fixture.ShotCount(), 1);
    TestEqual(TEXT("No damage before impact"), Enemy->GetCombatHealth(), Before);
    auto* Shot = Fixture.FirstShot();
    if (!TestNotNull(TEXT("Live projectile remains after instant GA ends"), Shot)) return false;
    const float Damage = Shot->Damage;
    TestTrue(TEXT("Owner is the caster"), Shot->GetOwner() == Fixture.Player);
    TestTrue(TEXT("Instigator is the caster"), Shot->GetInstigator() == Fixture.Player);
    TestTrue(TEXT("Spawn offset uses character space"), Shot->GetActorLocation().Equals(FVector(100, 0, 130), 0.01f));
    TestFalse(TEXT("GA does not stay active while projectile flies"), Fixture.ASC->FindAbilitySpecFromHandle(Fixture.Handle)->IsActive());
    TestTrue(TEXT("Cooldown GE committed"), Fixture.ASC->HasMatchingGameplayTag(CombatTags::State_Invincible));
    TestFalse(TEXT("Cooldown prevents repeated input"), Fixture.ASC->TryActivateAbility(Fixture.Handle, false));
    TestEqual(TEXT("Cooldown input creates no extra projectile"), Fixture.ShotCount(), 1);

    Fixture.Advance(0.4f);
    TestEqual(TEXT("Projectile impact applies GAS damage once"), Enemy->GetCombatHealth(), Before - Damage);
    TestEqual(TEXT("Projectile is consumed on impact"), Fixture.ShotCount(), 0);
    TestFalse(TEXT("Player is not damaged by own projectile"), Fixture.ASC->GetHealth() < Fixture.ASC->GetMaxHealth());
    Fixture.Advance(0.7f); // The test cooldown GE lasts one second.
    TestFalse(TEXT("Cooldown expires"), Fixture.ASC->HasMatchingGameplayTag(CombatTags::State_Invincible));
    TestTrue(TEXT("Skill can fire again after cooldown"), Fixture.ASC->TryActivateAbility(Fixture.Handle, false));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectileSkillCloseTargetTest, "TeamPotato.Combat.ProjectileSkill.PointBlankTarget",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FProjectileSkillCloseTargetTest::RunTest(const FString& Parameters)
{
    FProjectileSkillWorld Fixture;
    FActorSpawnParameters Spawn;
    Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Enemy = Fixture.World->SpawnActor<AEnemyCharacter>(FVector(80, 0, 130), FRotator::ZeroRotator, Spawn);
    const float Before = Enemy->GetCombatHealth();
    TestTrue(TEXT("An enemy inside the usual muzzle distance does not block casting"),
        Fixture.ASC->TryActivateAbility(Fixture.Handle, false));
    auto* Shot = Fixture.FirstShot();
    if (!TestNotNull(TEXT("Point-blank shot is spawned"), Shot)) return false;
    TestTrue(TEXT("Projectile starts before the close enemy"), Shot->GetActorLocation().X < Enemy->GetActorLocation().X);
    const float Damage = Shot->Damage;
    Fixture.Advance(0.15f);
    TestEqual(TEXT("Point-blank enemy takes one hit"), Enemy->GetCombatHealth(), Before - Damage);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectileSkillGateTest, "TeamPotato.Combat.ProjectileSkill.SpawnSafetyAndStateGates",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FProjectileSkillGateTest::RunTest(const FString& Parameters)
{
    FProjectileSkillWorld Fixture;
    Fixture.UseTestCooldown();
    AActor* Wall = Fixture.World->SpawnActor<AActor>();
    auto* Box = NewObject<UBoxComponent>(Wall);
    Wall->AddInstanceComponent(Box);
    Wall->SetRootComponent(Box);
    Box->InitBoxExtent(FVector(10, 50, 50));
    Box->SetCollisionProfileName(TEXT("BlockAll"));
    Box->RegisterComponent();
    Wall->SetActorLocation(FVector(50, 0, 120));
    TestFalse(TEXT("Wall between caster and muzzle blocks cast"), Fixture.ASC->TryActivateAbility(Fixture.Handle, false));
    TestEqual(TEXT("Blocked cast spawns no projectile"), Fixture.ShotCount(), 0);
    TestFalse(TEXT("Blocked cast does not spend cooldown"), Fixture.ASC->HasMatchingGameplayTag(CombatTags::State_Invincible));
    Wall->Destroy();

    Fixture.ASC->AddLooseGameplayTag(CombatTags::State_AttackBlocked);
    TestFalse(TEXT("Electric attack block prevents projectile skill"), Fixture.ASC->TryActivateAbility(Fixture.Handle, false));
    Fixture.ASC->RemoveLooseGameplayTag(CombatTags::State_AttackBlocked);
    Fixture.Skill->ProjectileClass = nullptr;
    TestFalse(TEXT("Missing projectile class safely rejects cast"), Fixture.ASC->TryActivateAbility(Fixture.Handle, false));
    Fixture.Skill->ProjectileClass = ATurretProjectile::StaticClass();
    Fixture.Skill->SpawnOffset.X = std::numeric_limits<float>::quiet_NaN();
    TestFalse(TEXT("Invalid spawn coordinates safely reject cast"), Fixture.ASC->TryActivateAbility(Fixture.Handle, false));
    Fixture.Skill->SpawnOffset = FVector(100, 0, 30);
    TestEqual(TEXT("Rejected casts leave no actors behind"), Fixture.ShotCount(), 0);

    Fixture.Player->SetActorRotation(FRotator(0, 90, 0));
    TestTrue(TEXT("Valid cast succeeds after failures"), Fixture.ASC->TryActivateAbility(Fixture.Handle, false));
    auto* Shot = Fixture.FirstShot();
    if (!TestNotNull(TEXT("Rotated cast creates projectile"), Shot)) return false;
    TestTrue(TEXT("Muzzle follows yaw"), Shot->GetActorLocation().Equals(FVector(0, 100, 130), 0.01f));
    TestTrue(TEXT("Velocity follows yaw"), Shot->Movement->Velocity.GetSafeNormal().Equals(FVector::RightVector, 0.01f));
    Fixture.ASC->MarkDead();
    TestFalse(TEXT("Dead caster cannot activate"), Fixture.ASC->TryActivateAbility(Fixture.Handle, false));
    return true;
}

#endif
