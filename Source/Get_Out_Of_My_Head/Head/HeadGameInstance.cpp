#include "HeadGameInstance.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameMapsSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/PackageName.h"

void UHeadGameInstance::Init()
{
	Super::Init();

	GConfig->GetString(TEXT("/Script/Get_Out_Of_My_Head.HeadGameInstance"), TEXT("Nickname"), Nickname, GGameUserSettingsIni);

	NetworkFailureHandle = GEngine->OnNetworkFailure().AddUObject(this, &UHeadGameInstance::HandleNetworkFailure);
	TravelFailureHandle = GEngine->OnTravelFailure().AddUObject(this, &UHeadGameInstance::HandleTravelFailure);
}

void UHeadGameInstance::Shutdown()
{
	GEngine->OnNetworkFailure().Remove(NetworkFailureHandle);
	GEngine->OnTravelFailure().Remove(TravelFailureHandle);

	Super::Shutdown();
}

void UHeadGameInstance::HostRoom()
{
	FrontEndMessage.Empty();
	bLeavingRoom = false;

	UGameplayStatics::OpenLevel(this, FName(*GetRoomMap()), true, TEXT("listen") + GetNameOption());
}

void UHeadGameInstance::JoinRoom(const FString& Address)
{
	FrontEndMessage.Empty();
	bLeavingRoom = false;

	if (APlayerController* PC = GetFirstLocalPlayerController())
	{
		PC->ClientTravel(Address + GetNameOption(), TRAVEL_Absolute);
	}
}

void UHeadGameInstance::LeaveRoom()
{
	FrontEndMessage = TEXT("방에서 나왔습니다.");
	bLeavingRoom = true;

	// opening the map without "listen" closes the room if we are the host
	UGameplayStatics::OpenLevel(this, FName(*GetRoomMap()));
}

void UHeadGameInstance::HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString)
{
	// the delegate is global, PIE runs several game instances
	if (!World || World->GetGameInstance() != this || bLeavingRoom)
	{
		return;
	}

	if (FailureType == ENetworkFailure::PendingConnectionFailure && !ErrorString.IsEmpty())
	{
		// refused by the host (round running, room full)
		FrontEndMessage = ErrorString;
	}
	else
	{
		FrontEndMessage = TEXT("호스트와의 연결이 끊겨 판이 종료되었습니다.");
	}
}

void UHeadGameInstance::HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorString)
{
	if (!World || World->GetGameInstance() != this)
	{
		return;
	}

	FrontEndMessage = FString::Printf(TEXT("방에 들어가지 못했습니다. (%s)"), *ErrorString);
}

void UHeadGameInstance::SetNickname(const FString& InNickname)
{
	// characters that would break the travel URL are dropped
	FString Clean;
	for (const TCHAR Character : InNickname.TrimStartAndEnd())
	{
		if (!FChar::IsWhitespace(Character) && !FCString::Strchr(TEXT("?=#/\\:;\"'"), Character))
		{
			Clean.AppendChar(Character);
		}
	}

	Nickname = Clean.Left(16);

	GConfig->SetString(TEXT("/Script/Get_Out_Of_My_Head.HeadGameInstance"), TEXT("Nickname"), *Nickname, GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
}

FString UHeadGameInstance::GetNameOption() const
{
	return Nickname.IsEmpty() ? FString() : FString::Printf(TEXT("?Name=%s"), *Nickname);
}

FString UHeadGameInstance::GetRoomMap()
{
	return FPackageName::ObjectPathToPackageName(UGameMapsSettings::GetGameDefaultMap());
}
