#pragma once

#include <unordered_set>
#include <functional>

using std::unordered_set;
using std::hash;
using std::equal_to;
using std::allocator;

/**
 * @brief 언리얼 엔진 스타일의 Set 컨테이너
 * 언리얼 호환성을 위하여 unordered_set을 대체할 목적으로 구현
 */
template<typename ElementType, typename HasherType = hash<ElementType>, typename KeyEqualType = equal_to<ElementType>, typename AllocatorType = allocator<ElementType>>
class TSet
{
public:
    // Type definitions
    using KeyType = ElementType;
    using SizeType = int32;
    using Iterator = typename unordered_set<ElementType, HasherType, KeyEqualType, AllocatorType>::iterator;
    using ConstIterator = typename unordered_set<ElementType, HasherType, KeyEqualType, AllocatorType>::const_iterator;

    // Constructors
    TSet() = default;

    explicit TSet(SizeType ExpectedNumElements)
        : Data(static_cast<size_t>(ExpectedNumElements))
    {
    }

    TSet(std::initializer_list<ElementType> InitList)
        : Data(InitList)
    {
    }

    // Copy and Move constructors/assignments
    TSet(const TSet&) = default;
    TSet(TSet&&) noexcept = default;
    TSet& operator=(const TSet&) = default;
    TSet& operator=(TSet&&) noexcept = default;

    // Destructor
    ~TSet() = default;

    /**
     * @brief 요소를 Set에 추가 (언리얼 스타일)
     * @param InElement 추가할 요소
     * @return 추가 성공 여부
     */
    bool Add(const ElementType& InElement)
    {
        auto [Iterator, bInserted] = Data.insert(InElement);
        return bInserted;
    }

    bool Add(ElementType&& InElement)
    {
        auto [Iterator, bInserted] = Data.insert(std::move(InElement));
        return bInserted;
    }

    /**
     * @brief 요소를 안전하게 추가 (언리얼 스타일)
     * @param InElement 추가할 요소
     * @return 추가 성공 여부
     */
    bool Emplace(const ElementType& InElement)
    {
        auto [Iterator, bInserted] = Data.emplace(InElement);
        return bInserted;
    }

    bool Emplace(ElementType&& InElement)
    {
        auto [Iterator, bInserted] = Data.emplace(std::move(InElement));
        return bInserted;
    }

    template<typename... ArgsType>
    bool Emplace(ArgsType&&... Args)
    {
        auto [Iterator, bInserted] = Data.emplace(std::forward<ArgsType>(Args)...);
        return bInserted;
    }

    /**
     * @brief 요소가 Set에 포함되어 있는지 확인 (언리얼 스타일)
     */
    bool Contains(const ElementType& InElement) const
    {
        return Data.find(InElement) != Data.end();
    }

    /**
     * @brief 요소를 Set에서 제거 (언리얼 스타일)
     * @param InElement 제거할 요소
     * @return 제거된 항목의 개수 (0 또는 1)
     */
    SizeType Remove(const ElementType& InElement)
    {
        return static_cast<SizeType>(Data.erase(InElement));
    }

    /**
     * @brief 요소를 찾아서 반환 (언리얼 스타일)
     * @param InElement 찾을 요소
     * @return 요소에 대한 포인터 (없으면 nullptr)
     */
    const ElementType* Find(const ElementType& InElement) const
    {
        auto It = Data.find(InElement);
        return (It != Data.end()) ? &(*It) : nullptr;
    }

    ElementType* Find(const ElementType& InElement)
    {
        auto It = Data.find(InElement);
        return (It != Data.end()) ? const_cast<ElementType*>(&(*It)) : nullptr;
    }

    // Size and capacity
    SizeType Num() const
    {
        return static_cast<SizeType>(Data.size());
    }

    bool IsEmpty() const
    {
        return Data.empty();
    }

    // Modifiers
    void Empty(SizeType ExpectedNumElements = 0)
    {
        Data.clear();
        if (ExpectedNumElements > 0)
        {
            Data.reserve(static_cast<size_t>(ExpectedNumElements));
        }
    }

    void Reset()
    {
        Data.clear();
    }

    void Shrink()
    {
        Data.rehash(0);
    }

    void Reserve(SizeType Number)
    {
        Data.reserve(static_cast<size_t>(Number));
    }

    // Iterators
    Iterator begin() { return Data.begin(); }
    Iterator end() { return Data.end(); }
    ConstIterator begin() const { return Data.begin(); }
    ConstIterator end() const { return Data.end(); }

    ConstIterator CreateConstIterator() const { return Data.begin(); }
    Iterator CreateIterator() { return Data.begin(); }

    /**
     * @brief 각 요소에 대해 함수를 실행하는 언리얼 스타일 함수
     * @param Func 각 요소에 대해 실행할 함수
     */
    template<typename FuncType>
    void ForEach(FuncType Func) const
    {
        for (const auto& Element : Data)
        {
            Func(Element);
        }
    }

    template<typename FuncType>
    void ForEach(FuncType Func)
    {
        for (auto& Element : Data)
        {
            Func(Element);
        }
    }

    /**
     * @brief 조건에 맞는 모든 요소를 제거
     * @param Predicate 제거 조건을 판단하는 함수
     * @return 제거된 요소의 개수
     */
    template<typename PredicateType>
    SizeType RemoveAll(PredicateType Predicate)
    {
        SizeType RemovedCount = 0;
        auto It = Data.begin();
        while (It != Data.end())
        {
            if (Predicate(*It))
            {
                It = Data.erase(It);
                ++RemovedCount;
            }
            else
            {
                ++It;
            }
        }
        return RemovedCount;
    }

private:
    unordered_set<ElementType, HasherType, KeyEqualType, AllocatorType> Data;
};