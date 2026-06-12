#include "ffp_settings.h"
#include "settings.h"

bool ffp_isEnabled()
{
	return g_settings->getBool("enable_shaders");
}
