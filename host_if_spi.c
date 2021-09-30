/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <sdk/config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <semaphore.h>

#include <nuttx/board.h>
#include <arch/board/board.h>
#include <nuttx/spi/spi.h>
#include <../../nuttx/arch/arm/src/cxd56xx/cxd56_spi.h>
#include <../../nuttx/arch/arm/src/cxd56xx/cxd56_pinconfig.h>

#include "host_if.h"
#include "host_if_bs.h"
#include "gacrux_protocol_def.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define LOCK()   pthread_mutex_lock(&g_mutex)
#define UNLOCK() pthread_mutex_unlock(&g_mutex)

#define SPI5_BUS          (5)

#define SPI_DEFAULT_SPEED (2600000)
#define SPI_DEFAULT_DFS   (8)

 /****************************************************************************
 * Private Types
 ****************************************************************************/

static int spi_recv_task(int argc, FAR char *argv[]);
static FAR struct spi_dev_s *spi_dev_init(void);
static int spi_dev_uninit(void);
static int change_dfs(uint8_t dfs_number);
static int change_dfs_old(int dfs);
static int32_t conv_spi_clk_number(int clk_no);
static int set_clock(int clock);
static int spi_write(FAR uint8_t *data, uint32_t sz);
static int spi_read(FAR uint8_t *buf, uint32_t sz);
static int spi_dummy_exchange(void);
static int host_if_spi_write(
  FAR struct host_if_s *thiz, FAR uint8_t *data, uint32_t sz);
static int host_if_spi_read(
  FAR struct host_if_s *thiz, FAR uint8_t *buf, uint32_t sz);
static int host_if_spi_transaction(
  FAR struct host_if_s *thiz,
  FAR uint8_t *data, uint32_t w_sz,
  FAR uint8_t *buf, uint32_t r_sz,
  FAR uint32_t *res_len);
static int host_if_spi_dbg_write(
  FAR struct host_if_s *thiz, FAR uint8_t *data, uint32_t sz);
static int host_if_spi_set_config(
  FAR struct host_if_s *thiz, uint32_t req, FAR void *arg);

/****************************************************************************
 * Private Data
 ****************************************************************************/

struct host_if_s g_spi = {
  .write = host_if_spi_write,
  .read = host_if_spi_read,
  .transaction = host_if_spi_transaction,
  .dbg_write = host_if_spi_dbg_write,
  .set_config = host_if_spi_set_config
};

static struct spi_dev_s *g_dev = NULL;
static FAR uint8_t      *g_spi_local_buf = NULL;
static uint32_t         g_spi_freq = SPI_DEFAULT_SPEED;
static int              g_spi_dfs = SPI_DEFAULT_DFS;
static pthread_mutex_t  g_mutex;
static pid_t            g_spi_task_pid = 0;
static hostif_evt_cb    g_evt_cb = NULL;
static int              g_spi_dbg_recv = 0;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static int spi_recv_task(int argc, FAR char *argv[])
{
  int         ret;
  int         i;
  int         read_len;
  int         read_len_tmp;
  uint8_t     opc;
  uint16_t    opr_len;

  memset(g_spi_local_buf, 0, LOCAL_BUFF_SZ);
  printf("SPI frequency(recv):%u\n", g_spi_freq);

  if (!g_dev)
    {
      return -ENODEV;
    }

  while (1)
    {
      ret = bus_req_wait_spi();
      if (ret != 0)
        {
          printf("Failed to wait bus req:%d\n", ret);
          continue;
        }
      // LOCK();
      printf("Detect Bus Request.(SPI)\n");
      opr_len = 0;

      if (g_spi_dbg_recv != 0)
        {
          read_len = spi_read(g_spi_local_buf, 256);
          printf("Data dump.\n");
          for (i=0; i<256; i++)
            {
              printf("%02X ", g_spi_local_buf[i]);
            }
          printf("\n");
          memset(g_spi_local_buf, 0, LOCAL_BUFF_SZ);
          // UNLOCK();
          continue;
        }
      else
        {
          /* read_len = spi_read(g_spi_local_buf, GHIFP_HEADER_SIZE); */

          /* If dfs is 16, read_len will be not header size(5byte) but 6byte.
            It means that read data include the first byte of data part. */

          /* printf("Header dump.(SPI)\n");
          for (i=0; i<read_len; i++)
            {
              printf("%02X ", g_spi_local_buf[i]);
            }
          printf("\n"); */

          /////////////////////////////////////////////////////////////////
          /* Read header. */
          read_len = spi_read(g_spi_local_buf, GHIFP_HEADER_SIZE);
          while (read_len <= 0)
            {
              printf("Failed to read header1:%d ", read_len);
            }

          ret = check_header(g_spi_local_buf, &opc, &opr_len);
          if (ret != 0)
          {
            printf("Invalid header.(SPI)\n");
            printf("Header dump.(SPI)\n");
            for (i=0; i<read_len; i++)
              {
                printf("%02X ", g_spi_local_buf[i]);
              }
            printf("\n");
            continue;
          }
          else {
            for (i=0; i<GHIFP_HEADER_SIZE; i++)
            {
              printf("%d   ", opc);
              break;
            }
            // printf("\n");
          }
          /////////////////////////////////////////////////////////////////////////////////
        }

      /* Check header */
      ret = check_header(g_spi_local_buf, &opc, &opr_len);
      if (ret != 0)
        {
          printf("Invalid header.(SPI)\n");
          // UNLOCK();
          continue;
        }

      /* Read data. */
      /* read_len_tmp = read_len;

      read_len =
        spi_read(g_spi_local_buf + read_len_tmp,
                 GHIFP_DATA_SIZE(opr_len) + GHIFP_HEADER_SIZE - read_len_tmp);
      spi_dummy_exchange();

      printf("Data dump.(SPI)\n");
      for (i=0; i<read_len; i++)
        {
          printf("%02X ", g_spi_local_buf[read_len_tmp + i]);
        }
      printf("\n"); */

      int rec_size = GHIFP_DATA_SIZE(opr_len);
      int num = rec_size / 512 + ((rec_size % 512) ? 0 : -1);

       while(num--) {
        ret = bus_req_wait_spi();
        if (ret != 0)
          {
            printf("Failed to wait bus req:%d\n", ret);
            continue;
          }
        // printf("Detect Bus Request.(2)\n");

        read_len = spi_read(g_spi_local_buf + GHIFP_HEADER_SIZE, 512);
        while (read_len <= 0)
          {
            printf("Failed to read data0:%d\n", ret);
            read_len = spi_read(g_spi_local_buf + GHIFP_HEADER_SIZE, 512);
          }
       }

      ret = bus_req_wait_spi();
      if (ret != 0) {
        printf("Failed to wait bus req:%d\n", ret);
        continue;
      }

      read_len = spi_read(g_spi_local_buf + GHIFP_HEADER_SIZE + 512 * (int)(rec_size / 513),
                     rec_size % 512);
      while (ret != 0) {
        printf("Failed to read data:%d\n", ret);
        read_len = spi_read(g_spi_local_buf + GHIFP_HEADER_SIZE + 512 * (int)(rec_size / 513),
                     rec_size % 512);
      }

      for (i=0; i<GHIFP_DATA_SIZE(opr_len); i++){
          printf("%d ", g_spi_local_buf[GHIFP_HEADER_SIZE + i]);
        }
      printf("\n");

      /* Check data */
      ret = check_data(g_spi_local_buf + GHIFP_HEADER_SIZE, opr_len);
      if (ret != 0)
        {
          printf("Invalid data.(SPI)\n");
          continue;
        }

      if (true == is_evt_data(opc)) {
        /* OPC type -> Event, notify by callback. */
        if (g_evt_cb) {
            g_evt_cb(g_spi_local_buf, GHIFP_FRAME_SIZE(opr_len));
        }
      } else {
        /* OPC type -> Normal response, push to queue. */
        if (get_host_if_state() == HOST_IF_STATE_WAIT_RESPONSE) {
            ret = push_dataframe(g_spi_local_buf,
                                  GHIFP_FRAME_SIZE(opr_len));
            if (ret != 0) {
                printf("Failed to push dataframe.\n");
                continue;
              }
        } else {
          // printf("Discard dataframe.\n");
        }
      }

      /* Check data */
      // ret = check_data(g_spi_local_buf + GHIFP_HEADER_SIZE, opr_len);
      // if (ret != 0)
      //   {
      //     printf("Invalid data.(SPI)\n");
      //     UNLOCK();
      //     continue;
      //   }

      // if (true == is_evt_data(opc))
      //   {
      //     /* OPC type -> Event, notify by callback. */
      //     if (g_evt_cb)
      //       {
      //         g_evt_cb(g_spi_local_buf,
      //                  GHIFP_FRAME_SIZE(opr_len));
      //       }
      //   }
      // else
      //   {
      //     /* OPC type -> Normal response, push to queue. */
      //     if (get_host_if_state() == HOST_IF_STATE_WAIT_RESPONSE)
      //       {
      //         ret = push_dataframe(g_spi_local_buf,
      //                              GHIFP_FRAME_SIZE(opr_len));
      //         if (ret != 0)
      //           {
      //             printf("Failed to push dataframe.\n");
      //             UNLOCK();
      //             continue;
      //           }
      //       }
      //     else
      //       {
      //         printf("Discard dataframe.\n");
      //       }
      //   }
      // UNLOCK();
    }

  printf("Entering abnormal loop.\n");
  while (1)
    {
      sleep(3);
    }

  task_delete(0);

  return 0;
}

static int spi_write(FAR uint8_t *data, uint32_t sz)
{
  uint32_t write_len;

  if (g_spi_dfs == SPI_DEFAULT_DFS)
    {
      write_len = sz;
      SPI_EXCHANGE(g_dev, data, NULL, write_len);
      return (int)sz;
    }
  else if (g_spi_dfs == 16)
    {
      write_len = (int)(sz+1)/2;
      SPI_EXCHANGE(g_dev, data, NULL, write_len);
      return (sz%2 == 1) ? (int)(sz + 1) : (int)sz;
    }
  else
    {
      printf("Invalid dfs.\n");
      return -ENOTSUP;
    }
}

static int spi_read(FAR uint8_t *buf, uint32_t sz)
{
  uint32_t read_len;

  if (g_spi_dfs == SPI_DEFAULT_DFS)
    {
      read_len = sz;
      SPI_EXCHANGE(g_dev, NULL, buf, read_len);
      return (int)sz;
    }
  else if (g_spi_dfs == 16)
    {
      read_len = (int)(sz+1)/2;
      SPI_EXCHANGE(g_dev, NULL, buf, read_len);
      return (sz%2 == 1) ? (int)(sz + 1) : (int)sz;
    }
  else
    {
      printf("Invalid dfs.\n");
      return -ENOTSUP;
    }
}

static int spi_dummy_exchange(void)
{
  uint8_t dummy[2] = {0x0}; /* Consider 16 bit DFS. */
  uint8_t dummy_recv[2] = {0x0}; /* Consider 16 bit DFS. */

  SPI_EXCHANGE(g_dev, &dummy, &dummy_recv, 1);

  if (g_spi_dfs == SPI_DEFAULT_DFS)
    {
      printf("Dummy packet -> 0x%02X\n",
             dummy_recv[0]);
    }
  else
    {
      printf("Dummy packet -> 0x%02X, 0x%02X\n",
             dummy_recv[0], dummy_recv[1]);
    }

  return 0;
}

static FAR struct spi_dev_s *spi_dev_init(void)
{
  FAR struct spi_dev_s *dev = NULL;

  dev = cxd56_spibus_initialize(SPI5_BUS);
  if (!dev)
    {
      printf("Failed to initialize SPI.\n");
      return NULL;
    }
  CXD56_PIN_CONFIGS(PINCONFS_EMMCA_SPI5);

  /* SPI settings */

  SPI_LOCK(dev, true);
  SPI_SETMODE(dev, SPIDEV_MODE0);
  SPI_SETBITS(dev, g_spi_dfs);
  SPI_SETFREQUENCY(dev, g_spi_freq);
  SPI_LOCK(dev, false);

  return dev;
}

static int spi_dev_uninit(void)
{
  CXD56_PIN_CONFIGS(PINCONFS_EMMCA_GPIO);

  return 0;
}

static int conv_spi_dfs_number(uint8_t dfs_no)
{
  switch (dfs_no)
    {
      case DFS_SPI_8:
        return 8;
        break;
      case DFS_SPI_16:
        return 16;
        break;
      default:
        break;
    }

  return -EINVAL;
}

static int change_dfs(uint8_t dfs_number)
{
  int ret;

  ret = conv_spi_dfs_number(dfs_number);
  if (ret < 0)
    {
      return ret;
    }
  else
    {
      printf("Change Data Frame Size. %u -> %u\n", g_spi_dfs, ret);
      g_spi_dfs = ret;
      SPI_LOCK(g_dev, true);
      SPI_SETBITS(g_dev, g_spi_dfs);
      SPI_LOCK(g_dev, false);
    }

  /* No need to re-create task.
     New dfs is applied spi_recv_task. */
#if 0
  /* Restart receive task */
  ret = restart_task(g_spi_task_pid, "ghifp_spi_task", spi_recv_task, NULL);
  if (0 < ret)
    {
      g_spi_task_pid = ret;
    }
#endif

  return ret;
}

static int change_dfs_old(int dfs)
{
  if (16 <= dfs)
    {
      g_spi_dfs = 16;
    }
  else
    {
      g_spi_dfs = SPI_DEFAULT_DFS;
      printf("Restore DFS to default\n");
    }
  printf("Set SPI data frame size -> %d\n", g_spi_dfs);
  SPI_LOCK(g_dev, true);
  SPI_SETBITS(g_dev, g_spi_dfs);
  SPI_LOCK(g_dev, false);

  return 0;
}

static int32_t conv_spi_clk_number(int clk_no)
{
  printf("clk_no=%d\n", clk_no);

  switch (clk_no)
    {
      case SPEED_SPI_1300000BPS:
        return 1300000;
        break;
      case SPEED_SPI_2600000BPS:
        return 2600000;
        break;
      case SPEED_SPI_5200000BPS:
        return 5200000;
        break;
      case SPEED_SPI_6500000BPS:
        return 6500000;
        break;
      case SPEED_SPI_7800000BPS:
        return 7800000;
        break;
      case SPEED_SPI_9750000BPS:
        return 9750000;
        break;
      case SPEED_SPI_13000000BPS:
        return 13000000;
        break;
      default:
        break;
    }

  printf("Unsupported clk_no.\n");
  return -EINVAL;
}

static int set_clock(int clk_no)
{
  int     ret;
  int32_t new_clock;

  new_clock = conv_spi_clk_number(clk_no);
  if (0 < new_clock)
    {
      printf("Change SPI frequency. %u -> %d\n", g_spi_freq, new_clock);
      g_spi_freq = (uint32_t)new_clock;
      SPI_LOCK(g_dev, true);
      SPI_SETFREQUENCY(g_dev, g_spi_freq);
      SPI_LOCK(g_dev, false);
      ret = 0;
    }
  else
    {
      ret = new_clock;
    }

  return ret;
}

static int host_if_spi_write(
  FAR struct host_if_s *thiz, FAR uint8_t *data, uint32_t sz)
{
  int                 ret = -EINVAL;
  uint8_t             opc;
  uint16_t            opr_len;

  printf("host_if_spi_write() len=%d\n", sz);

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

  LOCK();
  spi_write(data, sz);
//  spi_dummy_exchange(); /* No need to send dummy. */
  UNLOCK();

  set_host_if_state(HOST_IF_STATE_WAIT_RESPONSE);

  return 0;
}

static int host_if_spi_read(
  FAR struct host_if_s *thiz, FAR uint8_t *buf, uint32_t sz)
{
  int      ret = -EINVAL;
  uint32_t res_len;

  printf("host_if_spi_read() buflen=%d\n", sz);

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

static int host_if_spi_transaction(
  FAR struct host_if_s *thiz,
  FAR uint8_t *data, uint32_t w_sz,
  FAR uint8_t *buf, uint32_t r_sz,
  FAR uint32_t *res_len)
{
  int      ret = -EINVAL;
  uint32_t df_len;
  uint8_t  opc;
  uint16_t opr_len;

  printf("SPI transaction. write len=%d, read len=%d\n", w_sz, r_sz);

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

  LOCK();
  spi_write(data, w_sz);
//  spi_dummy_exchange(); /* No need to send dummy. */
  UNLOCK();
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

static int host_if_spi_dbg_write(
  FAR struct host_if_s *thiz, FAR uint8_t *data, uint32_t sz)
{
  int ret = -EINVAL;

  printf("host_if_spi_dbg_write() len=%d\n", sz);

  if (!thiz || !data || !sz)
    {
      return ret;
    }

  if (!g_dev)
    {
      return -EPERM;
    }

  LOCK();
  spi_write(data, sz);
  UNLOCK();

  return 0;
}

static int host_if_spi_set_config(
  FAR struct host_if_s *thiz, uint32_t req, FAR void *arg)
{
  int ret = -ENOTSUP;

  if (!arg)
    {
      return -EINVAL;
    }

  switch (req)
    {
      case HOST_IF_SET_CONFIG_REQ_SETDFS:
        ret = change_dfs_old(atoi((char *)arg));
        break;
      case HOST_IF_SET_CONFIG_REQ_DBGRECV:
        g_spi_dbg_recv = atoi((char *)arg);
        printf("Debug receive mode:%s\n",
               g_spi_dbg_recv ? "ON" : "OFF");
        ret = restart_task(g_spi_task_pid, "ghifp_spi_task",
                           spi_recv_task, NULL);
        if (0 < ret)
          {
            g_spi_task_pid = ret;
          }
        break;
      case HOST_IF_SET_CONFIG_REQ_SETSPICLK:
        ret = set_clock(atoi((char *)arg));
        break;
      case HOST_IF_SET_CONFIG_REQ_SETSLVCONF:
        ret = change_dfs(*(uint8_t *)arg);
        break;
      default:
        printf("Nothing to do in SPI mode.\n");
        break;
    }
  return ret;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

FAR struct host_if_s *host_if_spi_create(hostif_evt_cb evt_cb)
{
  int ret;

  if (!evt_cb)
    {
      return NULL;
    }
  else
    {
      g_evt_cb = evt_cb;
    }

  ret = pthread_mutex_init(&g_mutex, NULL);
  if (ret < 0)
    {
      goto errout;
    }

  g_dev = spi_dev_init();
  if (!g_dev)
    {
      goto errout;
    }

  g_spi_local_buf = (FAR uint8_t *)malloc(LOCAL_BUFF_SZ);
  if (!g_spi_local_buf)
    {
      goto errout;
    }

  g_spi_task_pid = start_task("ghifp_spi_task", spi_recv_task, NULL);
  if (g_spi_task_pid < 0)
    {
      goto errout;
    }

  return &g_spi;

errout:

  if (g_spi_local_buf)
    {
      free(g_spi_local_buf);
    }
  g_spi_local_buf = NULL;

  if (g_dev)
    {
      spi_dev_uninit();
      g_dev = NULL;
    }

  pthread_mutex_destroy(&g_mutex);

  g_evt_cb = NULL;

  return NULL;
}

int host_if_spi_delete(FAR struct host_if_s *this)
{
  stop_task(g_spi_task_pid);
  g_spi_task_pid = 0;

  if (g_spi_local_buf)
    {
      free(g_spi_local_buf);
    }
  g_spi_local_buf = NULL;

  if (g_dev)
    {
      spi_dev_uninit();
    }

  pthread_mutex_destroy(&g_mutex);

  g_evt_cb = NULL;

  return 0;
}
