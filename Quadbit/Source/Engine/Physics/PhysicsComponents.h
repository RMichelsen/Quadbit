#pragma once

namespace Quadbit {
	struct StaticBodyComponent {
		PxRigidStatic* rigidBody;
	};

	struct CharacterControllerComponent {
		PxController* controller;
	};
}