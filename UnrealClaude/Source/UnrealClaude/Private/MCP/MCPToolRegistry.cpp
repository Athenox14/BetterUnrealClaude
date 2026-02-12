// Copyright Natali Caggiano. All Rights Reserved.

#include "MCPToolRegistry.h"
#include "MCPTaskQueue.h"
#include "UnrealClaudeModule.h"
#include "UnrealClaudeConstants.h"

#include "Tools/MCPTool_Actor.h"
#include "Tools/MCPTool_SetProperty.h"
#include "Tools/MCPTool_Editor.h"
#include "Tools/MCPTool_ExecuteScript.h"
#include "Tools/MCPTool_CaptureViewport.h"
#include "Tools/MCPTool_BlueprintQuery.h"
#include "Tools/MCPTool_BlueprintModify.h"
#include "Tools/MCPTool_AnimBlueprintModify.h"
#include "Tools/MCPTool_EnhancedInput.h"
#include "Tools/MCPTool_Character.h"
#include "Tools/MCPTool_Material.h"
#include "Tools/MCPTool_Asset.h"
#include "Tools/MCPTool_OpenLevel.h"
#include "Tools/MCPTool_BehaviorTreeModify.h"

#include "Tools/MCPTool_Task.h"

FMCPToolRegistry::FMCPToolRegistry()
{
	RegisterBuiltinTools();
}

FMCPToolRegistry::~FMCPToolRegistry()
{
	StopTaskQueue();
	Tools.Empty();
}

void FMCPToolRegistry::StartTaskQueue()
{
	if (TaskQueue.IsValid())
	{
		TaskQueue->Start();
	}
}

void FMCPToolRegistry::StopTaskQueue()
{
	if (TaskQueue.IsValid())
	{
		TaskQueue->Shutdown();
	}
}

void FMCPToolRegistry::RegisterBuiltinTools()
{
	UE_LOG(LogUnrealClaude, Log, TEXT("Registering MCP tools..."));

	RegisterTool(MakeShared<FMCPTool_Actor>());
	RegisterTool(MakeShared<FMCPTool_SetProperty>());
	RegisterTool(MakeShared<FMCPTool_Editor>());

	RegisterTool(MakeShared<FMCPTool_ExecuteScript>());

	RegisterTool(MakeShared<FMCPTool_CaptureViewport>());

	RegisterTool(MakeShared<FMCPTool_BlueprintQuery>());
	RegisterTool(MakeShared<FMCPTool_BlueprintModify>());
	RegisterTool(MakeShared<FMCPTool_AnimBlueprintModify>());

	RegisterTool(MakeShared<FMCPTool_EnhancedInput>());

	RegisterTool(MakeShared<FMCPTool_Character>());

	RegisterTool(MakeShared<FMCPTool_Material>());
	RegisterTool(MakeShared<FMCPTool_Asset>());

	RegisterTool(MakeShared<FMCPTool_OpenLevel>());

	RegisterTool(MakeShared<FMCPTool_BehaviorTreeModify>());

	TaskQueue = MakeShared<FMCPTaskQueue>(this);

	if (TSharedPtr<IMCPTool>* ExecuteScriptToolPtr = Tools.Find(TEXT("execute_script")))
	{
		if (FMCPTool_ExecuteScript* ExecuteScriptTool = static_cast<FMCPTool_ExecuteScript*>(ExecuteScriptToolPtr->Get()))
		{
			ExecuteScriptTool->SetTaskQueue(TaskQueue);
			UE_LOG(LogUnrealClaude, Log, TEXT("  Wired up execute_script to task queue for async execution"));
		}
	}

	RegisterTool(MakeShared<FMCPTool_Task>(TaskQueue));

	UE_LOG(LogUnrealClaude, Log, TEXT("Registered %d MCP tools"), Tools.Num());
}

void FMCPToolRegistry::RegisterTool(TSharedPtr<IMCPTool> Tool)
{
	if (!Tool.IsValid())
	{
		return;
	}

	FMCPToolInfo Info = Tool->GetInfo();
	if (Info.Name.IsEmpty())
	{
		UE_LOG(LogUnrealClaude, Warning, TEXT("Cannot register tool with empty name"));
		return;
	}

	if (Tools.Contains(Info.Name))
	{
		UE_LOG(LogUnrealClaude, Warning, TEXT("Tool '%s' is already registered, replacing"), *Info.Name);
	}

	Tools.Add(Info.Name, Tool);
	UE_LOG(LogUnrealClaude, Log, TEXT("  Registered tool: %s"), *Info.Name);
}

void FMCPToolRegistry::UnregisterTool(const FString& ToolName)
{
	if (Tools.Remove(ToolName) > 0)
	{
		InvalidateToolCache();
		UE_LOG(LogUnrealClaude, Log, TEXT("Unregistered tool: %s"), *ToolName);
	}
}

void FMCPToolRegistry::InvalidateToolCache()
{
	bCacheValid = false;
	CachedToolInfo.Empty();
}

TArray<FMCPToolInfo> FMCPToolRegistry::GetAllTools() const
{
	// Return cached result if valid
	if (bCacheValid)
	{
		return CachedToolInfo;
	}

	// Rebuild cache
	CachedToolInfo.Empty(Tools.Num());
	for (const auto& Pair : Tools)
	{
		if (Pair.Value.IsValid())
		{
			CachedToolInfo.Add(Pair.Value->GetInfo());
		}
	}
	bCacheValid = true;

	return CachedToolInfo;
}

FMCPToolResult FMCPToolRegistry::ExecuteTool(const FString& ToolName, const TSharedRef<FJsonObject>& Params)
{
	TSharedPtr<IMCPTool>* FoundTool = Tools.Find(ToolName);
	if (!FoundTool || !FoundTool->IsValid())
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("Tool '%s' not found"), *ToolName));
	}

	UE_LOG(LogUnrealClaude, Log, TEXT("Executing MCP tool: %s"), *ToolName);

	FMCPToolResult Result;

	if (IsInGameThread())
	{
		Result = (*FoundTool)->Execute(Params);
	}
	else
	{
		TSharedPtr<FMCPToolResult> SharedResult = MakeShared<FMCPToolResult>();
		TSharedPtr<FEvent, ESPMode::ThreadSafe> CompletionEvent = MakeShareable(FPlatformProcess::GetSynchEventFromPool(),
			[](FEvent* Event) { FPlatformProcess::ReturnSynchEventToPool(Event); });
		TSharedPtr<TAtomic<bool>, ESPMode::ThreadSafe> bTaskCompleted = MakeShared<TAtomic<bool>, ESPMode::ThreadSafe>(false);

		AsyncTask(ENamedThreads::GameThread, [SharedResult, FoundTool, Params, CompletionEvent, bTaskCompleted]()
		{
			*SharedResult = (*FoundTool)->Execute(Params);
			*bTaskCompleted = true;
			CompletionEvent->Trigger();
		});

		const uint32 TimeoutMs = UnrealClaudeConstants::MCPServer::GameThreadTimeoutMs;
		const bool bSignaled = CompletionEvent->Wait(TimeoutMs);

		if (!bSignaled || !(*bTaskCompleted))
		{
			UE_LOG(LogUnrealClaude, Error, TEXT("Tool '%s' execution timed out after %d ms"), *ToolName, TimeoutMs);
			return FMCPToolResult::Error(FString::Printf(TEXT("Tool execution timed out after %d seconds"), TimeoutMs / 1000));
		}

		Result = *SharedResult;
	}

	UE_LOG(LogUnrealClaude, Log, TEXT("Tool '%s' execution %s: %s"),
		*ToolName,
		Result.bSuccess ? TEXT("succeeded") : TEXT("failed"),
		*Result.Message);

	return Result;
}

bool FMCPToolRegistry::HasTool(const FString& ToolName) const
{
	return Tools.Contains(ToolName);
}
