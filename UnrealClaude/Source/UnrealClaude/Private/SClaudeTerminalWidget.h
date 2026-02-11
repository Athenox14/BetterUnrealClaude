// Copyright Natali Caggiano. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

class FClaudeTerminalServer;

/**
 * Embedded terminal widget that hosts xterm.js via SWebBrowser.
 * Replaces the old SClaudeEditorWidget (custom Slate chat) with a real terminal
 * connected to the Claude CLI via WebSocket bridge.
 */
class UNREALCLAUDE_API SClaudeTerminalWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SClaudeTerminalWidget)
	{}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SClaudeTerminalWidget();

private:
	/** Build the toolbar with New Session / Restart buttons */
	TSharedRef<SWidget> BuildToolbar();

	/** Start a new CLI session (kill + relaunch) */
	void NewSession();

	/** Get the URL for the terminal HTML page */
	FString GetTerminalURL() const;

	/** Terminal server (WebSocket bridge to CLI) */
	TSharedPtr<FClaudeTerminalServer> TerminalServer;

	/** The port used by the WebSocket server */
	uint32 TerminalPort = 3001;

	/** Web browser widget */
	TSharedPtr<class SWebBrowser> WebBrowserWidget;
};
