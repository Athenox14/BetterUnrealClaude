// Copyright Natali Caggiano. All Rights Reserved.

#include "UnrealClaudeModule.h"
#include "UnrealClaudeCommands.h"
#include "ClaudeCodeRunner.h"
#include "ScriptExecutionManager.h"
#include "MCP/UnrealClaudeMCPServer.h"
#include "ProjectContext.h"

#include "LevelEditor.h"
#include "ToolMenus.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#include "Windows/AllowWindowsPlatformTypes.h"
#include <windows.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

DEFINE_LOG_CATEGORY(LogUnrealClaude);

#define LOCTEXT_NAMESPACE "FUnrealClaudeModule"

void FUnrealClaudeModule::StartupModule()
{
	UE_LOG(LogUnrealClaude, Warning, TEXT("=== UnrealClaude BUILD 20260107-1450 THREAD_TESTS_DISABLED ==="));

	// Register commands
	FUnrealClaudeCommands::Register();

	PluginCommands = MakeShareable(new FUICommandList);

	// Map OpenClaudePanel -> launch Claude CLI in a new terminal window
	PluginCommands->MapAction(
		FUnrealClaudeCommands::Get().OpenClaudePanel,
		FExecuteAction::CreateLambda([]()
		{
			FUnrealClaudeModule::LaunchClaudeTerminal();
		}),
		FCanExecuteAction::CreateLambda([]()
		{
			return FClaudeCodeRunner::IsClaudeAvailable();
		})
	);

	// Register menus after engine init
	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FUnrealClaudeModule::RegisterMenus));

	// Bind keyboard shortcuts to the Level Editor
	FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
	LevelEditorModule.GetGlobalLevelEditorActions()->Append(PluginCommands.ToSharedRef());

	// Check Claude availability
	if (FClaudeCodeRunner::IsClaudeAvailable())
	{
		UE_LOG(LogUnrealClaude, Log, TEXT("Claude CLI found at: %s"), *FClaudeCodeRunner::GetClaudePath());
	}
	else
	{
		UE_LOG(LogUnrealClaude, Warning, TEXT("Claude CLI not found. Please install with: npm install -g @anthropic-ai/claude-code"));
	}

	// Start MCP Server
	StartMCPServer();

	// Initialize project context (async, will gather in background)
	FProjectContextManager::Get().RefreshContext();

	// Initialize script execution manager (creates script directories)
	FScriptExecutionManager::Get();
}

void FUnrealClaudeModule::ShutdownModule()
{
	UE_LOG(LogUnrealClaude, Log, TEXT("UnrealClaude module shutting down"));

	// Stop MCP Server
	StopMCPServer();

	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);

	FUnrealClaudeCommands::Unregister();
}

FUnrealClaudeModule& FUnrealClaudeModule::Get()
{
	return FModuleManager::LoadModuleChecked<FUnrealClaudeModule>("UnrealClaude");
}

bool FUnrealClaudeModule::IsAvailable()
{
	return FModuleManager::Get().IsModuleLoaded("UnrealClaude");
}

void FUnrealClaudeModule::RegisterMenus()
{
	// Owner will be used for cleanup in call to UToolMenus::UnregisterOwner
	FToolMenuOwnerScoped OwnerScoped(this);

	// Add to the main menu bar under Tools
	{
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Tools");
		FToolMenuSection& Section = Menu->FindOrAddSection("UnrealClaude");

		Section.AddMenuEntryWithCommandList(
			FUnrealClaudeCommands::Get().OpenClaudePanel,
			PluginCommands,
			LOCTEXT("OpenClaudeMenuItem", "Claude Terminal"),
			LOCTEXT("OpenClaudeMenuItemTooltip", "Open Claude CLI in a new terminal window (Ctrl+Shift+C)"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Help")
		);

	}

	// Add to the toolbar
	{
		UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar.PlayToolBar");
		FToolMenuSection& Section = ToolbarMenu->FindOrAddSection("UnrealClaude");

		Section.AddEntry(FToolMenuEntry::InitToolBarButton(
			FUnrealClaudeCommands::Get().OpenClaudePanel,
			LOCTEXT("ClaudeToolbarButton", "Claude"),
			LOCTEXT("ClaudeToolbarTooltip", "Open Claude Terminal (Ctrl+Shift+C)"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Help")
		));
	}
}

void FUnrealClaudeModule::UnregisterMenus()
{
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);
}

// ========== Launch Claude Terminal ==========

void FUnrealClaudeModule::LaunchClaudeTerminal()
{
#if PLATFORM_WINDOWS
	FString ClaudePath = FClaudeCodeRunner::GetClaudePath();
	if (ClaudePath.IsEmpty())
	{
		UE_LOG(LogUnrealClaude, Error, TEXT("Terminal: Claude CLI not found"));
		return;
	}

	FString Args = TEXT("--verbose --dangerously-skip-permissions ");

	// MCP config
	FString PluginDir;
	FString EnginePluginPath = FPaths::Combine(FPaths::EnginePluginsDir(), TEXT("UnrealClaude"));
	FString ProjectPluginPath = FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("UnrealClaude"));
	if (FPaths::DirectoryExists(EnginePluginPath))
		PluginDir = EnginePluginPath;
	else if (FPaths::DirectoryExists(ProjectPluginPath))
		PluginDir = ProjectPluginPath;

	if (!PluginDir.IsEmpty())
	{
		FString MCPBridgePath = FPaths::ConvertRelativePathToFull(
			FPaths::Combine(PluginDir, TEXT("Resources"), TEXT("mcp-bridge"), TEXT("index.js")));

		if (FPaths::FileExists(MCPBridgePath))
		{
			FString MCPConfigDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("UnrealClaude"));
			IFileManager::Get().MakeDirectory(*MCPConfigDir, true);
			FString MCPConfigPath = FPaths::Combine(MCPConfigDir, TEXT("mcp-config.json"));

			FString MCPConfigContent = FString::Printf(
				TEXT("{\n  \"mcpServers\": {\n    \"unrealclaude\": {\n      \"command\": \"node\",\n      \"args\": [\"%s\"],\n      \"env\": {\n        \"UNREAL_MCP_URL\": \"http://localhost:%d\"\n      }\n    }\n  }\n}"),
				*MCPBridgePath.Replace(TEXT("\\"), TEXT("/")),
				GetMCPServerPort()
			);

			FFileHelper::SaveStringToFile(MCPConfigContent, *MCPConfigPath);
			FString EscapedConfigPath = MCPConfigPath.Replace(TEXT("\\"), TEXT("/"));
			Args += FString::Printf(TEXT("--mcp-config \"%s\" "), *EscapedConfigPath);
		}
	}

	Args += TEXT("--allowedTools \"mcp__unrealclaude__*\" ");

	// cmd.exe /K keeps the prompt open if Claude exits
	FString Command = FString::Printf(TEXT("cmd.exe /K \"\"%s\" %s\""), *ClaudePath, *Args);
	FString WorkingDir = FPaths::ProjectDir();

	STARTUPINFOW si;
	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);

	PROCESS_INFORMATION pi;
	ZeroMemory(&pi, sizeof(pi));

	BOOL bCreated = CreateProcessW(
		NULL,
		const_cast<LPWSTR>(*Command),
		NULL, NULL,
		0,
		CREATE_NEW_CONSOLE,
		NULL,
		*WorkingDir,
		&si, &pi
	);

	if (bCreated)
	{
		UE_LOG(LogUnrealClaude, Log, TEXT("Terminal: Launched Claude CLI (PID: %d)"), pi.dwProcessId);
		CloseHandle(pi.hThread);
		CloseHandle(pi.hProcess);
	}
	else
	{
		DWORD err = GetLastError();
		UE_LOG(LogUnrealClaude, Error, TEXT("Terminal: CreateProcess failed (%d)"), err);
	}
#else
	UE_LOG(LogUnrealClaude, Warning, TEXT("Terminal: Only supported on Windows"));
#endif
}

uint32 FUnrealClaudeModule::GetMCPServerPort()
{
	static uint32 CachedPort = 0;
	if (CachedPort > 0)
	{
		return CachedPort;
	}

	// Try to read port from config.json at the plugin root
	FString ConfigPath = FPaths::Combine(
		FPaths::ProjectPluginsDir(), TEXT("UnrealClaude"), TEXT("config.json"));

	FString JsonString;
	if (FFileHelper::LoadFileToString(JsonString, *ConfigPath))
	{
		TSharedPtr<FJsonObject> JsonObject;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
		if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
		{
			double PortValue = 0;
			if (JsonObject->TryGetNumberField(TEXT("server_port"), PortValue))
			{
				int32 Port = static_cast<int32>(PortValue);
				if (Port > 0 && Port <= 65535)
				{
					CachedPort = static_cast<uint32>(Port);
					UE_LOG(LogUnrealClaude, Log, TEXT("MCP server port %d read from config.json"), CachedPort);
					return CachedPort;
				}
			}
		}
		UE_LOG(LogUnrealClaude, Warning, TEXT("config.json found at '%s' but server_port invalid, using default"), *ConfigPath);
	}

	CachedPort = UnrealClaudeConstants::MCPServer::DefaultPort;
	return CachedPort;
}

void FUnrealClaudeModule::StartMCPServer()
{
	if (MCPServer.IsValid())
	{
		UE_LOG(LogUnrealClaude, Warning, TEXT("MCP Server already exists"));
		return;
	}

	MCPServer = MakeShared<FUnrealClaudeMCPServer>();

	if (!MCPServer->Start(GetMCPServerPort()))
	{
		UE_LOG(LogUnrealClaude, Error, TEXT("Failed to start MCP Server on port %d"), GetMCPServerPort());
		MCPServer.Reset();
	}
}

void FUnrealClaudeModule::StopMCPServer()
{
	if (MCPServer.IsValid())
	{
		MCPServer->Stop();
		MCPServer.Reset();
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FUnrealClaudeModule, UnrealClaude)
