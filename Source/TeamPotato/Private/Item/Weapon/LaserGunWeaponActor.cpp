// Fill out your copyright notice in the Description page of Project Settings.


#include "Item/Weapon/LaserGunWeaponActor.h"
#include "Component/WeaponComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Player/TestCharacter.h"
#include "Combat/CombatFunctionLibrary.h"

ALaserGunWeaponActor::ALaserGunWeaponActor()
{
    // 무기 타입 설정
    WeaponType = EWeaponType::Gun;
    AttackDamage = 10.0f;
    AttackCost = 1.0f;
}

void ALaserGunWeaponActor::Attack(ATestCharacter* OwningPlayer)
{
    if (!HasAuthority() || !UCombatFunctionLibrary::CanActorAttack(this)) return;
    Super::Attack(OwningPlayer);

    // 오류 검사
    if (!OwnerWeaponComponent || !WeaponMesh || !LaserClass) return;

    OwningPlayer = Cast<ATestCharacter>(GetOwner());
    if (!OwningPlayer) return;

    // 총구 위치 계산
    FTransform SocketTransform = WeaponMesh->GetSocketTransform("FirePoint", RTS_World);
    FVector MuzzleLocation = SocketTransform.GetLocation();

    const FVector ShootDirection = GetRangedShootDirection(OwningPlayer, MuzzleLocation);

    if (AttackSound)
        UGameplayStatics::PlaySound2D(this, AttackSound);

    // 총알 스폰
    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = OwningPlayer;
    SpawnParams.Instigator = OwningPlayer;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    GetWorld()->SpawnActor<AActor>(
        LaserClass,
        MuzzleLocation,
        ShootDirection.Rotation(),
        SpawnParams
    );
}
