#pragma once

/**
 * @brief 비동기 로그 파일 작성기
 * 워커 스레드를 사용하여 메인 스레드 성능 영향 최소화
 */
class FLogFileWriter
{
public:
	FLogFileWriter();
	~FLogFileWriter();

	// 초기화 및 종료
	void Initialize();
	void Shutdown();

	// 로그 추가
	void AddLog(const FString& InLog);

	bool IsInitialized() const
	{
		return bIsInitialized;
	}

	FString GetCurrentLogFileName() const
	{
		return CurrentLogFileName;
	}

private:
	// 워커 스레드 함수
	void WorkerThreadFunc();

	// 로그 폴더 관리
	static void CreateLogDirectory();
	static void CleanupOldLogFiles();
	static FString GenerateLogFileName();

	// 파일 쓰기
	void WriteBatchToFile(const TArray<FString>& InBatch) const;

	thread WorkerThread;
	TQueue<FString> LogQueue;
	mutex QueueMutex;
	condition_variable QueueCondition;
	atomic<bool> bShouldStop;
	atomic<bool> bIsInitialized;

	HANDLE FileHandle;
	FString CurrentLogFileName;

	static constexpr uint32 MaxLogFiles = 20;
	static constexpr uint32 BatchSize = 100;
};
