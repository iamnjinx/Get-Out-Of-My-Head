#pragma once

#include "CoreMinimal.h"
#include "Get_Out_Of_My_HeadPlayerController.h"
#include "HeadPlayerController.generated.h"

class UInputAction;
class UInputMappingContext;
class AHeadGameState;
class AHeadPlayerState;
class AHeadObject;
class AHeadMindRoom;
class USoundBase;
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

	/** Local: the mind room whose screen this machine renders */
	TWeakObjectPtr<AHeadMindRoom> MindRoom;

	/** Local: the audio listener sits in the body's head */
	bool bListenerOnBody = false;

	/** Local: post-round survey on the results screen */
	TSharedPtr<SWidget> SurveyWidget;

	/** Local: the survey was answered or skipped for the current results screen */
	bool bSurveyClosed = false;

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

	/** UI sound for messages to this player only ("조건 부족"...). Only the controller hears it */
	UPROPERTY(EditAnywhere, Category="Head|UI")
	TObjectPtr<USoundBase> UISound;

	UPROPERTY(EditAnywhere, Category="Head|UI")
	float UISoundVolume = 0.3f;

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

	/** Sends the survey answers to the host and closes the survey */
	void SubmitSurvey(const TArray<FString>& Answers);

	/** Closes the survey without answering */
	void CloseSurvey();

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

	UFUNCTION(Server, Reliable)
	void ServerSubmitSurvey(const TArray<FString>& Answers);

	/** Opens the survey on the results screen, closes it when the results end */
	void UpdateSurvey();

	/** Finds the pointed object and moves the highlight to it */
	void UpdateAim();

	/** Renders the mind room screen while not controlling, keeps the listener in the body's head */
	void UpdateMindRoomView();

	void OnInteractPressed();
	void OnInteractReleased();
	void OnHostAction();
	void OnLeave();
	void OnPickUp();
	void OnDropPressed();
	void OnDropReleased();
	void OnTakeOut();
};
