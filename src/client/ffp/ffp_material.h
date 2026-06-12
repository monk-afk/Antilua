#pragma once

#include "SMaterial.h"

struct TileLayer;

void ffp_applyTileMaterial(video::SMaterial &material, const TileLayer &layer);

struct FFPEntityMaterial {
	video::E_MATERIAL_TYPE type;
	f32 param = 0.0f;
};
FFPEntityMaterial ffp_getEntityMaterial(bool use_texture_alpha);
