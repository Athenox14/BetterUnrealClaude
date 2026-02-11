// Copyright Natali Caggiano. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "../MCPToolBase.h"

/**
 * MCP Tool for executing scripts with user permission
 *
 * Supports: C++, Python, Console commands, Editor Utility
 *
 * IMPORTANT: Scripts MUST include a header comment with @Description
 * This description is stored in history for context restoration.
 *
 * C++ Header Format:
 * /**
 *  * @UnrealClaude Script
 *  * @Name: MyScript
 *  * @Description: Brief description of what this script does
 *  * @Created: 2026-01-03T10:30:00Z
 *  * /
 *
 * Python Header Format:
 * """
 * @UnrealClaude Script
 * @Name: MyScript
 * @Description: Brief description of what this script does
 * @Created: 2026-01-03T10:30:00Z
 * """
 */
class FMCPTaskQueue;

class FMCPTool_ExecuteScript : public FMCPToolBase
{
public:
	/** Set the task queue for async execution */
	void SetTaskQueue(TSharedPtr<FMCPTaskQueue> InTaskQueue) { TaskQueue = InTaskQueue; }

	virtual FMCPToolInfo GetInfo() const override
	{
		FMCPToolInfo Info;
		Info.Name = TEXT("execute_script");
		Info.Description = TEXT(
			"Execute custom scripts, view history, or cleanup generated files.\n\n"
			"Operations: 'run' (default), 'history', 'cleanup'.\n\n"
			"IMPORTANT for 'run': Only use when no dedicated MCP tool can accomplish the task. "
			"Prefer specific tools first. Scripts require user approval.\n\n"
			"Script types for 'run': 'cpp', 'python', 'console', 'editor_utility'.\n"
			"Include @Description in script header for history tracking.");
		Info.Parameters = {
			FMCPToolParameter(TEXT("operation"), TEXT("string"),
				TEXT("Operation: 'run' (default), 'history', 'cleanup'"), false, TEXT("run")),
			FMCPToolParameter(TEXT("script_type"), TEXT("string"),
				TEXT("Type for 'run': 'cpp', 'python', 'console', 'editor_utility'"), false),
			FMCPToolParameter(TEXT("script_content"), TEXT("string"),
				TEXT("Script code for 'run'. MUST include @Description in header."), false),
			FMCPToolParameter(TEXT("description"), TEXT("string"),
				TEXT("Brief description for 'run' (optional if @Description in header)"), false),
			FMCPToolParameter(TEXT("count"), TEXT("number"),
				TEXT("Number of entries for 'history' (1-50, default: 10)"), false, TEXT("10"))
		};
		Info.Annotations = FMCPToolAnnotations::Modifying();
		Info.Annotations.bDestructiveHint = true; // Scripts can do anything
		return Info;
	}

	virtual FMCPToolResult Execute(const TSharedRef<FJsonObject>& Params) override;

private:
	/** Task queue for async execution */
	TSharedPtr<FMCPTaskQueue> TaskQueue;

	/** Execute script synchronously (called by task queue) */
	FMCPToolResult ExecuteSync(const TSharedRef<FJsonObject>& Params);

	/** Handle history operation */
	FMCPToolResult HandleHistory(const TSharedRef<FJsonObject>& Params);

	/** Handle cleanup operation */
	FMCPToolResult HandleCleanup();
};
