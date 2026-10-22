#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Engine/EngineBaseTypes.h"
#include "HeadGameInstance.generated.h"

class UNetDriver;

/**
 *  Hosting, joining and leaving rooms
 *  Remembers why the player was sent back to the front end (host left, join refused...)
 */
UCLASS()
class UHeadGameInstance : public UGameInstance
{
	GENERATED_BODY()

	/** Message shown on the front end */
	FString FrontEndMessage;

	/** Name shown to the other players, kept in GameUserSettings.ini */
	FString Nickname;

	/** True while leaving on purpose, so the disconnect is not reported as an error */
	bool bLeavingRoom = false;

	FDelegateHandle NetworkFailureHandle;
	FDelegateHandle TravelFailureHandle;

public:

	virtual void Init() override;
	virtual void Shutdown() override;

	/** Opens the room map as a listen server */
	void HostRoom();

	/** Connects to a host by address */
	void JoinRoom(const FString& Address);

	/** Goes back to the front end. When the host leaves, the room closes for everyone */
	void LeaveRoom();

	const FString& GetFrontEndMessage() const { return FrontEndMessage; }

	const FString& GetNickname() const { return Nickname; }

	/** Cleans up and remembers the nickname used when hosting or joining */
	void SetNickname(const FString& InNickname);

protected:

	void HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString);
	void HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorString);

	/** Returns the room map package name */
	static FString GetRoomMap();

	/** "?Name=..." URL option, empty without a nickname */
	FString GetNameOption() const;
};
