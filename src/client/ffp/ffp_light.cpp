#include "ffp_light.h"
#include "S3DVertex.h"
#include "client/mapblock_mesh.h" // for encode_light

static void ffp_getSunlightColor(video::SColorf *sunlight, u32 daynight_ratio)
{
	f32 rg = daynight_ratio / 1000.0f - 0.04f;
	f32 b = (0.98f * daynight_ratio) / 1000.0f + 0.078f;
	sunlight->r = rg;
	sunlight->g = rg;
	sunlight->b = b;
}

video::SColorf ffp_getSunlightColor(u32 daynight_ratio)
{
	video::SColorf c;
	ffp_getSunlightColor(&c, daynight_ratio);
	return c;
}

void ffp_blendDayNight(video::SColor *result, u16 light, u32 daynight_ratio)
{
	video::SColorf dayLight;
	ffp_getSunlightColor(&dayLight, daynight_ratio);

	static const video::SColorf artificialColor(1.04f, 1.04f, 1.04f);

	video::SColorf c(encode_light(light, 0));
	f32 n = 1 - c.a;

	f32 r = c.r * (c.a * dayLight.r + n * artificialColor.r) * 2.0f;
	f32 g = c.g * (c.a * dayLight.g + n * artificialColor.g) * 2.0f;
	f32 b = c.b * (c.a * dayLight.b + n * artificialColor.b) * 2.0f;

	static const u8 emphase_blue_when_dark[32] = {
		1, 4, 6, 6, 6, 5, 4, 3, 2, 1, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	};
	b += emphase_blue_when_dark[core::clamp((s32)((r + g + b) / 3 * 255), 0, 255) / 8] / 255.0f;

	result->setRed(core::clamp((s32)(r * 255.0f), 0, 255));
	result->setGreen(core::clamp((s32)(g * 255.0f), 0, 255));
	result->setBlue(core::clamp((s32)(b * 255.0f), 0, 255));
}

void ffp_blendDayNight(video::SColor *result, const video::SColor &data, const video::SColorf &dayLight)
{
	static const video::SColorf artificialColor(1.04f, 1.04f, 1.04f);

	video::SColorf c(data);
	f32 n = 1 - c.a;

	f32 r = c.r * (c.a * dayLight.r + n * artificialColor.r) * 2.0f;
	f32 g = c.g * (c.a * dayLight.g + n * artificialColor.g) * 2.0f;
	f32 b = c.b * (c.a * dayLight.b + n * artificialColor.b) * 2.0f;

	static const u8 emphase_blue_when_dark[32] = {
		1, 4, 6, 6, 6, 5, 4, 3, 2, 1, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	};
	b += emphase_blue_when_dark[core::clamp((s32)((r + g + b) / 3 * 255), 0, 255) / 8] / 255.0f;

	result->setRed(core::clamp((s32)(r * 255.0f), 0, 255));
	result->setGreen(core::clamp((s32)(g * 255.0f), 0, 255));
	result->setBlue(core::clamp((s32)(b * 255.0f), 0, 255));
}

void ffp_colorizeMeshBuffer(scene::IMeshBuffer *buf, const video::SColor &color)
{
	video::S3DVertex *vertices = (video::S3DVertex *)buf->getVertices();
	u32 count = buf->getVertexCount();
	for (u32 i = 0; i < count; i++) {
		vertices[i].Color = color;
		static const v3f light_dir = v3f(1.0f, 1.0f, -0.5f).normalize();
		f32 dot = vertices[i].Normal.dotProduct(light_dir);
		if (dot < 0.0f)
			dot = 0.0f;
		f32 shading = 0.7f + 0.3f * dot;
		vertices[i].Color.setRed((u8)(vertices[i].Color.getRed() * shading));
		vertices[i].Color.setGreen((u8)(vertices[i].Color.getGreen() * shading));
		vertices[i].Color.setBlue((u8)(vertices[i].Color.getBlue() * shading));
	}
}

void ffp_setMeshColor(scene::IMesh *mesh, const video::SColor &color)
{
	for (u32 i = 0; i < mesh->getMeshBufferCount(); i++) {
		auto *buf = mesh->getMeshBuffer(i);
		ffp_colorizeMeshBuffer(buf, color);
		buf->setDirty(scene::EBT_VERTEX);
	}
}
