#include "pch.h"
#include "Utility/Public/ScopeCycleCounter.h"

TMap<FString, FTimeProfile> FScopeCycleCounter::TimeProfileMap;

//Map에 이미 있으면 시간, 콜스택 추가
void FScopeCycleCounter::AddTimeProfile(const TStatId& Key, double InMilliseconds)
{
    FTimeProfile& Profile = TimeProfileMap.FindOrAdd(Key.Key);

    Profile.Milliseconds += InMilliseconds;
    ++Profile.CallCount;
}

const FTimeProfile& FScopeCycleCounter::GetTimeProfile(const FString& Key)
{
    return TimeProfileMap[Key];
}

TArray<FString> FScopeCycleCounter::GetTimeProfileKeys()
{
    TArray<FString> Keys;
    Keys.Reserve(TimeProfileMap.Num());
    for (const auto& Pair : TimeProfileMap)
    {
        Keys.Add(Pair.first);
    }
    return Keys;
}

TArray<FTimeProfile> FScopeCycleCounter::GetTimeProfileValues()
{
    TArray<FTimeProfile> Values;
    Values.Reserve(TimeProfileMap.Num());
    for (const auto& Pair : TimeProfileMap)
    {
        Values.Add(Pair.second);
    }
    return Values;
}

double FWindowsPlatformTime::GSecondsPerCycle = 0.0;
bool FWindowsPlatformTime::bInitialized = false;
