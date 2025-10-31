#include "pch.h"
#include "Core/Public/ObjectHandle.h"
#include "Core/Public/Object.h"

FObjectHandle::FObjectHandle(const UObject* Object)
{
	if (Object)
	{
		ObjectIndex = Object->GetObjectIndex();
		SerialNumber = Object->GetSerialNumber();
	}
	else
	{
		ObjectIndex = UINT32_MAX;
		SerialNumber = 0;
	}
}

UObject* FObjectHandle::Get() const
{
	// 무효한 핸들인 경우
	if (ObjectIndex == UINT32_MAX)
	{
		return nullptr;
	}

	// 배열 범위 체크
	TArray<FUObjectItem>& ObjectArray = GetUObjectArray();
	if (ObjectIndex >= ObjectArray.size())
	{
		return nullptr;
	}

	// 해당 슬롯의 아이템 가져오기
	FUObjectItem& Item = ObjectArray[ObjectIndex];

	// 세대 번호가 일치하지 않으면 다른 객체가 슬롯을 재사용한 것
	if (Item.SerialNumber != SerialNumber)
	{
		return nullptr;
	}

	// 객체 포인터 반환 (nullptr일 수도 있음)
	return Item.Object;
}
