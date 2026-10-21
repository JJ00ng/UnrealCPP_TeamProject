// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Data/EnumBase.h"
#include "WeaponBase.generated.h"

class UWeaponComponent;
class UPlayerResource;
class USoundBase;

UCLASS()
class TEAMPOTATO_API AWeaponBase : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AWeaponBase();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

    FVector GetRangedShootDirection(const class ATestCharacter* Player, const FVector& MuzzleLocation) const;

public:
	// 공격 함수
    virtual void Attack(class ATestCharacter* OwningPlayer);

    // 데이터 에셋에서 불러오는 함수
    void InitializeFromData(class UWeaponDataAsset* InData);
    
    // 애니메이션 노티파이에서 호출
    virtual void BeginAttack() {}
    virtual void EndAttack() {}
    virtual void CancelAttack() { EndAttack(); }

	inline void SetOwnerComponent(UWeaponComponent* InWeaponComponent) { OwnerWeaponComponent = InWeaponComponent; }

	EWeaponType GetWeaponType() const { return WeaponType; }
    UWeaponDataAsset* GetWeaponData() const { return WeaponData; }

protected:
    // 무기 매쉬
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
    TObjectPtr<USkeletalMeshComponent> WeaponMesh = nullptr;

	// 무기 타입
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	EWeaponType WeaponType = EWeaponType::None;

    // 무기 공격 데미지
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
    float AttackDamage = 10.0f;

    // 무기 공격 소모 
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
    float AttackCost = 1.0f;

    // 무기 데이터
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Data")
    TObjectPtr<UWeaponDataAsset> WeaponData = nullptr;

	// 웨폰 컴포넌트
	UPROPERTY()
	TObjectPtr<UWeaponComponent> OwnerWeaponComponent = nullptr;

    // 무기 장착 사운드
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effects")
    TObjectPtr<USoundBase> EquipSound = nullptr;

    // 무기 공격 사운드
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effects")
    TObjectPtr<USoundBase> AttackSound = nullptr;

public:
	bool bIsActivated = false;
};
