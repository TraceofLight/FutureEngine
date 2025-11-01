#pragma once

/**
 * @brief 템플릿 타입에서 참조(reference)를 제거 (std::remove_reference와 동일)
 */
template <typename T>
struct TRemoveReference
{
	using Type = T;
};
template <typename T>
struct TRemoveReference<T&>
{
	using Type = T;
};
template <typename T>
struct TRemoveReference<T&&>
{
	using Type = T;
};

/**
 * @brief 주어진 객체를 rvalue 참조로 캐스팅하여 소유권 이전을 가능하게 하는 함수
 * 'MoveTemp'라는 이름은 임시 객체로 옮긴다는 의미를 명확히 하기 위함
 * @param InObject 이동시킬 객체
 * @return 객체의 rvalue 참조
 */
template <typename T>
TRemoveReference<T>::Type&& MoveTemp(T&& InObject) noexcept
{
	return static_cast<TRemoveReference<T>::Type&&>(InObject);
}
