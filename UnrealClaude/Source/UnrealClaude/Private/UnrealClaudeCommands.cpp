// Copyright Natali Caggiano. All Rights Reserved.

#include "UnrealClaudeCommands.h"

#define LOCTEXT_NAMESPACE "UnrealClaude"

void FUnrealClaudeCommands::RegisterCommands()
{
	UI_COMMAND(
		OpenClaudePanel,
		"Claude Terminal",
		"Open Claude CLI in a new terminal window",
		EUserInterfaceActionType::Button,
		FInputChord()
	);
}

#undef LOCTEXT_NAMESPACE
