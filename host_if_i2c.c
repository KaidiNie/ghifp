/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <sdk/config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>

#include <nuttx/board.h>
#include <arch/board/board.h>
#include <../../nuttx/arch/arm/src/cxd56xx/cxd56_i2c.h>

#include "host_if.h"
#include "host_if_bs.h"
#include "gacrux_protocol_def.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define BUS_REQ_ENABLE

#define I2C0_BUS          (0)

#define I2C_DEFAULT_SPEED (400000) /* 400K */

#define I2C_DEFAULT_TARGET_ADDRESS     (0x24)
#define I2C_DEFAULT_TARGET_ADDRESS_LEN (7)

 /****************************************************************************
 * Private Types
 ****************************************************************************/

static int i2c_recv_task(int argc, FAR char *argv[]);
static int change_speed(uint8_t speed_number);
static FAR struct i2c_master_s *i2c_dev_init(void);
static int i2c_dev_uninit(FAR struct i2c_master_s *dev);
static int host_if_i2c_write(
  FAR struct host_if_s *thiz, FAR uint8_t *data, uint32_t sz);
static int host_if_i2c_read(
  FAR struct host_if_s *thiz, FAR uint8_t *buf, uint32_t sz);
static int host_if_i2c_transaction(
  FAR struct host_if_s *thiz,
  FAR uint8_t *data, uint32_t w_sz,
  FAR uint8_t *buf, uint32_t r_sz,
  FAR uint32_t *res_len);
static int host_if_i2c_dbg_write(
  FAR struct host_if_s *thiz, FAR uint8_t *data, uint32_t sz);
static int host_if_i2c_set_config(
  FAR struct host_if_s *thiz, uint32_t req, FAR void *arg);

/****************************************************************************
 * Private Data
 ****************************************************************************/

struct host_if_s g_i2c = {
  .write = host_if_i2c_write,
  .read = host_if_i2c_read,
  .transaction = host_if_i2c_transaction,
  .dbg_write = host_if_i2c_dbg_write,
  .set_config = host_if_i2c_set_config
};

static struct i2c_master_s *g_dev = NULL;
static FAR uint8_t         *g_i2c_local_buf = NULL;
static uint32_t            g_i2c_freq = I2C_DEFAULT_SPEED;
static pid_t               g_i2c_task_pid = 0;
static hostif_evt_cb       g_evt_cb = NULL;
static uint16_t            g_targetaddr = I2C_DEFAULT_TARGET_ADDRESS;
static int                 g_i2c_dbg_recv = 0;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static int i2c_recv_task(int argc, FAR char *argv[])
{
  int                 ret;
  int                 i;
  struct i2c_config_s config;
  uint8_t             opc;
  uint16_t            opr_len = 0;

  memset(g_i2c_local_buf, 0, LOCAL_BUFF_SZ);

  printf("I2C frequency(recv):%u\n", g_i2c_freq);

  if (!g_dev)
    {
      return -ENODEV;
    }

  while (1)
    {

#ifdef BUS_REQ_ENABLE /* Bus req interrupt */
      ret = bus_req_wait_i2c();
      if (ret != 0)
        {
          printf("Failed to wait bus req:%d\n", ret);
          continue;
        }

      // printf("Detect Bus Request.(1)\n");

      opr_len = 0;

      config.frequency = g_i2c_freq;
      config.address   = g_targetaddr;
      config.addrlen   = I2C_DEFAULT_TARGET_ADDRESS_LEN;

      if (g_i2c_dbg_recv != 0)
        {
          /* Read header. */
          ret = i2c_read(g_dev, &config,
                        g_i2c_local_buf,
                        256);
          printf("Data dump.\n");
          for (i=0; i<256; i++)
            {
              printf("%02X ", g_i2c_local_buf[i]);
            }
          printf("\n");
          continue;
        }
      else
        {
          /* Read header. */
          ret = i2c_read(g_dev, &config,
                        g_i2c_local_buf,
                        GHIFP_HEADER_SIZE);
          while (ret != 0)
            {
              printf("Failed to read header1:%d ", ret);
              ret = i2c_read(g_dev, &config,
                        g_i2c_local_buf,
                        GHIFP_HEADER_SIZE);
              // continue;
            }
          // printf("header: %x %x %x %x % x\n", g_i2c_local_buf[0], g_i2c_local_buf[1], g_i2c_local_buf[2], g_i2c_local_buf[3], g_i2c_local_buf[4]);

          ret = check_header(g_i2c_local_buf, &opc, &opr_len);
          if (ret != 0)
          {
            printf("Invalid header.(I2C)\n");
            continue;
          }
          else {
            // printf("Header dump.(I2C)\n");
            for (i=0; i<GHIFP_HEADER_SIZE; i++)
            {
              // printf("%02X ", g_i2c_local_buf[i]);
              printf("%d   ", opc);
              break;
            }
            // printf("\n");
          }
        }
#else /* Polling */
      printf("Polling\n");
      /* Read first a byte. */
      ret = i2c_read(g_dev, &config,
                     g_i2c_local_buf,
                     1);
      if (ret != 0)
        {
          printf("Failed to read first a byte:%d\n", ret);
          sleep(2);
          continue;
        }

      if (g_i2c_local_buf[0] != GHIFP_SYNC)
        {
          sleep(1);
          continue;
        }

      /* Read header. */
      ret = i2c_read(g_dev, &config,
                     g_i2c_local_buf + 1,
                     GHIFP_HEADER_SIZE - 1);
      if (ret != 0)
        {
          printf("Failed to read header:%d\n", ret);
          continue;
        }
#endif

      /* Check header */
      ret = check_header(g_i2c_local_buf, &opc, &opr_len);
      if (ret != 0)
        {
          printf("Invalid header.(I2C)\n\n");
          continue;
        }

      /* Read data. */
#ifdef BUS_REQ_ENABLE

      int rec_size = GHIFP_DATA_SIZE(opr_len);
      int num = rec_size / 512 + ((rec_size % 512) ? 0 : -1);
      // printf("rec_size: %d, packet: %d\n", rec_size, num);

      while(num--) {
        ret = bus_req_wait_i2c();
        if (ret != 0)
          {
            printf("Failed to wait bus req:%d\n", ret);
            continue;
          }
        // printf("Detect Bus Request.(2)\n");

        ret = i2c_read(g_dev, &config,
                     g_i2c_local_buf + GHIFP_HEADER_SIZE,
                     512);
        while (ret != 0)
          {
            printf("Failed to read data0:%d\n", ret);
            ret = i2c_read(g_dev, &config,
                     g_i2c_local_buf + GHIFP_HEADER_SIZE,
                     512);
            // continue;
          }
       }

       ret = bus_req_wait_i2c();
        if (ret != 0)
          {
            printf("Failed to wait bus req:%d\n", ret);
            continue;
          }
        // printf("Detect Bus Request.(3)\n");

       ret = i2c_read(g_dev, &config,
                     g_i2c_local_buf + GHIFP_HEADER_SIZE + 512 * (int)(rec_size / 513),
                     rec_size % 512);
      while (ret != 0)
        {
          printf("Failed to read data:%d\n", ret);
          ret = i2c_read(g_dev, &config,
                     g_i2c_local_buf + GHIFP_HEADER_SIZE + 512 * (int)(rec_size / 513),
                     rec_size % 512);
          // continue;
        }

      // printf("Data dump.(I2C)\n");
      for (i=0; i<GHIFP_DATA_SIZE(opr_len); i++)
        {
          // printf("%02X(%d) ", g_i2c_local_buf[GHIFP_HEADER_SIZE + i], i);
          printf("%d ", g_i2c_local_buf[GHIFP_HEADER_SIZE + i]);
        }
      printf("\n");

      /* Check data */
      ret = check_data(g_i2c_local_buf + GHIFP_HEADER_SIZE, opr_len);
      if (ret != 0)
        {
          printf("Invalid data.(I2C)\n");
          continue;
        }

      // int input = 0;
      // if (opc == 48)
      // {
      //   FILE *fp;
      //   fp = fopen("/mnt/spif/hoge.txt", "w");
      //   int k = 100;
      //   while (k--)
      //   {
      //     if (input == 0x0d)
      //     {
      //       break;
      //     }
      //     ret = bus_req_wait_i2c();
      //     ret = i2c_read(g_dev, &config, g_i2c_local_buf, 512);
      //     printf("k = %d\n", k);
      //   }
      //   fclose(fp);
      // }
      // printf("finished.(I2C)\n");
#endif
      if (true == is_evt_data(opc))
        {
          /* OPC type -> Event, notify by callback. */
          if (g_evt_cb)
            {
              g_evt_cb(g_i2c_local_buf,
                       GHIFP_FRAME_SIZE(opr_len));
            }
        }
      else
        {
          /* OPC type -> Normal response, push to queue. */
          if (get_host_if_state() == HOST_IF_STATE_WAIT_RESPONSE)
            {
              ret = push_dataframe(g_i2c_local_buf,
                                   GHIFP_FRAME_SIZE(opr_len));
              if (ret != 0)
                {
                  printf("Failed to push dataframe.\n");
                  continue;
                }
            }
          else
            {
              // printf("Discard dataframe.\n");
            }
        }
    }

  printf("Entering abnormal loop.\n");
  while (1)
    {
      sleep(3);
    }

  task_delete(0);

  return 0;
}

static int conv_i2c_speed_number(uint8_t speed_no)
{
  switch (speed_no)
    {
      case SPEED_I2C_100000BPS:
        return 100000;
        break;
      case SPEED_I2C_400000BPS:
        return 400000;
        break;
      default:
        break;
    }

  return -EINVAL;
}

static int change_speed(uint8_t speed_number)
{
  int ret;

  ret = conv_i2c_speed_number(speed_number);
  if (ret < 0)
    {
      return ret;
    }
  else
    {
      printf("Change speed. %u -> %u\n", g_i2c_freq, (uint32_t)ret);
      g_i2c_freq = (uint32_t)ret;
    }

  /* No need to re-create task.
     New speed is applied i2c_recv_task. */
#if 0
  /* Restart receive task */
  ret = restart_task(g_i2c_task_pid, "ghifp_i2c_task", i2c_recv_task, NULL);
  if (0 < ret)
    {
      g_i2c_task_pid = ret;
    }
#endif

  return ret;
}

static FAR struct i2c_master_s *i2c_dev_init(void)
{
  FAR struct i2c_master_s *dev = NULL;

  dev = cxd56_i2cbus_initialize(I2C0_BUS);
  if (!dev)
    {
      printf("Failed to initialize I2C.\n");
      return NULL;
    }

  return dev;
}

static int i2c_dev_uninit(FAR struct i2c_master_s *dev)
{
  int ret;

  ret = cxd56_i2cbus_uninitialize(dev);
  if (ret != 0)
    {
      printf("Failed to initialize I2C.\n");
    }

  return ret;
}

static int host_if_i2c_write(
  FAR struct host_if_s *thiz, FAR uint8_t *data, uint32_t sz)
{
  int                 ret = -EINVAL;
  struct i2c_config_s config;
  uint8_t             opc;
  uint16_t            opr_len;

  printf("host_if_i2c_write() len=%d\n", sz);

  if (!thiz || !data || !sz)
    {
      return ret;
    }

  if (!g_dev)
    {
      return -EPERM;
    }

  if (0 != check_header(data, &opc, &opr_len))
    {
      return ret;
    }

  if (0 != check_data(data + GHIFP_HEADER_SIZE, opr_len))
    {
      return ret;
    }

  printf("I2C frequency:%u\n", g_i2c_freq);

  config.frequency = g_i2c_freq;
  config.address   = g_targetaddr;
  config.addrlen   = I2C_DEFAULT_TARGET_ADDRESS_LEN;

  ret = i2c_write(g_dev, &config,
                  (FAR const uint8_t *)data,
                  sz);
  if (ret != 0)
    {
      printf("Failed to send dataframe:%d\n", ret);
      goto exit;
    }

  set_host_if_state(HOST_IF_STATE_WAIT_RESPONSE);

exit:
  return ret;
}

static int host_if_i2c_read(
  FAR struct host_if_s *thiz, FAR uint8_t *buf, uint32_t sz)
{
  int      ret = -EINVAL;
  uint32_t res_len;

  printf("host_if_i2c_read() buflen=%d\n", sz);

  if (!thiz || !buf || !sz)
    {
      return ret;
    }

  ret = pop_dataframe(buf, sz, &res_len);
  set_host_if_state(HOST_IF_STATE_IDLE);
  if (ret < 0)
    {
      printf("Response pop error:%d\n", ret);
      return ret;
    }
  else
    {
      printf("Response dataframe len:%d\n", res_len);
      return res_len;
    }
}

static int host_if_i2c_transaction(
  FAR struct host_if_s *thiz,
  FAR uint8_t *data, uint32_t w_sz,
  FAR uint8_t *buf, uint32_t r_sz,
  FAR uint32_t *res_len)
{
  int                 ret = -EINVAL;
  struct i2c_config_s config;
  uint32_t            df_len;
  uint8_t             opc;
  uint16_t            opr_len;

  printf("I2C transaction. write len=%d, read len=%d\n", w_sz, r_sz);

  if (!thiz || !data || !w_sz || !buf || !r_sz || !res_len)
    {
      return ret;
    }

  if (!g_dev)
    {
      return -EPERM;
    }

  if (0 != check_header(data, &opc, &opr_len))
    {
      return ret;
    }

  if (0 != check_data(data + GHIFP_HEADER_SIZE, opr_len))
    {
      return ret;
    }

  printf("I2C frequency:%u\n", g_i2c_freq);

  config.frequency = g_i2c_freq;
  config.address   = g_targetaddr;
  config.addrlen   = I2C_DEFAULT_TARGET_ADDRESS_LEN;

  ret = i2c_write(g_dev, &config,
                  (FAR const uint8_t *)data,
                  w_sz);
  if (ret != 0)
    {
      printf("Failed to send dataframe:%d\n", ret);
      goto exit;
    }
  set_host_if_state(HOST_IF_STATE_WAIT_RESPONSE);

  ret = pop_dataframe(buf, r_sz, &df_len);
  set_host_if_state(HOST_IF_STATE_IDLE);
  if (ret < 0)
    {
      printf("Response pop error:%d\n", ret);
      goto exit;
    }
  else
    {
      printf("Response dataframe len:%d\n", df_len);
      *res_len = df_len;
    }

exit:

  return ret;
}

static int host_if_i2c_dbg_write(
  FAR struct host_if_s *thiz, FAR uint8_t *data, uint32_t sz)
{
  int                 ret = -EINVAL;
  struct i2c_config_s config;

  printf("host_if_i2c_dbg_write() len=%d\n", sz);

  if (!thiz || !data || !sz)
    {
      return ret;
    }

  if (!g_dev)
    {
      return -EPERM;
    }

  printf("I2C frequency:%u\n", g_i2c_freq);

  config.frequency = g_i2c_freq;
  config.address   = g_targetaddr;
  config.addrlen   = I2C_DEFAULT_TARGET_ADDRESS_LEN;

  ret = i2c_write(g_dev, &config,
                  (FAR const uint8_t *)data,
                  sz);
  if (ret != 0)
    {
      printf("I2C write error:%d\n", ret);
    }

  return ret;
}

static int host_if_i2c_set_config(
  FAR struct host_if_s *thiz, uint32_t req, FAR void *arg)
{
  int ret = -ENOTSUP;

  if (!arg)
    {
      return -EINVAL;
    }

  switch (req)
    {
      case HOST_IF_SET_CONFIG_REQ_SETADDR:
        g_targetaddr = (uint16_t)strtol(arg, NULL, 16);
        printf("Set I2C target address -> 0x%02X\n", g_targetaddr);
        ret = 0;
        break;
      case HOST_IF_SET_CONFIG_REQ_DBGRECV:
        g_i2c_dbg_recv = atoi((char *)arg);
        printf("Debug receive mode:%s\n",
               g_i2c_dbg_recv ? "ON" : "OFF");
        ret = restart_task(g_i2c_task_pid, "ghifp_i2c_task",
                           i2c_recv_task, NULL);
        if (0 < ret)
          {
            g_i2c_task_pid = ret;
          }
        break;
      case HOST_IF_SET_CONFIG_REQ_SETSLVCONF:
        ret = change_speed(*(uint8_t *)arg);
        break;
      default:
        printf("Nothing to do in I2C mode.\n");
        break;
    }
  return ret;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

FAR struct host_if_s *host_if_i2c_create(hostif_evt_cb evt_cb)
{
  if (!evt_cb)
    {
      return NULL;
    }
  else
    {
      g_evt_cb = evt_cb;
    }

  g_dev = i2c_dev_init();
  if (!g_dev)
    {
      goto errout;
    }

  g_i2c_local_buf = (FAR uint8_t *)malloc(LOCAL_BUFF_SZ);
  if (!g_i2c_local_buf)
    {
      goto errout;
    }

  g_i2c_task_pid = start_task("ghifp_i2c_task", i2c_recv_task, NULL);
  if (g_i2c_task_pid < 0)
    {
      goto errout;
    }

  return &g_i2c;

errout:

  if (g_i2c_local_buf)
    {
      free(g_i2c_local_buf);
    }
  g_i2c_local_buf = NULL;

  if (g_dev)
    {
      i2c_dev_uninit(g_dev);
      g_dev = NULL;
    }

  g_evt_cb = NULL;

  return NULL;
}

int host_if_i2c_delete(FAR struct host_if_s *this)
{
  stop_task(g_i2c_task_pid);
  g_i2c_task_pid = 0;

  if (g_i2c_local_buf)
    {
      free(g_i2c_local_buf);
    }
  g_i2c_local_buf = NULL;

  if (g_dev)
    {
      i2c_dev_uninit(g_dev);
      g_dev = NULL;
    }

  g_evt_cb = NULL;

  return 0;
}
