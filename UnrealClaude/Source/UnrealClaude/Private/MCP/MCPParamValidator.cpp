// Copyright Natali Caggiano. All Rights Reserved.

#include "MCPParamValidator.h"
#include "UnrealClaudeConstants.h"

// Use constants from centralized header
using namespace UnrealClaudeConstants::MCPValidation;

const TArray<FString>& FMCPParamValidator::GetBlockedConsoleCommands()
{
	static TArray<FString> BlockedCommands = {
		TEXT("quit"), TEXT("exit"), TEXT("crash"), TEXT("forcegc"),
		TEXT("forcecrash"), TEXT("debug crash"),
		TEXT("mem"), TEXT("memreport"), TEXT("obj"),
		TEXT("exec"), TEXT("savepackage"), TEXT("deletepackage"),
		TEXT("net"), TEXT("admin"),
		TEXT("shutdown"), TEXT("restartlevel"), TEXT("open"), TEXT("servertravel"),
		TEXT("toggledebugcamera"), TEXT("enablecheats"),
		TEXT("stat slow"),
		TEXT("gc."), TEXT("r."),
	};
	return BlockedCommands;
}

// ===== Private helpers =====

/** Shared logic for path-style validators: empty, length, dangerous chars, path traversal */
static bool ValidatePathCommon(const FString& Value, const FString& FieldLabel, int32 MaxLength, bool bCheckPathTraversal, FString& OutError)
{
	if (Value.IsEmpty())
	{
		OutError = FString::Printf(TEXT("%s cannot be empty"), *FieldLabel);
		return false;
	}

	if (Value.Len() > MaxLength)
	{
		OutError = FString::Printf(TEXT("%s exceeds maximum length of %d characters"), *FieldLabel, MaxLength);
		return false;
	}

	// Check for dangerous characters
	int32 FoundIndex;
	for (const TCHAR* c = DangerousChars; *c; ++c)
	{
		if (Value.FindChar(*c, FoundIndex))
		{
			OutError = FString::Printf(TEXT("%s contains invalid character: '%c'"), *FieldLabel, *c);
			return false;
		}
	}

	if (bCheckPathTraversal && Value.Contains(TEXT("..")))
	{
		OutError = FString::Printf(TEXT("%s cannot contain path traversal sequences"), *FieldLabel);
		return false;
	}

	return true;
}

/** Shared logic for identifier validators (variable names, function names, etc.) */
static bool ValidateIdentifier(const FString& Name, const FString& FieldLabel, int32 MaxLength, FString& OutError)
{
	if (Name.IsEmpty())
	{
		OutError = FString::Printf(TEXT("%s cannot be empty"), *FieldLabel);
		return false;
	}

	if (Name.Len() > MaxLength)
	{
		OutError = FString::Printf(TEXT("%s exceeds maximum length of %d characters"), *FieldLabel, MaxLength);
		return false;
	}

	if (!FChar::IsAlpha(Name[0]) && Name[0] != TEXT('_'))
	{
		OutError = FString::Printf(TEXT("%s must start with a letter or underscore"), *FieldLabel);
		return false;
	}

	for (TCHAR C : Name)
	{
		if (!FChar::IsAlnum(C) && C != TEXT('_'))
		{
			OutError = FString::Printf(TEXT("%s contains invalid character: '%c'"), *FieldLabel, C);
			return false;
		}
	}

	return true;
}

// ===== Public API =====

bool FMCPParamValidator::ValidateActorName(const FString& Name, FString& OutError)
{
	if (!ValidatePathCommon(Name, TEXT("Actor name"), MaxActorNameLength, /*bCheckPathTraversal=*/false, OutError))
	{
		return false;
	}

	// Additional: reject control characters
	for (TCHAR c : Name)
	{
		if (c < 32 && c != TEXT('\0'))
		{
			OutError = TEXT("Actor name contains control characters");
			return false;
		}
	}

	return true;
}

bool FMCPParamValidator::ValidatePropertyPath(const FString& PropertyPath, FString& OutError)
{
	if (PropertyPath.IsEmpty())
	{
		OutError = TEXT("Property path cannot be empty");
		return false;
	}

	if (PropertyPath.Len() > MaxPropertyPathLength)
	{
		OutError = FString::Printf(TEXT("Property path exceeds maximum length of %d characters"), MaxPropertyPathLength);
		return false;
	}

	// Property paths: only alphanumeric, underscore, dot
	for (TCHAR c : PropertyPath)
	{
		if (!FChar::IsAlnum(c) && c != TEXT('_') && c != TEXT('.'))
		{
			OutError = FString::Printf(TEXT("Property path contains invalid character: '%c'. Only alphanumeric, underscore, and dot are allowed."), c);
			return false;
		}
	}

	if (PropertyPath.Contains(TEXT("..")))
	{
		OutError = TEXT("Property path cannot contain consecutive dots");
		return false;
	}

	if (PropertyPath.StartsWith(TEXT(".")) || PropertyPath.EndsWith(TEXT(".")))
	{
		OutError = TEXT("Property path cannot start or end with a dot");
		return false;
	}

	return true;
}

bool FMCPParamValidator::ValidateClassPath(const FString& ClassPath, FString& OutError)
{
	return ValidatePathCommon(ClassPath, TEXT("Class path"), MaxClassPathLength, /*bCheckPathTraversal=*/true, OutError);
}

bool FMCPParamValidator::ValidateConsoleCommand(const FString& Command, FString& OutError)
{
	if (Command.IsEmpty())
	{
		OutError = TEXT("Command cannot be empty");
		return false;
	}

	if (Command.Len() > MaxCommandLength)
	{
		OutError = FString::Printf(TEXT("Command exceeds maximum length of %d characters"), MaxCommandLength);
		return false;
	}

	// Check against blocklist
	FString CommandLower = Command.ToLower().TrimStartAndEnd();
	for (const FString& Blocked : GetBlockedConsoleCommands())
	{
		if (CommandLower.StartsWith(Blocked.ToLower()))
		{
			OutError = FString::Printf(TEXT("Command '%s' is blocked for safety"), *Blocked);
			return false;
		}
	}

	// Check for command chaining attempts
	if (Command.Contains(TEXT(";")) || Command.Contains(TEXT("|")) || Command.Contains(TEXT("&&")))
	{
		OutError = TEXT("Command chaining is not allowed");
		return false;
	}

	// Check for shell escape attempts
	if (Command.Contains(TEXT("`")) || Command.Contains(TEXT("$(")) || Command.Contains(TEXT("${")))
	{
		OutError = TEXT("Shell escape sequences are not allowed");
		return false;
	}

	return true;
}

bool FMCPParamValidator::ValidateNumericValue(double Value, const FString& FieldName, FString& OutError, double MaxAbsValue)
{
	if (FMath::IsNaN(Value))
	{
		OutError = FString::Printf(TEXT("%s: NaN is not a valid value"), *FieldName);
		return false;
	}

	if (!FMath::IsFinite(Value))
	{
		OutError = FString::Printf(TEXT("%s: Infinite values are not allowed"), *FieldName);
		return false;
	}

	if (FMath::Abs(Value) > MaxAbsValue)
	{
		OutError = FString::Printf(TEXT("%s: Value %f exceeds maximum allowed magnitude of %f"), *FieldName, Value, MaxAbsValue);
		return false;
	}

	return true;
}

bool FMCPParamValidator::ValidateStringLength(const FString& Value, const FString& FieldName, int32 MaxLength, FString& OutError)
{
	if (Value.Len() > MaxLength)
	{
		OutError = FString::Printf(TEXT("%s: String length %d exceeds maximum of %d"), *FieldName, Value.Len(), MaxLength);
		return false;
	}
	return true;
}

FString FMCPParamValidator::SanitizeString(const FString& Input)
{
	FString Output;
	Output.Reserve(Input.Len());

	for (TCHAR InputChar : Input)
	{
		bool bIsDangerous = false;
		for (const TCHAR* c = DangerousChars; *c; ++c)
		{
			if (InputChar == *c)
			{
				bIsDangerous = true;
				break;
			}
		}

		if (!bIsDangerous && (InputChar >= 32 || InputChar == TEXT('\0')))
		{
			Output.AppendChar(InputChar);
		}
	}

	return Output;
}

bool FMCPParamValidator::ValidateBlueprintPath(const FString& BlueprintPath, FString& OutError)
{
	if (!ValidatePathCommon(BlueprintPath, TEXT("Blueprint path"), 512, /*bCheckPathTraversal=*/true, OutError))
	{
		return false;
	}

	// Additional: block engine Blueprints
	if (BlueprintPath.StartsWith(TEXT("/Engine/")) || BlueprintPath.StartsWith(TEXT("/Script/")))
	{
		OutError = TEXT("Cannot access engine or script Blueprints");
		return false;
	}

	return true;
}

bool FMCPParamValidator::ValidateBlueprintVariableName(const FString& VariableName, FString& OutError)
{
	return ValidateIdentifier(VariableName, TEXT("Variable name"), 128, OutError);
}

bool FMCPParamValidator::ValidateBlueprintFunctionName(const FString& FunctionName, FString& OutError)
{
	return ValidateIdentifier(FunctionName, TEXT("Function name"), 128, OutError);
}
