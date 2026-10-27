#include "NewEnemyAnimationLibrary.h"

#include "Animation/AnimBlueprint.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/Character.h"
#include "K2Node_DynamicCast.h"
#include "Kismet2/BlueprintEditorUtils.h"

bool UNewEnemyAnimationLibrary::ConfigureCharacterOwner(UAnimBlueprint* Blueprint, USkeletalMesh* PreviewMesh)
{
    if (!Blueprint || !PreviewMesh || PreviewMesh->GetSkeleton() != Blueprint->TargetSkeleton)
    {
        return false;
    }

    TArray<UK2Node_DynamicCast*> Casts;
    FBlueprintEditorUtils::GetAllNodesOfClass(Blueprint, Casts);
    if (Casts.Num() != 1)
    {
        return false;
    }

    UK2Node_DynamicCast* Cast = Casts[0];
    UEdGraphPin* Result = Cast->GetCastResultPin();
    if (!Result || Result->LinkedTo.IsEmpty())
    {
        return false;
    }

    Blueprint->Modify();
    Cast->Modify();
    // Preserve connections while reconstructing the old EliteEnemy cast.
    // Its consumers are already typed as Character in the reference graph.
    Result->PinType.PinSubCategoryObject = ACharacter::StaticClass();
    Result->PinName = FName(UEdGraphSchema_K2::PN_CastedValuePrefix + ACharacter::StaticClass()->GetName());
    Cast->TargetType = ACharacter::StaticClass();
    Cast->ReconstructNode();
    Result = Cast->GetCastResultPin();
    if (!Result || Result->LinkedTo.IsEmpty())
    {
        return false;
    }

    Blueprint->SetPreviewMesh(PreviewMesh);
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    return true;
}
