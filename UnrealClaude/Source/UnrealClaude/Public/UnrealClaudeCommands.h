// Copyright Natali Caggiano. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "Styling/AppStyle.h"

/**
 * Editor commands for the Unreal Claude plugin
 */
class FUnrealClaudeCommands : public TCommands<FUnrealClaudeCommands>
{
public:
	FUnrealClaudeCommands()
		: TCommands<FUnrealClaudeCommands>(
			TEXT("UnrealClaude"),
			NSLOCTEXT("Contexts", "UnrealClaude", "Unreal Claude"),
			NAME_None,
			FAppStyle::GetAppStyleSetName())
	{
	}

	// TCommands interface
	virtual void RegisterCommands() override;

public:
	/** Open Claude CLI in a new terminal window */
	TSharedPtr<FUICommandInfo> OpenClaudePanel;
};
