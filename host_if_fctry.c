/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <stdio.h>
#include <errno.h>

#include "host_if_fctry.h"
#include "host_if_bs.h"
#include "host_if_uart.h"
#include "host_if_i2c.h"
#include "host_if_spi.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

 /****************************************************************************
 * Private Types
 ****************************************************************************/

/****************************************************************************
 * Private Data
 ****************************************************************************/

static FAR struct host_if_s *g_host_list[HOST_IF_FCTRY_TYPE_NUM] = {0};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int host_if_fctry_init(hostif_evt_cb evt_cb)
{
  int ret;
  printf("[IN]host_if_fctry_init %d\n", ret);
  ret = create_df_queue();
  if (ret < 0)
    {
      printf("create_df_queue %d.\n", ret);
      goto errout;
    }

  ret = bus_req_init();
  if (ret < 0)
    {
      printf("bus_req_init %d.\n", ret);
      goto errout;
    }

#if 1 /* UART Enable */
  /* Create object for UART */
  g_host_list[HOST_IF_FCTRY_TYPE_UART] = host_if_uart_create(evt_cb);

  if (!g_host_list[HOST_IF_FCTRY_TYPE_UART])
    {
      ret = -ENODEV;
      printf("UART Enable %d.\n", ret);
      goto errout;
    }
  printf("Created UART object successfully.\n");
#endif

#if 1 /* I2C Enable */
  /* Create object for I2C */
  g_host_list[HOST_IF_FCTRY_TYPE_I2C] = host_if_i2c_create(evt_cb);

  if (!g_host_list[HOST_IF_FCTRY_TYPE_I2C])
    {
      ret = -ENODEV;
      printf("I2C Enable %d.\n", ret);
      goto errout;
    }
  printf("Created I2C object successfully.\n");
#endif

#if 1 /* SPI Enable */
  /* Create object for SPI */
  g_host_list[HOST_IF_FCTRY_TYPE_SPI] = host_if_spi_create(evt_cb);

  if (!g_host_list[HOST_IF_FCTRY_TYPE_SPI])
    {
      ret = -ENODEV;
      printf("SPI Enable %d.\n", ret);
      goto errout;
    }
  printf("Created SPI object successfully.\n");
#endif

  return 0;

errout:
  printf("[OUT]host_if_fctry_init %d\n", ret);
  if (g_host_list[HOST_IF_FCTRY_TYPE_UART])
    {
      host_if_uart_delete(g_host_list[HOST_IF_FCTRY_TYPE_UART]);
      g_host_list[HOST_IF_FCTRY_TYPE_UART] = NULL;
    }

  if (g_host_list[HOST_IF_FCTRY_TYPE_I2C])
    {
      host_if_i2c_delete(g_host_list[HOST_IF_FCTRY_TYPE_I2C]);
      g_host_list[HOST_IF_FCTRY_TYPE_I2C] = NULL;
    }

  if (g_host_list[HOST_IF_FCTRY_TYPE_SPI])
    {
      host_if_spi_delete(g_host_list[HOST_IF_FCTRY_TYPE_SPI]);
      g_host_list[HOST_IF_FCTRY_TYPE_SPI] = NULL;
    }

  bus_req_deinit();

  delete_df_queue();

  return ret;
}

int host_if_fctry_fin(void)
{
  if (g_host_list[HOST_IF_FCTRY_TYPE_UART])
    {
      host_if_uart_delete(g_host_list[HOST_IF_FCTRY_TYPE_UART]);
      g_host_list[HOST_IF_FCTRY_TYPE_UART] = NULL;
    }

  if (g_host_list[HOST_IF_FCTRY_TYPE_I2C])
    {
      host_if_i2c_delete(g_host_list[HOST_IF_FCTRY_TYPE_I2C]);
      g_host_list[HOST_IF_FCTRY_TYPE_I2C] = NULL;
    }

  if (g_host_list[HOST_IF_FCTRY_TYPE_SPI])
    {
      host_if_spi_delete(g_host_list[HOST_IF_FCTRY_TYPE_SPI]);
      g_host_list[HOST_IF_FCTRY_TYPE_SPI] = NULL;
    }

  bus_req_deinit();

  delete_df_queue();

  return 0;
}

FAR struct host_if_s *host_if_fctry_get_obj(enum host_if_fctry_type_e type)
{
  if (type != HOST_IF_FCTRY_TYPE_UART &&
      type != HOST_IF_FCTRY_TYPE_I2C  &&
      type != HOST_IF_FCTRY_TYPE_SPI)
    {
      printf("Unsupported I/F type:%d\n", type);
      return NULL;
    }

  return g_host_list[type];
}
