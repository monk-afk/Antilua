#pragma once

#include <map>
#include <vector>
#include <memory>
#include "SColor.h"
#include "SMesh.h"
#include "S3DVertex.h"
#include "irr_ptr.h"

class FFPMapBlockDayNightAnimator {
public:
	FFPMapBlockDayNightAnimator() = default;

	void addLayer(std::vector<video::S3DVertex> &vertices,
			u8 layer, u32 mesh_index);

	bool animate(irr_ptr<scene::IMesh> *meshes, u32 daynight_ratio);

	bool hasAnimation() const { return !m_daynight_diffs.empty(); }

private:
	std::map<std::pair<u8, u32>, std::map<u32, video::SColor>> m_daynight_diffs;
	u32 m_last_daynight_ratio = (u32)-1;
};
