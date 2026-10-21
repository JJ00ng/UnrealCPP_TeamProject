#include "Combat/RangedTargetingComponent.h"
#include "Combat/CombatAbilitySystemComponent.h"
#include "Enemy/EnemyCharacter.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "Components/WidgetComponent.h"

URangedTargetingComponent::URangedTargetingComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

AEnemyCharacter* URangedTargetingComponent::FindLockOnTarget(FVector FireOrigin) const
{
    AActor* Shooter = GetOwner();
    UWorld* World = GetWorld();
    if (!bEnableLockOn || !IsValid(Shooter) || Shooter->IsActorBeingDestroyed() || !World ||
        FireOrigin.ContainsNaN() || !FMath::IsFinite(LockOnRange) || LockOnRange <= 0.0f ||
        !FMath::IsFinite(LockOnHalfAngle) || LockOnHalfAngle < 0.0f || LockOnHalfAngle > 180.0f) return nullptr;

    const FVector Center = Shooter->GetActorLocation();
    const FVector Forward = Shooter->GetActorForwardVector().GetSafeNormal2D();
    if (Center.ContainsNaN() || Forward.IsNearlyZero()) return nullptr;
    const float MinDot = FMath::Cos(FMath::DegreesToRadians(LockOnHalfAngle));
    FCollisionQueryParams Query(SCENE_QUERY_STAT(PlayerLockOn), false, Shooter);
    TArray<AActor*> Attachments;
    Shooter->GetAttachedActors(Attachments, true, true);
    Query.AddIgnoredActors(Attachments);
    TArray<FOverlapResult> Overlaps;
    World->OverlapMultiByObjectType(Overlaps, Center, FQuat::Identity,
        FCollisionObjectQueryParams::AllDynamicObjects, FCollisionShape::MakeSphere(LockOnRange), Query);

    // Earlier shots and other friendly owned actors must not occlude the next shot's aim query.
    for (const FOverlapResult& Overlap : Overlaps)
    {
        AActor* Actor = Overlap.GetActor();
        if (IsValid(Actor) && (Actor->GetOwner() == Shooter || Actor->GetInstigator() == Shooter)) Query.AddIgnoredActor(Actor);
        // World-space health bars are UI, not cover between the shooter and an enemy.
        if (auto* Widget = Cast<UWidgetComponent>(Overlap.GetComponent())) Query.AddIgnoredComponent(Widget);
    }

    AEnemyCharacter* Best = nullptr;
    float BestDistance = FMath::Square(LockOnRange);
    TSet<AActor*> Visited;
    for (const FOverlapResult& Overlap : Overlaps)
    {
        auto* Enemy = Cast<AEnemyCharacter>(Overlap.GetActor());
        if (!IsValid(Enemy) || Enemy == Shooter || Enemy->GetOwner() == Shooter ||
            Enemy->GetInstigator() == Shooter || Visited.Contains(Enemy)) continue;
        Visited.Add(Enemy);
        const auto* ASC = Cast<UCombatAbilitySystemComponent>(Enemy->GetAbilitySystemComponent());
        if (Enemy->IsActorBeingDestroyed() || Enemy->IsHidden() || !Enemy->GetActorEnableCollision() ||
            !Enemy->CanBeDamaged() || Enemy->GetCombatHealth() <= 0.0f || (ASC && ASC->IsDead())) continue;

        const FVector AimPoint = Enemy->GetActorLocation();
        const FVector Offset = AimPoint - Center;
        const float Distance = Offset.SizeSquared();
        if (AimPoint.ContainsNaN() || Distance > BestDistance) continue;
        const FVector FlatDirection = Offset.GetSafeNormal2D();
        if (!FlatDirection.IsNearlyZero() && FVector::DotProduct(Forward, FlatDirection) + UE_KINDA_SMALL_NUMBER < MinDot) continue;

        // Check from the caster as well as the actual muzzle, so an offset muzzle cannot lock through a wall.
        FVector BodyOrigin = Center;
        BodyOrigin.Z = FireOrigin.Z;
        FHitResult Hit;
        if (World->LineTraceSingleByChannel(Hit, BodyOrigin, AimPoint, ECC_Visibility, Query) && Hit.GetActor() != Enemy) continue;
        if (World->LineTraceSingleByChannel(Hit, FireOrigin, AimPoint, ECC_Visibility, Query) && Hit.GetActor() != Enemy) continue;
        Best = Enemy;
        BestDistance = Distance;
    }
    return Best;
}

FVector URangedTargetingComponent::ResolveFireDirection(FVector FireOrigin, FVector FallbackDirection) const
{
    if (AEnemyCharacter* Enemy = FindLockOnTarget(FireOrigin))
    {
        const FVector Direction = (Enemy->GetActorLocation() - FireOrigin).GetSafeNormal();
        if (!Direction.IsNearlyZero()) return Direction;
    }
    if (!FallbackDirection.ContainsNaN() && !FallbackDirection.IsNearlyZero()) return FallbackDirection.GetSafeNormal();
    return IsValid(GetOwner()) ? GetOwner()->GetActorForwardVector() : FVector::ForwardVector;
}
