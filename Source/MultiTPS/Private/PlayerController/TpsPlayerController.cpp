// Fill out your copyright notice in the Description page of Project Settings.


#include "PlayerController/TpsPlayerController.h"

#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"

void ATpsPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (!IsLocalController() || !DefaultMappingContext)
	{
		return;
	}

	if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem =
			LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
		{
			InputSubsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}
}
