#pragma once

#include <SColor.h>
#include <IMeshBuffer.h>
#include <IMesh.h>

void ffp_blendDayNight(video::SColor *result, u16 light, u32 daynight_ratio);
void ffp_blendDayNight(video::SColor *result, const video::SColor &src, const video::SColorf &dayLight);
video::SColorf ffp_getSunlightColor(u32 daynight_ratio);
void ffp_colorizeMeshBuffer(scene::IMeshBuffer *buf, const video::SColor &color);
void ffp_setMeshColor(scene::IMesh *mesh, const video::SColor &color);
