// Copyright Natali Caggiano. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"

/**
 * Lightweight WebSocket server that bridges xterm.js to a Claude CLI process.
 *
 * Architecture:
 *   [SWebBrowser + xterm.js] <-> [WebSocket on port 3001] <-> [Claude CLI process pipes]
 *
 * The server:
 * - Accepts a single WebSocket connection
 * - Launches the Claude CLI in interactive mode
 * - Forwards CLI stdout to WebSocket (displayed by xterm.js)
 * - Forwards WebSocket input to CLI stdin (typed by user)
 * - Handles resize messages from xterm.js
 */
class UNREALCLAUDE_API FClaudeTerminalServer
{
public:
	FClaudeTerminalServer();
	~FClaudeTerminalServer();

	/** Start the WebSocket server and CLI process */
	bool Start(uint32 Port = 3001);

	/** Stop everything */
	void Stop();

	/** Check if server is running */
	bool IsRunning() const { return bIsRunning; }

	/** Get the port the server is listening on */
	uint32 GetPort() const { return ServerPort; }

	/** Restart the CLI process (for New Session) */
	void RestartCLI();

private:
	// WebSocket server thread
	class FServerRunnable : public FRunnable
	{
	public:
		FServerRunnable(FClaudeTerminalServer* InOwner) : Owner(InOwner) {}
		virtual bool Init() override { return true; }
		virtual uint32 Run() override;
		virtual void Stop() override;

	private:
		FClaudeTerminalServer* Owner;
	};

	// CLI output reader thread
	class FReaderRunnable : public FRunnable
	{
	public:
		FReaderRunnable(FClaudeTerminalServer* InOwner) : Owner(InOwner) {}
		virtual bool Init() override { return true; }
		virtual uint32 Run() override;
		virtual void Stop() override;

	private:
		FClaudeTerminalServer* Owner;
	};

	/** Launch the Claude CLI process with pipes */
	bool LaunchCLIProcess();

	/** Kill the running CLI process */
	void KillCLIProcess();

	/** Perform WebSocket handshake on accepted connection */
	bool PerformWebSocketHandshake(class FSocket* ClientSocket);

	/** Send a WebSocket frame to the client */
	bool SendWebSocketFrame(class FSocket* ClientSocket, const uint8* Data, int32 Length, bool bIsText = true);

	/** Send text as WebSocket frame */
	bool SendWebSocketText(class FSocket* ClientSocket, const FString& Text);

	/** Parse incoming WebSocket frames, returns payload */
	bool ParseWebSocketFrame(const uint8* Data, int32 DataLength, TArray<uint8>& OutPayload, bool& bOutIsClose);

	/** Write data to CLI stdin */
	void WriteToCLI(const FString& Data);

	// State
	TAtomic<bool> bIsRunning;
	TAtomic<bool> bShouldStop;
	uint32 ServerPort;

	// Threads
	FRunnableThread* ServerThread;
	FRunnableThread* ReaderThread;
	TUniquePtr<FServerRunnable> ServerRunnable;
	TUniquePtr<FReaderRunnable> ReaderRunnable;

	// Socket
	class FSocket* ListenSocket;
	class FSocket* ClientSocket;
	FCriticalSection ClientSocketMutex;

	// CLI Process handles (Windows HANDLE stored as void*)
	void* CLIProcessHandle;
	void* CLIStdOutRead;
	void* CLIStdInWrite;

	FCriticalSection CLIMutex;
};
