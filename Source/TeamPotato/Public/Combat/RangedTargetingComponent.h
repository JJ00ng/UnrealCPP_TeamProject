#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RangedTargetingComponent.generated.h"

class AEnemyCharacter;

// Shared, shot-time aim assistance for player guns and projectile abilities. No tick or homing.
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class TEAMPOTATO_API URangedTargetingComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    URangedTargetingComponent();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Lock On")
    bool bEnableLockOn = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Lock On", meta = (ClampMin = "1.0", Units = "cm"))
    float LockOnRange = 1500.0f;

    // Horizontal half-angle relative to the character's facing direction.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Lock On", meta = (ClampMin = "0.0", ClampMax = "180.0", Units = "deg"))
    float LockOnHalfAngle = 60.0f;

    UFUNCTION(BlueprintPure, Category = "Combat|Lock On")
    AEnemyCharacter* FindLockOnTarget(FVector FireOrigin) const;

    UFUNCTION(BlueprintPure, Category = "Combat|Lock On")
    FVector ResolveFireDirection(FVector FireOrigin, FVector FallbackDirection) const;
};
