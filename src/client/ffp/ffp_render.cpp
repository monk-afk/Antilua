#include "ffp_render.h"
#include "ffp_settings.h"
#include "IrrlichtDevice.h"

std::vector<video::E_DRIVER_TYPE> ffp_getDriverOrder()
{
	if (ffp_isEnabled()) {
		static const video::E_DRIVER_TYPE drivers[] = {
			video::EDT_OPENGL3,
			video::EDT_OPENGL,
			video::EDT_OGLES2,
			video::EDT_NULL,
		};
		std::vector<video::E_DRIVER_TYPE> result;
		for (auto d : drivers) {
			if (IrrlichtDevice::isDriverSupported(d))
				result.push_back(d);
		}
		return result;
	}
	static const video::E_DRIVER_TYPE drivers[] = {
		video::EDT_OPENGL,
		video::EDT_OPENGL3,
		video::EDT_OGLES2,
		video::EDT_NULL,
	};
	std::vector<video::E_DRIVER_TYPE> result;
	for (auto d : drivers) {
		if (IrrlichtDevice::isDriverSupported(d))
			result.push_back(d);
	}
	return result;
}

bool ffp_shouldEnableVBO()
{
	return ffp_isEnabled();
}
