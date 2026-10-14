#include "Combat/TurretProjectile.h"
#include "Combat/CombatFunctionLibrary.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Enemy/EnemyCharacter.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"

ATurretProjectile::ATurretProjectile()
{
    PrimaryActorTick.bCanEverTick = false;
    Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
    SetRootComponent(Collision);
    Collision->InitSphereRadius(8.0f);
    Collision->SetCollisionObjectType(ECC_WorldDynamic);
    Collision->SetCollisionResponseToAllChannels(ECR_Block);
    Collision->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
    Collision->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
    Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Collision->SetGenerateOverlapEvents(true);
    Collision->OnComponentHit.AddDynamic(this, &ThisClass::OnHit);
    Collision->OnComponentBeginOverlap.AddDynamic(this, &ThisClass::OnOverlap);
    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    Mesh->SetupAttachment(Collision);
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Trail = CreateDefaultSubobject<UNiagaraComponent>(TEXT("Trail"));
    Trail->SetupAttachment(Collision);
    Trail->SetAutoActivate(false);
    Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
    Movement->SetUpdatedComponent(Collision);
    Movement->ProjectileGravityScale = 0.0f;
    Movement->bRotationFollowsVelocity = true;
    Movement->bShouldBounce = false;
    Movement->bAutoActivate = false;
    InitialLifeSpan = 5.0f;
}

bool ATurretProjectile::Launch(FVector Direction)
{
    if (!HasAuthority() || bLaunched || IsActorBeingDestroyed() || Direction.ContainsNaN() ||
        !Direction.Normalize() || !FMath::IsFinite(Speed) || Speed <= 0.0f ||
        !FMath::IsFinite(Damage) || Damage < 0.0f || !FMath::IsFinite(MaxLifetime) || MaxLifetime <= 0.0f) return false;
    bLaunched = true;
    if (GetOwner()) Collision->IgnoreActorWhenMoving(GetOwner(), true);
    if (GetInstigator()) Collision->IgnoreActorWhenMoving(GetInstigator(), true);
    // A launch before BeginPlay must survive Actor::BeginPlay resetting the initial lifespan.
    InitialLifeSpan = MaxLifetime;
    SetLifeSpan(MaxLifetime);
    SetActorRotation(Direction.Rotation());
    Movement->InitialSpeed = Speed;
    Movement->MaxSpeed = Speed;
    Movement->Velocity = Direction * Speed;
    Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    if (!bImpacted)
    {
        // Movement tick registration uses bAutoActivate, including registration after an early launch.
        Movement->bAutoActivate = true;
        Movement->Activate(true);
        Trail->Activate();
    }
    return true;
}

void ATurretProjectile::OnHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
    UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit)
{
    ResolveImpact(OtherActor, Hit);
}

void ATurretProjectile::OnOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
    UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    // Trigger volumes and friendly overlaps are not impacts.
    if (!Cast<AEnemyCharacter>(OtherActor)) return;
    FHitResult Hit = SweepResult;
    if (!bFromSweep)
    {
        Hit.ImpactPoint = GetActorLocation();
        Hit.ImpactNormal = -GetActorForwardVector();
    }
    ResolveImpact(OtherActor, Hit);
}

void ATurretProjectile::ResolveImpact(AActor* OtherActor, const FHitResult& Hit)
{
    if (!HasAuthority() || !bLaunched || bImpacted || OtherActor == this ||
        (OtherActor && (OtherActor == GetOwner() || OtherActor == GetInstigator()))) return;
    bImpacted = true;
    Movement->StopMovementImmediately();
    Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    if (AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(OtherActor))
    {
        if (!Enemy->IsActorBeingDestroyed() && !Enemy->IsHidden() && Enemy->GetCombatHealth() > 0.0f)
        {
            // Resolve from the projectile's instigator, not a controller's potentially replaced pawn.
            UCombatFunctionLibrary::ApplyCombatDamageWithHit(Enemy, Damage, this, nullptr, &Hit);
        }
    }
    if (ImpactEffect)
        UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, ImpactEffect, Hit.ImpactPoint, Hit.ImpactNormal.Rotation());
    Destroy();
}
