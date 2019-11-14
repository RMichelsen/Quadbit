#pragma once

#include "Engine/Core/Entry.h"
#include "Engine/Core/Game.h"

class Testing : public Quadbit::Game {
public:
	void Init() override;
	void Simulate(float deltaTime) override;
	void DrawFrame();
};

Quadbit::Game* Quadbit::CreateGame() {
	return new Testing();
}