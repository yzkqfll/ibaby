
#ifndef __THER_PREIVATE_SERVICE_H__
#define __THER_PREIVATE_SERVICE_H__


#define THER_PRIVATE_SERVICE 0x00000001

enum {
	THER_PS_GET_WARNING_ENABLED = 0,
	THER_PS_SET_WARNING_ENABLED,

	THER_PS_CLEAR_WARNING,

	THER_PS_GET_HIGH_WARN_TEMP,
	THER_PS_SET_HIGH_WARN_TEMP,

	THER_PS_GET_TIME,
	THER_PS_SET_TIME,

	THER_PS_GET_DEBUG,
	THER_PS_SET_DEBUG,
};

bStatus_t ther_add_private_service(uint32 services, void (*profile_change)(uint8 event, uint8 *data, uint8 *len));
bStatus_t ther_ps_get_param( uint8 param, unsigned char *value);
bStatus_t ther_ps_set_param(uint8 param, unsigned char *value);

#endif

