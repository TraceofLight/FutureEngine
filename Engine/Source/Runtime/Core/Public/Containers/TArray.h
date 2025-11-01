#pragma once

using std::vector;
using std::allocator;
using std::initializer_list;
using std::is_trivially_constructible_v;

/**
 * @brief 언리얼 엔진 스타일의 동적 배열 컨테이너
 * 언리얼 호환성을 위하여 vector를 대체할 목적으로 추가 구현
 */
template<typename T, typename AllocatorType = allocator<T>>
class TArray
{
public:
    // Type definitions
    using ElementType = T;
    using SizeType = int32;
    using IndexType = int32;
    using Iterator = typename vector<T, AllocatorType>::iterator;
    using ConstIterator = typename vector<T, AllocatorType>::const_iterator;
    using ReverseIterator = typename vector<T, AllocatorType>::reverse_iterator;
    using ConstReverseIterator = typename vector<T, AllocatorType>::const_reverse_iterator;

    // Constructors
    TArray() = default;

    explicit TArray(SizeType InNum)
        : Data(static_cast<size_t>(InNum))
    {
    }

    TArray(SizeType InNum, const ElementType& Value)
        : Data(static_cast<size_t>(InNum), Value)
    {
    }

    TArray(initializer_list<ElementType> InitList)
        : Data(InitList)
    {
    }

    template<typename OtherAllocator>
    TArray(const TArray<ElementType, OtherAllocator>& Other)
        : Data(Other.begin(), Other.end())
    {
    }

    // Copy and Move constructors/assignments
    TArray(const TArray&) = default;
    TArray(TArray&&) noexcept = default;
    TArray& operator=(const TArray&) = default;
    TArray& operator=(TArray&&) noexcept = default;

    // Destructor
    ~TArray() = default;

    // Element access
    ElementType& operator[](IndexType Index)
    {
        return Data[static_cast<size_t>(Index)];
    }

    const ElementType& operator[](IndexType Index) const
    {
        return Data[static_cast<size_t>(Index)];
    }

    ElementType& GetData(IndexType Index)
    {
        return Data[static_cast<size_t>(Index)];
    }

    const ElementType& GetData(IndexType Index) const
    {
        return Data[static_cast<size_t>(Index)];
    }

    ElementType* GetData()
    {
        return Data.data();
    }

    const ElementType* GetData() const
    {
        return Data.data();
    }

    ElementType& Last(IndexType IndexFromTheEnd = 0)
    {
        return Data[Data.size() - 1 - static_cast<size_t>(IndexFromTheEnd)];
    }

    const ElementType& Last(IndexType IndexFromTheEnd = 0) const
    {
        return Data[Data.size() - 1 - static_cast<size_t>(IndexFromTheEnd)];
    }

    ElementType& Top()
    {
        return Data.back();
    }

    const ElementType& Top() const
    {
        return Data.back();
    }

    // Size and capacity
    SizeType Num() const
    {
        return static_cast<SizeType>(Data.size());
    }

    SizeType Max() const
    {
        return static_cast<SizeType>(Data.capacity());
    }

    bool IsEmpty() const
    {
        return Data.empty();
    }

    SizeType GetSlack() const
    {
        return Max() - Num();
    }

    // Modifiers
    void Empty(SizeType Slack = 0)
    {
        Data.clear();
        if (Slack > 0)
        {
            Data.reserve(static_cast<size_t>(Slack));
        }
    }

    void Reset(SizeType NewSize = 0)
    {
        Data.clear();
        if (NewSize > 0)
        {
            Data.resize(static_cast<size_t>(NewSize));
        }
    }

    void Shrink()
    {
        Data.shrink_to_fit();
    }

    void Reserve(SizeType Number)
    {
        Data.reserve(static_cast<size_t>(Number));
    }

    void SetNum(SizeType NewNum)
    {
        Data.resize(static_cast<size_t>(NewNum));
    }

    void SetNum(SizeType NewNum, const ElementType& Value)
    {
        Data.resize(static_cast<size_t>(NewNum), Value);
    }

    void SetNumZeroed(SizeType NewNum)
    {
        SizeType OldNum = Num();
        SetNum(NewNum);
        if (NewNum > OldNum && is_trivially_constructible_v<ElementType>)
        {
            memset(GetData() + OldNum, 0, (NewNum - OldNum) * sizeof(ElementType));
        }
    }

    void SetNumUninitialized(SizeType NewNum)
    {
        Data.resize(static_cast<size_t>(NewNum));
    }

    // Adding elements
    SizeType Add(const ElementType& Item)
    {
        Data.push_back(Item);
        return Num() - 1;
    }

    SizeType Add(ElementType&& Item)
    {
        Data.push_back(std::move(Item));
        return Num() - 1;
    }

    template<typename... ArgsType>
    SizeType Emplace(ArgsType&&... Args)
    {
        Data.emplace_back(std::forward<ArgsType>(Args)...);
        return Num() - 1;
    }

    template<typename... ArgsType>
    ElementType& EmplaceAt(IndexType Index, ArgsType&&... Args)
    {
        return *Data.emplace(Data.begin() + Index, forward<ArgsType>(Args)...);
    }

    SizeType AddZeroed(SizeType Count = 1)
    {
        SizeType OldNum = Num();
        SetNum(OldNum + Count);
        if (is_trivially_constructible_v<ElementType>)
        {
            memset(GetData() + OldNum, 0, Count * sizeof(ElementType));
        }
        return OldNum;
    }

    SizeType AddUninitialized(SizeType Count = 1)
    {
        SizeType OldNum = Num();
        SetNumUninitialized(OldNum + Count);
        return OldNum;
    }

    SizeType AddUnique(const ElementType& Item)
    {
        IndexType Index = Find(Item);
        if (Index != INDEX_NONE)
        {
            return Index;
        }
        return Add(Item);
    }

    SizeType AddUnique(ElementType&& Item)
    {
        IndexType Index = Find(Item);
        if (Index != INDEX_NONE)
        {
            return Index;
        }
        return Add(move(Item));
    }

    void Append(const TArray& Source)
    {
        Data.insert(Data.end(), Source.Data.begin(), Source.Data.end());
    }

    void Append(TArray&& Source)
    {
        if (IsEmpty())
        {
            *this = move(Source);
        }
        else
        {
            Data.insert(Data.end(),
                       make_move_iterator(Source.Data.begin()),
                       make_move_iterator(Source.Data.end()));
            Source.Empty();
        }
    }

    void Append(const ElementType* Ptr, SizeType Count)
    {
        Data.insert(Data.end(), Ptr, Ptr + Count);
    }

    void Append(initializer_list<ElementType> InitList)
    {
        Data.insert(Data.end(), InitList.begin(), InitList.end());
    }

    // Inserting elements
    void Insert(const ElementType& Item, IndexType Index)
    {
        Data.insert(Data.begin() + Index, Item);
    }

    void Insert(ElementType&& Item, IndexType Index)
    {
        Data.insert(Data.begin() + Index, move(Item));
    }

    void Insert(const TArray& Items, IndexType Index)
    {
        Data.insert(Data.begin() + Index, Items.Data.begin(), Items.Data.end());
    }

    void Insert(const ElementType* Ptr, SizeType Count, IndexType Index)
    {
        Data.insert(Data.begin() + Index, Ptr, Ptr + Count);
    }

    void Insert(initializer_list<ElementType> InitList, IndexType Index)
    {
        Data.insert(Data.begin() + Index, InitList.begin(), InitList.end());
    }

    // Removing elements
    void RemoveAt(IndexType Index, SizeType Count = 1)
    {
        Data.erase(Data.begin() + Index, Data.begin() + Index + Count);
    }

    SizeType RemoveAtSwap(IndexType Index, SizeType Count = 1)
    {
        if (Index + Count < Num())
        {
            // Move elements from the end to fill the gap
            SizeType NumToMove = std::min(Count, Num() - Index - Count);
            for (SizeType i = 0; i < NumToMove; ++i)
            {
                Data[Index + i] = std::move(Data[Num() - NumToMove + i]);
            }
        }

        SizeType NewSize = Num() - Count;
        Data.resize(NewSize);
        return NewSize;
    }

    SizeType Remove(const ElementType& Item)
    {
        SizeType NumRemoved = 0;
        for (auto it = Data.begin(); it != Data.end();)
        {
            if (*it == Item)
            {
                it = Data.erase(it);
                ++NumRemoved;
            }
            else
            {
                ++it;
            }
        }
        return NumRemoved;
    }

    SizeType RemoveAll(const ElementType& Item)
    {
        return Remove(Item);
    }

    template<typename Predicate>
    SizeType RemoveAll(Predicate Pred)
    {
        SizeType OriginalNum = Num();
        Data.erase(remove_if(Data.begin(), Data.end(), Pred), Data.end());
        return OriginalNum - Num();
    }

    SizeType RemoveSingle(const ElementType& Item)
    {
        auto it = find(Data.begin(), Data.end(), Item);
        if (it != Data.end())
        {
            Data.erase(it);
            return 1;
        }
        return 0;
    }

    bool RemoveSwap(const ElementType& Item)
    {
        IndexType Index = Find(Item);
        if (Index != INDEX_NONE)
        {
            RemoveAtSwap(Index);
            return true;
        }
        return false;
    }

    SizeType RemoveAllSwap(const ElementType& Item)
    {
        SizeType NumRemoved = 0;
        SizeType WriteIndex = 0;

        // Find all items that should be kept and compact them to the front
        for (SizeType ReadIndex = 0; ReadIndex < Num(); ++ReadIndex)
        {
            if (Data[ReadIndex] != Item)
            {
                if (WriteIndex != ReadIndex)
                {
                    Data[WriteIndex] = std::move(Data[ReadIndex]);
                }
                ++WriteIndex;
            }
            else
            {
                ++NumRemoved;
            }
        }

        // Resize to new size
        Data.resize(WriteIndex);
        return NumRemoved;
    }

    template<typename Predicate>
    SizeType RemoveAllSwap(Predicate Pred)
    {
        SizeType NumRemoved = 0;
        SizeType WriteIndex = 0;

        // Find all items that should be kept and compact them to the front
        for (SizeType ReadIndex = 0; ReadIndex < Num(); ++ReadIndex)
        {
            if (!Pred(Data[ReadIndex]))
            {
                if (WriteIndex != ReadIndex)
                {
                    Data[WriteIndex] = std::move(Data[ReadIndex]);
                }
                ++WriteIndex;
            }
            else
            {
                ++NumRemoved;
            }
        }

        // Resize to new size
        Data.resize(WriteIndex);
        return NumRemoved;
    }

    void Pop()
    {
        Data.pop_back();
    }

    ElementType Pop(bool bAllowShrinking)
    {
        ElementType Result = std::move(Data.back());
        Data.pop_back();
        if (bAllowShrinking)
        {
            Shrink();
        }
        return Result;
    }

    // Searching
    IndexType Find(const ElementType& Item) const
    {
        auto it = find(Data.begin(), Data.end(), Item);
        return (it != Data.end()) ? static_cast<IndexType>(distance(Data.begin(), it)) : INDEX_NONE;
    }

    /**
     * @brief 요소를 찾고 인덱스를 OutIndex에 저장 (언리얼 스타일)
     * @param Item 찾을 요소
     * @param OutIndex 찾은 인덱스를 저장할 변수
     * @return 요소를 찾았으면 true, 아니면 false
     */
    bool Find(const ElementType& Item, IndexType& OutIndex) const
    {
        auto it = find(Data.begin(), Data.end(), Item);
        if (it != Data.end())
        {
            OutIndex = static_cast<IndexType>(distance(Data.begin(), it));
            return true;
        }
        OutIndex = INDEX_NONE;
        return false;
    }

    IndexType FindLast(const ElementType& Item) const
    {
        for (IndexType i = Num() - 1; i >= 0; --i)
        {
            if (Data[i] == Item)
            {
                return i;
            }
        }
        return INDEX_NONE;
    }

    template<typename Predicate>
    IndexType IndexOfByPredicate(Predicate Pred) const
    {
        auto it = find_if(Data.begin(), Data.end(), Pred);
        return (it != Data.end()) ? static_cast<IndexType>(distance(Data.begin(), it)) : INDEX_NONE;
    }

    template<typename Predicate>
    const ElementType* FindByPredicate(Predicate Pred) const
    {
        auto it = find_if(Data.begin(), Data.end(), Pred);
        return (it != Data.end()) ? &(*it) : nullptr;
    }

    template<typename Predicate>
    ElementType* FindByPredicate(Predicate Pred)
    {
        auto it = find_if(Data.begin(), Data.end(), Pred);
        return (it != Data.end()) ? &(*it) : nullptr;
    }

    template<typename KeyType>
    IndexType IndexOfByKey(const KeyType& Key) const
    {
        for (IndexType i = 0; i < Num(); ++i)
        {
            if (Data[i] == Key)
            {
                return i;
            }
        }
        return INDEX_NONE;
    }

    template<typename KeyType>
    const ElementType* FindByKey(const KeyType& Key) const
    {
        IndexType Index = IndexOfByKey(Key);
        return (Index != INDEX_NONE) ? &Data[Index] : nullptr;
    }

    template<typename KeyType>
    ElementType* FindByKey(const KeyType& Key)
    {
        IndexType Index = IndexOfByKey(Key);
        return (Index != INDEX_NONE) ? &Data[Index] : nullptr;
    }

    bool Contains(const ElementType& Item) const
    {
        return Find(Item) != INDEX_NONE;
    }

    template<typename Predicate>
    bool ContainsByPredicate(Predicate Pred) const
    {
        return find_if(Data.begin(), Data.end(), Pred) != Data.end();
    }

    // Sorting
    void Sort()
    {
        sort(Data.begin(), Data.end());
    }

    template<typename Predicate>
    void Sort(Predicate Pred)
    {
        sort(Data.begin(), Data.end(), Pred);
    }

    void StableSort()
    {
        stable_sort(Data.begin(), Data.end());
    }

    template<typename Predicate>
    void StableSort(Predicate Pred)
    {
        stable_sort(Data.begin(), Data.end(), Pred);
    }

    // Utility functions
    void Swap(IndexType FirstIndexToSwap, IndexType SecondIndexToSwap)
    {
        if (FirstIndexToSwap != SecondIndexToSwap)
        {
            swap(Data[FirstIndexToSwap], Data[SecondIndexToSwap]);
        }
    }

    void SwapMemory(IndexType FirstIndexToSwap, IndexType SecondIndexToSwap)
    {
        Swap(FirstIndexToSwap, SecondIndexToSwap);
    }

    bool IsValidIndex(IndexType Index) const
    {
        return Index >= 0 && Index < Num();
    }

    SizeType GetTypeSize() const
    {
        return sizeof(ElementType);
    }

    void* GetRawData(IndexType Index)
    {
        return static_cast<void*>(&Data[Index]);
    }

    const void* GetRawData(IndexType Index) const
    {
        return static_cast<const void*>(&Data[Index]);
    }

    // Iterator support (STL and Unreal Engine style)
    Iterator begin() { return Data.begin(); }
    ConstIterator begin() const { return Data.begin(); }
    ConstIterator cbegin() const { return Data.cbegin(); }

    Iterator end() { return Data.end(); }
    ConstIterator end() const { return Data.end(); }
    ConstIterator cend() const { return Data.cend(); }

    ReverseIterator rbegin() { return Data.rbegin(); }
    ConstReverseIterator rbegin() const { return Data.rbegin(); }
    ConstReverseIterator crbegin() const { return Data.crbegin(); }

    ReverseIterator rend() { return Data.rend(); }
    ConstReverseIterator rend() const { return Data.rend(); }
    ConstReverseIterator crend() const { return Data.crend(); }

    // Conversion to STL
    const vector<T, AllocatorType>& ToStdVector() const
    {
        return Data;
    }

    vector<T, AllocatorType>& ToStdVector()
    {
        return Data;
    }

    // Comparison operators
    bool operator==(const TArray& Other) const
    {
        return Data == Other.Data;
    }

    bool operator!=(const TArray& Other) const
    {
        return Data != Other.Data;
    }

private:
	vector<T, AllocatorType> Data;
    static constexpr IndexType INDEX_NONE = -1;
};

// Utility functions for TArray
template<typename T, typename AllocatorType>
void Swap(TArray<T, AllocatorType>& A, TArray<T, AllocatorType>& B)
{
    A.ToStdVector().swap(B.ToStdVector());
}
