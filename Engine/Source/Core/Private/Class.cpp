#include "pch.h"
#include "Core/Public/Class.h"
#include "Core/Public/Object.h"

using std::stringstream;

void UClass::SignUpClass(UClass* InClass)
{
	if (InClass)
	{
		for (UClass* Class : GetAllClasses())
		{
			if (Class == InClass) return;
		}
		GetAllClasses().Emplace(InClass);
		UE_LOG("UClass: Class registered: %s (Total: %d)", InClass->GetName().ToString().data(), GetAllClasses().Num());
	}
}

UClass* UClass::FindClass(const FName& InClassName)
{
	for (UClass* Class : GetAllClasses())
	{
		if (Class && Class->GetName() == InClassName)
		{
			return Class;
		}
	}

	return nullptr;
}

TArray<UClass*> UClass::FindClasses(UClass* SuperClass)
{
	TArray<UClass*> Classes;

	for (UClass* Class: GetAllClasses())
	{
		if (Class->IsChildOf(SuperClass))
		{
			Classes.Add(Class);
		}
	}

	return Classes;
}

TArray<UClass*>& UClass::GetAllClasses()
{
	static TArray<UClass*> AllClasses;
	return AllClasses;
}

/**
 * @brief UClass Constructor
 * @param InName Class 이름
 * @param InSuperClass Parent Class
 * @param InClassSize Class Size
 * @param InConstructor 생성자 함수 포인터
 */
UClass::UClass(const FName& InName, UClass* InSuperClass, size_t InClassSize, ClassConstructorType InConstructor, bool InIsAbstract)
	: ClassName(InName), SuperClass(InSuperClass), ClassSize(InClassSize), Constructor(InConstructor), bIsAbstract(InIsAbstract)
{
	UE_LOG("UClass: 클래스 등록: %s", ClassName.ToString().data());
}

/**
 * @brief 이 클래스가 지정된 클래스의 하위 클래스인지 확인
 * @param InClass 확인할 클래스
 * @return 하위 클래스이거나 같은 클래스면 true
 */
bool UClass::IsChildOf(UClass* InClass) const
{
	if (!InClass)
	{
		return false;
	}

	// 자기 자신과 같은 클래스인 경우
	if (this == InClass)
	{
		return true;
	}

	// 부모 클래스들을 거슬러 올라가면서 확인
	const UClass* CurrentClass = this;
	while (CurrentClass)
	{
		if (CurrentClass->ClassName == InClass->ClassName)
		{
			return true;
		}

		CurrentClass = CurrentClass->SuperClass;
	}

	return false;
}

/**
 * @brief 새로운 인스턴스 생성
 * @return 생성된 객체 포인터
 */
UObject* UClass::CreateDefaultObject() const
{
	if (Constructor)
	{
		return Constructor();
	}

	return nullptr;
}