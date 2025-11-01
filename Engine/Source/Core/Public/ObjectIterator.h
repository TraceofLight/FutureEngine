#pragma once

#include "Core/Public/Object.h"
template<typename TObject>
class TObjectIterator
{
public:
	TObjectIterator() : UObjectArray(GetUObjectArray())
	{
		CurrentIndex = 0;
		AdvanceToNextValidObject();
	}

	explicit operator bool() const
	{
		return CurrentObject != nullptr;
	}

	TObject* operator*() const
	{
		return CurrentObject;
	}

	TObject* operator->() const
	{
		return CurrentObject;
	}

	TObjectIterator& operator++()
	{
		++CurrentIndex;
		AdvanceToNextValidObject();
		return *this;
	}

	TObjectIterator operator++(int)
	{
		TObjectIterator Tmp = *this;
		++(*this);
		return Tmp;
	}

	bool operator==(const TObjectIterator& Other) const
	{
		return CurrentObject == Other.CurrentObject;
	}

	bool operator!=(const TObjectIterator& Other) const
	{
		return CurrentObject != Other.CurrentObject;
	}
private:
	void AdvanceToNextValidObject()
	{
		CurrentObject = nullptr;
		while (CurrentIndex < UObjectArray.Num())
		{
			UObject* Obj = UObjectArray[CurrentIndex].Object;  // FUObjectItem에서 Object 필드 접근
			if (Obj && Obj->IsA(TObject::StaticClass()))
			{
				CurrentObject = Cast<TObject>(Obj);
				return;
			}
			++CurrentIndex;
		}
	}

	int32 CurrentIndex;
	TObject* CurrentObject = nullptr;
	TArray<FUObjectItem>& UObjectArray;  // FUObjectItem 배열로 변경
};
