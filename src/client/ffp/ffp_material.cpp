#include "ffp_material.h"
#include "client/tile.h"

void ffp_applyTileMaterial(video::SMaterial &material, const TileLayer &layer)
{
	if (!layer.texture)
		return;
	material.setTexture(0, layer.texture);
	material.BackfaceCulling = (layer.material_flags & MATERIAL_FLAG_BACKFACE_CULLING) != 0;
	material.setTexture(1, nullptr);

	switch (layer.material_type) {
	case TILE_MATERIAL_BASIC:
	case TILE_MATERIAL_WAVING_LEAVES:
	case TILE_MATERIAL_WAVING_PLANTS:
	case TILE_MATERIAL_WAVING_LIQUID_BASIC:
	case TILE_MATERIAL_LIQUID_OPAQUE:
	case TILE_MATERIAL_WAVING_LIQUID_OPAQUE:
		material.MaterialType = video::EMT_TRANSPARENT_ALPHA_CHANNEL_REF;
		break;
	case TILE_MATERIAL_OPAQUE:
	case TILE_MATERIAL_PLAIN:
		material.MaterialType = video::EMT_SOLID;
		break;
	case TILE_MATERIAL_ALPHA:
	case TILE_MATERIAL_LIQUID_TRANSPARENT:
	case TILE_MATERIAL_WAVING_LIQUID_TRANSPARENT:
	case TILE_MATERIAL_PLAIN_ALPHA:
		material.MaterialType = video::EMT_TRANSPARENT_ALPHA_CHANNEL;
		break;
	default:
		material.MaterialType = video::EMT_SOLID;
		break;
	}

	if (!(layer.material_flags & MATERIAL_FLAG_TILEABLE_HORIZONTAL))
		material.TextureLayers[0].TextureWrapU = video::ETC_CLAMP_TO_EDGE;
	if (!(layer.material_flags & MATERIAL_FLAG_TILEABLE_VERTICAL))
		material.TextureLayers[0].TextureWrapV = video::ETC_CLAMP_TO_EDGE;
}

FFPEntityMaterial ffp_getEntityMaterial(bool use_texture_alpha)
{
	if (use_texture_alpha)
		return {video::EMT_TRANSPARENT_ALPHA_CHANNEL, 1.0f / 256.f};
	return {video::EMT_TRANSPARENT_ALPHA_CHANNEL_REF, 0.0f};
}
