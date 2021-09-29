#ifndef __APPS_EXAMPLES_GHIFP_HOST_IF_I2C_H
#define __APPS_EXAMPLES_GHIFP_HOST_IF_I2C_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "host_if.h"

/****************************************************************************
 * Public Types
 ****************************************************************************/

/****************************************************************************
 * Public Functions
 ****************************************************************************/

FAR struct host_if_s *host_if_i2c_create(hostif_evt_cb evt_cb);
int host_if_i2c_delete(FAR struct host_if_s *this);

#endif /* __APPS_EXAMPLES_GHIFP_HOST_IF_I2C_H */
