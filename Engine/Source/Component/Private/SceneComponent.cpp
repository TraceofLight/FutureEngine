#include "pch.h"
#include "Component/Public/SceneComponent.h"
#include "Manager/Asset/Public/AssetManager.h"
#include "Utility/Public/JsonSerializer.h"

#include "Component/Public/PrimitiveComponent.h"
#include "Level/Public/Level.h"

#include <json.hpp>

IMPLEMENT_CLASS(USceneComponent, UActorComponent)

USceneComponent::USceneComponent()
{
}

void USceneComponent::BeginPlay()
{

}

void USceneComponent::TickComponent(float DeltaTime)
{
    Super::TickComponent(DeltaTime);
}
void USceneComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);

	// 불러오기
	if (bInIsLoading)
	{
		FJsonSerializer::ReadVector(InOutHandle, "Location", RelativeLocation, FVector::ZeroVector());
		FVector RotationEuler;
		FJsonSerializer::ReadVector(InOutHandle, "Rotation", RotationEuler, FVector::ZeroVector());
		RelativeRotation = FQuaternion::FromEuler(RotationEuler);

		FJsonSerializer::ReadVector(InOutHandle, "Scale", RelativeScale3D, FVector::OneVector());
	}
	// 저장
	else
	{
		InOutHandle["Location"] = FJsonSerializer::VectorToJson(RelativeLocation);
		InOutHandle["Rotation"] = FJsonSerializer::VectorToJson(RelativeRotation.ToEuler());
		InOutHandle["Scale"] = FJsonSerializer::VectorToJson(RelativeScale3D);
	}
}

void USceneComponent::AttachToComponent(USceneComponent* Parent, bool bRemainTransform)
{
	if (!Parent || Parent == this || GetOwner() != Parent->GetOwner()) { return; }
	if (AttachParent)
	{
		AttachParent->DetachChild(this);
	}

	AttachParent = Parent;
	Parent->AttachChildren.Emplace(this);

	MarkAsDirty();
}

void USceneComponent::DetachFromComponent()
{
	if (AttachParent)
	{
		AttachParent->DetachChild(this);
		AttachParent = nullptr;
	}
}

void USceneComponent::DetachChild(USceneComponent* ChildToDetach)
{
	AttachChildren.Remove(ChildToDetach);
}

UObject* USceneComponent::Duplicate()
{
	USceneComponent* SceneComponent = Cast<USceneComponent>(Super::Duplicate());
	SceneComponent->RelativeLocation = RelativeLocation;
	SceneComponent->RelativeRotation = RelativeRotation;
	SceneComponent->RelativeScale3D = RelativeScale3D;
	SceneComponent->MarkAsDirty();
	return SceneComponent;
}

void USceneComponent::DuplicateSubObjects(UObject* DuplicatedObject)
{
	Super::DuplicateSubObjects(DuplicatedObject);
}

void USceneComponent::MarkAsDirty()
{
	bIsTransformDirty = true;
	bIsTransformDirtyInverse = true;

	for (USceneComponent* Child : AttachChildren)
	{
		Child->MarkAsDirty();
	}
}

void USceneComponent::SetRelativeLocation(const FVector& Location)
{
	RelativeLocation = Location;
	MarkAsDirty();
	// Note: PrimitiveComponent::MarkAsDirty() handles octree update and overlap checks
}

void USceneComponent::SetRelativeRotation(const FQuaternion& Rotation)
{
	RelativeRotation = Rotation;
	MarkAsDirty();
	// Note: PrimitiveComponent::MarkAsDirty() handles octree update and overlap checks
}

void USceneComponent::SetRelativeScale3D(const FVector& Scale)
{
	RelativeScale3D = Scale;
	MarkAsDirty();
	// Note: PrimitiveComponent::MarkAsDirty() handles octree update and overlap checks
}

const FMatrix& USceneComponent::GetWorldTransformMatrix() const
{
	if (bIsTransformDirty)
	{
		WorldTransformMatrix = FMatrix::GetModelMatrix(RelativeLocation, RelativeRotation, RelativeScale3D);

		if (AttachParent)
		{
			WorldTransformMatrix *= AttachParent->GetWorldTransformMatrix();
		}

		bIsTransformDirty = false;
	}

	return WorldTransformMatrix;
}

const FMatrix& USceneComponent::GetWorldTransformMatrixInverse() const
{
	if (bIsTransformDirtyInverse)
	{
		WorldTransformMatrixInverse = FMatrix::Identity();

		if (AttachParent)
		{
			WorldTransformMatrixInverse *= AttachParent->GetWorldTransformMatrixInverse();
		}

		WorldTransformMatrixInverse *= FMatrix::GetModelMatrixInverse(RelativeLocation, RelativeRotation, RelativeScale3D);

		bIsTransformDirtyInverse = false;
	}

	return WorldTransformMatrixInverse;
}

FVector USceneComponent::GetWorldLocation() const
{
    return GetWorldTransformMatrix().GetLocation();
}

FQuaternion USceneComponent::GetWorldRotationAsQuaternion() const
{
    if (AttachParent)
    {
        return RelativeRotation * AttachParent->GetWorldRotationAsQuaternion();
    }
    return RelativeRotation;
}

FVector USceneComponent::GetWorldRotation() const
{
    return GetWorldRotationAsQuaternion().ToEuler();
}

FVector USceneComponent::GetWorldScale3D() const
{
    return GetWorldTransformMatrix().GetScale();
}

void USceneComponent::SetWorldLocation(const FVector& NewLocation)
{
    if (AttachParent)
    {
        const FMatrix ParentWorldMatrixInverse = AttachParent->GetWorldTransformMatrixInverse();
        SetRelativeLocation(ParentWorldMatrixInverse.TransformPosition(NewLocation));
    }
    else
    {
        SetRelativeLocation(NewLocation);
    }
}

void USceneComponent::SetWorldRotation(const FVector& NewRotation)
{
    FQuaternion NewWorldRotationQuat = FQuaternion::FromEuler(NewRotation);
    if (AttachParent)
    {
        FQuaternion ParentWorldRotationQuat = AttachParent->GetWorldRotationAsQuaternion();
        SetRelativeRotation(NewWorldRotationQuat * ParentWorldRotationQuat.Inverse());
    }
    else
    {
        SetRelativeRotation(NewWorldRotationQuat);
    }
}

void USceneComponent::SetWorldRotation(const FQuaternion& NewRotation)
{
	if (AttachParent)
	{
		FQuaternion ParentWorldRotationQuat = AttachParent->GetWorldRotationAsQuaternion();
		SetRelativeRotation(NewRotation * ParentWorldRotationQuat.Inverse());
	}
	else
	{
		SetRelativeRotation(NewRotation);
	}
}

void USceneComponent::SetWorldRotationPreservingChildren(const FVector& NewRotation)
{
	SetWorldRotationPreservingChildren(FQuaternion::FromEuler(NewRotation));
}

void USceneComponent::SetWorldRotationPreservingChildren(const FQuaternion& NewRotation)
{
	// Child component들의 world rotation 저장
	TArray<TPair<USceneComponent*, FQuaternion>> ChildWorldRotations;
	ChildWorldRotations.Reserve(AttachChildren.Num());

	for (USceneComponent* Child : AttachChildren)
	{
		if (Child)
		{
			ChildWorldRotations.Emplace(Child, Child->GetWorldRotationAsQuaternion());
		}
	}

	// 본인의 world rotation 변경
	SetWorldRotation(NewRotation);

	// Child component들의 world rotation 복원
	for (const auto& Pair : ChildWorldRotations)
	{
		USceneComponent* Child = Pair.first;
		const FQuaternion& ChildWorldRotation = Pair.second;

		if (Child)
		{
			Child->SetWorldRotation(ChildWorldRotation);
		}
	}
}

void USceneComponent::SetRelativeRotationPreservingChildren(const FQuaternion& NewRotation)
{
	// Child component들의 world rotation 저장
	TArray<TPair<USceneComponent*, FQuaternion>> ChildWorldRotations;
	ChildWorldRotations.Reserve(AttachChildren.Num());

	for (USceneComponent* Child : AttachChildren)
	{
		if (Child)
		{
			ChildWorldRotations.Emplace(Child, Child->GetWorldRotationAsQuaternion());
		}
	}

	// 본인의 relative rotation 변경
	SetRelativeRotation(NewRotation);

	// Child component들의 world rotation 복원
	for (const auto& Pair : ChildWorldRotations)
	{
		USceneComponent* Child = Pair.first;
		const FQuaternion& ChildWorldRotation = Pair.second;

		if (Child)
		{
			Child->SetWorldRotation(ChildWorldRotation);
		}
	}
}

void USceneComponent::SetWorldScale3D(const FVector& NewScale)
{
    if (AttachParent)
    {
        const FVector ParentWorldScale = AttachParent->GetWorldScale3D();
        SetRelativeScale3D(FVector(NewScale.X / ParentWorldScale.X, NewScale.Y / ParentWorldScale.Y, NewScale.Z / ParentWorldScale.Z));
    }
    else
    {
        SetRelativeScale3D(NewScale);
    }
}
