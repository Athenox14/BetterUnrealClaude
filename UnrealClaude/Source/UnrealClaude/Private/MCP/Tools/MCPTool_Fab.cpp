// Copyright Natali Caggiano. All Rights Reserved.

#include "MCPTool_Fab.h"
#include "MCP/MCPParamValidator.h"
#include "UnrealClaudeModule.h"

#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "HttpModule.h"
#include "HttpManager.h"
#include "Async/TaskGraphInterfaces.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "GenericPlatform/GenericPlatformProcess.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

// Public Fab header — FFabAssetMetadata is public
#include "Workflows/FabWorkflow.h"

// ────────────────────────────────────────────────────────────────
// Tool info
// ────────────────────────────────────────────────────────────────

FMCPToolInfo FMCPTool_Fab::GetInfo() const
{
	FMCPToolInfo Info;
	Info.Name = TEXT("fab");
	Info.Description = TEXT(
		"Interact with the Fab marketplace (fab.com). "
		"Search assets and add them directly to the current project.\n\n"
		"Operations:\n"
		"- 'search': Search Fab for assets by keyword. Returns uid, title, type, price, seller.\n"
		"- 'add_to_project': Download and import a Fab asset into the project. "
		"  Requires being logged into Fab inside the editor (Window > Fab).\n\n"
		"Typical workflow: search → pick an asset uid → add_to_project."
	);

	Info.Parameters = {
		FMCPToolParameter(TEXT("operation"), TEXT("string"),
			TEXT("'search' or 'add_to_project'"), true),

		// search params
		FMCPToolParameter(TEXT("query"), TEXT("string"),
			TEXT("[search] Search keywords (e.g. 'rock wall tileable')"), false),
		FMCPToolParameter(TEXT("limit"), TEXT("integer"),
			TEXT("[search] Max results (1-50, default 10)"), false),
		FMCPToolParameter(TEXT("category"), TEXT("string"),
			TEXT("[search] Category filter, e.g. '3d-assets', 'blueprints', 'materials'"), false),

		// add_to_project params
		FMCPToolParameter(TEXT("asset_id"), TEXT("string"),
			TEXT("[add_to_project] Asset UID from search results"), false),
		FMCPToolParameter(TEXT("asset_name"), TEXT("string"),
			TEXT("[add_to_project] Asset name from search results"), false),
		FMCPToolParameter(TEXT("asset_type"), TEXT("string"),
			TEXT("[add_to_project] Asset type from search results (e.g. 'StaticMesh', 'Blueprint')"), false),
		FMCPToolParameter(TEXT("asset_namespace"), TEXT("string"),
			TEXT("[add_to_project] Asset namespace from search results"), false),
	};

	Info.Annotations = FMCPToolAnnotations::Modifying();
	return Info;
}

// ────────────────────────────────────────────────────────────────
// Dispatch
// ────────────────────────────────────────────────────────────────

FMCPToolResult FMCPTool_Fab::Execute(const TSharedRef<FJsonObject>& Params)
{
	FString Operation;
	if (!Params->TryGetStringField(TEXT("operation"), Operation))
		return FMCPToolResult::Error(TEXT("'operation' required: 'search' or 'add_to_project'"));

	if (Operation == TEXT("search"))          return ExecuteSearch(Params);
	if (Operation == TEXT("add_to_project"))  return ExecuteAddToProject(Params);

	return FMCPToolResult::Error(FString::Printf(TEXT("Unknown operation '%s'"), *Operation));
}

// ────────────────────────────────────────────────────────────────
// search
// ────────────────────────────────────────────────────────────────

FMCPToolResult FMCPTool_Fab::ExecuteSearch(const TSharedRef<FJsonObject>& Params)
{
	FString Query;
	if (!Params->TryGetStringField(TEXT("query"), Query) || Query.IsEmpty())
		return FMCPToolResult::Error(TEXT("'query' required for search"));

	int32 Limit = 10;
	Params->TryGetNumberField(TEXT("limit"), Limit);
	Limit = FMath::Clamp(Limit, 1, 50);

	FString Category;
	Params->TryGetStringField(TEXT("category"), Category);

	// Build Fab search URL
	FString URL = FString::Printf(
		TEXT("https://www.fab.com/i/listings?search_query=%s&channels=unreal-engine&max_results=%d"),
		*FGenericPlatformHttp::UrlEncode(Query), Limit
	);
	if (!Category.IsEmpty())
		URL += FString::Printf(TEXT("&category=%s"), *FGenericPlatformHttp::UrlEncode(Category));

	TMap<FString, FString> Headers;
	Headers.Add(TEXT("Accept"), TEXT("application/json"));
	Headers.Add(TEXT("User-Agent"), TEXT("UnrealEngine-FabPlugin"));

	// Optionally add auth if user is logged in
	FString AuthToken = GetFabAuthToken();
	if (!AuthToken.IsEmpty())
		Headers.Add(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AuthToken));

	FString Body;
	if (!SyncHttpGet(URL, Headers, Body))
		return FMCPToolResult::Error(TEXT("Fab search request failed. Check internet connection."));

	// Parse response
	TSharedPtr<FJsonObject> JsonObj;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Body);
	if (!FJsonSerializer::Deserialize(Reader, JsonObj) || !JsonObj.IsValid())
		return FMCPToolResult::Error(FString::Printf(TEXT("Failed to parse Fab response: %s"), *Body.Left(200)));

	const TArray<TSharedPtr<FJsonValue>>* Results;
	if (!JsonObj->TryGetArrayField(TEXT("results"), Results))
		return FMCPToolResult::Error(FString::Printf(TEXT("No 'results' in Fab response: %s"), *Body.Left(300)));

	// Build result JSON
	TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> Items;

	for (const TSharedPtr<FJsonValue>& Val : *Results)
	{
		const TSharedPtr<FJsonObject>& Item = Val->AsObject();
		if (!Item.IsValid()) continue;

		TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();

		FString UID;
		Item->TryGetStringField(TEXT("uid"), UID);
		Entry->SetStringField(TEXT("uid"), UID);

		FString Title;
		Item->TryGetStringField(TEXT("title"), Title);
		Entry->SetStringField(TEXT("title"), Title);

		FString AssetType;
		Item->TryGetStringField(TEXT("type"), AssetType);
		Entry->SetStringField(TEXT("type"), AssetType);

		// Seller
		const TSharedPtr<FJsonObject>* Seller;
		if (Item->TryGetObjectField(TEXT("seller"), Seller))
		{
			FString SellerName;
			(*Seller)->TryGetStringField(TEXT("username"), SellerName);
			Entry->SetStringField(TEXT("seller"), SellerName);
		}

		// Price
		const TSharedPtr<FJsonObject>* Pricing;
		if (Item->TryGetObjectField(TEXT("pricing"), Pricing))
		{
			FString Price;
			double Amount = 0.0;
			if ((*Pricing)->TryGetNumberField(TEXT("amount"), Amount))
				Entry->SetStringField(TEXT("price"), Amount == 0.0 ? TEXT("Free") : FString::Printf(TEXT("$%.2f"), Amount));
			else
				Entry->SetStringField(TEXT("price"), TEXT("N/A"));
		}

		// Thumbnail
		const TSharedPtr<FJsonObject>* Thumbnail;
		if (Item->TryGetObjectField(TEXT("thumbnail"), Thumbnail))
		{
			FString ThumbUrl;
			(*Thumbnail)->TryGetStringField(TEXT("url"), ThumbUrl);
			Entry->SetStringField(TEXT("thumbnail_url"), ThumbUrl);
		}

		// Asset namespace (needed for add_to_project)
		FString Ns;
		Item->TryGetStringField(TEXT("asset_namespace"), Ns);
		Entry->SetStringField(TEXT("asset_namespace"), Ns);

		Items.Add(MakeShared<FJsonValueObject>(Entry));
	}

	int32 TotalCount = 0;
	JsonObj->TryGetNumberField(TEXT("count"), TotalCount);
	Out->SetNumberField(TEXT("total"), TotalCount);
	Out->SetNumberField(TEXT("returned"), Items.Num());
	Out->SetArrayField(TEXT("results"), Items);

	FString OutStr;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutStr);
	FJsonSerializer::Serialize(Out.ToSharedRef(), Writer);
	return FMCPToolResult::Success(OutStr);
}

// ────────────────────────────────────────────────────────────────
// add_to_project
// ────────────────────────────────────────────────────────────────

FMCPToolResult FMCPTool_Fab::ExecuteAddToProject(const TSharedRef<FJsonObject>& Params)
{
	FString AssetId, AssetName, AssetType, AssetNamespace;
	if (!Params->TryGetStringField(TEXT("asset_id"), AssetId) || AssetId.IsEmpty())
		return FMCPToolResult::Error(TEXT("'asset_id' required"));
	if (!Params->TryGetStringField(TEXT("asset_name"), AssetName) || AssetName.IsEmpty())
		return FMCPToolResult::Error(TEXT("'asset_name' required"));
	Params->TryGetStringField(TEXT("asset_type"), AssetType);
	Params->TryGetStringField(TEXT("asset_namespace"), AssetNamespace);

	// Find UFabBrowserApi instance
	UObject* FabApi = GetFabBrowserApi();
	if (!FabApi)
		return FMCPToolResult::Error(
			TEXT("Fab plugin not loaded or browser not initialized. "
			     "Open Window > Fab inside the editor first, then retry."));

	// Get auth token
	FString AuthToken = GetFabAuthToken();
	if (AuthToken.IsEmpty())
		return FMCPToolResult::Error(
			TEXT("Not logged into Fab. Open Window > Fab and sign in, then retry."));

	// Fetch signed download URL from Fab API
	FString DownloadUrl;
	FString FetchError;
	if (!FetchDownloadUrl(AssetId, AuthToken, DownloadUrl, FetchError))
		return FMCPToolResult::Error(FString::Printf(
			TEXT("Could not fetch download URL: %s"), *FetchError));

	// Build FFabAssetMetadata (public struct from FabWorkflow.h)
	FFabAssetMetadata Meta;
	Meta.AssetId         = AssetId;
	Meta.AssetName       = AssetName;
	Meta.AssetType       = AssetType;
	Meta.AssetNamespace  = AssetNamespace;

	// Call UFabBrowserApi::AddToProject via UObject reflection (avoids Private header)
	UClass* FabApiClass = FabApi->GetClass();
	UFunction* AddToProjectFn = FabApiClass->FindFunctionByName(TEXT("AddToProject"));
	if (!AddToProjectFn)
		return FMCPToolResult::Error(TEXT("UFabBrowserApi::AddToProject not found via reflection. Fab plugin version mismatch?"));

	// Params struct must match the UFUNCTION signature exactly:
	// void AddToProject(const FString& DownloadUrl, const FFabAssetMetadata& AssetMetadata)
	struct FAddToProjectParams
	{
		FString  DownloadUrl;
		FFabAssetMetadata AssetMetadata;
	};

	FAddToProjectParams CallParams;
	CallParams.DownloadUrl   = DownloadUrl;
	CallParams.AssetMetadata = Meta;

	FabApi->ProcessEvent(AddToProjectFn, &CallParams);

	return FMCPToolResult::Success(FString::Printf(
		TEXT("AddToProject triggered for '%s' (id: %s). Asset will appear in Content Browser when import completes."),
		*AssetName, *AssetId));
}

// ────────────────────────────────────────────────────────────────
// Helpers
// ────────────────────────────────────────────────────────────────

bool FMCPTool_Fab::SyncHttpGet(const FString& URL, const TMap<FString, FString>& Headers,
                                FString& OutBody, int32 TimeoutSecs)
{
	bool bComplete = false;
	bool bSuccess  = false;

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetVerb(TEXT("GET"));
	Request->SetURL(URL);
	Request->SetTimeout(static_cast<float>(TimeoutSecs));
	for (const auto& H : Headers)
		Request->SetHeader(H.Key, H.Value);

	Request->OnProcessRequestComplete().BindLambda(
		[&](FHttpRequestPtr, FHttpResponsePtr Response, bool bWasSuccess)
		{
			bSuccess = bWasSuccess && Response.IsValid() &&
			           EHttpResponseCodes::IsOk(Response->GetResponseCode());
			if (Response.IsValid())
				OutBody = Response->GetContentAsString();
			bComplete = true;
		});

	Request->ProcessRequest();

	// UE HTTP callbacks dispatch via AsyncTask(GameThread).
	// Must pump the game thread task graph — Sleep alone deadlocks.
	const double Deadline = FPlatformTime::Seconds() + TimeoutSecs;
	while (!bComplete && FPlatformTime::Seconds() < Deadline)
	{
		FHttpModule::Get().GetHttpManager().Tick(0.016f);
		FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
		if (!bComplete)
			FPlatformProcess::Sleep(0.016f);
	}

	return bSuccess;
}

UObject* FMCPTool_Fab::GetFabBrowserApi()
{
	UClass* FabApiClass = FindObject<UClass>(nullptr, TEXT("/Script/Fab.FabBrowserApi"));
	if (!FabApiClass)
		return nullptr;

	for (TObjectIterator<UObject> It; It; ++It)
	{
		if (It->GetClass() == FabApiClass && IsValid(*It))
			return *It;
	}
	return nullptr;
}

FString FMCPTool_Fab::GetFabAuthToken()
{
	UObject* FabApi = GetFabBrowserApi();
	if (!FabApi) return FString();

	UClass* FabApiClass = FabApi->GetClass();
	UFunction* GetTokenFn = FabApiClass->FindFunctionByName(TEXT("GetAuthToken"));
	if (!GetTokenFn) return FString();

	struct { FString ReturnValue; } TokenParams;
	FabApi->ProcessEvent(GetTokenFn, &TokenParams);
	return TokenParams.ReturnValue;
}

bool FMCPTool_Fab::FetchDownloadUrl(const FString& ListingUid, const FString& AuthToken,
                                     FString& OutDownloadUrl, FString& OutError)
{
	// Fab's API endpoint to get a signed download URL for a listing
	const FString URL = FString::Printf(
		TEXT("https://www.fab.com/i/listings/%s/download-info"), *ListingUid);

	TMap<FString, FString> Headers;
	Headers.Add(TEXT("Accept"),        TEXT("application/json"));
	Headers.Add(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AuthToken));
	Headers.Add(TEXT("User-Agent"),    TEXT("UnrealEngine-FabPlugin"));

	FString Body;
	if (!SyncHttpGet(URL, Headers, Body))
	{
		OutError = FString::Printf(
			TEXT("download-info request failed for uid '%s'. "
			     "Asset may not be owned/free, or Fab API endpoint changed."), *ListingUid);
		return false;
	}

	TSharedPtr<FJsonObject> JsonObj;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Body);
	if (!FJsonSerializer::Deserialize(Reader, JsonObj) || !JsonObj.IsValid())
	{
		OutError = FString::Printf(TEXT("Unparseable download-info response: %s"), *Body.Left(200));
		return false;
	}

	// Response shape is undocumented; try common field names
	if (!JsonObj->TryGetStringField(TEXT("download_url"), OutDownloadUrl) &&
	    !JsonObj->TryGetStringField(TEXT("url"),          OutDownloadUrl) &&
	    !JsonObj->TryGetStringField(TEXT("signedUrl"),    OutDownloadUrl))
	{
		OutError = FString::Printf(
			TEXT("No download URL field in response. Raw: %s"), *Body.Left(300));
		return false;
	}

	return true;
}
