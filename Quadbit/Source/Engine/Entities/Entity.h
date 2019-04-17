#pragma once

namespace Quadbit {
	struct EntityID {
	public:
		EntityID() : id(0), version(0) {}
		EntityID(uint32_t id, uint32_t version) : id(id), version(version) {};

		bool operator == (const EntityID& other) const { return id == other.id; }
		bool operator != (const EntityID& other) const { return id != other.id; }

	private:
		uint32_t id : 24;
		uint32_t version : 8;
	};

	class Entity {
	public:
		Entity(EntityID id) : id_(id) {}

	private:
		EntityID id_;

	};
}