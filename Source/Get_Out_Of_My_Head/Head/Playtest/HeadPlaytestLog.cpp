#include "HeadPlaytestLog.h"
#include "Get_Out_Of_My_Head.h"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

void HeadPlaytestLog::Append(const TSharedRef<FJsonObject>& Entry)
{
	FString Line;
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Line);
	FJsonSerializer::Serialize(Entry, Writer);
	Line += TEXT("\n");

	const FString Path = GetLogFolder() / FString::Printf(TEXT("Playtest_%s.jsonl"), *FDateTime::Now().ToString(TEXT("%Y-%m-%d")));

	if (!FFileHelper::SaveStringToFile(Line, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append))
	{
		UE_LOG(LogGet_Out_Of_My_Head, Warning, TEXT("Could not write the playtest log %s"), *Path);
	}
}

FString HeadPlaytestLog::GetLogFolder()
{
	return FPaths::ProjectSavedDir() / TEXT("PlaytestLogs");
}

FString HeadPlaytestLog::MakeRoundId()
{
	return FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S"));
}
