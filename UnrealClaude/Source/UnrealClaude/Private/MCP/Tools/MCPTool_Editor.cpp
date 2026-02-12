// Copyright Natali Caggiano. All Rights Reserved.

#include "MCPTool_Editor.h"
#include "MCP/MCPParamValidator.h"
#include "UnrealClaudeModule.h"
#include "UnrealClaudeUtils.h"
#include "UnrealClaudeConstants.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFileManager.h"

FMCPToolResult FMCPTool_Editor::Execute(const TSharedRef<FJsonObject>& Params)
{
	FString Operation;
	TOptional<FMCPToolResult> Error;
	if (!ExtractRequiredString(Params, TEXT("operation"), Operation, Error))
	{
		return Error.GetValue();
	}

	Operation = Operation.ToLower();

	if (Operation == TEXT("console_command"))
	{
		return ExecuteConsoleCommand(Params);
	}
	else if (Operation == TEXT("get_log"))
	{
		return ExecuteGetLog(Params);
	}

	return FMCPToolResult::Error(FString::Printf(
		TEXT("Unknown operation: '%s'. Valid operations: 'console_command', 'get_log'"),
		*Operation));
}

FMCPToolResult FMCPTool_Editor::ExecuteConsoleCommand(const TSharedRef<FJsonObject>& Params)
{
	UWorld* World = nullptr;
	if (auto Error = ValidateEditorContext(World))
	{
		return Error.GetValue();
	}

	FString Command;
	TOptional<FMCPToolResult> ParamError;
	if (!ExtractRequiredString(Params, TEXT("command"), Command, ParamError))
	{
		return ParamError.GetValue();
	}
	if (!ValidateConsoleCommandParam(Command, ParamError))
	{
		return ParamError.GetValue();
	}

	UE_LOG(LogUnrealClaude, Log, TEXT("Executing console command: %s"), *Command);

	FUnrealClaudeOutputDevice OutputDevice;

	GEditor->Exec(World, *Command, OutputDevice);

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetStringField(TEXT("command"), Command);
	ResultData->SetStringField(TEXT("output"), OutputDevice.GetTrimmedOutput());

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Executed command: %s"), *Command),
		ResultData
	);
}

FMCPToolResult FMCPTool_Editor::ExecuteGetLog(const TSharedRef<FJsonObject>& Params)
{
	int32 NumLines = UnrealClaudeConstants::MCPServer::DefaultOutputLogLines;
	if (Params->HasField(TEXT("lines")))
	{
		NumLines = FMath::Clamp(static_cast<int32>(Params->GetNumberField(TEXT("lines"))), 1, UnrealClaudeConstants::MCPServer::MaxOutputLogLines);
	}

	FString Filter;
	Params->TryGetStringField(TEXT("filter"), Filter);

	FString ProjectLogDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectLogDir());
	FString EngineLogDir = FPaths::ConvertRelativePathToFull(FPaths::EngineDir() / TEXT("Saved/Logs"));

	FString LogFilePath;
	bool bFound = false;
	TArray<FString> SearchedPaths;

	{
		FString Candidate = ProjectLogDir / FApp::GetProjectName() + TEXT(".log");
		SearchedPaths.Add(Candidate);
		if (FPaths::FileExists(Candidate))
		{
			LogFilePath = Candidate;
			bFound = true;
		}
	}

	if (!bFound)
	{
		FString Candidate = ProjectLogDir / TEXT("UnrealEditor.log");
		SearchedPaths.Add(Candidate);
		if (FPaths::FileExists(Candidate))
		{
			LogFilePath = Candidate;
			bFound = true;
		}
	}

	if (!bFound)
	{
		TArray<FString> LogFiles;
		IFileManager::Get().FindFiles(LogFiles, *ProjectLogDir, TEXT("*.log"));
		if (LogFiles.Num() > 0)
		{
			LogFilePath = ProjectLogDir / LogFiles[0];
			bFound = true;
		}
	}

	if (!bFound)
	{
		FString Candidate = EngineLogDir / TEXT("UnrealEditor.log");
		SearchedPaths.Add(Candidate);
		if (FPaths::FileExists(Candidate))
		{
			LogFilePath = Candidate;
			bFound = true;
		}
	}

	if (!bFound)
	{
		TArray<FString> LogFiles;
		IFileManager::Get().FindFiles(LogFiles, *EngineLogDir, TEXT("*.log"));
		if (LogFiles.Num() > 0)
		{
			LogFilePath = EngineLogDir / LogFiles[0];
			bFound = true;
		}
	}

	if (!bFound)
	{
		FString AllPaths = FString::Join(SearchedPaths, TEXT(", "));
		return FMCPToolResult::Error(
			FString::Printf(TEXT("No log file found. Searched paths: %s. Also scanned directories: %s, %s"),
				*AllPaths, *ProjectLogDir, *EngineLogDir));
	}

	FString LogContent;
	if (!FFileHelper::LoadFileToString(LogContent, *LogFilePath, FFileHelper::EHashOptions::None, FILEREAD_AllowWrite))
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("Failed to read log file: %s"), *LogFilePath));
	}

	TArray<FString> AllLines;
	LogContent.ParseIntoArrayLines(AllLines);

	TArray<FString> FilteredLines;
	if (Filter.IsEmpty())
	{
		FilteredLines = AllLines;
	}
	else
	{
		for (const FString& Line : AllLines)
		{
			if (Line.Contains(Filter, ESearchCase::IgnoreCase))
			{
				FilteredLines.Add(Line);
			}
		}
	}

	int32 StartIndex = FMath::Max(0, FilteredLines.Num() - NumLines);
	TArray<FString> ResultLines;
	for (int32 i = StartIndex; i < FilteredLines.Num(); ++i)
	{
		ResultLines.Add(FilteredLines[i]);
	}

	FString LogOutput = FString::Join(ResultLines, TEXT("\n"));

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetStringField(TEXT("log_file"), LogFilePath);
	ResultData->SetNumberField(TEXT("total_lines"), AllLines.Num());
	ResultData->SetNumberField(TEXT("returned_lines"), ResultLines.Num());
	if (!Filter.IsEmpty())
	{
		ResultData->SetStringField(TEXT("filter"), Filter);
		ResultData->SetNumberField(TEXT("filtered_lines"), FilteredLines.Num());
	}
	ResultData->SetStringField(TEXT("content"), LogOutput);

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Retrieved %d log lines from %s"), ResultLines.Num(), *FPaths::GetCleanFilename(LogFilePath)),
		ResultData
	);
}
