// Fill out your copyright notice in the Description page of Project Settings.


#include "GameMode/LobbyGameMode.h"

ALobbyGameMode::ALobbyGameMode()
{
	bUseSeamlessTravel = true;
}

void ALobbyGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	TryStartMatchTravel();
}

void ALobbyGameMode::TryStartMatchTravel()
{
	if (!HasAuthority() || bTravelStarted)
	{
		return;
	}

	if (MatchMap.IsNull())
	{
		UE_LOG(LogTemp, Error, TEXT("大厅 GameMode 未配置对战地图，无法执行 ServerTravel。"));
		return;
	}

	const FString MapPath = MatchMap.ToSoftObjectPath().GetLongPackageName();
	if (MapPath.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("大厅 GameMode 的对战地图引用无效，无法执行 ServerTravel。"));
		return;
	}

	const int32 CurrentPlayers = GetNumPlayers();
	if (CurrentPlayers < MinimumPlayers)
	{
		return;
	}

	bTravelStarted = true;
	UE_LOG(LogTemp, Log, TEXT("大厅人数已达到阈值（%d/%d），服务器开始切换到对战地图：%s"), CurrentPlayers, MinimumPlayers, *MapPath);
	GetWorld()->ServerTravel(MapPath, true);
}
