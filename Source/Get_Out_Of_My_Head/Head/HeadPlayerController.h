#pragma once

#include "CoreMinimal.h"
#include "Get_Out_Of_My_HeadPlayerController.h"
#include "HeadPlayerController.generated.h"

class UInputAction;
class UInputMappingContext;
class AHeadGameState;
class AHeadPlayerState;
class AHeadObject;
class UHeadBodyComponent;
class SWidget;

/** A line shown at the bottom of the screen for a few seconds */
struct FHeadNotice
{
	FString Text;
	double ExpireTime = 0.0;
};

/**
 *  Player controller for the room
 *  Watches the shared body when not controlling it, sends interact / host actions to the server
 *  and owns the on-screen UI (front end menu in standalone, round HUD otherwise).
 */
UCLASS()
class AHeadPlayerController : public AGet_Out_Of_My_HeadPlayerController
{
	GENERATED_BODY()

	/** E: press a button */
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> InteractAction;

	/** Enter: host starts the round / returns to lobby */
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> HostAction;

	/** F10: leave the room */
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> LeaveAction;

	/** Left mouse: pick up */
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> PickUpAction;

	/** Right mouse: drop, hold 1s to throw */
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> DropAction;

	/** Q: take out the last attachment */
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> TakeOutAction;

	/** Local: object under the crosshair while controlling the body */
	TWeakObjectPtr<AHeadObject> AimTarget;

	/** Mappings for the actions above */
	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> RoomMappingContext;

	/** Front end menu or round HUD */
	TSharedPtr<SWidget> ScreenWidget;

	/** Notices currently on screen */
	TArray<FHeadNotice> Notices;

public:

	AHeadPlayerController();

	/** Seconds a notice stays on screen */
	UPROPERTY(EditAnywhere, Category="Head|UI")
	float NoticeSeconds = 4.0f;

	/** Adds a notice on this machine's screen */
	void AddNotice(const FString& Text);

	/** Returns notices that have not expired yet, oldest first */
	TArray<FString> GetActiveNotices() const;

	/** Shows a notice to this player only */
	UFUNCTION(Client, Reliable)
	void ClientNotice(const FString& Text);

	/** True for the listen server's own player */
	bool IsHost() const;

	/** True when running the front end (not in a room) */
	bool IsFrontEnd() const;

	AHeadGameState* GetHeadGameState() const;

	AHeadPlayerState* GetHeadPlayerState() const;

	/** True while this player controls the shared body */
	bool IsControllingBody() const;

	/** Body component of the shared body, or null */
	UHeadBodyComponent* GetBodyComponent() const;

	/** Key guide shown under the crosshair, only for the controller ("E: 수건 장착") */
	FText GetGuideText() const;

protected:

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void SetupInputComponent() override;
	virtual void PlayerTick(float DeltaTime) override;

	UFUNCTION(Server, Reliable)
	void ServerInteractPressed(AHeadObject* Target);

	UFUNCTION(Server, Reliable)
	void ServerInteractReleased();

	UFUNCTION(Server, Reliable)
	void ServerHostAction();

	UFUNCTION(Server, Reliable)
	void ServerPickUp(AHeadObject* Target);

	UFUNCTION(Server, Reliable)
	void ServerDropPressed();

	UFUNCTION(Server, Reliable)
	void ServerDropReleased();

	UFUNCTION(Server, Reliable)
	void ServerTakeOut(AHeadObject* Target);

	/** Finds the pointed object and moves the highlight to it */
	void UpdateAim();

	void OnInteractPressed();
	void OnInteractReleased();
	void OnHostAction();
	void OnLeave();
	void OnPickUp();
	void OnDropPressed();
	void OnDropReleased();
	void OnTakeOut();
};
