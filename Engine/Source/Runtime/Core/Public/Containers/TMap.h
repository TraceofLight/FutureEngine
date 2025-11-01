#pragma once

#include <unordered_map>
#include <functional>

using std::unordered_map;
using std::hash;
using std::equal_to;
using std::allocator;
using std::pair;

/**
 * @brief 언리얼 엔진 스타일의 키-값 맵 컨테이너
 * 언리얼 호환성을 위하여 unordered_map을 대체할 목적으로 구현
 */
template<typename KeyType, typename ValueType, typename HasherType = hash<KeyType>, typename KeyEqualType = equal_to<KeyType>, typename AllocatorType = allocator<pair<const KeyType, ValueType>>>
class TMap
{
public:
    // Type definitions
    using ElementType = pair<const KeyType, ValueType>;
    using KeyInitType = KeyType;
    using ValueInitType = ValueType;
    using SizeType = int32;
    using Iterator = typename unordered_map<KeyType, ValueType, HasherType, KeyEqualType, AllocatorType>::iterator;
    using ConstIterator = typename unordered_map<KeyType, ValueType, HasherType, KeyEqualType, AllocatorType>::const_iterator;

    // Constructors
    TMap() = default;

    explicit TMap(SizeType ExpectedNumElements)
        : Data(static_cast<size_t>(ExpectedNumElements))
    {
    }

    TMap(std::initializer_list<ElementType> InitList)
        : Data(InitList)
    {
    }

    // Copy and Move constructors/assignments
    TMap(const TMap&) = default;
    TMap(TMap&&) noexcept = default;
    TMap& operator=(const TMap&) = default;
    TMap& operator=(TMap&&) noexcept = default;

    // Destructor
    ~TMap() = default;

    // Element access
    ValueType& operator[](const KeyType& Key)
    {
        return Data[Key];
    }

    ValueType& operator[](KeyType&& Key)
    {
        return Data[std::move(Key)];
    }

    /**
     * @brief 키에 해당하는 값을 찾아 반환 (언리얼 스타일)
     * @param Key 찾을 키
     * @return 값에 대한 포인터 (없으면 nullptr)
     */
    ValueType* Find(const KeyType& Key)
    {
        auto It = Data.find(Key);
        return (It != Data.end()) ? &It->second : nullptr;
    }

    const ValueType* Find(const KeyType& Key) const
    {
        auto It = Data.find(Key);
        return (It != Data.end()) ? &It->second : nullptr;
    }

    /**
     * @brief 키에 해당하는 값을 찾아 반환 (언리얼 스타일, 기본값 포함)
     * @param Key 찾을 키
     * @param DefaultValue 키가 없을 때 반환할 기본값
     * @return 찾은 값 또는 기본값
     */
    ValueType FindRef(const KeyType& Key, const ValueType& DefaultValue = ValueType{}) const
    {
        auto It = Data.find(Key);
        return (It != Data.end()) ? It->second : DefaultValue;
    }

    /**
     * @brief 키를 찾거나 추가하고 값의 참조를 반환 (언리얼 스타일)
     * @param Key 찾거나 추가할 키
     * @return 값에 대한 참조 (없으면 기본 생성된 값 추가 후 반환)
     */
    ValueType& FindOrAdd(const KeyType& Key)
    {
        return Data[Key];
    }

    ValueType& FindOrAdd(KeyType&& Key)
    {
        return Data[std::move(Key)];
    }

    /**
     * @brief 키를 찾거나 지정된 값으로 추가하고 참조를 반환 (언리얼 스타일)
     * @param Key 찾거나 추가할 키
     * @param Value 키가 없을 때 추가할 값
     * @return 값에 대한 참조
     */
    ValueType& FindOrAdd(const KeyType& Key, const ValueType& Value)
    {
        auto [It, bInserted] = Data.try_emplace(Key, Value);
        return It->second;
    }

    ValueType& FindOrAdd(KeyType&& Key, ValueType&& Value)
    {
        auto [It, bInserted] = Data.try_emplace(std::move(Key), std::move(Value));
        return It->second;
    }

    /**
     * @brief 키가 존재하는지 확인 (언리얼 스타일)
     */
    bool Contains(const KeyType& Key) const
    {
        return Data.find(Key) != Data.end();
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

    /**
     * @brief 키-값 쌍을 추가 (언리얼 스타일)
     * @param Key 추가할 키
     * @param Value 추가할 값
     * @return 추가된 값에 대한 참조
     */
    ValueType& Add(const KeyType& Key, const ValueType& Value)
    {
        return Data[Key] = Value;
    }

    ValueType& Add(const KeyType& Key, ValueType&& Value)
    {
        return Data[Key] = std::move(Value);
    }

    ValueType& Add(KeyType&& Key, const ValueType& Value)
    {
        return Data[std::move(Key)] = Value;
    }

    ValueType& Add(KeyType&& Key, ValueType&& Value)
    {
        return Data[std::move(Key)] = std::move(Value);
    }

    /**
     * @brief 키-값 쌍을 안전하게 추가 (언리얼 스타일)
     * @param Key 추가할 키
     * @param Value 추가할 값
     * @return 추가된 값에 대한 참조
     */
    ValueType& Emplace(const KeyType& Key, const ValueType& Value)
    {
        auto [It, bInserted] = Data.emplace(Key, Value);
        return It->second;
    }

    ValueType& Emplace(KeyType&& Key, ValueType&& Value)
    {
        auto [It, bInserted] = Data.emplace(std::move(Key), std::move(Value));
        return It->second;
    }

    template<typename... ArgsType>
    ValueType& Emplace(const KeyType& Key, ArgsType&&... Args)
    {
        auto [It, bInserted] = Data.emplace(std::piecewise_construct, std::forward_as_tuple(Key), std::forward_as_tuple(std::forward<ArgsType>(Args)...));
        return It->second;
    }

    /**
     * @brief 키에 해당하는 항목을 제거 (언리얼 스타일)
     * @param Key 제거할 키
     * @return 제거된 항목의 개수 (0 또는 1)
     */
    SizeType Remove(const KeyType& Key)
    {
        return static_cast<SizeType>(Data.erase(Key));
    }

    /**
     * @brief 키에 해당하는 항목을 제거하고 값을 반환 (언리얼 스타일)
     * @param Key 제거할 키
     * @param OutValue 제거된 값을 받을 변수
     * @return 항목이 제거되었는지 여부
     */
    bool RemoveAndCopyValue(const KeyType& Key, ValueType& OutValue)
    {
        auto It = Data.find(Key);
        if (It != Data.end())
        {
            OutValue = It->second;
            Data.erase(It);
            return true;
        }
        return false;
    }

    // Iterators
    Iterator begin() { return Data.begin(); }
    Iterator end() { return Data.end(); }
    ConstIterator begin() const { return Data.begin(); }
    ConstIterator end() const { return Data.end(); }

    ConstIterator CreateConstIterator() const { return Data.begin(); }
    Iterator CreateIterator() { return Data.begin(); }

    /**
     * @brief 키-값 쌍을 순회하는 언리얼 스타일 함수
     * @param Func 각 쌍에 대해 실행할 함수 (KeyType, ValueType 매개변수)
     */
    template<typename FuncType>
    void ForEach(FuncType Func) const
    {
        for (const auto& Pair : Data)
        {
            Func(Pair.first, Pair.second);
        }
    }

    template<typename FuncType>
    void ForEach(FuncType Func)
    {
        for (auto& Pair : Data)
        {
            Func(Pair.first, Pair.second);
        }
    }

private:
    unordered_map<KeyType, ValueType, HasherType, KeyEqualType, AllocatorType> Data;
};

/**
 * @brief 언리얼 엔진 스타일의 다중값 맵 (하나의 키에 여러 값)
 */
template<typename KeyType, typename ValueType, typename HasherType = hash<KeyType>, typename KeyEqualType = equal_to<KeyType>>
class TMultiMap
{
public:
    using ElementType = pair<const KeyType, ValueType>;
    using SizeType = int32;
    using Iterator = typename std::unordered_multimap<KeyType, ValueType, HasherType, KeyEqualType>::iterator;
    using ConstIterator = typename std::unordered_multimap<KeyType, ValueType, HasherType, KeyEqualType>::const_iterator;

private:
    std::unordered_multimap<KeyType, ValueType, HasherType, KeyEqualType> Data;

public:
    // 기본 생성자들
    TMultiMap() = default;
    TMultiMap(const TMultiMap&) = default;
    TMultiMap(TMultiMap&&) noexcept = default;
    TMultiMap& operator=(const TMultiMap&) = default;
    TMultiMap& operator=(TMultiMap&&) noexcept = default;

    // 크기 관련
    SizeType Num() const { return static_cast<SizeType>(Data.size()); }
    bool IsEmpty() const { return Data.empty(); }

    // 수정자
    void Empty() { Data.clear(); }
    void Add(const KeyType& Key, const ValueType& Value) { Data.emplace(Key, Value); }
    SizeType Remove(const KeyType& Key) { return static_cast<SizeType>(Data.erase(Key)); }

    // 검색
    bool Contains(const KeyType& Key) const { return Data.find(Key) != Data.end(); }

    // 반복자
    Iterator begin() { return Data.begin(); }
    Iterator end() { return Data.end(); }
    ConstIterator begin() const { return Data.begin(); }
    ConstIterator end() const { return Data.end(); }
};