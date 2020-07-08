#include "stdafx.h"
#include "NetPlayerState.h"

namespace Net
{
	VisiblePlayerState PlayerObject::GetVisibleState()
	{
		ASSERT(states.size() > 0);

		if (states.size() == 1)
			return states[0];

		// interpolate if > 1 state
		VisiblePlayerState state;
		state.pos = glm::mix(states[0].pos, states[1].pos, keyframe);
		state.front = glm::mix(states[0].front, states[1].front, keyframe);
		return state;
	}


	std::optional<PlayerObject> Net::PlayerWorld::GetObj(int id)
	{
		std::shared_lock lk(mtx);
		if (auto f = objects.find(id); f != objects.end())
			return f->second;
		return std::nullopt;
	}


	void PlayerWorld::RemoveObject(int id)
	{
		std::lock_guard lk(mtx);
		objects.erase(id);
	}


	void PlayerWorld::PushState(int id, VisiblePlayerState state)
	{
		std::lock_guard lk(mtx);
		objects[id].states.push_back(state);
	}


	void PlayerWorld::PopState(int id)
	{
		std::lock_guard lk(mtx);
		objects[id].states.pop_front();
	}


	void PlayerWorld::UpdateStates()
	{
		std::lock_guard lk(mtx); // we locking it ALL down
		for (auto& [key, obj] : objects)
		{
			// advance keyframe of each object by server tick
			// pop old player states if necessary
		}

		// remove any object with empty state; the end of their life was implicitly reached
	}
}
