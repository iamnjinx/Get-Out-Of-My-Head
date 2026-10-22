#pragma once

#include "CoreMinimal.h"

class FJsonObject;

/**
 *  Playtest records written by the host (M4)
 *  One JSON object per line in Saved/PlaytestLogs/Playtest_<date>.jsonl: rounds and post-round surveys.
 */
namespace HeadPlaytestLog
{
	/** Appends one line to today's log file */
	void Append(const TSharedRef<FJsonObject>& Entry);

	/** Folder the logs go to */
	FString GetLogFolder();

	/** Timestamp used to link a round and its surveys */
	FString MakeRoundId();
}
