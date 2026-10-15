// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "LobbyGameMode.generated.h"

/**
 *
 */
UCLASS()
class MULTITPS_API ALobbyGameMode : public AGameMode
{
	GENERATED_BODY()

public:
	ALobbyGameMode();

	/** 大厅开始切换所需的最少玩家数量。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "大厅")
	int32 MinimumPlayers = 2;

	/** 由蓝图选择的对战地图资源。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "大厅")
	TSoftObjectPtr<UWorld> MatchMap;

	virtual void PostLogin(APlayerController* NewPlayer) override;

protected:
	/** 检查人数并在服务器上发起一次无缝切图。 */
	void TryStartMatchTravel();

	/** 防止多个玩家同时登录时重复发起切图。 */
	bool bTravelStarted = false;
};
