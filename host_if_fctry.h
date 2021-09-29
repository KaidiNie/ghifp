#ifndef __APPS_EXAMPLES_GHIFP_HOST_IF_FCTRY_H
#define __APPS_EXAMPLES_GHIFP_HOST_IF_FCTRY_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "host_if.h"

/****************************************************************************
 * Public Types
 ****************************************************************************/

enum host_if_fctry_type_e
{
  HOST_IF_FCTRY_TYPE_UART = 0,
  HOST_IF_FCTRY_TYPE_I2C,
  HOST_IF_FCTRY_TYPE_SPI,
  HOST_IF_FCTRY_TYPE_NUM
};

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int host_if_fctry_init(hostif_evt_cb evt_cb);
int host_if_fctry_fin(void);
FAR struct host_if_s *host_if_fctry_get_obj(enum host_if_fctry_type_e type);

#endif /* __APPS_EXAMPLES_GHIFP_HOST_IF_FCTRY_H */
