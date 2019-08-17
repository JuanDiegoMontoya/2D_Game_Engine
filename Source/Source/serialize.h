#pragma once

namespace cereal
{
	template<class Archive>
	void serialize(Archive& archive, glm::vec3& v)
	{
		archive(v.x, v.y, v.z);
	}

	template<class Archive>
	void serialize(Archive& archive, glm::ivec3& v)
	{
		archive(v.x, v.y, v.z);
	}
}