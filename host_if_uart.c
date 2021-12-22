/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <sdk/config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "host_if.h"
#include "host_if_bs.h"
#include "gacrux_protocol_def.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define DEV_PATH "/dev/ttyS2"

#define UART_DEFAULT_BAUDRATE (B115200)

 /****************************************************************************
 * Private Types
 ****************************************************************************/

static int uart_recv_task(int argc, FAR char *argv[]);
static int conv_uart_speed_number(uint8_t speed_no);
static int set_baudrate(int fd, speed_t baudrate);
static int change_baudrate(uint8_t speed_number);
static int uart_open(const char *devpath, speed_t baudrate);
static int uart_close(int fd);
static int host_if_uart_write(
  FAR struct host_if_s *thiz, FAR uint8_t *data, uint32_t sz);
static int host_if_uart_read(
  FAR struct host_if_s *thiz, FAR uint8_t *buf, uint32_t sz);
static int host_if_uart_transaction(
  FAR struct host_if_s *thiz,
  FAR uint8_t *data, uint32_t w_sz,
  FAR uint8_t *buf, uint32_t r_sz,
  FAR uint32_t *res_len);
static int host_if_uart_dbg_write(
  FAR struct host_if_s *thiz, FAR uint8_t *data, uint32_t sz);
static int host_if_uart_set_config(
  FAR struct host_if_s *thiz, uint32_t req, FAR void *arg);

enum recv_state_e
{
  RECV_STATE_WAIT_FOR_HEADER,
  RECV_STATE_WAIT_FOR_DATA
};

/****************************************************************************
 * Private Data
 ****************************************************************************/

struct host_if_s g_uart = {
  .write = host_if_uart_write,
  .read = host_if_uart_read,
  .transaction = host_if_uart_transaction,
  .dbg_write = host_if_uart_dbg_write,
  .set_config = host_if_uart_set_config
};

static FAR uint8_t   *g_uart_recv_buf = NULL;
static speed_t       g_uart_baudrate = UART_DEFAULT_BAUDRATE;
static pid_t         g_uart_task_pid = 0;
static hostif_evt_cb g_evt_cb = NULL;
static int           g_uart_dbg_recv = 0;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static int uart_recv_task(int argc, FAR char *argv[])
{
  int                ret;
  int                i;
  int                fd;
  uint32_t           total_sz = 0;
  uint8_t            opc;
  uint16_t           opr_len;
  enum recv_state_e  recv_stat = RECV_STATE_WAIT_FOR_HEADER;

  memset(g_uart_recv_buf, 0, LOCAL_BUFF_SZ);

  fd = uart_open(DEV_PATH, g_uart_baudrate);
  if (fd < 0)
    {
      return fd;
    }

  while (1)
    {
      if (g_uart_dbg_recv != 0)
        {
          ret = read(fd, g_uart_recv_buf,
                     LOCAL_BUFF_SZ);
          if (ret <= 0)
            {
              printf("Failed to read:%d\n", ret);
              continue;
            }
          printf("Data dump.\n");
          for (i=0; i<ret; i++)
            {
              printf("%02X ", g_uart_recv_buf[i]);
            }
          printf("\n");
          continue;
        }
      else
        {
          ret = read(fd, g_uart_recv_buf + total_sz,
                    LOCAL_BUFF_SZ - total_sz);
          if (ret <= 0)
            {
              printf("Failed to read:%d\n", ret);
              continue;
            }
        }

#if 1 /* kanamori debug */
      printf("read len:%d\n", ret);
      for (i=total_sz; i<total_sz+ret; i++)
        {
          printf("%02X ", g_uart_recv_buf[i]);
        }
      printf("\n");
#endif

      total_sz += ret;

      if (recv_stat == RECV_STATE_WAIT_FOR_HEADER)
        {
          if (GHIFP_HEADER_SIZE <= total_sz)
            {
              ret = check_header(g_uart_recv_buf, &opc, &opr_len);
              if (ret != 0)
                {
                  printf("Invalid header.(UART)\n");
                  total_sz = 0;
                  continue;
                }
              recv_stat = RECV_STATE_WAIT_FOR_DATA;
            }
        }

      if (recv_stat == RECV_STATE_WAIT_FOR_DATA)
        {
          if (GHIFP_FRAME_SIZE(opr_len) <= total_sz)
            {
              ret = check_data(g_uart_recv_buf + GHIFP_HEADER_SIZE,
                               opr_len);
              if (ret != 0)
                {
                  printf("Invalid data.(UART)\n");
                  total_sz = 0;
                  recv_stat = RECV_STATE_WAIT_FOR_HEADER;
                  continue;
                }

              printf("Received dataframe completely. total_sz:%lu\n",
                     total_sz);

              /* Completed to receive dataframe. */

              if (true == is_evt_data(opc))
                {
                  /* OPC type -> Event, notify by callback. */
                  if (g_evt_cb)
                    {
                      g_evt_cb(g_uart_recv_buf,
                               GHIFP_FRAME_SIZE(opr_len));
                    }
                }
              else
                {
                  /* OPC type -> Normal response, push to queue. */
                  if (get_host_if_state() == HOST_IF_STATE_WAIT_RESPONSE)
                    {
                      ret = push_dataframe(g_uart_recv_buf,
                                           GHIFP_FRAME_SIZE(opr_len));
                      if (ret != 0)
                        {
                          printf("Failed to send dataframe.\n");
                          continue;
                        }
                    }
                  else
                    {
                      printf("Discard dataframe.\n");
                    }
                }

              total_sz -= GHIFP_FRAME_SIZE(opr_len);
              memmove(g_uart_recv_buf,
                      g_uart_recv_buf+GHIFP_FRAME_SIZE(opr_len),
                      LOCAL_BUFF_SZ - GHIFP_FRAME_SIZE(opr_len));
              opc = 0;
              opr_len = 0;
              recv_stat = RECV_STATE_WAIT_FOR_HEADER;
            }
        }
    }

  uart_close(fd);

  printf("Entering abnormal loop.\n");
  while (1)
    {
      sleep(3);
    }

  task_delete(0);

  return 0;
}

static int conv_uart_speed_number(uint8_t speed_no)
{
  switch (speed_no)
    {
      case SPEED_UART_4800BPS:
        return B4800;
        break;
      case SPEED_UART_9600BPS:
        return B9600;
        break;
      case SPEED_UART_14400BPS:
        return -ENOTSUP;
        break;
      case SPEED_UART_19200BPS:
        return B19200;
        break;
      case SPEED_UART_38400BPS:
        return B38400;
        break;
      case SPEED_UART_57600BPS:
        return B57600;
        break;
      case SPEED_UART_115200BPS:
        return B115200;
        break;
      case SPEED_UART_230400BPS:
        return B230400;
        break;
      case SPEED_UART_460800BPS:
        return B460800;
        break;
      case SPEED_UART_921600BPS:
        return B921600;
        break;
      case SPEED_UART_1000000BPS:
        return B1000000;
        break;
      default:
        break;
    }

  return -EINVAL;
}

static int set_baudrate(int fd, speed_t baudrate)
{
  int            ret;
  struct termios tio = {0};

  ret = tcgetattr(fd, &tio);
  if (ret < 0)
    {
      return ret;
    }

  printf("baudrate:%u\n", baudrate);

  tio.c_cflag += CREAD;  /* Enable receive */
  tio.c_cflag += CLOCAL; /* Local line, no modem control */
  tio.c_cflag += CS8;    /* Data bit 8bit */
  tio.c_cflag += 0;      /* Stop bit 1bit */
  tio.c_cflag += 0;      /* Paritiy none */
  cfsetispeed(&tio, baudrate);
  cfsetospeed(&tio, baudrate);

  /* tty: set to tty device */

  tcsetattr(fd, TCSAFLUSH, &tio);

  /* tty: Enable settings */

  ret = ioctl(fd, TCSETS, (unsigned long)&tio);
  printf("UART ioctl(TCSETS). ret=%d\n", ret);

  return ret;
}

static int change_baudrate(uint8_t speed_number)
{
  int ret;

  ret = conv_uart_speed_number(speed_number);
  if (ret < 0)
    {
      return ret;
    }
  else
    {
      printf("Change baudrate. %u -> %u\n", g_uart_baudrate, (speed_t)ret);
      g_uart_baudrate = (speed_t)ret;
    }

  /* Apply new baudrate */
  ret = uart_open(DEV_PATH, g_uart_baudrate);
  if (0 < ret)
    {
      uart_close(ret);
    }

  /* No need to re-create task.
     New baudrate is applied uart_recv_task. */
#if 0
  /* Restart receive task */
  ret = restart_task(g_uart_task_pid, "ghifp_uart_task",
                     uart_recv_task, NULL);
  if (0 < ret)
    {
      g_uart_task_pid = ret;
    }
#endif

  return ret;
}

static int uart_open(const char *devpath, speed_t baudrate)
{
  int ret;
  int fd;

  fd = open(DEV_PATH, O_RDWR);
  if (fd < 0)
    {
      return -errno;
    }

  ret = set_baudrate(fd, baudrate);
  if (ret < 0)
    {
      close(fd);
      fd = -1;
      return -errno;
    }

  return ret < 0 ? ret : fd;
}

static int uart_close(int fd)
{
  return close(fd);
}

static int host_if_uart_write(
  FAR struct host_if_s *thiz, FAR uint8_t *data, uint32_t sz)
{
  int      ret = -EINVAL;
  int      fd;
  uint8_t  opc;
  uint16_t opr_len;
  uint32_t total_sz = 0;

  printf("host_if_uart_write() len=%ld\n", sz);

  if (!thiz || !data || !sz)
    {
      return ret;
    }

  if (0 != check_header(data, &opc, &opr_len))
    {
      return ret;
    }

  if (0 != check_data(data + GHIFP_HEADER_SIZE, opr_len))
    {
      return ret;
    }

  fd = uart_open(DEV_PATH, g_uart_baudrate);
  if (fd < 0)
    {
      return fd;
    }

  while (total_sz < sz)
    {
      ret = write(fd, data + total_sz, (size_t)sz - total_sz);
      if (ret < 0)
        {
          printf("UART write error:%d\n", ret);
          break;
        }
      total_sz += ret;
    }

  uart_close(fd);

  if (0 < ret)
    {
      set_host_if_state(HOST_IF_STATE_WAIT_RESPONSE);
      return total_sz;
    }
  else
    {
      return ret;
    }
}

static int host_if_uart_read(
  FAR struct host_if_s *thiz, FAR uint8_t *buf, uint32_t sz)
{
  int      ret = -EINVAL;
  uint32_t res_len;

  printf("host_if_uart_read() buflen=%ld\n", sz);

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
      printf("Response dataframe len:%ld\n", res_len);
      return res_len;
    }
}

static int host_if_uart_transaction(
  FAR struct host_if_s *thiz,
  FAR uint8_t *data, uint32_t w_sz,
  FAR uint8_t *buf, uint32_t r_sz,
  FAR uint32_t *res_len)
{
  int      ret = -EINVAL;
  int      fd;
  uint32_t df_len;
  uint8_t  opc;
  uint16_t opr_len;
  uint32_t total_sz = 0;

  printf("UART transaction. write len=%ld, read len=%ld\n", w_sz, r_sz);

  if (!thiz || !data || !w_sz || !buf || !r_sz || !res_len)
    {
      return ret;
    }

  if (0 != check_header(data, &opc, &opr_len))
    {
      return ret;
    }

  if (0 != check_data(data + GHIFP_HEADER_SIZE, opr_len))
    {
      return ret;
    }

  fd = uart_open(DEV_PATH, g_uart_baudrate);
  if (fd < 0)
    {
      return fd;
    }

  while (total_sz < w_sz)
    {
      ret = write(fd, data + total_sz, (size_t)w_sz - total_sz);
      if (ret < 0)
        {
          printf("UART write error:%d\n", ret);
          goto exit;
        }
      total_sz += ret;
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
      printf("Response dataframe len:%ld\n", df_len);
      *res_len = df_len;
    }

exit:
  uart_close(fd);

  return ret;
}

static int host_if_uart_dbg_write(
  FAR struct host_if_s *thiz, FAR uint8_t *data, uint32_t sz)
{
  int      ret = -EINVAL;
  int      fd;
  uint32_t total_sz = 0;

  printf("host_if_uart_dbg_write() len=%ld\n", sz);

  if (!thiz || !data || !sz)
    {
      return ret;
    }

  fd = uart_open(DEV_PATH, g_uart_baudrate);
  if (fd < 0)
    {
      return fd;
    }

  while (total_sz < sz)
    {
      ret = write(fd, data + total_sz, (size_t)sz - total_sz);
      if (ret < 0)
        {
          printf("UART write error:%d\n", ret);
          break;
        }
      total_sz += ret;
    }

  uart_close(fd);

  return ret < 0 ? ret : total_sz;
}

static int host_if_uart_set_config(
  FAR struct host_if_s *thiz, uint32_t req, FAR void *arg)
{
  int ret = -ENOTSUP;

  if (!arg)
    {
      return -EINVAL;
    }

  switch (req)
    {
      case HOST_IF_SET_CONFIG_REQ_DBGRECV:
        g_uart_dbg_recv = atoi((char *)arg);
        printf("Debug receive mode:%s\n",
               g_uart_dbg_recv ? "ON" : "OFF");
        ret = restart_task(g_uart_task_pid, "ghifp_uart_task",
                           uart_recv_task, NULL);
        if (0 < ret)
          {
            g_uart_task_pid = ret;
          }
        break;
      case HOST_IF_SET_CONFIG_REQ_SETSLVCONF:
        ret = change_baudrate(*(uint8_t *)arg);
        break;
      default:
        printf("Nothing to do in UART mode.\n");
        break;
    }
  return ret;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

FAR struct host_if_s *host_if_uart_create(hostif_evt_cb evt_cb)
{
  if (!evt_cb)
    {
      goto errout;
    }
  else
    {
      g_evt_cb = evt_cb;
    }

  g_uart_recv_buf = (FAR uint8_t *)malloc(LOCAL_BUFF_SZ);
  if (!g_uart_recv_buf)
    {
      goto errout;
    }

  g_uart_task_pid = start_task("ghifp_uart_task", uart_recv_task, NULL);
  if (g_uart_task_pid < 0)
    {
      goto errout;
    }

  return &g_uart;

errout:

  if (g_uart_recv_buf)
    {
      free(g_uart_recv_buf);
    }
  g_uart_recv_buf = NULL;

  return NULL;
}

int host_if_uart_delete(FAR struct host_if_s *this)
{
  stop_task(g_uart_task_pid);
  g_uart_task_pid = 0;

  if (g_uart_recv_buf)
    {
      free(g_uart_recv_buf);
    }
  g_uart_recv_buf = NULL;

  g_evt_cb = NULL;

  return 0;
}
