// Fill out your copyright notice in the Description page of Project Settings.


#include "Item/Weapon/GunWeaponActor.h"
#include "Item/Weapon/BulletActor.h"
#include "Component/WeaponComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Player/TestCharacter.h"
#include "NiagaraComponent.h"
#include "Combat/CombatFunctionLibrary.h"

// Sets default values
AGunWeaponActor::AGunWeaponActor()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	// 무기 타입 설정
	WeaponType = EWeaponType::Gun;
    AttackDamage = 10.0f;
    AttackCost = 1.0f;

    // 트레일 초기화
    ShootEffect = CreateDefaultSubobject<UNiagaraComponent>(TEXT("Muzzle"));
    ShootEffect->SetupAttachment(WeaponMesh, TEXT("FirePoint"));
    ShootEffect->bAutoActivate = false;
}

void AGunWeaponActor::Attack(ATestCharacter* OwningPlayer)
{
    if (!HasAuthority() || !UCombatFunctionLibrary::CanActorAttack(this)) return;
    Super::Attack(OwningPlayer);

	// 오류 검사
	if (!OwnerWeaponComponent || !WeaponMesh || !BulletClass) return;

	OwningPlayer = Cast<ATestCharacter>(GetOwner());
	if (!OwningPlayer) return;

	// 총구 위치 계산
	FTransform SocketTransform = WeaponMesh->GetSocketTransform("FirePoint", RTS_World);
	FVector MuzzleLocation = SocketTransform.GetLocation();
	const FVector ShootDirection = GetRangedShootDirection(OwningPlayer, MuzzleLocation);

    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = OwningPlayer;          // 총알의 주인을 플레이어로 설정
    SpawnParams.Instigator = OwningPlayer;     // 데미지 가해자를 플레이어로 설정
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// 총알 스폰
	ABulletActor* Bullet = GetWorld()->SpawnActor<ABulletActor>(
		BulletClass,
		MuzzleLocation,
		ShootDirection.Rotation(),
        SpawnParams
	);

    if(ShootEffect)
       ShootEffect->Activate();
    if (AttackSound)
        UGameplayStatics::PlaySound2D(this, AttackSound);

	if (Bullet)
	{
		Bullet->GunFire(ShootDirection, BulletSpeed, AttackDamage);
	}
}
