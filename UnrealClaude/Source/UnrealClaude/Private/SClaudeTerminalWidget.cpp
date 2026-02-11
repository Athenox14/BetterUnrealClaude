// Copyright Natali Caggiano. All Rights Reserved.

#include "SClaudeTerminalWidget.h"
#include "ClaudeTerminalServer.h"
#include "ClaudeCodeRunner.h"
#include "UnrealClaudeModule.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SSeparator.h"
#include "SWebBrowser.h"
#include "Misc/Paths.h"

#define LOCTEXT_NAMESPACE "SClaudeTerminalWidget"

void SClaudeTerminalWidget::Construct(const FArguments& InArgs)
{
	// Pick a port for the WebSocket server (offset from MCP port to avoid conflicts)
	TerminalPort = FUnrealClaudeModule::GetMCPServerPort() + 1;

	// Start the terminal server
	TerminalServer = MakeShared<FClaudeTerminalServer>();
	if (!TerminalServer->Start(TerminalPort))
	{
		UE_LOG(LogUnrealClaude, Error, TEXT("Failed to start terminal server on port %d"), TerminalPort);
	}

	FString TerminalURL = GetTerminalURL();
	UE_LOG(LogUnrealClaude, Log, TEXT("Terminal URL: %s"), *TerminalURL);

	ChildSlot
	[
		SNew(SVerticalBox)

		// Toolbar
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			BuildToolbar()
		]

		// Separator
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SSeparator)
		]

		// Web browser (xterm.js terminal)
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			SAssignNew(WebBrowserWidget, SWebBrowser)
			.InitialURL(TerminalURL)
			.ShowControls(false)
			.ShowAddressBar(false)
			.ShowErrorMessage(true)
			.SupportsTransparency(false)
		]
	];
}

SClaudeTerminalWidget::~SClaudeTerminalWidget()
{
	if (TerminalServer.IsValid())
	{
		TerminalServer->Stop();
		TerminalServer.Reset();
	}
}

TSharedRef<SWidget> SClaudeTerminalWidget::BuildToolbar()
{
	return SNew(SHorizontalBox)

	// Title
	+ SHorizontalBox::Slot()
	.AutoWidth()
	.VAlign(VAlign_Center)
	.Padding(8, 4)
	[
		SNew(STextBlock)
		.Text(LOCTEXT("TerminalTitle", "Claude Terminal"))
		.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
	]

	// Spacer
	+ SHorizontalBox::Slot()
	.FillWidth(1.0f)
	[
		SNullWidget::NullWidget
	]

	// New Session button
	+ SHorizontalBox::Slot()
	.AutoWidth()
	.VAlign(VAlign_Center)
	.Padding(2, 2)
	[
		SNew(SButton)
		.Text(LOCTEXT("NewSession", "New Session"))
		.ToolTipText(LOCTEXT("NewSessionTooltip", "Kill current CLI and start a fresh Claude session"))
		.OnClicked_Lambda([this]()
		{
			NewSession();
			return FReply::Handled();
		})
	]

	// Restart button
	+ SHorizontalBox::Slot()
	.AutoWidth()
	.VAlign(VAlign_Center)
	.Padding(2, 2, 8, 2)
	[
		SNew(SButton)
		.Text(LOCTEXT("Restart", "Restart"))
		.ToolTipText(LOCTEXT("RestartTooltip", "Restart the CLI process and reconnect"))
		.OnClicked_Lambda([this]()
		{
			if (TerminalServer.IsValid())
			{
				TerminalServer->RestartCLI();
			}
			return FReply::Handled();
		})
	];
}

void SClaudeTerminalWidget::NewSession()
{
	if (TerminalServer.IsValid())
	{
		TerminalServer->RestartCLI();
	}

	// Reload the web page to clear xterm history
	if (WebBrowserWidget.IsValid())
	{
		WebBrowserWidget->LoadURL(GetTerminalURL());
	}
}

FString SClaudeTerminalWidget::GetTerminalURL() const
{
	// Build path to the terminal HTML file bundled with the plugin
	FString HTMLPath = FPaths::Combine(
		FPaths::ProjectPluginsDir(),
		TEXT("UnrealClaude"),
		TEXT("Resources"),
		TEXT("terminal"),
		TEXT("index.html")
	);

	// Normalize and convert to absolute
	FPaths::NormalizeFilename(HTMLPath);
	HTMLPath = FPaths::ConvertRelativePathToFull(HTMLPath);

	// SWebBrowser needs a file:// URL
	// On Windows, paths are like C:/foo/bar - need file:///C:/foo/bar
	HTMLPath.ReplaceInline(TEXT("\\"), TEXT("/"));

	return FString::Printf(TEXT("file:///%s?port=%d"), *HTMLPath, TerminalPort);
}

#undef LOCTEXT_NAMESPACE
