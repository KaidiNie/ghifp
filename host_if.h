#ifndef __APPS_EXAMPLES_GHIFP_HOST_IF_H
#define __APPS_EXAMPLES_GHIFP_HOST_IF_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <stdint.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

// #define Bit16Test
#define HOST_IF_SET_CONFIG_REQ_SETADDR    (1)
#define HOST_IF_SET_CONFIG_REQ_SETDFS     (2)
#define HOST_IF_SET_CONFIG_REQ_DBGRECV    (3)
#define HOST_IF_SET_CONFIG_REQ_SETSLVCONF (4)
#define HOST_IF_SET_CONFIG_REQ_SETSPICLK  (5)

#define SPEED_UART_4800BPS    (0x0)
#define SPEED_UART_9600BPS    (0x1)
#define SPEED_UART_14400BPS   (0x2)
#define SPEED_UART_19200BPS   (0x3)
#define SPEED_UART_38400BPS   (0x4)
#define SPEED_UART_57600BPS   (0x5)
#define SPEED_UART_115200BPS  (0x6)
#define SPEED_UART_230400BPS  (0x7)
#define SPEED_UART_460800BPS  (0x8)
#define SPEED_UART_921600BPS  (0x9)
#define SPEED_UART_1000000BPS (0xa)
#define SPEED_UART_MAX        (SPEED_UART_1000000BPS)

#define SPEED_I2C_100000BPS   (0x0)
#define SPEED_I2C_400000BPS   (0x1)
#define SPEED_I2C_MAX         (SPEED_I2C_400000BPS)

#define DFS_SPI_8   (0x0)
#define DFS_SPI_16  (0x1)
#define DFS_SPI_MAX (DFS_SPI_16)

#define SPEED_SPI_1300000BPS  (0x0)
#define SPEED_SPI_2600000BPS  (0x1)
#define SPEED_SPI_5200000BPS  (0x2)
#define SPEED_SPI_6500000BPS  (0x3)
#define SPEED_SPI_7800000BPS  (0x4)
#define SPEED_SPI_9750000BPS  (0x5)
#define SPEED_SPI_13000000BPS (0x6)

#define SPEED_SPI_MAX         (SPEED_SPI_9750000BPS)

/****************************************************************************
 * Public Types
 ****************************************************************************/

struct host_if_s
{
  int (*write)(FAR struct host_if_s *thiz,
               FAR uint8_t *data, uint32_t sz);
  int (*read)(FAR struct host_if_s *thiz,
              FAR uint8_t *buf, uint32_t sz);
  int (*transaction)(FAR struct host_if_s *thiz,
                     FAR uint8_t *data, uint32_t w_sz,
                     FAR uint8_t *buf, uint32_t r_sz,
                     FAR uint32_t *res_len);
  int (*dbg_write)(FAR struct host_if_s *thiz,
                   FAR uint8_t *data, uint32_t sz);
  int (*set_config)(FAR struct host_if_s *thiz,
                    uint32_t req, FAR void *arg);
};

typedef void (*hostif_evt_cb)(FAR uint8_t *dataframe, int32_t len);

#endif /* __APPS_EXAMPLES_GHIFP_HOST_IF_H */
