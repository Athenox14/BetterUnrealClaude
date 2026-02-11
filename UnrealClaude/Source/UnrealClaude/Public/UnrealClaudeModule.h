// Copyright Natali Caggiano. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "UnrealClaudeConstants.h"

DECLARE_LOG_CATEGORY_EXTERN(LogUnrealClaude, Log, All);

class FUnrealClaudeMCPServer;

class FUnrealClaudeModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/** Get the singleton instance */
	static FUnrealClaudeModule& Get();

	/** Check if module is available */
	static bool IsAvailable();

	/** Get the MCP server instance */
	TSharedPtr<FUnrealClaudeMCPServer> GetMCPServer() const { return MCPServer; }

	/** Get MCP server port - reads from config.json, falls back to default constant */
	static uint32 GetMCPServerPort();

	/** Launch Claude CLI in a new cmd.exe window */
	static void LaunchClaudeTerminal();

private:
	void RegisterMenus();
	void UnregisterMenus();
	void StartMCPServer();
	void StopMCPServer();

	TSharedPtr<class FUICommandList> PluginCommands;
	TSharedPtr<FUnrealClaudeMCPServer> MCPServer;
};
