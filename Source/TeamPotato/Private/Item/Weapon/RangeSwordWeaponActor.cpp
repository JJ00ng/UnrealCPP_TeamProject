// Fill out your copyright notice in the Description page of Project Settings.


#include "Item/Weapon/RangeSwordWeaponActor.h"
#include "Item/Weapon/SlashActor.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Player/TestCharacter.h"
#include "Combat/CombatFunctionLibrary.h"
#include "Combat/RangedTargetingComponent.h"

void ARangeSwordWeaponActor::EndAttack()
{
    Super::EndAttack();
    if (!UCombatFunctionLibrary::CanActorAttack(this)) return;

    if (!SlashActorClass)
        return;

    OwnerCharacter = Cast<ATestCharacter>(GetOwner());
    if (!OwnerCharacter)
        return;

    FVector Forward = OwnerCharacter->GetActorForwardVector();
    FVector SpawnLocation = OwnerCharacter->GetActorLocation() + Forward * 100.f + FVector(0, 0, 50.f);
    FRotator SpawnRotation = OwnerCharacter->GetActorRotation();
    if (const auto* Targeting = OwnerCharacter->GetRangedTargeting())
        SpawnRotation = Targeting->ResolveFireDirection(SpawnLocation, Forward).Rotation();

    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = OwnerCharacter;
    SpawnParams.Instigator = OwnerCharacter;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    ASlashActor* Slash = GetWorld()->SpawnActor<ASlashActor>(
        SlashActorClass,
        SpawnLocation,
        SpawnRotation,
        SpawnParams
    );

    if (Slash)
    {
        // Use the same shot-time lock-on direction as the other ranged attacks.
        FVector ForwardDir = SpawnRotation.Vector();
        Slash->ProjectileMovementComponent->Velocity =
            ForwardDir * Slash->ProjectileMovementComponent->InitialSpeed;
    }
}
