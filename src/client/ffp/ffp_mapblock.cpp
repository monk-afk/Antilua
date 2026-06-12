#include "ffp_mapblock.h"
#include "ffp_light.h"
#include "S3DVertex.h"

void FFPMapBlockDayNightAnimator::addLayer(
		std::vector<video::S3DVertex> &vertices,
		u8 layer, u32 mesh_index)
{
	video::SColorf sunlight = ffp_getSunlightColor(0);

	std::map<u32, video::SColor> colors;
	for (u32 j = 0; j < vertices.size(); j++) {
		auto &vc = vertices[j];
		video::SColor copy = vc.Color;
		if (vc.Color.getAlpha() == 0) {
			ffp_blendDayNight(&vc.Color, copy, sunlight);
		} else {
			colors[j] = copy;
		}
		vc.Color.setAlpha(255);
	}
	if (!colors.empty())
		m_daynight_diffs[{layer, mesh_index}] = std::move(colors);
}

bool FFPMapBlockDayNightAnimator::animate(
		irr_ptr<scene::IMesh> *meshes, u32 daynight_ratio)
{
	if (daynight_ratio == m_last_daynight_ratio)
		return false;

	video::SColorf day_color = ffp_getSunlightColor(daynight_ratio);

	for (const auto &entry : m_daynight_diffs) {
		auto *mesh = meshes[entry.first.first].get();
		mesh->setDirty(scene::EBT_VERTEX);
		scene::IMeshBuffer *buf = mesh->getMeshBuffer(entry.first.second);
		video::S3DVertex *vertices = (video::S3DVertex *)buf->getVertices();
		for (const auto &ve : entry.second)
			ffp_blendDayNight(&(vertices[ve.first].Color),
					ve.second, day_color);
	}
	m_last_daynight_ratio = daynight_ratio;
	return true;
}
