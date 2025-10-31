#pragma once
#include "Global/Types.h"

class UObject;
struct FUObjectItem;

/**
 * @brief UObject를 안전하게 참조하기 위한 핸들 구조체
 *
 * FObjectHandle은 객체의 인덱스와 세대 번호를 저장하여
 * 객체가 삭제된 후에도 안전하게 감지할 수 있도록 합니다.
 *
 * - ObjectIndex: GUObjectArray에서의 인덱스
 * - SerialNumber: 슬롯이 재사용될 때마다 증가하는 세대 번호
 *
 * @see TWeakObjectPtr
 */
struct FObjectHandle
{
	uint32 ObjectIndex;
	uint32 SerialNumber;

	/**
	 * @brief 기본 생성자 - 무효한 핸들 생성
	 */
	FObjectHandle()
		: ObjectIndex(UINT32_MAX)
		, SerialNumber(0)
	{
	}

	/**
	 * @brief UObject로부터 핸들 생성
	 * @param Object 핸들을 생성할 객체 (nullptr일 경우 무효한 핸들 생성)
	 */
	explicit FObjectHandle(const UObject* Object);

	/**
	 * @brief 핸들이 가리키는 객체를 반환
	 * @return 유효한 객체 포인터 또는 nullptr (객체가 삭제되었거나 무효한 경우)
	 */
	UObject* Get() const;

	/**
	 * @brief 핸들이 유효한지 확인
	 * @return 객체가 살아있고 유효하면 true
	 */
	bool IsValid() const
	{
		return Get() != nullptr;
	}

	/**
	 * @brief 핸들을 무효화
	 */
	void Reset()
	{
		ObjectIndex = UINT32_MAX;
		SerialNumber = 0;
	}

	/**
	 * @brief 핸들 비교 연산자
	 */
	bool operator==(const FObjectHandle& Other) const
	{
		return ObjectIndex == Other.ObjectIndex && SerialNumber == Other.SerialNumber;
	}

	bool operator!=(const FObjectHandle& Other) const
	{
		return !(*this == Other);
	}

	/**
	 * @brief 핸들이 유효한지 bool로 확인
	 */
	explicit operator bool() const
	{
		return IsValid();
	}
};

/**
 * @brief FObjectHandle에 대한 해시 함수 (TMap/TSet에서 사용)
 */
namespace std {
	template<>
	struct hash<FObjectHandle>
	{
		size_t operator()(const FObjectHandle& Handle) const
		{
			// 두 개의 uint32를 결합하여 해시 생성
			return static_cast<size_t>(Handle.ObjectIndex) ^
			       (static_cast<size_t>(Handle.SerialNumber) << 16);
		}
	};
}
