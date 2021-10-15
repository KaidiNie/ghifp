/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <sdk/config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <mqueue.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <semaphore.h>

#include <../../nuttx/arch/arm/src/cxd56xx/cxd56_gpio.h>
#include <../../nuttx/arch/arm/src/cxd56xx/cxd56_gpioint.h>

#include "host_if.h"
#include "host_if_bs.h"
#include "gacrux_protocol_def.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define MQUEUE_NAME    "ghifp_mq"
#define MQUEUE_MSG_MAX (8)
#define MQUEUE_MODE    0666

#define RECV_TIMEOUT_SEC (1) /* Change from 3 for spi wake up test */

#define BUS_REQ (PIN_PWM0)

 /****************************************************************************
 * Private Types
 ****************************************************************************/

struct dataframe_s
{
  uint8_t  *buf;
  uint32_t sz;
};

/****************************************************************************
 * Private Data
 ****************************************************************************/

static sem_t g_i2c_bus_req_sem;
static sem_t g_spi_bus_req_sem;

static enum host_if_state_e g_host_if_state = HOST_IF_STATE_IDLE;

/* Warning!! Interdependence. */
#include "host_if_fctry.h"
extern enum host_if_fctry_type_e g_hif_type;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static int bus_req_handler(int irq, FAR void *context, FAR void *arg)
{
  int ret;

  if (g_hif_type == HOST_IF_FCTRY_TYPE_I2C)
  {
    // printf("!!!bus!!!\n");
    ret = sem_post(&g_i2c_bus_req_sem);
    if (ret < 0)
    {
      return ret;
    }
    }

  if (g_hif_type == HOST_IF_FCTRY_TYPE_SPI)
    {

      ret = sem_post(&g_spi_bus_req_sem);
      if (ret < 0)
        {
          return ret;
        }
    }

  return 0;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int push_dataframe(FAR uint8_t *df, uint32_t df_len)
{
  int                ret;
  mqd_t              mqd;
  struct dataframe_s one_dataframe = {0};

  if (!df || !df_len)
    {
      return -EINVAL;
    }

  mqd = mq_open(MQUEUE_NAME, O_WRONLY);
  if (mqd < 0)
    {
      printf("Failed to open queue for dataframe.\n");
      return (int)mqd;
    }

  one_dataframe.buf = malloc(df_len);
  if (!one_dataframe.buf)
    {
      printf("Failed to allocate df buf.\n");
      ret = -errno;
      goto errout;
    }

  memcpy(one_dataframe.buf, df, df_len);

  one_dataframe.sz = df_len;
  ret = mq_send(mqd,
                (FAR const char*)&one_dataframe,
                sizeof(one_dataframe), 0);
  if (ret < 0)
    {
      printf("Failed to push dataframe.\n");
      goto errout;
    }

  mq_close(mqd);

  return 0;

errout:
  if (one_dataframe.buf)
    {
      free(one_dataframe.buf);
    }

  mq_close(mqd);

  return ret;
}

int pop_dataframe(FAR uint8_t *buf, uint32_t sz, FAR uint32_t *df_len)
{
  int                ret;
  mqd_t              mqd;
  struct dataframe_s one_dataframe = {0};
  struct timespec abs_time;

  if (!buf || !sz || !df_len)
    {
      return -EINVAL;
    }

  mqd = mq_open(MQUEUE_NAME, O_RDONLY);
  if (mqd < 0)
    {
      printf("Failed to open queue for dataframe.\n");
      return (int)mqd;
    }

  ret = clock_gettime(CLOCK_REALTIME, &abs_time);
  if (ret != OK)
    {
      return ret;
    }

  abs_time.tv_sec += RECV_TIMEOUT_SEC;

  ret = mq_timedreceive(mqd,
                        (FAR char*)&one_dataframe,
                        sizeof(one_dataframe), 0, &abs_time);
  if (ret < 0)
    {
      printf("Failed to pop dataframe.\n");
      goto errout;
    }

  if (one_dataframe.buf && one_dataframe.sz)
    {
      memcpy(buf, one_dataframe.buf,
             sz < one_dataframe.sz ? sz : one_dataframe.sz);
      *df_len = one_dataframe.sz;
      free(one_dataframe.buf);
    }

  mq_close(mqd);

  return 0;

errout:

  mq_close(mqd);

  return ret;
}

int create_df_queue(void)
{
  mqd_t          mqd;
  struct mq_attr mq_attr;

  mq_attr.mq_maxmsg  = MQUEUE_MSG_MAX;
  mq_attr.mq_msgsize = sizeof(struct dataframe_s);
  mq_attr.mq_flags   = 0;

  mqd = mq_open(MQUEUE_NAME, (O_RDWR | O_CREAT), MQUEUE_MODE, &mq_attr);
  if (mqd < 0)
    {
      printf("Failed to create queue for dataframe.\n");
      return (int)mqd;
    }
  mq_close(mqd);

  printf("Queue for dataframe is created.\n");

  return 0;
}

int delete_df_queue(void)
{
  mq_unlink(MQUEUE_NAME);

  return 0;
}

int start_task(FAR const char *name, main_t entry, FAR const char *argv[])
{
  int pid;

  /* Set the priority of the recv task higher than
     the priority of the main task */
  pid = task_create(name, CONFIG_EXAMPLES_GHIFP_PRIORITY + 10,
                    CONFIG_EXAMPLES_GHIFP_STACKSIZE, entry, NULL);
  if (pid < 0)
    {
      printf("Failed to start task.\n");
      return -errno;
    }

  printf("%s started. pid:%d\n", name, pid);

  return pid;
}

int stop_task(pid_t pid)
{
  task_delete(pid);

  return 0;
}

int restart_task(pid_t pid, FAR const char *name,
                 main_t entry, FAR const char *argv[])
{
  int ret;

  /* Restart task */
  if (0 < pid)
    {
      stop_task(pid);
      ret = start_task("ghifp_uart_task", entry, argv);
    }
  else
    {
      ret = -EINVAL;
    }

  printf("Pid %d -> %d\n", pid, ret);

  if (0 < ret)
    {
      printf("Restart task successfully.\n");
    }
  else
    {
      printf("Failed to restart task:%d\n", ret);
    }

  return ret;
}

int bus_req_init(void)
{
  int ret;

  ret = sem_init(&g_i2c_bus_req_sem, 0, 0);
  if (ret < 0)
    {
      printf("bus_req_init, sem_init_i2c : %d\n", ret);
      return ret;
    }

  ret = sem_init(&g_spi_bus_req_sem, 0, 0);
  if (ret < 0)
    {
      printf("bus_req_init, sem_init_spi : %d\n", ret);
      return ret;
    }

  ret = cxd56_gpioint_config(BUS_REQ,
                             GPIOINT_TOGGLE_MODE_MASK     |
                             GPIOINT_NOISE_FILTER_DISABLE |
                             GPIOINT_LEVEL_HIGH,
                             bus_req_handler,
                             NULL);
  if (ret < 0)
    {
      printf("cxd56_gpioint_config : %d\n", ret);
      return ret;
    }

  cxd56_gpioint_enable(BUS_REQ);

  return 0;
}

int bus_req_deinit(void)
{
  cxd56_gpioint_disable(BUS_REQ);

  sem_destroy(&g_i2c_bus_req_sem);
  sem_destroy(&g_spi_bus_req_sem);

  return 0;
}

int bus_req_wait_i2c(void)
{
  int ret;

  // printf("Wait for I2c Bus Request.\n");
  ret = sem_wait(&g_i2c_bus_req_sem);
  if (ret != 0)
    {
      printf("Failed to take semaphore:%d\n", ret);
    }

  return ret;
}

int bus_req_wait_spi(void)
{
  int ret;

  // printf("Wait for SPI Bus Request.\n");
  ret = sem_wait(&g_spi_bus_req_sem);
  if (ret != 0)
    {
      printf("Failed to take semaphore:%d\n", ret);
    }

  return ret;
}

int set_host_if_state(enum host_if_state_e state)
{
  g_host_if_state = state;
  printf("Host I/F state:%d\n", g_host_if_state);
  return 0;
}

enum host_if_state_e get_host_if_state(void)
{
  return g_host_if_state;
}
