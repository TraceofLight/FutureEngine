#include "pch.h"
#include "Utility/Public/UELogParser.h"

#include <variant>

using std::variant;
using std::index_sequence;
using std::make_index_sequence;
using std::ostringstream;
using std::holds_alternative;
using std::hex;
using std::uppercase;
using std::oct;
using std::scientific;
using std::get;

/**
 * @brief 동적 UE_LOG 파서 클래스
 * 런타임에 문자열을 파싱하여 UELogParser 템플릿을 호출하는 시스템
 */
class FDynamicUELogParser
{
public:
    /**
     * @brief 인자 값을 저장하는 variant 타입
     */
    using ArgumentValue = variant<int, double, string, char>;

    /**
     * @brief 문자열에서 UE_LOG 구문을 파싱하여 처리
     * @param Input 전체 입력 문자열 (예: UE_LOG("Hello %d", 123))
     * @return 파싱 결과
     */
    static UELogParser::ParseResult ParseFromString(const string& Input)
    {
        try
        {
            // UE_LOG( 구문 탐지
            size_t StartPos = Input.find("UE_LOG(");
            if (StartPos != 0)
            {
                return UELogParser::CreateErrorResult("UE_LOG 구문이 아닙니다.");
            }

            StartPos += 7; // "UE_LOG(" 길이
            size_t EndPos = Input.rfind(')');

            if (EndPos == string::npos || EndPos <= StartPos)
            {
                return UELogParser::CreateErrorResult("UE_LOG 구문이 완료되지 않았습니다.");
            }

            string Arguments = Input.substr(StartPos, EndPos - StartPos);

            // 포맷 문자열과 인자들 분리
            string FormatString;
            TArray<string> Args;

            if (!ParseArguments(Arguments, FormatString, Args))
            {
                return UELogParser::CreateErrorResult("인자 파싱에 실패했습니다.");
            }

            // 인자들을 적절한 타입으로 변환
            TArray<ArgumentValue> ParsedArgs;
            if (!ConvertArguments(Args, ParsedArgs))
            {
                return UELogParser::CreateErrorResult("인자 타입 변환에 실패했습니다.");
            }

            // 동적 템플릿 호출
            return InvokeParser(FormatString, ParsedArgs);
        }
        catch (const exception& e)
        {
            return UELogParser::CreateErrorResult("파싱 중 오류: " + string(e.what()));
        }
    }

private:
    /**
     * @brief 인자 문자열을 파싱하여 포맷 문자열과 개별 인자들로 분리
     */
    static bool ParseArguments(const string& Arguments, string& FormatString, TArray<string>& Args)
    {
        if (Arguments.empty())
        {
            return false;
        }

        // 첫 번째 인자는 반드시 따옴표로 시작하는 문자열
        if (Arguments[0] != '"')
        {
            return false;
        }

        // 첫 번째 따옴표의 끝을 찾기
        size_t FormatEnd = FindMatchingQuote(Arguments, 0);
        if (FormatEnd == string::npos)
        {
            return false;
        }

        FormatString = Arguments.substr(1, FormatEnd - 1); // 따옴표 제외

        // 나머지 인자들 처리
        size_t CurrentPos = FormatEnd + 1;

        // 콤마 건너뛰기
        while (CurrentPos < Arguments.length() && (Arguments[CurrentPos] == ',' || Arguments[CurrentPos] == ' '))
        {
            ++CurrentPos;
        }

        if (CurrentPos >= Arguments.length())
        {
            // 인자가 없는 경우
            return true;
        }

        // 나머지 인자들을 콤마로 분리
        return ParseRemainingArguments(Arguments.substr(CurrentPos), Args);
    }

    /**
     * @brief 따옴표의 매칭되는 끝을 찾는 함수
     */
    static size_t FindMatchingQuote(const string& Str, size_t Start)
    {
        for (size_t i = Start + 1; i < Str.length(); ++i)
        {
            if (Str[i] == '"' && (i == 0 || Str[i-1] != '\\'))
            {
                return i;
            }
        }
        return string::npos;
    }

    /**
     * @brief 콤마로 구분된 나머지 인자들을 파싱
     */
    static bool ParseRemainingArguments(const string& ArgsString, TArray<string>& Args)
    {
        bool bInQuotes = false;
        string CurrentArgument;

        for (size_t i = 0; i < ArgsString.length(); ++i)
        {
            char c = ArgsString[i];

            if (c == '"' && (i == 0 || ArgsString[i-1] != '\\'))
            {
                bInQuotes = !bInQuotes;
                CurrentArgument += c;
            }
            else if (c == ',' && !bInQuotes)
            {
                // 인자 완료
                string TrimmedArg = TrimString(CurrentArgument);
                if (!TrimmedArg.empty())
                {
                    Args.Add(TrimmedArg);
                }
                CurrentArgument.clear();
            }
            else
            {
                CurrentArgument += c;
            }
        }

        // 마지막 인자 처리
        string TrimmedArgument = TrimString(CurrentArgument);
        if (!TrimmedArgument.empty())
        {
            Args.Add(TrimmedArgument);
        }

        return true;
    }

    /**
     * @brief 문자열 앞뒤 공백 제거
     */
    static string TrimString(const string& InString)
    {
        size_t Start = InString.find_first_not_of(" \t\r\n");
        if (Start == string::npos)
        {
	        return "";
        }

        size_t End = InString.find_last_not_of(" \t\r\n");
        return InString.substr(Start, End - Start + 1);
    }

    /**
     * @brief 문자열 인자들을 적절한 타입으로 변환
     */
    static bool ConvertArguments(const TArray<string>& Args, TArray<ArgumentValue>& ParsedArgs)
    {
        ParsedArgs.Empty();
        ParsedArgs.Reserve(Args.Num());

        for (const auto& Arg : Args)
        {
            ArgumentValue ParsedValue;
            if (!ConvertSingleArgument(Arg, ParsedValue))
            {
                return false;
            }
            ParsedArgs.Add(ParsedValue);
        }

        return true;
    }

    /**
     * @brief 단일 인자를 적절한 타입으로 변환
     */
    static bool ConvertSingleArgument(const string& Arg, ArgumentValue& ParsedValue)
    {
        string TrimmedArg = TrimString(Arg);

        if (TrimmedArg.empty())
        {
            return false;
        }

        // 문자열인지 확인 (따옴표로 둘러싸여 있음)
        if (TrimmedArg.length() >= 2 && TrimmedArg.front() == '"' && TrimmedArg.back() == '"')
        {
            ParsedValue = TrimmedArg.substr(1, TrimmedArg.length() - 2);
            return true;
        }

        // 숫자인지 확인
        try
        {
            // 16진수 리터럴인지 확인 (0x 또는 0X로 시작)
            if (TrimmedArg.length() > 2 &&
                (TrimmedArg.substr(0, 2) == "0x" || TrimmedArg.substr(0, 2) == "0X"))
            {
                int IntValue = static_cast<int>(stoul(TrimmedArg, nullptr, 16));
                ParsedValue = IntValue;
                return true;
            }
            // 8진수 리터럴인지 확인 (0으로 시작하는 숫자)
            else if (TrimmedArg.length() > 1 && TrimmedArg[0] == '0' &&
                     all_of(TrimmedArg.begin() + 1, TrimmedArg.end(),
                     [](char c) { return c >= '0' && c <= '7'; }))
            {
                int IntValue = static_cast<int>(stoul(TrimmedArg, nullptr, 8));
                ParsedValue = IntValue;
                return true;
            }
            // 일반 정수인지 확인
            else if (TrimmedArg.find('.') == string::npos &&
                     TrimmedArg.find('e') == string::npos &&
                     TrimmedArg.find('E') == string::npos)
            {
                int IntValue = stoi(TrimmedArg);
                ParsedValue = IntValue;
                return true;
            }
            else
            {
                double DoubleValue = stod(TrimmedArg);
                ParsedValue = DoubleValue;
                return true;
            }
        }
        catch (const exception&)
        {
            // 숫자 변환 실패시 문자열로 처리
            ParsedValue = TrimmedArg;
            return true;
        }
    }

    /**
     * @brief 파싱된 인자들로 UELogParser를 동적으로 호출
     * 최대 20개까지의 인자를 지원하며, 더 많은 인자가 필요한 경우 확장 가능
     */
    static UELogParser::ParseResult InvokeParser(const string& FormatString, const TArray<ArgumentValue>& Args)
    {
        // 인자 개수에 따라 적절한 템플릿 함수 호출
        switch (Args.Num())
        {
            case 0: return UELogParser::Parse(FormatString);
            case 1: return InvokeWithArgs<1>(FormatString, Args);
            case 2: return InvokeWithArgs<2>(FormatString, Args);
            case 3: return InvokeWithArgs<3>(FormatString, Args);
            case 4: return InvokeWithArgs<4>(FormatString, Args);
            case 5: return InvokeWithArgs<5>(FormatString, Args);
            case 6: return InvokeWithArgs<6>(FormatString, Args);
            case 7: return InvokeWithArgs<7>(FormatString, Args);
            case 8: return InvokeWithArgs<8>(FormatString, Args);
            case 9: return InvokeWithArgs<9>(FormatString, Args);
            case 10: return InvokeWithArgs<10>(FormatString, Args);
            case 11: return InvokeWithArgs<11>(FormatString, Args);
            case 12: return InvokeWithArgs<12>(FormatString, Args);
            case 13: return InvokeWithArgs<13>(FormatString, Args);
            case 14: return InvokeWithArgs<14>(FormatString, Args);
            case 15: return InvokeWithArgs<15>(FormatString, Args);
            case 16: return InvokeWithArgs<16>(FormatString, Args);
            case 17: return InvokeWithArgs<17>(FormatString, Args);
            case 18: return InvokeWithArgs<18>(FormatString, Args);
            case 19: return InvokeWithArgs<19>(FormatString, Args);
            case 20: return InvokeWithArgs<20>(FormatString, Args);

            default:
            {
                // 20개를 초과하는 인자의 경우 메타프로그래밍으로 처리
                if (Args.Num() <= 100)
                {
                    return InvokeLargeArgumentSet(FormatString, Args);
                }
                else
                {
                    return UELogParser::CreateErrorResult("인자가 너무 많습니다 (최대 100개 지원): " + to_string(Args.Num()));
                }
            }
        }
    }

    /**
     * @brief N개의 인자로 UELogParser를 호출하는 템플릿 함수
     */
    template<size_t N>
    static UELogParser::ParseResult InvokeWithArgs(const string& FormatString, const TArray<ArgumentValue>& Args)
    {
        if (Args.Num() != N)
        {
            return UELogParser::CreateErrorResult("인자 개수 불일치");
        }

        return InvokeWithArgsImpl<N>(FormatString, Args, make_index_sequence<N>{});
    }

    /**
     * @brief 인덱스 시퀀스를 사용한 가변 인자 전개 - 문자열 기반
     */
    template<size_t N, size_t... Is>
    static UELogParser::ParseResult InvokeWithArgsImpl(const string& FormatString,
                                                      const TArray<ArgumentValue>& Args,
                                                      index_sequence<Is...>)
    {
        return UELogParser::Parse(FormatString, ConvertArgumentToString(Args[Is])...);
    }


    /**
     * @brief variant에서 포맷 지정자에 따라 문자열로 변환
     */
    static string FormatArgumentFromVariant(const ArgumentValue& Value, char FormatSpec)
    {
        if (holds_alternative<int>(Value))
        {
            int Val = get<int>(Value);
            switch (FormatSpec)
            {
                case 'd': case 'i': return to_string(Val);
                case 'x': { ostringstream ss; ss << hex << Val; return ss.str(); }
                case 'X': { ostringstream ss; ss << hex << uppercase << Val; return ss.str(); }
                case 'o': { ostringstream ss; ss << oct << Val; return ss.str(); }
                case 'c': return string(1, static_cast<char>(Val));
                default: return to_string(Val);
            }
        }
        else if (holds_alternative<double>(Value))
        {
            double Val = get<double>(Value);
            switch (FormatSpec)
            {
                case 'f': case 'F': return to_string(Val);
                case 'e': { ostringstream ss; ss << scientific << Val; return ss.str(); }
                case 'E': { ostringstream ss; ss << scientific << uppercase << Val; return ss.str(); }
                case 'g': case 'G':
                {
                    ostringstream ss;
                    if (FormatSpec == 'G') ss << uppercase;
                    if (abs(Val) < 1e-4 || abs(Val) >= 1e6) ss << scientific;
                    ss << Val;
                    return ss.str();
                }
                default: return to_string(Val);
            }
        }
        else if (holds_alternative<string>(Value))
        {
            return get<string>(Value);
        }
        else if (holds_alternative<char>(Value))
        {
            return string(1, get<char>(Value));
        }
        return "";
    }

    /**
     * @brief ArgumentValue를 실제 값으로 변환
     */
    template<typename T>
    static T ConvertArgumentValue(const ArgumentValue& Value)
    {
        if constexpr (is_same_v<T, int>)
        {
            if (holds_alternative<int>(Value))
                return get<int>(Value);
            else if (holds_alternative<double>(Value))
                return static_cast<int>(get<double>(Value));
            else if (holds_alternative<string>(Value))
                return stoi(get<string>(Value));
            else
                return static_cast<int>(get<char>(Value));
        }
        else if constexpr (is_same_v<T, double>)
        {
            if (holds_alternative<double>(Value))
                return get<double>(Value);
            else if (holds_alternative<int>(Value))
                return static_cast<double>(get<int>(Value));
            else if (holds_alternative<string>(Value))
                return stod(get<string>(Value));
            else
                return static_cast<double>(get<char>(Value));
        }
        else if constexpr (is_same_v<T, string>)
        {
            if (holds_alternative<string>(Value))
                return get<string>(Value);
            else if (holds_alternative<int>(Value))
                return to_string(get<int>(Value));
            else if (holds_alternative<double>(Value))
                return to_string(get<double>(Value));
            else
                return string(1, get<char>(Value));
        }
        else if constexpr (is_same_v<T, char>)
        {
            if (holds_alternative<char>(Value))
                return get<char>(Value);
            else if (holds_alternative<int>(Value))
                return static_cast<char>(get<int>(Value));
            else if (holds_alternative<string>(Value))
            {
                const auto& Str = get<string>(Value);
                return Str.empty() ? '\0' : Str[0];
            }
            else
                return static_cast<char>(get<double>(Value));
        }
    }

    /**
     * @brief ArgumentValue를 문자열로 변환 (visit 타입 통일)
     */
    static string ConvertArgumentToString(const ArgumentValue& Value)
    {
        return visit([](const auto& Val) -> string {
            using ValueType = decay_t<decltype(Val)>;
            if constexpr (is_same_v<ValueType, string>)
            {
                return Val;
            }
            else if constexpr (is_same_v<ValueType, int>)
            {
                return to_string(Val);
            }
            else if constexpr (is_same_v<ValueType, double>)
            {
                return to_string(Val);
            }
            else if constexpr (is_same_v<ValueType, char>)
            {
                return string(1, Val);
            }
            return "";
        }, Value);
    }

    /**
     * @brief ArgumentValue를 가장 적절한 타입으로 자동 변환
     */
    template<typename T>
    static T ConvertArgument(const ArgumentValue& Value)
    {
        if constexpr (is_same_v<T, int>)
        {
            if (holds_alternative<int>(Value))
                return get<int>(Value);
            else if (holds_alternative<double>(Value))
                return static_cast<int>(get<double>(Value));
            else if (holds_alternative<string>(Value))
                return stoi(get<string>(Value));
            else
                return static_cast<int>(get<char>(Value));
        }
        else if constexpr (is_same_v<T, double>)
        {
            if (holds_alternative<double>(Value))
                return get<double>(Value);
            else if (holds_alternative<int>(Value))
                return static_cast<double>(get<int>(Value));
            else if (holds_alternative<string>(Value))
                return stod(get<string>(Value));
            else
                return static_cast<double>(get<char>(Value));
        }
        else if constexpr (is_same_v<T, string>)
        {
            return ConvertArgumentToString(Value);
        }
        else if constexpr (is_same_v<T, char>)
        {
            if (holds_alternative<char>(Value))
                return get<char>(Value);
            else if (holds_alternative<int>(Value))
                return static_cast<char>(get<int>(Value));
            else if (holds_alternative<string>(Value))
            {
                const auto& Str = get<string>(Value);
                return Str.empty() ? '\0' : Str[0];
            }
            else
                return static_cast<char>(get<double>(Value));
        }
        return T{};
    }

    /**
     * @brief 100개까지의 대용량 인자 세트 처리
     * 재귀적 템플릿 확장을 통해 처리
     */
    static UELogParser::ParseResult InvokeLargeArgumentSet(const string& FormatString, const TArray<ArgumentValue>& Args)
    {
        // 큰 인자 세트의 경우, 청크 단위로 나누어 처리하거나
        // 직접 문자열 조작을 통해 결과를 생성
        try
        {
            string Result = FormatString;
            size_t ArgIndex = 0;
            size_t Pos = 0;

            while ((Pos = Result.find('%', Pos)) != string::npos)
            {
                if (Pos + 1 < Result.length())
                {
                    char FormatSpec = Result[Pos + 1];

                    if (FormatSpec == '%')
                    {
                        // %% 케이스, 건너뛰기
                        Pos += 2;
                        continue;
                    }

                    if (ArgIndex >= Args.Num())
                    {
                        return UELogParser::CreateErrorResult("포맷 지정자보다 인자가 부족합니다.");
                    }

                    // 인자를 문자열로 변환
                    string ArgString = FormatArgumentFromVariant(Args[ArgIndex], FormatSpec);

                    // %x를 실제 값으로 교체
                    Result.replace(Pos, 2, ArgString);
                    Pos += ArgString.length();
                    ++ArgIndex;
                }
                else
                {
                    return UELogParser::CreateErrorResult("불완전한 포맷 지정자입니다.");
                }
            }

            if (ArgIndex != Args.Num())
            {
                return UELogParser::CreateErrorResult("사용되지 않은 인자가 있습니다: " + to_string(Args.Num() - ArgIndex) + "개");
            }

            UELogParser::ParseResult Success(true);
            Success.FormattedMessage = Result;
            return Success;
        }
        catch (const exception& e)
        {
            return UELogParser::CreateErrorResult("대용량 인자 세트 처리 중 오류: " + string(e.what()));
        }
    }
};

/**
 * @brief 외부 인터페이스 함수 - ConsoleWidget에서 사용
 * @param InString UE_LOG 문자열 (예: "UE_LOG(\"Hello %d %s\", 123, \"World\")")
 * @return 파싱 결과
 */
UELogParser::ParseResult ParseUELogFromString(const FString& InString)
{
    return FDynamicUELogParser::ParseFromString(InString);
}
