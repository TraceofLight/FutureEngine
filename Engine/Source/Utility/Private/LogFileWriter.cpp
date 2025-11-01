#include "pch.h"
#include "Utility/Public/LogFileWriter.h"

FLogFileWriter::FLogFileWriter()
	: bShouldStop(false)
	, bIsInitialized(false)
	, FileHandle(INVALID_HANDLE_VALUE)
{
}

FLogFileWriter::~FLogFileWriter()
{
	Shutdown();
}

void FLogFileWriter::Initialize()
{
	if (bIsInitialized.load())
	{
		return;
	}

	CreateLogDirectory();
	CleanupOldLogFiles();

	CurrentLogFileName = GenerateLogFileName();

	// 전체 경로 생성 (Log/ + 파일명)
	FString FullPath = "Log/" + CurrentLogFileName;

	// CreateFile with async write optimization
	FileHandle = CreateFileA(
		FullPath.data(),
		GENERIC_WRITE,
		FILE_SHARE_READ,
		nullptr,
		CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
		nullptr
	);

	if (FileHandle == INVALID_HANDLE_VALUE)
	{
		return;
	}

	bIsInitialized.store(true);
	bShouldStop.store(false);

	WorkerThread = std::thread(&FLogFileWriter::WorkerThreadFunc, this);
}

void FLogFileWriter::Shutdown()
{
	if (!bIsInitialized.load())
	{
		return;
	}

	// 종료 신호 전송
	bShouldStop.store(true);
	QueueCondition.notify_all();

	// 워커 스레드가 모든 로그를 처리할 때까지 대기
	if (WorkerThread.joinable())
	{
		WorkerThread.join();
	}

	// 남은 로그가 있다면 마지막으로 직접 쓰기
	if (!LogQueue.empty() && FileHandle != INVALID_HANDLE_VALUE)
	{
		TArray<FString> RemainingLogs;
		while (!LogQueue.empty())
		{
			RemainingLogs.Emplace(LogQueue.front());
			LogQueue.pop();
		}
		WriteBatchToFile(RemainingLogs);
	}

	// 파일 핸들 닫기
	if (FileHandle != INVALID_HANDLE_VALUE)
	{
		FlushFileBuffers(FileHandle);
		CloseHandle(FileHandle);
		FileHandle = INVALID_HANDLE_VALUE;
	}

	bIsInitialized.store(false);
}

void FLogFileWriter::AddLog(const FString& InLog)
{
	if (!bIsInitialized.load())
	{
		return;
	}

	{
		std::lock_guard<std::mutex> Lock(QueueMutex);
		LogQueue.push(InLog);
	}

	QueueCondition.notify_one();
}

void FLogFileWriter::WorkerThreadFunc()
{
	TArray<FString> Batch;
	Batch.Reserve(BatchSize);

	while (!bShouldStop.load() || !LogQueue.empty())
	{
		{
			std::unique_lock<std::mutex> Lock(QueueMutex);
			QueueCondition.wait(Lock, [this]
			{
				return !LogQueue.empty() || bShouldStop.load();
			});

			while (!LogQueue.empty() && Batch.Num() < BatchSize)
			{
				Batch.Emplace(std::move(LogQueue.front()));
				LogQueue.pop();
			}
		}

		if (!Batch.IsEmpty())
		{
			WriteBatchToFile(Batch);
			Batch.Empty();
		}
	}
}

void FLogFileWriter::CreateLogDirectory()
{
	DWORD Attributes = GetFileAttributesA("Log");

	// 폴더가 존재하지 않거나 파일인 경우
	if (Attributes == INVALID_FILE_ATTRIBUTES)
	{
		// 폴더 생성
		if (!CreateDirectoryA("Log", nullptr))
		{
			DWORD Error = GetLastError();
			if (Error != ERROR_ALREADY_EXISTS)
			{
				return;
			}
		}
	}
	else if (!(Attributes & FILE_ATTRIBUTE_DIRECTORY))
	{
		return;
	}
}

void FLogFileWriter::CleanupOldLogFiles()
{
	WIN32_FIND_DATAA FindData;
	HANDLE hFind = FindFirstFileA("Log\\*.log", &FindData);

	if (hFind == INVALID_HANDLE_VALUE)
	{
		return;
	}

	TArray<FString> LogFiles;

	do
	{
		if (!(FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
		{
			LogFiles.Emplace(FindData.cFileName);
		}
	}
	while (FindNextFileA(hFind, &FindData));

	FindClose(hFind);

	std::ranges::sort(LogFiles);

	while (LogFiles.Num() >= MaxLogFiles)
	{
		FString FileToDelete = "Log\\" + LogFiles[0];
		DeleteFileA(FileToDelete.data());
		LogFiles.RemoveAt(0);
	}
}

FString FLogFileWriter::GenerateLogFileName()
{
	SYSTEMTIME LocalTime;
	GetLocalTime(&LocalTime);

	char FileName[256];
	(void)sprintf_s(FileName, sizeof(FileName), "%04d-%02d-%02d_%02d-%02d-%02d.log",
		LocalTime.wYear, LocalTime.wMonth, LocalTime.wDay,
		LocalTime.wHour, LocalTime.wMinute, LocalTime.wSecond);

	return FString(FileName);
}

void FLogFileWriter::WriteBatchToFile(const TArray<FString>& InBatch) const
{
	if (FileHandle == INVALID_HANDLE_VALUE || InBatch.IsEmpty())
	{
		return;
	}

	FString CombinedLog;
	CombinedLog.reserve(InBatch.Num() * 128);

	for (const auto& Log : InBatch)
	{
		CombinedLog += Log;
		CombinedLog += "\r\n";
	}

	DWORD BytesWritten = 0;
	WriteFile(
		FileHandle,
		CombinedLog.c_str(),
		static_cast<DWORD>(CombinedLog.size()),
		&BytesWritten,
		nullptr
	);

	FlushFileBuffers(FileHandle);
}
