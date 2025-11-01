#include "pch.h"
#include "Global/OverlapInfo.h"
#include "Component/Public/PrimitiveComponent.h"
#include "Actor/Public/Actor.h"

bool FOverlapInfo::IsValid() const
{
	return OverlapComponent.IsValid();
}

AActor* FOverlapInfo::GetActor() const
{
	if (UPrimitiveComponent* Comp = OverlapComponent.Get())
	{
		return Comp->GetOwner();
	}
	return nullptr;
}
