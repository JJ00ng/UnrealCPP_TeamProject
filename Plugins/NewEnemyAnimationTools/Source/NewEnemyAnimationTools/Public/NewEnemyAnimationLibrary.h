#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "NewEnemyAnimationLibrary.generated.h"

class UAnimBlueprint;
class USkeletalMesh;

/** Editor helper only. Generated ABPs do not reference this plugin. */
UCLASS()
class NEWENEMYANIMATIONTOOLS_API UNewEnemyAnimationLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Editor|NewEnemy")
    static bool ConfigureCharacterOwner(UAnimBlueprint* Blueprint, USkeletalMesh* PreviewMesh);
};
