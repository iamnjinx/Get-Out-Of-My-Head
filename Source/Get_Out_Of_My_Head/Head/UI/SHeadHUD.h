#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class AHeadPlayerController;

/**
 *  M1 round HUD, built in Slate so it needs no widget assets
 *  Public info for everyone, the player's own mission, handover and results screens.
 *  Every text is rebuilt from the replicated round state each frame.
 */
class SHeadHUD : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SHeadHUD) {}
		SLATE_ARGUMENT(TWeakObjectPtr<AHeadPlayerController>, OwningController)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:

	/** Order, controller, timers. Visible to everyone */
	FText GetPublicText() const;

	/** Who is in control right now */
	FText GetStatusText() const;

	/** The player's own mission */
	FText GetPrivateText() const;

	/** Lobby prompt, handover and results */
	FText GetCenterText() const;

	/** Recent notices and key help */
	FText GetBottomText() const;

	/** Key guide for the pointed object, controller only */
	FText GetGuideText() const;

	/** Screen-center crosshair, controller only */
	EVisibility GetCrosshairVisibility() const;

	TWeakObjectPtr<AHeadPlayerController> OwningController;
};
