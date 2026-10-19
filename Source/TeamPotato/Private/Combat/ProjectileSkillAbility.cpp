#include "Combat/ProjectileSkillAbility.h"
#include "Combat/CombatAbilitySystemComponent.h"
#include "Combat/CombatTags.h"
#include "Combat/TurretProjectile.h"
#include "Components/SphereComponent.h"
#include "Engine/World.h"
#include "Enemy/EnemyCharacter.h"
#include "Player/TestCharacter.h"

UProjectileSkillAbility::UProjectileSkillAbility()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
    ActivationBlockedTags.AddTag(CombatTags::State_Dead);
    ActivationBlockedTags.AddTag(CombatTags::State_AttackBlocked);
    ProjectileClass = ATurretProjectile::StaticClass();
}

bool UProjectileSkillAbility::IsProjectileConfigured(const ATurretProjectile* Projectile)
{
    return IsValid(Projectile) && Projectile->Collision &&
        FMath::IsFinite(Projectile->Damage) && Projectile->Damage >= 0.0f &&
        FMath::IsFinite(Projectile->Speed) && Projectile->Speed > 0.0f &&
        FMath::IsFinite(Projectile->MaxLifetime) && Projectile->MaxLifetime > 0.0f &&
        FMath::IsFinite(Projectile->Collision->GetScaledSphereRadius()) &&
        Projectile->Collision->GetScaledSphereRadius() > 0.0f;
}

bool UProjectileSkillAbility::GetLaunchTransform(const FGameplayAbilityActorInfo* ActorInfo, FTransform& OutTransform) const
{
    const auto* Character = ActorInfo ? Cast<ATestCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
    const auto* ASC = ActorInfo ? Cast<UCombatAbilitySystemComponent>(ActorInfo->AbilitySystemComponent.Get()) : nullptr;
    if (!IsValid(Character) || Character->IsActorBeingDestroyed() || !Character->HasAuthority() ||
        !ASC || !ASC->IsCombatInitialized() || !ASC->CanAttack() || !ProjectileClass ||
        ProjectileClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists) ||
        SpawnOffset.ContainsNaN()) return false;

    UWorld* World = Character->GetWorld();
    const auto* Defaults = ProjectileClass->GetDefaultObject<ATurretProjectile>();
    if (!World || !IsProjectileConfigured(Defaults)) return false;

    const FRotator Rotation(0.0f, Character->GetActorRotation().Yaw, 0.0f);
    const FVector Start = Character->GetActorLocation() + FVector(0.0f, 0.0f, SpawnOffset.Z);
    FVector Location = Character->GetActorLocation() + Rotation.RotateVector(SpawnOffset);
    if (Location.ContainsNaN() || Rotation.ContainsNaN()) return false;

    // The projectile enables collision on Launch; check the full path to its muzzle before committing.
    FCollisionQueryParams Query(SCENE_QUERY_STAT(ProjectileSkillSpawn), false, Character);
    TArray<AActor*> Attachments;
    Character->GetAttachedActors(Attachments, true, true);
    Query.AddIgnoredActors(Attachments);
    FHitResult Hit;
    const FCollisionResponseParams Response(Defaults->Collision->GetCollisionResponseToChannels());
    if (World->SweepSingleByChannel(Hit, Start, Location, FQuat::Identity, ECC_WorldDynamic,
        FCollisionShape::MakeSphere(Defaults->Collision->GetScaledSphereRadius()), Query, Response))
    {
        if (!Cast<AEnemyCharacter>(Hit.GetActor())) return false;
        // Do not spawn past a point-blank enemy. Start just before its swept contact instead.
        Location = Hit.Location - (Location - Start).GetSafeNormal();
    }

    OutTransform = FTransform(Rotation, Location);
    return true;
}

bool UProjectileSkillAbility::CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags,
    const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
    FTransform Transform;
    return Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags) &&
        GetLaunchTransform(ActorInfo, Transform);
}

void UProjectileSkillAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    FTransform Transform;
    if (!GetLaunchTransform(ActorInfo, Transform))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    auto* Character = CastChecked<ATestCharacter>(ActorInfo->AvatarActor.Get());
    FActorSpawnParameters Spawn;
    Spawn.Owner = Character;
    Spawn.Instigator = Character;
    Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Projectile = Character->GetWorld()->SpawnActor<ATurretProjectile>(ProjectileClass, Transform, Spawn);
    // Check construction-script overrides before paying the cost or starting the cooldown.
    if (!IsProjectileConfigured(Projectile) || Projectile->IsActorBeingDestroyed())
    {
        if (IsValid(Projectile)) Projectile->Destroy();
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    if (!CommitAbility(Handle, ActorInfo, ActivationInfo) || !IsActive() ||
        !IsValid(Character) || Character->IsActorBeingDestroyed() ||
        !CastChecked<UCombatAbilitySystemComponent>(ActorInfo->AbilitySystemComponent.Get())->CanAttack())
    {
        Projectile->Destroy();
        if (IsActive()) EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    if (!Projectile->Launch(Transform.GetRotation().GetForwardVector()))
    {
        Projectile->Destroy();
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    if (IsValid(Projectile) && !Projectile->IsActorBeingDestroyed()) OnProjectileLaunched(Projectile);
    // Ending the GA leaves its independently moving projectile and cooldown GE alive.
    if (IsActive()) EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
