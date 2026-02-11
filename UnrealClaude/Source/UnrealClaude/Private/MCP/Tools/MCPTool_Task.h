// Copyright Natali Caggiano. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "../MCPToolBase.h"
#include "../MCPTaskQueue.h"

/**
 * MCP Tool: task
 *
 * Unified async task management. Submit tools for background execution,
 * check status, retrieve results, list tasks, or cancel them.
 *
 * Operations:
 * - submit: Submit a tool for async background execution
 * - status: Get task status and progress
 * - result: Get completed task output
 * - list: List all tasks with stats
 * - cancel: Cancel a pending/running task
 */
class FMCPTool_Task : public FMCPToolBase
{
public:
	FMCPTool_Task(TSharedPtr<FMCPTaskQueue> InTaskQueue)
		: TaskQueue(InTaskQueue)
	{
	}

	virtual FMCPToolInfo GetInfo() const override
	{
		FMCPToolInfo Info;
		Info.Name = TEXT("task");
		Info.Description = TEXT(
			"Async task management for long-running operations. "
			"Operations: 'submit' (start async tool), 'status' (poll progress), "
			"'result' (get output), 'list' (all tasks), 'cancel' (stop task). "
			"Workflow: submit -> poll status -> get result.");
		Info.Parameters = {
			FMCPToolParameter(TEXT("operation"), TEXT("string"),
				TEXT("Operation: 'submit', 'status', 'result', 'list', 'cancel'"), true),
			FMCPToolParameter(TEXT("task_id"), TEXT("string"),
				TEXT("Task ID for status/result/cancel operations"), false),
			FMCPToolParameter(TEXT("tool_name"), TEXT("string"),
				TEXT("MCP tool name for 'submit' operation"), false),
			FMCPToolParameter(TEXT("params"), TEXT("object"),
				TEXT("Parameters for the tool being submitted"), false),
			FMCPToolParameter(TEXT("timeout_ms"), TEXT("number"),
				TEXT("Timeout in ms for 'submit' (default: 120000)"), false, TEXT("120000")),
			FMCPToolParameter(TEXT("include_completed"), TEXT("boolean"),
				TEXT("Include finished tasks in 'list' (default: true)"), false, TEXT("true")),
			FMCPToolParameter(TEXT("limit"), TEXT("number"),
				TEXT("Max tasks for 'list' (default: 50)"), false, TEXT("50")),
		};
		Info.Annotations = FMCPToolAnnotations::ReadOnly();
		return Info;
	}

	virtual FMCPToolResult Execute(const TSharedRef<FJsonObject>& Params) override
	{
		if (!TaskQueue.IsValid())
		{
			return FMCPToolResult::Error(TEXT("Task queue not initialized"));
		}

		FString Operation;
		TOptional<FMCPToolResult> Error;
		if (!ExtractRequiredString(Params, TEXT("operation"), Operation, Error))
		{
			return Error.GetValue();
		}

		Operation = Operation.ToLower();

		if (Operation == TEXT("submit"))
		{
			return HandleSubmit(Params);
		}
		else if (Operation == TEXT("status"))
		{
			return HandleStatus(Params);
		}
		else if (Operation == TEXT("result"))
		{
			return HandleResult(Params);
		}
		else if (Operation == TEXT("list"))
		{
			return HandleList(Params);
		}
		else if (Operation == TEXT("cancel"))
		{
			return HandleCancel(Params);
		}

		return FMCPToolResult::Error(FString::Printf(
			TEXT("Unknown operation: '%s'. Valid: submit, status, result, list, cancel"), *Operation));
	}

private:
	FGuid ParseTaskId(const TSharedRef<FJsonObject>& Params, FMCPToolResult& OutError)
	{
		FString TaskIdString = ExtractOptionalString(Params, TEXT("task_id"));
		if (TaskIdString.IsEmpty())
		{
			OutError = FMCPToolResult::Error(TEXT("'task_id' is required"));
			return FGuid();
		}

		FGuid TaskId;
		if (!FGuid::Parse(TaskIdString, TaskId))
		{
			OutError = FMCPToolResult::Error(TEXT("Invalid task_id format"));
			return FGuid();
		}
		return TaskId;
	}

	FMCPToolResult HandleSubmit(const TSharedRef<FJsonObject>& Params)
	{
		FString ToolName = ExtractOptionalString(Params, TEXT("tool_name"));
		if (ToolName.IsEmpty())
		{
			return FMCPToolResult::Error(TEXT("'tool_name' is required for submit"));
		}

		TSharedPtr<FJsonObject> ToolParams;
		const TSharedPtr<FJsonObject>* ParamsObj;
		if (Params->TryGetObjectField(TEXT("params"), ParamsObj))
		{
			ToolParams = *ParamsObj;
		}
		else
		{
			ToolParams = MakeShared<FJsonObject>();
		}

		uint32 TimeoutMs = static_cast<uint32>(ExtractOptionalNumber<int32>(Params, TEXT("timeout_ms"), 120000));

		FGuid TaskId = TaskQueue->SubmitTask(ToolName, ToolParams, TimeoutMs);
		if (!TaskId.IsValid())
		{
			return FMCPToolResult::Error(TEXT("Failed to submit task - queue may be at capacity or tool not found"));
		}

		TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
		ResultData->SetStringField(TEXT("task_id"), TaskId.ToString());
		ResultData->SetStringField(TEXT("tool_name"), ToolName);
		ResultData->SetStringField(TEXT("status"), TEXT("pending"));
		ResultData->SetNumberField(TEXT("timeout_ms"), TimeoutMs);

		return FMCPToolResult::Success(
			FString::Printf(TEXT("Task submitted: %s"), *TaskId.ToString()), ResultData);
	}

	FMCPToolResult HandleStatus(const TSharedRef<FJsonObject>& Params)
	{
		FMCPToolResult Error;
		FGuid TaskId = ParseTaskId(Params, Error);
		if (!TaskId.IsValid()) return Error;

		TSharedPtr<FMCPAsyncTask> Task = TaskQueue->GetTask(TaskId);
		if (!Task.IsValid())
		{
			return FMCPToolResult::Error(FString::Printf(TEXT("Task not found: %s"), *TaskId.ToString()));
		}

		TSharedPtr<FJsonObject> ResultData = Task->ToJson(false);
		FString StatusStr = FMCPAsyncTask::StatusToString(Task->Status.Load());
		return FMCPToolResult::Success(
			FString::Printf(TEXT("Task %s: %s"), *TaskId.ToString(), *StatusStr), ResultData);
	}

	FMCPToolResult HandleResult(const TSharedRef<FJsonObject>& Params)
	{
		FMCPToolResult Error;
		FGuid TaskId = ParseTaskId(Params, Error);
		if (!TaskId.IsValid()) return Error;

		TSharedPtr<FMCPAsyncTask> Task = TaskQueue->GetTask(TaskId);
		if (!Task.IsValid())
		{
			return FMCPToolResult::Error(FString::Printf(TEXT("Task not found: %s"), *TaskId.ToString()));
		}

		if (!Task->IsComplete())
		{
			EMCPTaskStatus Status = Task->Status.Load();
			return FMCPToolResult::Error(
				FString::Printf(TEXT("Task is still %s - use status to poll"),
					*FMCPAsyncTask::StatusToString(Status)));
		}

		TSharedPtr<FJsonObject> ResultData = Task->ToJson(true);
		FMCPToolResult Result;
		Result.bSuccess = Task->Result.bSuccess;
		Result.Message = Task->Result.Message;
		Result.Data = ResultData;
		return Result;
	}

	FMCPToolResult HandleList(const TSharedRef<FJsonObject>& Params)
	{
		bool bIncludeCompleted = ExtractOptionalBool(Params, TEXT("include_completed"), true);
		int32 Limit = FMath::Clamp(ExtractOptionalNumber<int32>(Params, TEXT("limit"), 50), 1, 500);

		TArray<TSharedPtr<FMCPAsyncTask>> AllTasks = TaskQueue->GetAllTasks(bIncludeCompleted);

		int32 Pending, Running, Completed;
		TaskQueue->GetStats(Pending, Running, Completed);

		TArray<TSharedPtr<FJsonValue>> TaskArray;
		int32 Count = 0;
		for (const TSharedPtr<FMCPAsyncTask>& Task : AllTasks)
		{
			if (Count >= Limit) break;
			TaskArray.Add(MakeShared<FJsonValueObject>(Task->ToJson(false)));
			Count++;
		}

		TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
		ResultData->SetArrayField(TEXT("tasks"), TaskArray);
		ResultData->SetNumberField(TEXT("count"), TaskArray.Num());
		ResultData->SetNumberField(TEXT("total_pending"), Pending);
		ResultData->SetNumberField(TEXT("total_running"), Running);
		ResultData->SetNumberField(TEXT("total_completed"), Completed);

		return FMCPToolResult::Success(
			FString::Printf(TEXT("Found %d tasks (pending: %d, running: %d, completed: %d)"),
				TaskArray.Num(), Pending, Running, Completed), ResultData);
	}

	FMCPToolResult HandleCancel(const TSharedRef<FJsonObject>& Params)
	{
		FMCPToolResult Error;
		FGuid TaskId = ParseTaskId(Params, Error);
		if (!TaskId.IsValid()) return Error;

		if (TaskQueue->CancelTask(TaskId))
		{
			TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
			ResultData->SetStringField(TEXT("task_id"), TaskId.ToString());
			ResultData->SetBoolField(TEXT("cancelled"), true);
			return FMCPToolResult::Success(
				FString::Printf(TEXT("Cancellation requested for task %s"), *TaskId.ToString()), ResultData);
		}

		TSharedPtr<FMCPAsyncTask> Task = TaskQueue->GetTask(TaskId);
		if (!Task.IsValid())
		{
			return FMCPToolResult::Error(FString::Printf(TEXT("Task not found: %s"), *TaskId.ToString()));
		}
		return FMCPToolResult::Error(
			FString::Printf(TEXT("Cannot cancel task (status: %s)"),
				*FMCPAsyncTask::StatusToString(Task->Status.Load())));
	}

	TSharedPtr<FMCPTaskQueue> TaskQueue;
};
