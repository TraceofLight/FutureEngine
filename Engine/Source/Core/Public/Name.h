#pragma once

/**
 * @brief 오브젝트의 이름을 담당하는 구조체
 * 대소문자 관계 없는 비교 처리와 사용자가 직접 작성한 Display Name을 동시에 사용할 수 있음
 * @param DisplayIndex 표시용 이름 배열에 접근하기 위한 인덱스
 * @param ComparisonIndex 이름 비교를 위한 인덱스
*/
class FName
{
public:
	FName();
	FName(const FString& Str);
	FName(const char* Str);
	FName(int32 InDisplayIndex, int32 InComparisonIndex, int32 InNumber);

	bool operator==(const FName& Other) const;
	bool operator!=(const FName& Other) const;
	int32 Compare(const FName& Other) const;

	FString ToString() const;
	FString ToBaseNameString() const;

	int32 GetComparisonIndex() const { return ComparisonIndex; }
	int32 GetDisplayIndex() const { return DisplayIndex; }
	int32 GetUniqueNumber() const { return Number; }
	bool IsNone() const { return ComparisonIndex == 0; }

private:
	int32 ComparisonIndex;
	int32 DisplayIndex;
	int32 Number;

public:
	static FName GetNone();
	static const FName None;
};

namespace std {
	// std::hash 구조체에 대한 템플릿 특수화
	template <>
	struct hash<FName>
	{
		// 해시 값을 계산하는 operator() 구현
		size_t operator()(const FName& Name) const noexcept
		{
			// ComparisonIndex와 Number를 기반으로 해시 값을 계산
			size_t HashValue = std::hash<int32>{}(Name.GetComparisonIndex());
			size_t NumberHash = std::hash<int32>{}(Name.GetUniqueNumber());
            
			HashValue ^= (NumberHash + 0x9e3779b9 + (HashValue << 6) + (HashValue >> 2));
            
			return HashValue;
		}
	};
}


class FNameTable
{
public:
	static FNameTable& GetInstance();

public:
	FNameTable();
	~FNameTable();
	TPair<int32, int32> FindOrAddName(const FString& Str);
	FName GetUniqueName(const FString& BaseStr);

	FString GetDisplayString(int32 Idx) const;

private:
	FString ToLower(const FString& Str) const;

	TArray<FString> ComparisonStringPool;
	TArray<FString> DisplayStringPool;

	TMap<FString, int32> ComparisonMap;
	TMap<FString, int32> DisplayMap;
	TMap<FString, int32> NextNumberMap;
};