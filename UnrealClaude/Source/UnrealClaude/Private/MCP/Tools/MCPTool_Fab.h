// Copyright Natali Caggiano. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "../MCPToolBase.h"

class FMCPTool_Fab : public FMCPToolBase
{
public:
	virtual FMCPToolInfo GetInfo() const override;
	virtual FMCPToolResult Execute(const TSharedRef<FJsonObject>& Params) override;

private:
	FMCPToolResult ExecuteSearch(const TSharedRef<FJsonObject>& Params);
	FMCPToolResult ExecuteAddToProject(const TSharedRef<FJsonObject>& Params);

	// Synchronous HTTP GET — pumps HttpManager while waiting (safe on game thread)
	static bool SyncHttpGet(const FString& URL, const TMap<FString, FString>& Headers,
	                        FString& OutBody, int32 TimeoutSecs = 10);
	static bool SyncHttpGetWithCode(const FString& URL, const TMap<FString, FString>& Headers,
	                                FString& OutBody, int32& OutCode, int32 TimeoutSecs = 10);

	// Get UFabBrowserApi instance via UObject reflection (avoids Private header)
	static UObject* GetFabBrowserApi();

	// Get OAuth token stored in Fab plugin
	static FString GetFabAuthToken();

	// Fetch signed download URL from Fab API for a given listing UID
	static bool FetchDownloadUrl(const FString& ListingUid, const FString& AuthToken,
	                             FString& OutDownloadUrl, FString& OutError);
};
