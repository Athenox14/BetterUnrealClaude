// Copyright Natali Caggiano. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "../MCPToolBase.h"

/**
 * MCP Tool: Editor utilities (console commands and output log)
 *
 * Operations:
 *   - console_command: Execute an Unreal Engine console command
 *   - get_log: Retrieve recent entries from the output log
 */
class FMCPTool_Editor : public FMCPToolBase
{
public:
	virtual FMCPToolInfo GetInfo() const override
	{
		FMCPToolInfo Info;
		Info.Name = TEXT("editor");
		Info.Description = TEXT(
			"Editor utilities for console commands and output log access.\n\n"
			"Operations:\n"
			"- 'console_command': Execute console commands (stat fps, show collision, etc.)\n"
			"- 'get_log': Retrieve recent log entries with optional filters\n\n"
			"Useful console commands:\n"
			"- 'stat fps' - Show FPS counter\n"
			"- 'stat unit' - Show frame timing\n"
			"- 'show collision' - Toggle collision visualization\n"
			"- 'show bounds' - Toggle bounding box display\n"
			"- 'r.SetRes 1920x1080' - Set resolution\n"
			"- 'slomo 0.5' - Slow motion (PIE only)\n\n"
			"Common log filters:\n"
			"- 'Error' - Show only errors\n"
			"- 'Warning' - Show warnings\n"
			"- 'LogTemp' - Show UE_LOG(LogTemp, ...) output\n"
			"- 'LogBlueprint' - Blueprint-related messages\n\n"
			"Returns: Command output or log entries."
		);
		Info.Parameters = {
			FMCPToolParameter(TEXT("operation"), TEXT("string"),
				TEXT("Operation: 'console_command' or 'get_log'"), true),
			FMCPToolParameter(TEXT("command"), TEXT("string"),
				TEXT("Console command to execute (required for 'console_command')"), false),
			FMCPToolParameter(TEXT("lines"), TEXT("number"),
				TEXT("Number of log lines to return for 'get_log' (default: 100, max: 1000)"), false, TEXT("100")),
			FMCPToolParameter(TEXT("filter"), TEXT("string"),
				TEXT("Optional category or text filter for 'get_log' (e.g., 'Warning', 'Error', 'LogTemp')"), false)
		};
		Info.Annotations = FMCPToolAnnotations::Modifying();
		return Info;
	}

	virtual FMCPToolResult Execute(const TSharedRef<FJsonObject>& Params) override;

private:
	FMCPToolResult ExecuteConsoleCommand(const TSharedRef<FJsonObject>& Params);
	FMCPToolResult ExecuteGetLog(const TSharedRef<FJsonObject>& Params);
};
