// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "TpsPlayerController.generated.h"

class UInputMappingContext;

/**
 *
 */
UCLASS()
class MULTITPS_API ATpsPlayerController : public APlayerController
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;

	/** Mapping context added to the local player's Enhanced Input subsystem. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<const UInputMappingContext> DefaultMappingContext;
};
