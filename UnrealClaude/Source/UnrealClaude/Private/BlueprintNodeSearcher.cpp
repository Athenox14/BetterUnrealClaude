// Copyright Natali Caggiano. All Rights Reserved.

#include "BlueprintNodeSearcher.h"
#include "UnrealClaudeModule.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "UObject/UObjectIterator.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetStringLibrary.h"
#include "Kismet/KismetArrayLibrary.h"
#include "Kismet/KismetTextLibrary.h"
#include "Blueprint/AIBlueprintHelperLibrary.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Engine/Blueprint.h"
#include "AssetRegistry/AssetRegistryModule.h"

TMap<FString, TArray<FNodeSearchResult>> FBlueprintNodeSearcher::CachedLibraryFunctions;
bool FBlueprintNodeSearcher::bCacheValid = false;

TArray<FCommonLibraryClasses::FLibraryEntry> FCommonLibraryClasses::GetAll()
{
	static const FLibraryEntry Libraries[] = {
		{ TEXT("KismetSystemLibrary"),      []() -> UClass* { return UKismetSystemLibrary::StaticClass(); } },
		{ TEXT("KismetMathLibrary"),         []() -> UClass* { return UKismetMathLibrary::StaticClass(); } },
		{ TEXT("GameplayStatics"),           []() -> UClass* { return UGameplayStatics::StaticClass(); } },
		{ TEXT("KismetStringLibrary"),       []() -> UClass* { return UKismetStringLibrary::StaticClass(); } },
		{ TEXT("KismetArrayLibrary"),        []() -> UClass* { return UKismetArrayLibrary::StaticClass(); } },
		{ TEXT("KismetTextLibrary"),         []() -> UClass* { return UKismetTextLibrary::StaticClass(); } },
		{ TEXT("AIBlueprintHelperLibrary"),  []() -> UClass* { return UAIBlueprintHelperLibrary::StaticClass(); } },
	};

	return TArray<FLibraryEntry>(Libraries, UE_ARRAY_COUNT(Libraries));
}

TSharedPtr<FJsonObject> FNodePinInfo::ToJson() const
{
	TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("name"), Name);
	Json->SetStringField(TEXT("direction"), Direction);
	Json->SetStringField(TEXT("type"), Type);
	if (!DefaultValue.IsEmpty())
	{
		Json->SetStringField(TEXT("default_value"), DefaultValue);
	}
	if (!SubCategory.IsEmpty())
	{
		Json->SetStringField(TEXT("sub_category"), SubCategory);
	}
	Json->SetBoolField(TEXT("is_exec"), bIsExec);
	return Json;
}

TSharedPtr<FJsonObject> FNodeSearchResult::ToJson() const
{
	TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("display_name"), DisplayName);
	Json->SetStringField(TEXT("class_name"), ClassName);
	Json->SetStringField(TEXT("function_name"), FunctionName);
	Json->SetStringField(TEXT("category"), Category);
	Json->SetStringField(TEXT("node_type"), NodeType);
	Json->SetStringField(TEXT("module"), Module);
	Json->SetStringField(TEXT("full_reference"), FullReference);
	Json->SetBoolField(TEXT("is_pure"), bIsPure);

	TArray<TSharedPtr<FJsonValue>> PinsArray;
	for (const FNodePinInfo& Pin : Pins)
	{
		PinsArray.Add(MakeShared<FJsonValueObject>(Pin.ToJson()));
	}
	Json->SetArrayField(TEXT("pins"), PinsArray);

	return Json;
}

TArray<FNodeSearchResult> FBlueprintNodeSearcher::SearchNodes(
	const FString& Keyword,
	const FString& CategoryFilter,
	bool bIncludeK2Nodes,
	bool bIncludeProjectFunctions,
	int32 MaxResults)
{
	TArray<FNodeSearchResult> Results;

	if (Keyword.IsEmpty())
	{
		return Results;
	}

	MaxResults = FMath::Clamp(MaxResults, 1, 500);

	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* TestClass = *It;

		if (!TestClass || TestClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated))
		{
			continue;
		}

		bool bIsFunctionLibrary = TestClass->IsChildOf(UBlueprintFunctionLibrary::StaticClass());

		if (!bIsFunctionLibrary && !bIncludeK2Nodes)
		{
			bool bHasBPFunctions = false;
			for (TFieldIterator<UFunction> FuncIt(TestClass, EFieldIteratorFlags::ExcludeSuper); FuncIt; ++FuncIt)
			{
				if (IsBlueprintCallable(*FuncIt))
				{
					bHasBPFunctions = true;
					break;
				}
			}
			if (!bHasBPFunctions)
			{
				continue;
			}
		}

		FString ClassPath = TestClass->GetPathName();
		if (!bIncludeProjectFunctions && ClassPath.StartsWith(TEXT("/Game/")))
		{
			continue;
		}

		for (TFieldIterator<UFunction> FuncIt(TestClass, EFieldIteratorFlags::ExcludeSuper); FuncIt; ++FuncIt)
		{
			UFunction* Function = *FuncIt;
			if (!Function || !IsBlueprintCallable(Function))
			{
				continue;
			}

			FString FuncName = Function->GetName();
			FString DisplayName = FuncName;

			if (DisplayName.StartsWith(TEXT("K2_")))
			{
				DisplayName = DisplayName.Mid(3);
			}

			FString FuncCategory;
			if (Function->HasMetaData(TEXT("Category")))
			{
				FuncCategory = Function->GetMetaData(TEXT("Category"));
			}

			bool bMatch = DisplayName.Contains(Keyword, ESearchCase::IgnoreCase)
				|| FuncName.Contains(Keyword, ESearchCase::IgnoreCase)
				|| FuncCategory.Contains(Keyword, ESearchCase::IgnoreCase)
				|| TestClass->GetName().Contains(Keyword, ESearchCase::IgnoreCase);

			if (!bMatch)
			{
				continue;
			}

			if (!CategoryFilter.IsEmpty() && !FuncCategory.Contains(CategoryFilter, ESearchCase::IgnoreCase))
			{
				continue;
			}

			FNodeSearchResult Result = BuildResultFromFunction(Function, TestClass);
			Results.Add(Result);

			if (Results.Num() >= MaxResults)
			{
				return Results;
			}
		}
	}

	return Results;
}

TOptional<FNodeSearchResult> FBlueprintNodeSearcher::GetNodePinLayout(const FString& FunctionReference)
{
	FString ClassName, FunctionName;
	if (!FunctionReference.Split(TEXT("::"), &ClassName, &FunctionName))
	{
		FunctionName = FunctionReference;
		ClassName = FString();
	}

	if (FunctionName.IsEmpty())
	{
		return TOptional<FNodeSearchResult>();
	}

	auto TryFindFunction = [&FunctionName](UClass* InClass) -> UFunction*
	{
		if (!InClass) return nullptr;
		UFunction* Found = InClass->FindFunctionByName(FName(*FunctionName));
		if (!Found)
		{
			FString K2Name = FString::Printf(TEXT("K2_%s"), *FunctionName);
			Found = InClass->FindFunctionByName(FName(*K2Name));
		}
		return Found;
	};

	if (!ClassName.IsEmpty())
	{
		UClass* FoundClass = ResolveClass(ClassName);
		if (FoundClass)
		{
			UFunction* Function = TryFindFunction(FoundClass);
			if (Function)
			{
				return BuildResultFromFunction(Function, FoundClass);
			}
		}
		return TOptional<FNodeSearchResult>();
	}

	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* TestClass = *It;
		if (TestClass && TestClass->IsChildOf(UBlueprintFunctionLibrary::StaticClass())
			&& TestClass != UBlueprintFunctionLibrary::StaticClass())
		{
			UFunction* Function = TryFindFunction(TestClass);
			if (Function)
			{
				return BuildResultFromFunction(Function, TestClass);
			}
		}
	}

	return TOptional<FNodeSearchResult>();
}

TArray<TSharedPtr<FJsonObject>> FBlueprintNodeSearcher::ListFunctionLibraries()
{
	TArray<TSharedPtr<FJsonObject>> Results;

	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* TestClass = *It;
		if (!TestClass || !TestClass->IsChildOf(UBlueprintFunctionLibrary::StaticClass())
			|| TestClass == UBlueprintFunctionLibrary::StaticClass()
			|| TestClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated))
		{
			continue;
		}

		int32 FunctionCount = 0;
		for (TFieldIterator<UFunction> FuncIt(TestClass, EFieldIteratorFlags::ExcludeSuper); FuncIt; ++FuncIt)
		{
			if (IsBlueprintCallable(*FuncIt))
			{
				FunctionCount++;
			}
		}

		if (FunctionCount == 0)
		{
			continue;
		}

		TSharedPtr<FJsonObject> LibJson = MakeShared<FJsonObject>();
		LibJson->SetStringField(TEXT("class_name"), TestClass->GetName());
		LibJson->SetStringField(TEXT("path"), TestClass->GetPathName());
		LibJson->SetNumberField(TEXT("function_count"), FunctionCount);

		FString ModulePath = TestClass->GetPathName();
		if (ModulePath.StartsWith(TEXT("/Script/")))
		{
			FString Module;
			ModulePath.RemoveFromStart(TEXT("/Script/"));
			int32 DotIndex;
			if (ModulePath.FindChar(TEXT('.'), DotIndex))
			{
				Module = ModulePath.Left(DotIndex);
			}
			else
			{
				Module = ModulePath;
			}
			LibJson->SetStringField(TEXT("module"), Module);
		}

		Results.Add(LibJson);
	}

	Results.Sort([](const TSharedPtr<FJsonObject>& A, const TSharedPtr<FJsonObject>& B)
	{
		return A->GetStringField(TEXT("class_name")) < B->GetStringField(TEXT("class_name"));
	});

	return Results;
}

TArray<FNodeSearchResult> FBlueprintNodeSearcher::ListLibraryFunctions(const FString& ClassName)
{
	if (bCacheValid)
	{
		if (TArray<FNodeSearchResult>* Cached = CachedLibraryFunctions.Find(ClassName))
		{
			return *Cached;
		}
	}

	TArray<FNodeSearchResult> Results;

	UClass* FoundClass = ResolveClass(ClassName);
	if (!FoundClass)
	{
		return Results;
	}

	for (TFieldIterator<UFunction> FuncIt(FoundClass, EFieldIteratorFlags::ExcludeSuper); FuncIt; ++FuncIt)
	{
		UFunction* Function = *FuncIt;
		if (!Function || !IsBlueprintCallable(Function))
		{
			continue;
		}

		Results.Add(BuildResultFromFunction(Function, FoundClass));
	}

	Results.Sort([](const FNodeSearchResult& A, const FNodeSearchResult& B)
	{
		return A.FunctionName < B.FunctionName;
	});

	CachedLibraryFunctions.Add(ClassName, Results);

	return Results;
}

void FBlueprintNodeSearcher::InvalidateCache()
{
	bCacheValid = false;
	CachedLibraryFunctions.Empty();
}

TArray<FNodePinInfo> FBlueprintNodeSearcher::ExtractFunctionPins(UFunction* Function)
{
	TArray<FNodePinInfo> Pins;
	if (!Function)
	{
		return Pins;
	}

	bool bHasExecPins = !Function->HasAnyFunctionFlags(FUNC_BlueprintPure);

	if (bHasExecPins)
	{
		FNodePinInfo ExecIn;
		ExecIn.Name = TEXT("execute");
		ExecIn.Direction = TEXT("Input");
		ExecIn.Type = TEXT("exec");
		ExecIn.bIsExec = true;
		Pins.Add(ExecIn);

		FNodePinInfo ExecOut;
		ExecOut.Name = TEXT("then");
		ExecOut.Direction = TEXT("Output");
		ExecOut.Type = TEXT("exec");
		ExecOut.bIsExec = true;
		Pins.Add(ExecOut);
	}

	for (TFieldIterator<FProperty> PropIt(Function); PropIt; ++PropIt)
	{
		FProperty* Prop = *PropIt;
		if (!Prop)
		{
			continue;
		}

		FNodePinInfo Pin;
		Pin.Name = Prop->GetName();
		Pin.Type = PropertyToPinType(Prop);
		Pin.SubCategory = PropertyToSubCategory(Prop);
		Pin.bIsExec = false;

		if (Prop->HasAnyPropertyFlags(CPF_ReturnParm))
		{
			Pin.Name = TEXT("ReturnValue");
			Pin.Direction = TEXT("Output");
		}
		else if (Prop->HasAnyPropertyFlags(CPF_OutParm))
		{
			Pin.Direction = TEXT("Output");
			if (!Prop->HasAnyPropertyFlags(CPF_ConstParm) && Prop->HasAnyPropertyFlags(CPF_ReferenceParm))
			{
				Pin.Direction = TEXT("Output");
			}
		}
		else
		{
			Pin.Direction = TEXT("Input");
		}

		FString MetaDefaultKey = FString::Printf(TEXT("CPP_Default_%s"), *Prop->GetName());
		if (Function->HasMetaData(FName(*MetaDefaultKey)))
		{
			Pin.DefaultValue = Function->GetMetaData(FName(*MetaDefaultKey));
		}

		if (Pin.Name == TEXT("WorldContextObject"))
		{
			continue;
		}

		if (Pin.Name == TEXT("self"))
		{
			continue;
		}

		Pins.Add(Pin);
	}

	return Pins;
}

FString FBlueprintNodeSearcher::PropertyToPinType(FProperty* Property)
{
	if (!Property)
	{
		return TEXT("unknown");
	}

	if (CastField<FBoolProperty>(Property))
	{
		return TEXT("bool");
	}
	if (CastField<FByteProperty>(Property))
	{
		return TEXT("byte");
	}
	if (CastField<FIntProperty>(Property))
	{
		return TEXT("int");
	}
	if (CastField<FInt64Property>(Property))
	{
		return TEXT("int64");
	}
	if (CastField<FFloatProperty>(Property))
	{
		return TEXT("float");
	}
	if (CastField<FDoubleProperty>(Property))
	{
		return TEXT("double");
	}
	if (CastField<FStrProperty>(Property))
	{
		return TEXT("string");
	}
	if (CastField<FNameProperty>(Property))
	{
		return TEXT("name");
	}
	if (CastField<FTextProperty>(Property))
	{
		return TEXT("text");
	}
	if (FEnumProperty* EnumProp = CastField<FEnumProperty>(Property))
	{
		UEnum* Enum = EnumProp->GetEnum();
		return Enum ? Enum->GetName() : TEXT("enum");
	}
	if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
	{
		UScriptStruct* Struct = StructProp->Struct;
		return Struct ? Struct->GetName() : TEXT("struct");
	}
	if (FObjectProperty* ObjProp = CastField<FObjectProperty>(Property))
	{
		UClass* PropClass = ObjProp->PropertyClass;
		return PropClass ? PropClass->GetName() + TEXT("*") : TEXT("object");
	}
	if (FClassProperty* ClassProp = CastField<FClassProperty>(Property))
	{
		UClass* MetaClass = ClassProp->MetaClass;
		return MetaClass ? TEXT("class<") + MetaClass->GetName() + TEXT(">") : TEXT("class");
	}
	if (FSoftObjectProperty* SoftObjProp = CastField<FSoftObjectProperty>(Property))
	{
		UClass* PropClass = SoftObjProp->PropertyClass;
		return PropClass ? TEXT("soft<") + PropClass->GetName() + TEXT(">") : TEXT("soft_object");
	}
	if (FArrayProperty* ArrayProp = CastField<FArrayProperty>(Property))
	{
		FString InnerType = PropertyToPinType(ArrayProp->Inner);
		return TEXT("array<") + InnerType + TEXT(">");
	}
	if (FSetProperty* SetProp = CastField<FSetProperty>(Property))
	{
		FString ElementType = PropertyToPinType(SetProp->ElementProp);
		return TEXT("set<") + ElementType + TEXT(">");
	}
	if (FMapProperty* MapProp = CastField<FMapProperty>(Property))
	{
		FString KeyType = PropertyToPinType(MapProp->KeyProp);
		FString ValueType = PropertyToPinType(MapProp->ValueProp);
		return TEXT("map<") + KeyType + TEXT(",") + ValueType + TEXT(">");
	}
	if (CastField<FDelegateProperty>(Property))
	{
		return TEXT("delegate");
	}
	if (CastField<FMulticastDelegateProperty>(Property))
	{
		return TEXT("multicast_delegate");
	}
	if (CastField<FInterfaceProperty>(Property))
	{
		return TEXT("interface");
	}

	return TEXT("unknown");
}

FString FBlueprintNodeSearcher::PropertyToSubCategory(FProperty* Property)
{
	if (!Property)
	{
		return FString();
	}

	if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
	{
		return StructProp->Struct ? StructProp->Struct->GetPathName() : FString();
	}
	if (FObjectProperty* ObjProp = CastField<FObjectProperty>(Property))
	{
		return ObjProp->PropertyClass ? ObjProp->PropertyClass->GetPathName() : FString();
	}
	if (FEnumProperty* EnumProp = CastField<FEnumProperty>(Property))
	{
		UEnum* Enum = EnumProp->GetEnum();
		return Enum ? Enum->GetPathName() : FString();
	}

	return FString();
}

UClass* FBlueprintNodeSearcher::ResolveClass(const FString& ClassName)
{
	UClass* Found = FindObject<UClass>(nullptr, *ClassName);

	if (!Found)
	{
		FString WithU = FString::Printf(TEXT("U%s"), *ClassName);
		Found = FindObject<UClass>(nullptr, *WithU);
	}

	if (!Found)
	{
		static const TCHAR* ScriptModules[] = {
			TEXT("/Script/Engine"),
			TEXT("/Script/CoreUObject"),
			TEXT("/Script/AIModule"),
			TEXT("/Script/NavigationSystem"),
			TEXT("/Script/GameplayTasks"),
			TEXT("/Script/UMG"),
			TEXT("/Script/EnhancedInput"),
		};

		for (const TCHAR* Module : ScriptModules)
		{
			if (Found) break;

			FString Path = FString::Printf(TEXT("%s.%s"), Module, *ClassName);
			Found = LoadClass<UObject>(nullptr, *Path);

			if (!Found)
			{
				Path = FString::Printf(TEXT("%s.U%s"), Module, *ClassName);
				Found = LoadClass<UObject>(nullptr, *Path);
			}
		}
	}

	if (!Found)
	{
		TArray<FCommonLibraryClasses::FLibraryEntry> KnownClasses = FCommonLibraryClasses::GetAll();
		for (const FCommonLibraryClasses::FLibraryEntry& Known : KnownClasses)
		{
			if (ClassName.Equals(Known.Name, ESearchCase::IgnoreCase))
			{
				Found = Known.GetClass();
				break;
			}
		}
	}

	if (!Found)
	{
		for (TObjectIterator<UClass> It; It; ++It)
		{
			if ((*It)->GetName().Equals(ClassName, ESearchCase::IgnoreCase))
			{
				Found = *It;
				break;
			}
		}
	}

	return Found;
}

FNodeSearchResult FBlueprintNodeSearcher::BuildResultFromFunction(UFunction* Function, UClass* OwnerClass)
{
	FNodeSearchResult Result;

	Result.FunctionName = Function->GetName();
	Result.DisplayName = Result.FunctionName;
	if (Result.DisplayName.StartsWith(TEXT("K2_")))
	{
		Result.DisplayName = Result.DisplayName.Mid(3);
	}

	Result.ClassName = OwnerClass ? OwnerClass->GetName() : TEXT("");
	Result.FullReference = Result.ClassName + TEXT("::") + Result.FunctionName;

	if (Function->HasMetaData(TEXT("Category")))
	{
		Result.Category = Function->GetMetaData(TEXT("Category"));
	}

	Result.bIsPure = Function->HasAnyFunctionFlags(FUNC_BlueprintPure);
	Result.NodeType = Result.bIsPure ? TEXT("PureFunction") : TEXT("Function");

	if (OwnerClass)
	{
		FString ClassPath = OwnerClass->GetPathName();
		if (ClassPath.StartsWith(TEXT("/Script/")))
		{
			FString ModulePath = ClassPath;
			ModulePath.RemoveFromStart(TEXT("/Script/"));
			int32 DotIndex;
			if (ModulePath.FindChar(TEXT('.'), DotIndex))
			{
				Result.Module = ModulePath.Left(DotIndex);
			}
		}
		else if (ClassPath.StartsWith(TEXT("/Game/")))
		{
			Result.Module = TEXT("Project");
		}
	}

	Result.Pins = ExtractFunctionPins(Function);

	return Result;
}

bool FBlueprintNodeSearcher::IsBlueprintCallable(UFunction* Function)
{
	if (!Function)
	{
		return false;
	}

	if (!Function->HasAnyFunctionFlags(FUNC_BlueprintCallable))
	{
		return false;
	}

	if (Function->HasMetaData(TEXT("DeprecatedFunction")))
	{
		return false;
	}

	if (Function->HasMetaData(TEXT("BlueprintInternalUseOnly")))
	{
		return false;
	}

	return true;
}
