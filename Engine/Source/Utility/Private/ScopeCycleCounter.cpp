#include "pch.h"
#include "Utility/Public/ScopeCycleCounter.h"

TMap<FString, FTimeProfile> FScopeCycleCounter::TimeProfileMap;

//Map에 이미 있으면 시간, 콜스택 추가
void FScopeCycleCounter::AddTimeProfile(const TStatId& Key, double InMilliseconds)
{
    auto It = TimeProfileMap.find(Key.Key);
    if (It != TimeProfileMap.end())
    {
        TimeProfileMap[Key.Key] = FTimeProfile{ InMilliseconds, 1 };
    }
    else
    {
        TimeProfileMap[Key.Key].Milliseconds += InMilliseconds;
        TimeProfileMap[Key.Key].CallCount++;
    }
}
const FTimeProfile& FScopeCycleCounter::GetTimeProfile(const FString& Key)
{
    return TimeProfileMap[Key];
}

TArray<FString> FScopeCycleCounter::GetTimeProfileKeys()
{
    TArray<FString> Keys;
    Keys.Reserve(TimeProfileMap.size());
    for (const auto& Pair : TimeProfileMap)
    {
        Keys.Add(Pair.first);
    }
    return Keys;
}

TArray<FTimeProfile> FScopeCycleCounter::GetTimeProfileValues()
{
    TArray<FTimeProfile> Values;
    Values.Reserve(TimeProfileMap.size());
    for (const auto& Pair : TimeProfileMap)
    {
        Values.Add(Pair.second);
    }
    return Values;
}

double FWindowsPlatformTime::GSecondsPerCycle = 0.0;
bool FWindowsPlatformTime::bInitialized = false;
