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
struct FHeadRuleContext;

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

	/** Max distance from the body to an object's edge to use it */
	UPROPERTY(EditDefaultsOnly, Config, Category="Head|Rules")
	float ObjectInteractRange = 150.0f;

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

	/** Object being held with E, locked when E was pressed */
	UPROPERTY()
	TObjectPtr<AHeadObject> HoldTarget;

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

	/** Controller pressed E */
	void HandleInteractPressed(AHeadPlayerController* PC);

	/** Controller released E */
	void HandleInteractReleased(AHeadPlayerController* PC);

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

	/** Closest usable object in reach of the body, measured to its edge */
	AHeadObject* FindObjectInReach() const;

	bool IsInReach(const AHeadObject* Object, float ExtraRange = 0.0f) const;

	FHeadRuleContext MakeContext(AHeadObject* Self) const;

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
