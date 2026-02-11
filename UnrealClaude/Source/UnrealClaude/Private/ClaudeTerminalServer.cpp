// Copyright Natali Caggiano. All Rights Reserved.

#include "ClaudeTerminalServer.h"
#include "ClaudeCodeRunner.h"
#include "UnrealClaudeModule.h"
#include "UnrealClaudeConstants.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "Interfaces/IPv4/IPv4Address.h"
#include "Interfaces/IPv4/IPv4Endpoint.h"
#include "Common/TcpSocketBuilder.h"
#include "Misc/Base64.h"
#include "Misc/SecureHash.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#include "Windows/AllowWindowsPlatformTypes.h"
#include <windows.h>
#include <winsock2.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

// WebSocket protocol constants
static const FString WebSocketMagicGUID = TEXT("258EAFA5-E914-47DA-95CA-C5AB0DC85B11");

FClaudeTerminalServer::FClaudeTerminalServer()
	: bIsRunning(false)
	, bShouldStop(false)
	, ServerPort(3001)
	, ServerThread(nullptr)
	, ReaderThread(nullptr)
	, ListenSocket(nullptr)
	, ClientSocket(nullptr)
	, CLIProcessHandle(nullptr)
	, CLIStdOutRead(nullptr)
	, CLIStdInWrite(nullptr)
{
}

FClaudeTerminalServer::~FClaudeTerminalServer()
{
	Stop();
}

bool FClaudeTerminalServer::Start(uint32 Port)
{
	if (bIsRunning)
	{
		return true;
	}

	ServerPort = Port;
	bShouldStop = false;

	// Create listen socket
	ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (!SocketSubsystem)
	{
		UE_LOG(LogUnrealClaude, Error, TEXT("Terminal: Failed to get socket subsystem"));
		return false;
	}

	ListenSocket = FTcpSocketBuilder(TEXT("ClaudeTerminalServer"))
		.AsReusable()
		.BoundToEndpoint(FIPv4Endpoint(FIPv4Address::InternalLoopback, ServerPort))
		.Listening(1)
		.Build();

	if (!ListenSocket)
	{
		UE_LOG(LogUnrealClaude, Error, TEXT("Terminal: Failed to create listen socket on port %d"), ServerPort);
		return false;
	}

	// Launch CLI process
	if (!LaunchCLIProcess())
	{
		UE_LOG(LogUnrealClaude, Error, TEXT("Terminal: Failed to launch Claude CLI"));
		if (ListenSocket)
		{
			ListenSocket->Close();
			ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ListenSocket);
			ListenSocket = nullptr;
		}
		return false;
	}

	// Start server thread
	ServerRunnable = MakeUnique<FServerRunnable>(this);
	ServerThread = FRunnableThread::Create(ServerRunnable.Get(), TEXT("ClaudeTerminalServer"), 0, TPri_Normal);

	// Start reader thread
	ReaderRunnable = MakeUnique<FReaderRunnable>(this);
	ReaderThread = FRunnableThread::Create(ReaderRunnable.Get(), TEXT("ClaudeTerminalReader"), 0, TPri_Normal);

	bIsRunning = true;
	UE_LOG(LogUnrealClaude, Log, TEXT("Terminal: WebSocket server started on port %d"), ServerPort);
	return true;
}

void FClaudeTerminalServer::Stop()
{
	if (!bIsRunning)
	{
		return;
	}

	bShouldStop = true;

	// Kill CLI
	KillCLIProcess();

	// Close client socket
	{
		FScopeLock Lock(&ClientSocketMutex);
		if (ClientSocket)
		{
			ClientSocket->Close();
			ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ClientSocket);
			ClientSocket = nullptr;
		}
	}

	// Close listen socket
	if (ListenSocket)
	{
		ListenSocket->Close();
		ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ListenSocket);
		ListenSocket = nullptr;
	}

	// Wait for threads
	if (ServerThread)
	{
		ServerThread->Kill(true);
		delete ServerThread;
		ServerThread = nullptr;
	}
	if (ReaderThread)
	{
		ReaderThread->Kill(true);
		delete ReaderThread;
		ReaderThread = nullptr;
	}

	ServerRunnable.Reset();
	ReaderRunnable.Reset();

	bIsRunning = false;
	UE_LOG(LogUnrealClaude, Log, TEXT("Terminal: Server stopped"));
}

void FClaudeTerminalServer::RestartCLI()
{
	KillCLIProcess();

	// Send info to client
	{
		FScopeLock Lock(&ClientSocketMutex);
		if (ClientSocket)
		{
			SendWebSocketText(ClientSocket, TEXT("\r\n\x1b[1;33m--- New Session ---\x1b[0m\r\n\r\n"));
		}
	}

	LaunchCLIProcess();
}

bool FClaudeTerminalServer::LaunchCLIProcess()
{
#if PLATFORM_WINDOWS
	FScopeLock Lock(&CLIMutex);

	FString ClaudePath = FClaudeCodeRunner::GetClaudePath();
	if (ClaudePath.IsEmpty())
	{
		UE_LOG(LogUnrealClaude, Error, TEXT("Terminal: Claude CLI not found"));
		return false;
	}

	// Create pipes for stdin/stdout
	SECURITY_ATTRIBUTES sa;
	sa.nLength = sizeof(sa);
	sa.bInheritHandle = TRUE;
	sa.lpSecurityDescriptor = NULL;

	HANDLE hStdOutRead = NULL, hStdOutWrite = NULL;
	HANDLE hStdInRead = NULL, hStdInWrite = NULL;

	if (!CreatePipe(&hStdOutRead, &hStdOutWrite, &sa, 0))
	{
		UE_LOG(LogUnrealClaude, Error, TEXT("Terminal: Failed to create stdout pipe"));
		return false;
	}
	SetHandleInformation(hStdOutRead, HANDLE_FLAG_INHERIT, 0);

	if (!CreatePipe(&hStdInRead, &hStdInWrite, &sa, 0))
	{
		CloseHandle(hStdOutRead);
		CloseHandle(hStdOutWrite);
		UE_LOG(LogUnrealClaude, Error, TEXT("Terminal: Failed to create stdin pipe"));
		return false;
	}
	SetHandleInformation(hStdInWrite, HANDLE_FLAG_INHERIT, 0);

	// Build command - interactive mode (no -p flag)
	FString WorkingDir = FPaths::ProjectDir();

	// Build MCP config path
	FString CommandLine = TEXT("--verbose ");

	// Skip permissions for now
	CommandLine += TEXT("--dangerously-skip-permissions ");

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
				UnrealClaudeConstants::MCPServer::DefaultPort
			);

			FFileHelper::SaveStringToFile(MCPConfigContent, *MCPConfigPath);
			FString EscapedConfigPath = MCPConfigPath.Replace(TEXT("\\"), TEXT("/"));
			CommandLine += FString::Printf(TEXT("--mcp-config \"%s\" "), *EscapedConfigPath);
		}
	}

	// Allow MCP tools
	CommandLine += TEXT("--allowedTools \"mcp__unrealclaude__*\" ");

	FString FullCommand = FString::Printf(TEXT("\"%s\" %s"), *ClaudePath, *CommandLine);

	// Launch process
	STARTUPINFOW si;
	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	si.hStdInput = hStdInRead;
	si.hStdOutput = hStdOutWrite;
	si.hStdError = hStdOutWrite;
	si.dwFlags |= STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
	si.wShowWindow = SW_HIDE;

	PROCESS_INFORMATION pi;
	ZeroMemory(&pi, sizeof(pi));

	BOOL bCreated = CreateProcessW(
		NULL,
		const_cast<LPWSTR>(*FullCommand),
		NULL, NULL, TRUE,
		CREATE_NO_WINDOW,
		NULL,
		*WorkingDir,
		&si, &pi
	);

	if (!bCreated)
	{
		DWORD err = GetLastError();
		UE_LOG(LogUnrealClaude, Error, TEXT("Terminal: CreateProcess failed with error %d"), err);
		CloseHandle(hStdOutRead);
		CloseHandle(hStdOutWrite);
		CloseHandle(hStdInRead);
		CloseHandle(hStdInWrite);
		return false;
	}

	CLIProcessHandle = pi.hProcess;
	CloseHandle(pi.hThread);

	// Close child-side handles
	CloseHandle(hStdOutWrite);
	CloseHandle(hStdInRead);

	CLIStdOutRead = hStdOutRead;
	CLIStdInWrite = hStdInWrite;

	UE_LOG(LogUnrealClaude, Log, TEXT("Terminal: Claude CLI launched"));
	return true;
#else
	return false;
#endif
}

void FClaudeTerminalServer::KillCLIProcess()
{
#if PLATFORM_WINDOWS
	FScopeLock Lock(&CLIMutex);

	if (CLIStdInWrite)
	{
		CloseHandle((HANDLE)CLIStdInWrite);
		CLIStdInWrite = nullptr;
	}
	if (CLIStdOutRead)
	{
		CloseHandle((HANDLE)CLIStdOutRead);
		CLIStdOutRead = nullptr;
	}
	if (CLIProcessHandle)
	{
		TerminateProcess((HANDLE)CLIProcessHandle, 1);
		CloseHandle((HANDLE)CLIProcessHandle);
		CLIProcessHandle = nullptr;
	}
#endif
}

// ========== WebSocket Protocol ==========

bool FClaudeTerminalServer::PerformWebSocketHandshake(FSocket* Socket)
{
	// Read HTTP request
	uint8 Buffer[4096];
	int32 BytesRead = 0;

	// Wait for data with timeout
	bool bHasData = false;
	Socket->Wait(ESocketWaitConditions::WaitForRead, FTimespan::FromSeconds(5.0));
	if (!Socket->Recv(Buffer, sizeof(Buffer) - 1, BytesRead))
	{
		return false;
	}
	Buffer[BytesRead] = 0;

	FString Request = UTF8_TO_TCHAR((const char*)Buffer);

	// Extract Sec-WebSocket-Key
	FString WebSocketKey;
	TArray<FString> Lines;
	Request.ParseIntoArrayLines(Lines);
	for (const FString& Line : Lines)
	{
		if (Line.StartsWith(TEXT("Sec-WebSocket-Key:")))
		{
			WebSocketKey = Line.Mid(18).TrimStartAndEnd();
			break;
		}
	}

	if (WebSocketKey.IsEmpty())
	{
		UE_LOG(LogUnrealClaude, Warning, TEXT("Terminal: No WebSocket key in handshake"));
		return false;
	}

	// Compute accept key: SHA1(key + magic GUID) -> base64
	FString ConcatKey = WebSocketKey + WebSocketMagicGUID;
	FSHAHash Hash;
	FSHA1 SHA1;
	SHA1.Update((const uint8*)TCHAR_TO_UTF8(*ConcatKey), ConcatKey.Len());
	SHA1.Final();
	SHA1.GetHash(Hash.Hash);

	FString AcceptKey = FBase64::Encode(Hash.Hash, 20);

	// Send response
	FString Response = FString::Printf(
		TEXT("HTTP/1.1 101 Switching Protocols\r\n")
		TEXT("Upgrade: websocket\r\n")
		TEXT("Connection: Upgrade\r\n")
		TEXT("Sec-WebSocket-Accept: %s\r\n")
		TEXT("\r\n"),
		*AcceptKey
	);

	FTCHARToUTF8 Utf8Response(*Response);
	int32 BytesSent = 0;
	return Socket->Send((const uint8*)Utf8Response.Get(), Utf8Response.Length(), BytesSent);
}

bool FClaudeTerminalServer::SendWebSocketFrame(FSocket* Socket, const uint8* Data, int32 Length, bool bIsText)
{
	if (!Socket) return false;

	TArray<uint8> Frame;

	// Opcode: 0x01 for text, 0x02 for binary
	Frame.Add(bIsText ? 0x81 : 0x82);

	// Payload length
	if (Length < 126)
	{
		Frame.Add((uint8)Length);
	}
	else if (Length < 65536)
	{
		Frame.Add(126);
		Frame.Add((uint8)((Length >> 8) & 0xFF));
		Frame.Add((uint8)(Length & 0xFF));
	}
	else
	{
		Frame.Add(127);
		for (int i = 7; i >= 0; i--)
		{
			Frame.Add((uint8)((Length >> (i * 8)) & 0xFF));
		}
	}

	// Payload
	Frame.Append(Data, Length);

	int32 BytesSent = 0;
	return Socket->Send(Frame.GetData(), Frame.Num(), BytesSent);
}

bool FClaudeTerminalServer::SendWebSocketText(FSocket* Socket, const FString& Text)
{
	FTCHARToUTF8 Utf8Text(*Text);
	return SendWebSocketFrame(Socket, (const uint8*)Utf8Text.Get(), Utf8Text.Length(), true);
}

bool FClaudeTerminalServer::ParseWebSocketFrame(const uint8* Data, int32 DataLength, TArray<uint8>& OutPayload, bool& bOutIsClose)
{
	bOutIsClose = false;
	OutPayload.Empty();

	if (DataLength < 2) return false;

	uint8 Opcode = Data[0] & 0x0F;
	bool bMasked = (Data[1] & 0x80) != 0;
	uint64 PayloadLength = Data[1] & 0x7F;

	int32 Offset = 2;

	if (PayloadLength == 126)
	{
		if (DataLength < 4) return false;
		PayloadLength = (Data[2] << 8) | Data[3];
		Offset = 4;
	}
	else if (PayloadLength == 127)
	{
		if (DataLength < 10) return false;
		PayloadLength = 0;
		for (int i = 0; i < 8; i++)
		{
			PayloadLength = (PayloadLength << 8) | Data[2 + i];
		}
		Offset = 10;
	}

	uint8 Mask[4] = { 0 };
	if (bMasked)
	{
		if (DataLength < Offset + 4) return false;
		FMemory::Memcpy(Mask, Data + Offset, 4);
		Offset += 4;
	}

	if (DataLength < Offset + (int32)PayloadLength) return false;

	OutPayload.SetNum((int32)PayloadLength);
	for (int32 i = 0; i < (int32)PayloadLength; i++)
	{
		OutPayload[i] = Data[Offset + i] ^ (bMasked ? Mask[i % 4] : 0);
	}

	if (Opcode == 0x08) bOutIsClose = true;

	return true;
}

void FClaudeTerminalServer::WriteToCLI(const FString& Data)
{
#if PLATFORM_WINDOWS
	FScopeLock Lock(&CLIMutex);
	if (CLIStdInWrite)
	{
		FTCHARToUTF8 Utf8Data(*Data);
		DWORD BytesWritten;
		WriteFile((HANDLE)CLIStdInWrite, Utf8Data.Get(), Utf8Data.Length(), &BytesWritten, NULL);
	}
#endif
}

// ========== Thread Implementations ==========

uint32 FClaudeTerminalServer::FServerRunnable::Run()
{
	while (!Owner->bShouldStop)
	{
		if (!Owner->ListenSocket)
		{
			FPlatformProcess::Sleep(0.1f);
			continue;
		}

		// Wait for incoming connection
		bool bHasPendingConnection = false;
		Owner->ListenSocket->WaitForPendingConnection(bHasPendingConnection, FTimespan::FromMilliseconds(500));

		if (!bHasPendingConnection || Owner->bShouldStop)
		{
			continue;
		}

		FSocket* NewSocket = Owner->ListenSocket->Accept(TEXT("ClaudeTerminalClient"));
		if (!NewSocket)
		{
			continue;
		}

		UE_LOG(LogUnrealClaude, Log, TEXT("Terminal: New WebSocket connection"));

		// Perform handshake
		if (!Owner->PerformWebSocketHandshake(NewSocket))
		{
			UE_LOG(LogUnrealClaude, Warning, TEXT("Terminal: WebSocket handshake failed"));
			NewSocket->Close();
			ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(NewSocket);
			continue;
		}

		// Close existing client
		{
			FScopeLock Lock(&Owner->ClientSocketMutex);
			if (Owner->ClientSocket)
			{
				Owner->ClientSocket->Close();
				ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(Owner->ClientSocket);
			}
			Owner->ClientSocket = NewSocket;
		}

		UE_LOG(LogUnrealClaude, Log, TEXT("Terminal: WebSocket connected"));

		// Read loop for this client
		uint8 Buffer[8192];
		while (!Owner->bShouldStop)
		{
			int32 BytesRead = 0;
			NewSocket->Wait(ESocketWaitConditions::WaitForRead, FTimespan::FromMilliseconds(100));

			if (NewSocket->GetConnectionState() != SCS_Connected)
			{
				break;
			}

			uint32 PendingSize = 0;
			if (!NewSocket->HasPendingData(PendingSize) || PendingSize == 0)
			{
				continue;
			}

			if (!NewSocket->Recv(Buffer, sizeof(Buffer), BytesRead) || BytesRead <= 0)
			{
				break;
			}

			// Parse WebSocket frame
			TArray<uint8> Payload;
			bool bIsClose = false;
			if (Owner->ParseWebSocketFrame(Buffer, BytesRead, Payload, bIsClose))
			{
				if (bIsClose)
				{
					UE_LOG(LogUnrealClaude, Log, TEXT("Terminal: Client sent close frame"));
					break;
				}

				// Parse JSON message
				FString PayloadStr = FString(Payload.Num(), UTF8_TO_TCHAR((const char*)Payload.GetData()));

				TSharedPtr<FJsonObject> JsonMsg;
				TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(PayloadStr);
				if (FJsonSerializer::Deserialize(Reader, JsonMsg) && JsonMsg.IsValid())
				{
					FString MsgType;
					if (JsonMsg->TryGetStringField(TEXT("type"), MsgType))
					{
						if (MsgType == TEXT("input"))
						{
							FString InputData;
							if (JsonMsg->TryGetStringField(TEXT("data"), InputData))
							{
								Owner->WriteToCLI(InputData);
							}
						}
						else if (MsgType == TEXT("resize"))
						{
							// Could be used for ConPTY resize in the future
							double Cols = 0, Rows = 0;
							JsonMsg->TryGetNumberField(TEXT("cols"), Cols);
							JsonMsg->TryGetNumberField(TEXT("rows"), Rows);
							UE_LOG(LogUnrealClaude, Verbose, TEXT("Terminal: Resize %dx%d"), (int)Cols, (int)Rows);
						}
					}
				}
			}
		}

		// Client disconnected
		{
			FScopeLock Lock(&Owner->ClientSocketMutex);
			if (Owner->ClientSocket == NewSocket)
			{
				Owner->ClientSocket = nullptr;
			}
		}
		NewSocket->Close();
		ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(NewSocket);

		UE_LOG(LogUnrealClaude, Log, TEXT("Terminal: WebSocket disconnected"));
	}

	return 0;
}

void FClaudeTerminalServer::FServerRunnable::Stop()
{
	Owner->bShouldStop = true;
}

uint32 FClaudeTerminalServer::FReaderRunnable::Run()
{
#if PLATFORM_WINDOWS
	char Buffer[4096];

	while (!Owner->bShouldStop)
	{
		HANDLE hRead = nullptr;
		{
			FScopeLock Lock(&Owner->CLIMutex);
			hRead = (HANDLE)Owner->CLIStdOutRead;
		}

		if (!hRead)
		{
			FPlatformProcess::Sleep(0.1f);
			continue;
		}

		// Check if there's data available (non-blocking)
		DWORD BytesAvailable = 0;
		if (!PeekNamedPipe(hRead, NULL, 0, NULL, &BytesAvailable, NULL))
		{
			// Pipe broken - CLI likely exited
			FPlatformProcess::Sleep(0.5f);

			// Notify client
			{
				FScopeLock Lock(&Owner->ClientSocketMutex);
				if (Owner->ClientSocket)
				{
					Owner->SendWebSocketText(Owner->ClientSocket,
						TEXT("\r\n\x1b[1;31m--- Claude process exited ---\x1b[0m\r\n"));
				}
			}

			// Wait for process to be restarted or server to stop
			while (!Owner->bShouldStop)
			{
				FScopeLock Lock(&Owner->CLIMutex);
				if (Owner->CLIStdOutRead)
				{
					break;
				}
				Lock.Unlock();
				FPlatformProcess::Sleep(0.5f);
			}
			continue;
		}

		if (BytesAvailable == 0)
		{
			FPlatformProcess::Sleep(0.01f);
			continue;
		}

		DWORD BytesRead = 0;
		if (ReadFile(hRead, Buffer, FMath::Min((DWORD)sizeof(Buffer) - 1, BytesAvailable), &BytesRead, NULL) && BytesRead > 0)
		{
			Buffer[BytesRead] = '\0';

			// Forward to WebSocket client
			FScopeLock Lock(&Owner->ClientSocketMutex);
			if (Owner->ClientSocket)
			{
				Owner->SendWebSocketFrame(Owner->ClientSocket, (const uint8*)Buffer, BytesRead, true);
			}
		}
	}
#endif

	return 0;
}

void FClaudeTerminalServer::FReaderRunnable::Stop()
{
	Owner->bShouldStop = true;
}
