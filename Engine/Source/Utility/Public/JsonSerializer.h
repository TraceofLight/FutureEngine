#pragma once

// --- Standard Library Includes ---
#include <string>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <algorithm>
#include <cstdint>

// --- Project Includes ---
// FString, FVector, TArray, TMap 등의 타입이 정의된 헤더 파일을 포함해야 합니다.
// #include "Core/Public/CoreTypes.h" 
// #include "Core/Public/Object.h" // UE_LOG 등
#include "json.hpp" // 사용하는 JSON 라이브러리

namespace json { class JSON; }
using JSON = JSON;

/**
 * @brief Level 직렬화에 관여하는 클래스
 * JSON 기반으로 레벨의 데이터를 Save / Load 처리
 */
class FJsonSerializer
{
public:
	//====================================================================================
	// Reading from JSON
	//====================================================================================

	/**
	 * @brief JSON 객체에서 키를 찾아 64비트 정수(int64) 값을 안전하게 읽어옵니다.
	 * @return 성공하면 true, 실패하면 false를 반환합니다.
	 */
	static bool ReadInt64(const JSON& InJson, const FString& InKey, int64& OutValue, int64 InDefaultValue = 0, bool bInUseLog = true)
	{
		if (InJson.hasKey(InKey))
		{
			const JSON& Value = InJson.at(InKey);
			if (Value.JSONType() == JSON::Class::Integral)
			{
				OutValue = Value.ToInt();
				return true;
			}
		}

		if (bInUseLog)
			UE_LOG_ERROR("[JsonSerializer] %s int64 파싱에 실패했습니다 (기본값 사용)", InKey.c_str());

		OutValue = InDefaultValue;
		return false;
	}

	/**
	 * @brief JSON 객체에서 키를 찾아 32비트 정수(int32) 값을 안전하게 읽어옵니다.
	 * @return 성공하면 true, 실패하면 false를 반환합니다.
	 */
	static bool ReadInt32(const JSON& InJson, const FString& InKey, int32& OutValue, int32 InDefaultValue = 0, bool bInUseLog = true)
	{
		int64 Value_i64;
		if (ReadInt64(InJson, InKey, Value_i64, 0, false))
		{
			// int32의 표현 범위를 벗어나는지 확인합니다.
			if (Value_i64 >= INT32_MIN && Value_i64 <= INT32_MAX)
			{
				OutValue = static_cast<int32>(Value_i64);
				return true;
			}
		}

		if (bInUseLog)// ReadInt64가 실패했거나, 값의 범위가 벗어난 경우
			UE_LOG_ERROR("[JsonSerializer] %s int32 파싱에 실패했습니다 (기본값 사용)", InKey.c_str());

		OutValue = InDefaultValue;
		return false;
	}

	/**
	 * @brief 부호 없는 32비트 정수(uint32)를 안전하게 읽어옵니다.
	 * @return 성공하면 true, 실패하면 false를 반환합니다.
	 */
	static bool ReadUint32(const JSON& InJson, const FString& InKey, uint32& OutValue, uint32 InDefaultValue = 0, bool bInUseLog = true)
	{
		int64 Value_i64;
		if (ReadInt64(InJson, InKey, Value_i64, 0, false))
		{
			if (Value_i64 >= 0 && Value_i64 <= UINT32_MAX)
			{
				OutValue = static_cast<uint32>(Value_i64);
				return true;
			}
		}

		if (bInUseLog)
			UE_LOG_ERROR("[JsonSerializer] %s uint32 파싱에 실패했습니다 (기본값 사용)", InKey.c_str());

		OutValue = InDefaultValue;
		return false;
	}

	/**
	 * @brief JSON 객체에서 키를 찾아 float 값을 안전하게 읽어옵니다.
	 * @return 성공하면 true, 실패하면 false를 반환합니다.
	 */
	static bool ReadFloat(const JSON& InJson, const FString& InKey, float& OutValue, float InDefaultValue = 0.0f, bool bInUseLog = true)
	{
		if (InJson.hasKey(InKey))
		{
			const JSON& Value = InJson.at(InKey);
			if (Value.JSONType() == JSON::Class::Floating)
			{
				// JSON 라이브러리의 숫자 변환 함수를 사용합니다.
				// ToDouble()로 읽은 후 float으로 캐스팅하거나, ToFloat()가 있다면 직접 사용합니다.
				OutValue = static_cast<float>(Value.ToFloat());
				return true;
			}
		}

		if (bInUseLog)
			UE_LOG_ERROR("[JsonSerializer] %s float 파싱에 실패했습니다 (기본값 사용)", InKey.c_str());

		OutValue = InDefaultValue;
		return false;
	}

	/**
	 * @brief JSON 객체에서 키를 찾아 FString 값을 읽어옵니다.
	 * @return 성공하면 true, 실패하면 false를 반환합니다.
	 */
	static bool ReadString(const JSON& InJson, const FString& InKey, FString& OutValue, const FString& InDefaultValue = "", bool bInUseLog = true)
	{
		if (InJson.hasKey(InKey))
		{
			const JSON& Value = InJson.at(InKey);
			if (Value.JSONType() == JSON::Class::String)
			{
				// json.hpp의 ToString()이 FString을 반환한다고 가정합니다.
				// FString이 FString으로부터 생성 가능해야 합니다.
				OutValue = Value.ToString();
				return true;
			}
		}

		if (bInUseLog)
			UE_LOG_ERROR("[JsonSerializer] %s String 파싱에 실패했습니다 (기본값 사용)", InKey.c_str());

		OutValue = InDefaultValue;
		return false;
	}

	/**
	 * @brief JSON 객체에서 키를 찾아 JSON::Class::Object 값을 안전하게 읽어옵니다.
	 * @return 성공하면 true, 실패하면 false를 반환합니다.
	 */
	static bool ReadObject(const JSON& InJson, const FString& InKey, JSON& OutValue, JSON InDefaultValue = nullptr, bool bInUseLog = true)
	{
		if (InJson.hasKey(InKey))
		{
			const JSON& Value = InJson.at(InKey);
			if (Value.JSONType() == JSON::Class::Object)
			{
				OutValue = Value;
				return true;
			}
		}

		if (bInUseLog)
			UE_LOG_ERROR("[JsonSerializer] %s Object 파싱에 실패했습니다 (기본값 사용)", InKey.c_str());

		OutValue = InDefaultValue;
		return false;
	}

	/**
	 * @brief JSON 객체에서 키를 찾아 JSON::Class::Array 값을 안전하게 읽어옵니다.
	 * @return 성공하면 true, 실패하면 false를 반환합니다.
	 */
	static bool ReadArray(const JSON& InJson, const FString& InKey, JSON& OutValue, JSON InDefaultValue = nullptr, bool bInUseLog = true)
	{
		if (InJson.hasKey(InKey))
		{
			const JSON& Value = InJson.at(InKey);
			if (Value.JSONType() == JSON::Class::Array)
			{
				OutValue = Value;
				return true;
			}
		}

		if (bInUseLog)
			UE_LOG_ERROR("[JsonSerializer] %s Array 파싱에 실패했습니다 (기본값 사용)", InKey.c_str());

		OutValue = InDefaultValue;
		return false;
	}

	/**
	 * @brief float 한칸짜리 배열 읽기
	 * @return 성공하면 true, 실패하면 false를 반환합니다.
	 */
	static bool ReadArrayFloat(const JSON& InJson, const FString& InKey, float& OutValue, const float& InDefaultValue = 0.0f, bool bInUseLog = true)
	{
		if (InJson.hasKey(InKey))
		{
			const JSON& VectorJson = InJson.at(InKey);
			if (VectorJson.JSONType() == JSON::Class::Array && VectorJson.size() == 1)
			{
				try
				{
					OutValue = static_cast<float>(VectorJson.at(0).ToFloat());
					return true;
				}
				catch (const std::exception&)
				{
				}
			}
		}
		if (bInUseLog)
			UE_LOG_ERROR("[JsonSerializer] %s Array Float 파싱에 실패했습니다 (기본값 사용)", InKey.c_str());

		OutValue = InDefaultValue;
		return false;
	}

	/**
	 * @brief JSON 객체에서 키를 찾아 FVector 값을 안전하게 읽어옵니다.
	 * @return 성공하면 true, 실패하면 false를 반환합니다.
	 */
	static bool ReadVector(const JSON& InJson, const FString& InKey, FVector& OutValue, const FVector& InDefaultValue = FVector::Zero(), bool bInUseLog = true)
	{
		if (InJson.hasKey(InKey))
		{
			const JSON& VectorJson = InJson.at(InKey);
			if (VectorJson.JSONType() == JSON::Class::Array && VectorJson.size() == 3)
			{
				try
				{
					OutValue = {
						static_cast<float>(VectorJson.at(0).ToFloat()),
						static_cast<float>(VectorJson.at(1).ToFloat()),
						static_cast<float>(VectorJson.at(2).ToFloat())
					};
					return true;
				}
				catch (const std::exception&)
				{
				}
			}
		}
		if (bInUseLog)
			UE_LOG_ERROR("[JsonSerializer] %s Vector 파싱에 실패했습니다 (기본값 사용)", InKey.c_str());

		OutValue = InDefaultValue;
		return false;
	}

	//====================================================================================
	// Converting To JSON
	//====================================================================================

	static JSON VectorToJson(const FVector& InVector)
	{
		JSON VectorArray = JSON::Make(JSON::Class::Array);
		VectorArray.append(InVector.X, InVector.Y, InVector.Z);
		return VectorArray;
	}


	static JSON FloatToArrayJson(const float& InFloat)
	{
		JSON VectorArray = JSON::Make(JSON::Class::Array);
		VectorArray.append(InFloat);
		return VectorArray;
	}


	//====================================================================================
	// File I/O
	//====================================================================================

	static bool SaveJsonToFile(const JSON& InJsonData, const FString& InFilePath)
	{
		try
		{
			std::ofstream File(InFilePath);
			if (!File.is_open())
			{
				return false;
			}
			File << std::setw(2) << InJsonData << "\n";
			File.close();
			return true;
		}
		catch (const std::exception&)
		{
			return false;
		}
	}

	static bool LoadJsonFromFile(JSON& OutJson, const FString& InFilePath)
	{
		try
		{
			std::ifstream File(InFilePath);
			if (!File.is_open())
			{
				return false;
			}

			string TempString(
				(std::istreambuf_iterator<char>(File)), 
				std::istreambuf_iterator<char>()
			);
			FString FileContent(TempString);
			File.close();

			std::cout << "[JsonSerializer] File Content Length: " << FileContent.length() << "\n";
			OutJson = JSON::Load(FileContent);
			return true;
		}
		catch (const std::exception&)
		{
			return false;
		}
	}


	//====================================================================================
	// Utility & Analysis Functions
	//====================================================================================

	static FString FormatJsonString(const JSON& JsonData, int Indent = 2)
	{
		return JsonData.dump(Indent);
	}

	struct FLevelStats
	{
		uint32 TotalPrimitives = 0;
		TMap<EPrimitiveType, uint32> PrimitiveCountByType;
	};
};
