// Fill out your copyright notice in the Description page of Project Settings.


#include "Item/Weapon/WeaponBase.h"
#include "Player/TestCharacter.h"
#include "Component/PlayerResource.h"
#include "Data/WeaponDataAsset.h"
#include "Kismet/GameplayStatics.h"
#include "Combat/RangedTargetingComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"

// Sets default values
AWeaponBase::AWeaponBase()
{
    // Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
    PrimaryActorTick.bCanEverTick = false;

    USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);

    // 매쉬 초기화
    WeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Mesh"));
    WeaponMesh->SetupAttachment(Root);
    WeaponMesh->SetCollisionProfileName(TEXT("NoCollision"));
}

// Called when the game starts or when spawned
void AWeaponBase::BeginPlay()
{
    Super::BeginPlay();

    if(EquipSound)
        UGameplayStatics::PlaySound2D(this, EquipSound);
}

void::AWeaponBase::Attack(class ATestCharacter* OwningPlayer)
{
}

FVector AWeaponBase::GetRangedShootDirection(const ATestCharacter* Player, const FVector& MuzzleLocation) const
{
    FVector Direction = Player->GetActorForwardVector();
    if (const auto* PC = Cast<APlayerController>(Player->GetController()))
    {
        int32 Width = 0;
        int32 Height = 0;
        PC->GetViewportSize(Width, Height);
        FVector Origin;
        FVector ViewDirection;
        if (Width > 0 && Height > 0 &&
            PC->DeprojectScreenPositionToWorld(Width * 0.5f, Height * 0.5f, Origin, ViewDirection))
        {
            FVector AimPoint = Origin + ViewDirection * 10000.0f;
            FCollisionQueryParams Query(SCENE_QUERY_STAT(WeaponAim), false, Player);
            Query.AddIgnoredActor(this);
            TArray<AActor*> Attachments;
            Player->GetAttachedActors(Attachments, true, true);
            Query.AddIgnoredActors(Attachments);
            FHitResult Hit;
            if (GetWorld()->LineTraceSingleByChannel(Hit, Origin, AimPoint, ECC_Visibility, Query)) AimPoint = Hit.ImpactPoint;
            Direction = (AimPoint - MuzzleLocation).GetSafeNormal();
        }
    }
    if (const auto* Targeting = Player->GetRangedTargeting())
        return Targeting->ResolveFireDirection(MuzzleLocation, Direction);
    return Direction;
}

void AWeaponBase::InitializeFromData(UWeaponDataAsset* InData)
{
    if (!InData) return;

    WeaponData = InData;
    WeaponType = InData->WeaponType;
    AttackDamage = InData->AttackDamage;
    AttackCost = InData->AttackCost;
}
