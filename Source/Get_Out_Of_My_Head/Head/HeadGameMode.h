#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "HeadTypes.h"
#include "HeadGameMode.generated.h"

class AHeadGameState;
class AHeadPlayerState;
class AHeadPlayerController;
class AMissionButton;
class AGet_Out_Of_My_HeadCharacter;
class AHeadObject;
class UHeadRuleSet;
class UHeadBodyComponent;
struct FHeadRuleContext;
struct FHeadAttachSlot;

/**
 *  Server-side round flow (M1) and object interaction (M2)
 *  One shared body is possessed by the next player every turn.
 *  Lobby -> Playing <-> Handover -> Results -> Lobby
 *  In standalone (front end) it does nothing and the controller shows the menu instead.
 */
UCLASS(Config=Game)
class AHeadGameMode : public AGameModeBase
{
	GENERATED_BODY()

protected:

	/** Class of the shared body */
	UPROPERTY(EditDefaultsOnly, Category="Head|Body")
	TSubclassOf<AGet_Out_Of_My_HeadCharacter> BodyClass;

	/** Class of the dummy mission buttons */
	UPROPERTY(EditDefaultsOnly, Category="Head|Mission")
	TSubclassOf<AMissionButton> MissionButtonClass;

	/** Number of dummy mission buttons. Buttons nobody owns are decoys */
	UPROPERTY(EditDefaultsOnly, Config, Category="Head|Mission")
	int32 NumMissionButtons = 4;

	/** Variables, missions and object layout of a round */
	UPROPERTY(EditDefaultsOnly, Config, Category="Head|Rules")
	TSoftObjectPtr<UHeadRuleSet> RuleSetAsset;

	/** Distance from the body ring the buttons are placed on */
	UPROPERTY(EditDefaultsOnly, Config, Category="Head|Mission")
	float MissionButtonRingRadius = 500.0f;

	/** Max distance between the body and a button to press it */
	UPROPERTY(EditDefaultsOnly, Config, Category="Head|Mission")
	float InteractRange = 180.0f;

	/** Seconds per turn */
	UPROPERTY(EditDefaultsOnly, Config, Category="Head|Round")
	float TurnSeconds = 30.0f;

	/** Seconds of the handover between turns, everyone's input is locked */
	UPROPERTY(EditDefaultsOnly, Config, Category="Head|Round")
	float HandoverSeconds = 2.0f;

	/** Round length in seconds, handovers included */
	UPROPERTY(EditDefaultsOnly, Config, Category="Head|Round")
	float RoundSeconds = 1200.0f;

	/** Players needed to start a round */
	UPROPERTY(EditDefaultsOnly, Config, Category="Head|Room")
	int32 MinPlayers = 2;

	/** Max players in a room */
	UPROPERTY(EditDefaultsOnly, Config, Category="Head|Room")
	int32 MaxPlayers = 4;

	/** Spawned body */
	UPROPERTY()
	TObjectPtr<AGet_Out_Of_My_HeadCharacter> Body;

	/** Spawned mission buttons, indexed by button index */
	UPROPERTY()
	TArray<TObjectPtr<AMissionButton>> MissionButtons;

	UPROPERTY()
	TObjectPtr<UHeadRuleSet> RuleSet;

	/** Every object in the house */
	UPROPERTY()
	TArray<TObjectPtr<AHeadObject>> Objects;

	/** Object being held with E (or the host of an attach), locked when E was pressed */
	UPROPERTY()
	TObjectPtr<AHeadObject> HoldTarget;

	/** Item being attached to HoldTarget by a hold, null for a plain use */
	UPROPERTY()
	TObjectPtr<AHeadObject> HoldItem;

	/** Server time the right mouse button went down, for drop vs throw */
	double SecondaryPressTime = 0.0;
	bool bSecondaryPressed = false;

	/** Where the body returns to between rounds */
	FTransform BodyStartTransform;

	/** Index in TurnOrder of the current controller (Playing) or the previous one (Handover) */
	int32 CurrentTurnIndex = INDEX_NONE;

	/** Index in TurnOrder of the player who takes over when the handover ends */
	int32 NextTurnIndex = INDEX_NONE;

	/** True when running as the front end (standalone) */
	bool bFrontEnd = false;

public:

	AHeadGameMode();

	virtual void StartPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** Blocks joining during a round and when the room is full */
	virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;

	/** Players never get their own pawn, they watch the shared body instead */
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;

	/** Host pressed the room action key: start in the lobby, back to lobby on results */
	void HandleHostAction(AHeadPlayerController* PC);

	/** E pressed on the pointed object (may be null). Follows the design sheet's hand / target table */
	void HandleInteractPressed(AHeadPlayerController* PC, AHeadObject* AimTarget);

	/** E released: breaks the hold */
	void HandleInteractReleased(AHeadPlayerController* PC);

	/** Left mouse: pick up the pointed object, swapping with the one in hand */
	void HandlePickUp(AHeadPlayerController* PC, AHeadObject* AimTarget);

	/** Right mouse down / up: drop, or throw when held long enough */
	void HandleSecondaryPressed(AHeadPlayerController* PC);
	void HandleSecondaryReleased(AHeadPlayerController* PC);

	/** Q: take out the last attachment of the pointed object, or of the one in hand */
	void HandleTakeOut(AHeadPlayerController* PC, AHeadObject* AimTarget);

protected:

	AHeadGameState* GetHeadGameState() const;

	/** Current server time */
	double Now() const;

	void SpawnBody();
	void SpawnMissionButtons();
	void SpawnObjects();

	/** Objects, variables and body statuses back to the start of a round */
	void ResetWorld();

	/** Presses the closest mission button in reach. Returns false if none */
	bool TryPressMissionButton(AHeadPlayerController* PC);

	UHeadBodyComponent* GetBodyComponent() const;

	/** True when PC controls the body during a turn and the body is on its feet */
	bool CanAct(const AHeadPlayerController* PC) const;

	/** Returns the client's pointed object if the server agrees it can be pointed at */
	AHeadObject* ValidateTarget(AHeadObject* AimTarget) const;

	/** True when the object is in hand, or still pointable within reach (plus a margin) */
	bool IsHeldOrReachable(const AHeadObject* Object) const;

	FHeadRuleContext MakeContext(AHeadObject* Self, AHeadObject* Item = nullptr) const;

	/** E on an object: conditions, then instant effect or hold */
	void BeginUse(AHeadPlayerController* PC, AHeadObject* Object);

	/** E attaching Item to Host: slot conditions, then instant attach or hold */
	void BeginAttach(AHeadPlayerController* PC, AHeadObject* Host, AHeadObject* Item, const FHeadAttachSlot& Slot);

	void FinishAttach(AHeadObject* Host, AHeadObject* Item, const FHeadAttachSlot& Slot);

	/** Floor in front of the body */
	FVector GetDropLocation() const;

	void DropHeld();
	void ThrowHeld(const AHeadPlayerController* PC);

	/** Advances the hold, checks breaks and completes it */
	void UpdateHold(float DeltaSeconds);

	/** Stops the hold. Reset mode objects lose their progress */
	void CancelHold();

	/** Body fell: break the hold */
	void HandleBodyFell();

	void StartRound();
	void BeginTurn(int32 TurnIndex);
	void BeginHandover();
	void EndRound(EHeadEndReason Reason, AHeadPlayerState* InWinner);
	void ReturnToLobby();

	/** Removes a leaving player from the turn order and keeps the round going */
	void RemoveFromTurnOrder(AHeadPlayerState* Leaving);

	/** Puts the body back at its start and releases it */
	void ResetBody();

	/** Releases the body from whoever controls it */
	void ReleaseBody();

	/** True while a round is running (Playing or Handover) */
	bool IsRoundRunning() const;
};
