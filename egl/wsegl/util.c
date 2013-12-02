#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

#include "wayland-wsegl.h"

uint64_t
get_time_ms(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

bool
debug_get_bool_option(const char *name, bool dfault)
{
	const char *str = getenv(name);
	bool result;

	if (str == NULL)
		result = dfault;
	else if (!strcasecmp(str, "n"))
		result = false;
	else if (!strcasecmp(str, "no"))
		result = false;
	else if (!strcmp(str, "0"))
		result = false;
	else if (!strcasecmp(str, "f"))
		result = false;
	else if (!strcasecmp(str, "false"))
		result = false;
	else
		result = true;

	return result;
}

const char *
pvr2d_strerror(PVR2DERROR err)
{
	switch (err) {
	case PVR2D_OK:
		return "success";
	case PVR2DERROR_INVALID_PARAMETER:
		return "invalid parameter";
	case PVR2DERROR_DEVICE_UNAVAILABLE:
		return "device unavailable";
	case PVR2DERROR_INVALID_CONTEXT:
		return "invalid context";
	case PVR2DERROR_MEMORY_UNAVAILABLE:
		return "memory unavailable";
	case PVR2DERROR_DEVICE_NOT_PRESENT:
		return "device not present";
	case PVR2DERROR_IOCTL_ERROR:
		return "ioctl error";
	case PVR2DERROR_GENERIC_ERROR:
		return "generic error";
	case PVR2DERROR_BLT_NOTCOMPLETE:
		return "blit not complete";
	case PVR2DERROR_HW_FEATURE_NOT_SUPPORTED:
		return "hw feature not supported";
	case PVR2DERROR_NOT_YET_IMPLEMENTED:
		return "not yet implemented";
	case PVR2DERROR_MAPPING_FAILED:
		return "mapping failed";
	default:
		return "unknown error";
	};
}
