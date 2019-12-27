#pragma once

#include <EASTL/unique_ptr.h>

#include "Engine/Core/Entry.h"
#include "Engine/Core/Game.h"

class Testing : public Quadbit::Game {
public:
	void Init() override;
	void Simulate(float deltaTime) override;

private:
};

eastl::unique_ptr<Quadbit::Game> Quadbit::CreateGame() {
	return eastl::make_unique<Testing>();
}