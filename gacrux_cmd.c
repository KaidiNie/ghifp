/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "gacrux_cmd.h"
#include "host_if.h"
#include "host_if_fctry.h"
#include "gacrux_protocol_def.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Internal definition */

#define RECV_BUFF_SZ (4096)

#define EVT_TIMEOUT_SEC (3)

#define TX_FW_ONE_PACKET_SZ (2048)
#define BIN_INPUT_ONE_PACKET_SZ (500)

/* Function macro */

#define CHECKINIT()                         \
do                                          \
  {                                         \
    if (g_is_cmd_init == false)             \
      {                                     \
        printf("Not initialized...\n");     \
        return -EPERM;                      \
      }                                     \
  } while(0)

#define CHECKUNINIT()                       \
do                                          \
  {                                         \
    if (g_is_cmd_init == true)              \
      {                                     \
        printf("Already initialized...\n"); \
        return -EPERM;                      \
      }                                     \
  } while(0)

                                            /* If correct total size is 4, */
#define TOTAL_PKT_NUM_TYPE_NORMAL       (0) /* 4, 4, 4, 4      */
#define TOTAL_PKT_NUM_TYPE_ZERO         (1) /* 0, 0, 0, 0      */
#define TOTAL_PKT_NUM_TYPE_INVALID      (2) /* 5, 5, 5, 5      */
#define TOTAL_PKT_NUM_TYPE_SHIFT_PLUS   (3) /* 4, 4, 3, 3      */
#define TOTAL_PKT_NUM_TYPE_SHIFT_MINUS  (4) /* 4, 4, 5, 5      */
#define TOTAL_PKT_NUM_TYPE_MAX          (TOTAL_PKT_NUM_TYPE_SHIFT_MINUS)

#define PKT_NO_TYPE_NORMAL              (0) /* 1, 2, 3, 4      */
#define PKT_NO_TYPE_INCREMENT_FROM_ZERO (1) /* 0, 1, 2, 3      */
#define PKT_NO_TYPE_NO_INCREMENT        (2) /* 1, 1, 1, 1      */
#define PKT_NO_TYPE_OVER_INCREMENT      (3) /* 1, 3, 5, 7      */
#define PKT_NO_TYPE_OVER_TOTAL_NUM      (4) /* 1, 2, 3, 4, "5" */
#define PKT_NO_TYPE_MAX                 (PKT_NO_TYPE_OVER_TOTAL_NUM)

 /****************************************************************************
 * Private Types
 ****************************************************************************/

enum gacrux_status_e {
  GACRUX_STATUS_IDLE_ROM,
  GACRUX_STATUS_IDLE,
  GACRUX_STATUS_DEEPSLEEP,
};

/****************************************************************************
 * Private Data
 ****************************************************************************/

static bool                      g_is_cmd_init = false;
enum host_if_fctry_type_e        g_hif_type = HOST_IF_FCTRY_TYPE_UART;
static enum gacrux_status_e      g_gacrux_state = GACRUX_STATUS_IDLE;

static FAR uint8_t               *g_recv_buff = NULL;
static sem_t                     g_chgstat_evt_sem;

static uint16_t                  g_tx_fw_one_packet_sz = TX_FW_ONE_PACKET_SZ;
static uint16_t                  g_bin_input_one_packet_sz = BIN_INPUT_ONE_PACKET_SZ;
/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static int str_to_bin(char *str, uint32_t str_len,
                      uint8_t *binbuff, uint32_t binbufflen);
static int calc_file_sz(FAR const char *file_path);
static int wait_chgstat_evt(void);
static int chgstat_cmd_create(uint8_t *buf, uint32_t buf_len, int stat);
static int chgstat_evt_hander(uint8_t notification);
static int tx_fw_res_check(uint8_t *res, uint32_t res_len);
static int execute_fw_cmd_create(uint8_t *buf, uint32_t buf_len);
static int execute_fw_res_check(uint8_t *res, uint32_t res_len);
static int uartconf_cmd_create(uint8_t *buf, uint32_t buf_len,
                               uint8_t baudrate, uint8_t flow_ctrl);
static int uartconf_res_check(uint8_t *res, uint32_t res_len);
static int i2cconf_cmd_create(uint8_t *buf, uint32_t buf_len,
                              uint8_t speed);
static int cmd_create(uint8_t *buf, uint8_t bin_cmd, uint8_t *opr,
                               uint16_t opr_len);
static int i2cconf_res_check(uint8_t *res, uint32_t res_len);
static int spiconf_cmd_create(uint8_t *buf, uint32_t buf_len,
                              uint8_t dfs);
static int spiconf_res_check(uint8_t *res, uint32_t res_len);
static void gacrux_cmd_evt_handler(FAR uint8_t *dataframe, int32_t len);
static int bin_input_res_check(uint8_t *res, uint32_t res_len);

static int8_t OprLength[160] = {
    [0x00] = 1, [0x01] = -1, [0x02] = 0, [0x03] = 2, [0x04] = 1,
    [0x05] = 1, [0x06] = 0, [0x07] = -1, [0x08] = 3, [0x09] = 3,
    [0x0a] = -99, [0x0b] = 2, [0x0c] = -99, [0x0d] = -99, [0x0e] = -99,
    [0x0f] = -99,
    [0x10] = 1, [0x11] = 4, [0x12] = 2, [0x13] = 0, [0x14] = 0,
    [0x15] = 1, [0x16] = 2, [0x17] = 4, [0x18] = -99, [0x19] = -99,
    [0x1a] = -99, [0x1b] = -99, [0x1c] = -99, [0x1d] = -99, [0x1e] = -99,
    [0x1f] = -99,
    [0x20] = -99, [0x21] = -99, [0x22] = -99, [0x23] = -99, [0x24] = -99,
    [0x25] = -99, [0x26] = -99, [0x27] = -99, [0x28] = -99, [0x29] = -99,
    [0x2a] = -99, [0x2b] = -99, [0x2c] = -99, [0x2d] = -99, [0x2e] = -99,
    [0x2f] = -99,
    [0x30] = 1, [0x31] = 0, [0x32] = 2, [0x33] = 0, [0x34] = -99,
    [0x35] = 12, [0x36] = 8, [0x37] = 4, [0x38] = 0, [0x39] = 2,
    [0x3a] = 7, [0x3b] = 0, [0x3c] = 3, [0x3d] = 5, [0x3e] = 1,
    [0x3f] = 1,
    [0x40] = 3, [0x41] = 1, [0x42] = 11, [0x43] = 0, [0x44] = 6,
    [0x45] = -99, [0x46] = -99, [0x47] = -99, [0x48] = -99, [0x49] = -99,
    [0x4a] = -99, [0x4b] = -99, [0x4c] = -99, [0x4d] = -99, [0x4e] = -99,
    [0x4f] = -99,
    [0x50] = 1, [0x51] = 0, [0x52] = 1, [0x53] = 1, [0x54] = 0,
    [0x55] = 1, [0x56] = 2, [0x57] = -99, [0x58] = -99, [0x59] = -99,
    [0x5a] = -99, [0x5b] = -99, [0x5c] = -99, [0x5d] = -99, [0x5e] = -99,
    [0x5f] = -99,
    [0x60] = -99, [0x61] = -99, [0x62] = -99, [0x63] = -99, [0x64] = -99,
    [0x65] = -99, [0x66] = -99, [0x67] = -99, [0x68] = -99, [0x69] = -99,
    [0x6a] = -99, [0x6b] = -99, [0x6c] = -99, [0x6d] = -99, [0x6e] = -99,
    [0x6f] = -99,
    [0x70] = -99, [0x71] = -99, [0x72] = -99, [0x73] = -99, [0x74] = -99,
    [0x75] = -99, [0x76] = -99, [0x77] = -99, [0x78] = -99, [0x79] = -99,
    [0x7a] = -99, [0x7b] = -99, [0x7c] = -99, [0x7d] = -99, [0x7e] = -99,
    [0x7f] = -99,
    [0x80] = 8, [0x81] = 22, [0x82] = 15, [0x83] = -1, [0x84] = -1,
    [0x85] = 45, [0x86] = 33, [0x87] = 1, [0x88] = -1, [0x89] = -99,
    [0x9a] = -99, [0x9b] = -99, [0x9c] = -99, [0x9d] = -99, [0x9e] = -99,
    [0x9f] = -99,
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static int str_to_bin(char *str, uint32_t str_len,
                      uint8_t *binbuff, uint32_t binbufflen)
{
  int  i, j;
  char tmp[3] = {0};
  int str_to_bin_len = ((str_len%2 == 1) ? (str_len+1)/2 : str_len/2);

  if (binbufflen < str_to_bin_len)
    {
      printf("Buffer is too small. str:%d,bin:%d\n", str_len, binbufflen);
      return -1;
    }

  for (i=0, j=0; i<str_len; i+=2, j++)
    {
      tmp[0] = str[i];
      if (str_len%2 ==1 && i+1 == str_len)
        {
          printf("Padding character 'F'.");
          tmp[1] = 'F';
        }
      else
        {
          tmp[1] = str[i+1];
        }
      binbuff[j] = strtol((const char *)tmp, 0, 16);
    }

  return 0;
}

static int calc_file_sz(FAR const char *file_path)
{
  int   fd;
  off_t file_sz;

  if (!file_path)
    {
      return -EINVAL;
    }

  fd = open(file_path, O_RDONLY);
  if (fd < 0)
    {
      printf("Failed to open file.\n");
      return -errno;
    }

  file_sz = lseek(fd, 0, SEEK_END); /* Calc file size. */
  if (0 < file_sz)
    {
      printf("File size:%u\n", file_sz);
    }

  close(fd);

  return file_sz;
}

static int wait_chgstat_evt(void)
{
  int             ret;
  struct timespec abs_time;
  int             errval;

  ret = clock_gettime(CLOCK_REALTIME, &abs_time);
  if (ret != OK)
    {
      return ret;
    }

  abs_time.tv_sec += EVT_TIMEOUT_SEC;

  printf("Wait chgstat evt.\n");
  ret = sem_timedwait(&g_chgstat_evt_sem, &abs_time);
  if (ret < 0)
    {
      errval = -errno;
      printf("Cannot detect chgstat evt:%d\n", errval);
    }

  return ret;
}

static int chgstat_cmd_create(uint8_t *buf, uint32_t buf_len, int stat)
{
  if (!buf || buf_len < CHGSTAT_CMD_SIZE)
    {
      return -EINVAL;
    }

  buf[GHIFP_SYNC_OFFSET]       = GHIFP_SYNC;
  *(uint16_t *)&buf[GHIFP_OPR_LEN_OFFSET] = CHGSTAT_OPR_SIZE; /* Set OPC len by little endian. */
  buf[GHIFP_OPC_OFFSET]        = CHGSTAT_OPC;
  buf[GHIFP_H_CHECKSUM_OFFSET] = calc_checksum(buf, 4);
  buf[GHIFP_OPR_OFFSET]        = (uint8_t)stat;
  buf[GHIFP_D_CHECKSUM_OFFSET(CHGSTAT_OPR_SIZE)]
                               = calc_checksum(&buf[GHIFP_OPR_OFFSET],
                                               CHGSTAT_OPR_SIZE);

  return 0;
}

static int chgstat_evt_hander(uint8_t notification)
{
  int ret = 0;

  printf("Detect Gacrux system state change:%d\n", notification);

  switch (notification)
    {
      case CHGSTAT_OPR_ROM_RESTART:
        printf("Event -> Restart(ROM)\n");
        g_gacrux_state = GACRUX_STATUS_IDLE_ROM;
        break;
      case CHGSTAT_OPR_RESTART:
        printf("Event -> Restart(FW)\n");
        g_gacrux_state = GACRUX_STATUS_IDLE;
        break;
      case CHGSTAT_OPR_WAKEUP:
        printf("Event -> Wake up\n");
        g_gacrux_state = GACRUX_STATUS_IDLE;
        break;
      case CHGSTAT_OPR_SUSPEND:
        printf("Event -> Deep sleep\n");
        g_gacrux_state = GACRUX_STATUS_DEEPSLEEP;
        break;
      case CHGSTAT_OPR_BROKEN_DOWM:
        printf("Event -> Broken down\n");
        g_gacrux_state = GACRUX_STATUS_IDLE_ROM;
        break;
      default:
        printf("Unexpected chgstat opr:%d\n", notification);
        ret = -EIO;
        break;
    }

  sem_post(&g_chgstat_evt_sem);

  return ret;
}

static int tx_fw_res_check(uint8_t *res, uint32_t res_len)
{
  int ret = 0;

  if (res_len == TXFW_RES_SIZE)
    {
      if (res[GHIFP_OPC_OFFSET] != TXFW_OPC)
        {
          /* OPC check */
          printf("Unexpected OPC:0x%02X\n", res[GHIFP_OPC_OFFSET]);
          ret = -EIO;
          goto errout;
        }

      if (res[GHIFP_OPR_OFFSET] != GHIFP_STDERR_OK)
        {
          /* Error code check */
          printf("OPR error:%d\n", (int8_t)res[GHIFP_OPR_OFFSET]);
          ret = (int8_t)-res[GHIFP_OPR_OFFSET];
          goto errout;
        }

      printf("Transmit FW result:%d\n", res[GHIFP_OPR_OFFSET]);
    }
  else
    {
      printf("Unexpected res_len:%d\n", res_len);
      ret = -EIO;
      goto errout;
    }

errout:
  return ret;
}

static int bin_input_res_check(uint8_t *res, uint32_t res_len)
{
  int ret = 0;

  if (res_len == 4+1+4+1)
    {
      if (res[GHIFP_OPC_OFFSET] != 0x07)
        {
          /* OPC check */
          printf("Unexpected OPC:0x%02X\n", res[GHIFP_OPC_OFFSET]);
          ret = -EIO;
          goto errout;
        }

      if (res[GHIFP_OPR_OFFSET] != GHIFP_STDERR_OK)
        {
          /* Error code check */
          printf("OPR error:%d\n", (int8_t)res[GHIFP_OPR_OFFSET]);
          ret = (int8_t)-res[GHIFP_OPR_OFFSET];
          goto errout;
        }

      printf("Transmit FW result:%d\n", res[GHIFP_OPR_OFFSET]);
    }
  else
    {
      printf("Unexpected res_len:%d\n", res_len);
      ret = -EIO;
      goto errout;
    }

errout:
  return ret;
}

static int execute_fw_cmd_create(uint8_t *buf, uint32_t buf_len)
{
  if (!buf || buf_len < EXECFW_CMD_SIZE)
    {
      return -EINVAL;
    }

  buf[GHIFP_SYNC_OFFSET] = GHIFP_SYNC;
  *(uint16_t *)&buf[GHIFP_OPR_LEN_OFFSET] = EXECFW_OPR_SIZE; /* Set OPC len by little endian. */
  buf[GHIFP_OPC_OFFSET] = EXECFW_OPC;
  buf[GHIFP_H_CHECKSUM_OFFSET] = calc_checksum(buf, 4);

  return 0;
}

static int execute_fw_res_check(uint8_t *res, uint32_t res_len)
{
  int ret = 0;

  if (res_len == EXECFW_RES_SIZE)
    {
      if (res[GHIFP_OPC_OFFSET] != EXECFW_OPC)
        {
          /* OPC check */
          printf("Unexpected OPC:0x%02X\n", res[GHIFP_OPC_OFFSET]);
          ret = -EIO;
          goto errout;
        }

      if (res[GHIFP_OPR_OFFSET] != GHIFP_STDERR_OK)
        {
          /* Error code check */
          printf("OPR error:%d\n", (int8_t)res[GHIFP_OPR_OFFSET]);
          ret = (int8_t)-res[GHIFP_OPR_OFFSET];
          goto errout;
        }

      printf("Execute FW result:%d\n", res[GHIFP_OPR_OFFSET]);
    }
  else
    {
      printf("Unexpected res_len:%d\n", res_len);
      ret = -EIO;
      goto errout;
    }

errout:
  return ret;
}

static int uartconf_cmd_create(uint8_t *buf, uint32_t buf_len,
                               uint8_t baudrate, uint8_t flow_ctrl)
{
  if (!buf || buf_len < UARTCONF_CMD_SIZE)
    {
      return -EINVAL;
    }

  buf[GHIFP_SYNC_OFFSET]       = GHIFP_SYNC;
  *(uint16_t *)&buf[GHIFP_OPR_LEN_OFFSET] = UARTCONF_OPR_SIZE; /* Set OPC len by little endian. */
  buf[GHIFP_OPC_OFFSET]        = UARTCONF_OPC;
  buf[GHIFP_H_CHECKSUM_OFFSET] = calc_checksum(buf, 4);
  buf[GHIFP_OPR_OFFSET]        = baudrate;
  buf[GHIFP_OPR_OFFSET + 1]    = flow_ctrl;
  buf[GHIFP_D_CHECKSUM_OFFSET(UARTCONF_OPR_SIZE)]
                               = calc_checksum(&buf[GHIFP_OPR_OFFSET],
                                               UARTCONF_OPR_SIZE);

  return 0;
}

static int uartconf_res_check(uint8_t *res, uint32_t res_len)
{
  int ret = 0;

  if (res_len == UARTCONF_RES_SIZE)
    {
      if (res[GHIFP_OPC_OFFSET] != UARTCONF_OPC)
        {
          /* OPC check */
          printf("Unexpected OPC:0x%02X\n", res[GHIFP_OPC_OFFSET]);
          ret = -EIO;
          goto errout;
        }

      if (res[GHIFP_OPR_OFFSET] != GHIFP_STDERR_OK)
        {
          /* Error code check */
          printf("OPR error:%d\n", (int8_t)res[GHIFP_OPR_OFFSET]);
          ret = (int8_t)-res[GHIFP_OPR_OFFSET];
          goto errout;
        }

      printf("Change UART configuration result:%d\n", res[GHIFP_OPR_OFFSET]);
    }
  else
    {
      printf("Unexpected res_len:%d\n", res_len);
      ret = -EIO;
      goto errout;
    }

errout:
  return ret;
}

static int i2cconf_cmd_create(uint8_t *buf, uint32_t buf_len,
                              uint8_t speed)
{
  if (!buf || buf_len < I2CCONF_CMD_SIZE)
    {
      return -EINVAL;
    }

  buf[GHIFP_SYNC_OFFSET]       = GHIFP_SYNC;
  *(uint16_t *)&buf[GHIFP_OPR_LEN_OFFSET] = I2CCONF_OPR_SIZE; /* Set OPC len by little endian. */
  buf[GHIFP_OPC_OFFSET]        = I2CCONF_OPC;
  buf[GHIFP_H_CHECKSUM_OFFSET] = calc_checksum(buf, 4);
  buf[GHIFP_OPR_OFFSET]        = speed;
  buf[GHIFP_D_CHECKSUM_OFFSET(I2CCONF_OPR_SIZE)]
                               = calc_checksum(&buf[GHIFP_OPR_OFFSET],
                                               I2CCONF_OPR_SIZE);

  return 0;
}

static int cmd_create(uint8_t *buf, uint8_t bin_cmd, uint8_t *opr,
                                uint16_t opr_len)
{
  // if (!buf || (OprLength[bin_cmd] == -99))
  //   {
  //     return -EINVAL;
  //   }

  buf[GHIFP_SYNC_OFFSET]       = GHIFP_SYNC;
  *(uint16_t *)&buf[GHIFP_OPR_LEN_OFFSET] = opr_len;
  buf[GHIFP_OPC_OFFSET]        = bin_cmd;
  buf[GHIFP_H_CHECKSUM_OFFSET] = calc_checksum(buf, 4);

  if (opr_len > 0) {
    for (int i = 0; i < opr_len; i++) {
      buf[GHIFP_OPR_OFFSET + i] = opr[i];
      // printf("--- opr[%d] = %d ---\n", i, opr[i]);
    }
    buf[GHIFP_D_CHECKSUM_OFFSET(opr_len)]
                              = calc_checksum(&buf[GHIFP_OPR_OFFSET], opr_len);
  }
  return 0;
}

static int i2cconf_res_check(uint8_t *res, uint32_t res_len)
{
  int ret = 0;

  if (res_len == I2CCONF_RES_SIZE)
    {
      if (res[GHIFP_OPC_OFFSET] != I2CCONF_OPC)
        {
          /* OPC check */
          printf("Unexpected OPC:0x%02X\n", res[GHIFP_OPC_OFFSET]);
          ret = -EIO;
          goto errout;
        }

      if (res[GHIFP_OPR_OFFSET] != GHIFP_STDERR_OK)
        {
          /* Error code check */
          printf("OPR error:%d\n", (int8_t)res[GHIFP_OPR_OFFSET]);
          ret = (int8_t)-res[GHIFP_OPR_OFFSET];
          goto errout;
        }

      printf("Change I2C configuration result:%d\n", res[GHIFP_OPR_OFFSET]);
    }
  else
    {
      printf("Unexpected res_len:%d\n", res_len);
      ret = -EIO;
      goto errout;
    }

errout:
  return ret;
}

static int spiconf_cmd_create(uint8_t *buf, uint32_t buf_len,
                              uint8_t dfs)
{
  if (!buf || buf_len < SPICONF_CMD_SIZE)
    {
      return -EINVAL;
    }

  buf[GHIFP_SYNC_OFFSET]       = GHIFP_SYNC;
  *(uint16_t *)&buf[GHIFP_OPR_LEN_OFFSET] = SPICONF_OPR_SIZE; /* Set OPC len by little endian. */
  buf[GHIFP_OPC_OFFSET]        = SPICONF_OPC;
  buf[GHIFP_H_CHECKSUM_OFFSET] = calc_checksum(buf, 4);
  buf[GHIFP_OPR_OFFSET]        = dfs;
  buf[GHIFP_D_CHECKSUM_OFFSET(SPICONF_OPR_SIZE)]
                               = calc_checksum(&buf[GHIFP_OPR_OFFSET],
                                               SPICONF_OPR_SIZE);

  return 0;
}

static int spiconf_res_check(uint8_t *res, uint32_t res_len)
{
  int ret = 0;

  if (res_len == SPICONF_RES_SIZE)
    {
      if (res[GHIFP_OPC_OFFSET] != SPICONF_OPC)
        {
          /* OPC check */
          printf("Unexpected OPC:0x%02X\n", res[GHIFP_OPC_OFFSET]);
          ret = -EIO;
          goto errout;
        }

      if (res[GHIFP_OPR_OFFSET] != GHIFP_STDERR_OK)
        {
          /* Error code check */
          printf("OPR error:%d\n", (int8_t)res[GHIFP_OPR_OFFSET]);
          ret = (int8_t)-res[GHIFP_OPR_OFFSET];
          goto errout;
        }

      printf("Change SPI configuration result:%d\n", res[GHIFP_OPR_OFFSET]);
    }
  else
    {
      printf("Unexpected res_len:%d\n", res_len);
      ret = -EIO;
      goto errout;
    }

errout:
  return ret;
}

static void gacrux_cmd_evt_handler(FAR uint8_t *dataframe, int32_t len)
{
  int      ret;
  uint8_t  opc;
  uint16_t opr_len;

  printf("Get event.\n");

  if (!dataframe || len < GHIFP_HEADER_SIZE)
    {
      return;
    }

  ret = check_header(dataframe, &opc, &opr_len);
  if (ret != 0)
    {
      return;
    }
  printf("opc:0x%02X\n", opc);

  ret = check_data(dataframe + GHIFP_HEADER_SIZE, opr_len);
  if (ret != 0)
    {
      return;
    }

  switch (opc)
    {
      // case CHGSTAT_OPC:
      //   if (opr_len < CHGSTAT_RES_SIZE)
      //     {
      //       chgstat_evt_hander(dataframe[GHIFP_OPR_OFFSET]);
      //     }
      //   else
      //     {
      //       printf("Unexpected opr_len:%d\n", opr_len);
      //     }
      //   break;
      case FRAMECHKERR_OPC:
        printf("Data frame error.\n");
        if (opr_len == FRAMECHKERR_OPR_SIZE)
          {
            printf("Error notification, OPC      :0x%02X\n",
              dataframe[FRAMECHKERR_OPR1_OFFSET]);
            printf("Error notification, ErrorCode:0x%02X\n",
              dataframe[FRAMECHKERR_OPR2_OFFSET]);
          }
        else
          {
            printf("Unexpected opr_len:%d\n", opr_len);
          }
        break;
      default:
        break;
    }
  return;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int gacrux_cmd_init(void)
{
  int ret;

  CHECKUNINIT();

  ret = sem_init(&g_chgstat_evt_sem, 0, 0);
  if (ret < 0)
    {
      printf("sem_init");
      goto errout;
    }

  g_recv_buff = malloc(RECV_BUFF_SZ);
  if (!g_recv_buff)
    {
      sem_destroy(&g_chgstat_evt_sem);
      printf("g_recv_buff");
      ret = -errno;
      goto errout;
    }

  ret = host_if_fctry_init(gacrux_cmd_evt_handler);
  if (ret == 0)
    {
      g_is_cmd_init = true;
    }
  else
    {
      printf("host_if_fctry_init %d\n", ret);
      sem_destroy(&g_chgstat_evt_sem);
      free(g_recv_buff);
    }

errout:
  return ret;
}

int gacrux_cmd_deinit(void)
{
  int ret;

  CHECKINIT();

  ret = host_if_fctry_fin();
  if (ret != 0)
    {
      goto errout;
    }

  if (g_recv_buff)
    {
      free((FAR void *)g_recv_buff);
      g_recv_buff = NULL;
    }

  ret = sem_destroy(&g_chgstat_evt_sem);
  if (ret != 0)
    {
      goto errout;
    }

  g_is_cmd_init = false;

errout:
  return ret;
}

int gacrux_cmd_change_sys_status(int stat)
{
  int                  ret;
  FAR struct host_if_s *host;
  uint8_t              cmd[CHGSTAT_CMD_SIZE] = {0};

  CHECKINIT();

  if ((g_gacrux_state == GACRUX_STATUS_IDLE_ROM) || /* Cannot change status. */
      (g_gacrux_state == GACRUX_STATUS_IDLE && stat == CHGSTAT_OPR_WAKEUP) || /* Already awake. */
      (g_gacrux_state == GACRUX_STATUS_DEEPSLEEP && stat == CHGSTAT_OPR_SUSPEND)) /* Already asleep. */
    {
      printf("No need to publish chgstat.\n");
    }

  while (0 == sem_trywait(&g_chgstat_evt_sem))
    {
      ; /* Set semaphore count to 0. */
    }

  host = host_if_fctry_get_obj(g_hif_type);

  ret = chgstat_cmd_create(cmd, CHGSTAT_CMD_SIZE, stat);
  if (ret != 0)
    {
      printf("Cmd create error:%d\n", ret);
      goto exit;
    }

  ret = host->write(host, cmd, CHGSTAT_CMD_SIZE);
  if (ret < 0)
    {
      printf("Write error:%d\n", ret);
      goto exit;
    }

  ret = wait_chgstat_evt();

exit:
  return ret;
}

int gacrux_cmd_tx_fw(const char *fw_path)
{
  int                  ret;
  int                  i;
  int                  fd;
  FAR struct host_if_s *host;
  FAR uint8_t          *cmd = NULL;
  int                  loop_num;
  uint32_t             pkt_len;
  uint32_t             total_tx_len = 0;
  int32_t              file_sz;
  uint32_t             res_len;

  CHECKINIT();

  if (!fw_path)
  {
    return -EINVAL;
    }

  file_sz = calc_file_sz(fw_path);
  if (file_sz < 0)
    {
      return file_sz;
    }

  printf("Division size = %d\n", g_tx_fw_one_packet_sz);

  loop_num =
    (file_sz % g_tx_fw_one_packet_sz) == 0 ?
    (file_sz / g_tx_fw_one_packet_sz) : (file_sz / g_tx_fw_one_packet_sz) + 1;
  printf("loop_num:%d\n", loop_num);

  fd = open(fw_path, O_RDONLY);
  if (fd < 0)
    {
      printf("Failed to open FW file.\n");
      return fd;
    }

  cmd = malloc(TXFW_CMD_SIZE(g_tx_fw_one_packet_sz));
  if (!cmd)
    {
      printf("Failed to allocate divided FW buf.\n");
      ret = -errno;
      goto exit;
    }

  host = host_if_fctry_get_obj(g_hif_type);

  for (i=0; i<loop_num; i++)
    {
      pkt_len = g_tx_fw_one_packet_sz < (file_sz - total_tx_len) ?
                g_tx_fw_one_packet_sz : (file_sz - total_tx_len);

      /* ++ Create command ++ */
      cmd[GHIFP_SYNC_OFFSET]                           = GHIFP_SYNC;
      *(uint16_t *)&cmd[GHIFP_OPR_LEN_OFFSET]          = TXFW_OPR_SIZE(pkt_len); /* Set OPC len by little endian. */
      cmd[GHIFP_OPC_OFFSET]                            = TX_BIN_OPC;
      cmd[GHIFP_H_CHECKSUM_OFFSET]                     = calc_checksum(cmd, 4);
      *(uint16_t *)&cmd[TXFW_OPR_TOTAL_PKT_NUM_OFFSET] = loop_num;
      *(uint16_t *)&cmd[TXFW_OPR_PKT_NUM_OFFSET]       = i+1;

      ret = read(fd, &cmd[TXFW_OPR_PKT_OFFSET], pkt_len);

      if (ret != pkt_len)
        {
          printf("File read error. len:%d expected len:%u\n", ret, pkt_len);
          ret = -EIO;
          break;
        }
      total_tx_len += ret;

      cmd[GHIFP_D_CHECKSUM_OFFSET(TXFW_OPR_SIZE(pkt_len))] =
        calc_checksum(&cmd[GHIFP_OPR_OFFSET], TXFW_OPR_SIZE(pkt_len));

      /* -- Create command -- */

      printf("Packet(%d/%d) fw part size:%d\n", i+1, loop_num, pkt_len);

      ret = host->transaction(host, cmd, TXFW_CMD_SIZE(pkt_len),
                              g_recv_buff, RECV_BUFF_SZ, &res_len);
      if (ret != 0)
        {
          printf("Transaction error:%d\n", ret);
          break;
        }

      ret = tx_fw_res_check(g_recv_buff, res_len);
      if (ret != 0)
        {
          printf("Transaction error:%d\n", ret);
          break;
        }
    }

    if (ret == 0)
      {
        printf("Completed all transfers!!\n");
      }

exit:

  close(fd);

  if (cmd)
    {
      free(cmd);
    }

  return ret;
}

int gacrux_cmd_execute_fw(void)
{
  int                  ret;
  FAR struct host_if_s *host;
  uint8_t              cmd[EXECFW_CMD_SIZE] = {0};
  uint32_t             res_len;

  CHECKINIT();

  host = host_if_fctry_get_obj(g_hif_type);

  ret = execute_fw_cmd_create(cmd, EXECFW_CMD_SIZE);
  if (ret != 0)
    {
      printf("Cmd create error:%d\n", ret);
      goto exit;
    }

  ret = host->transaction(host, cmd, EXECFW_CMD_SIZE,
                          g_recv_buff, RECV_BUFF_SZ, &res_len);
  if (ret != 0)
    {
      printf("Transaction error:%d\n", ret);
      goto exit;
    }

  ret = execute_fw_res_check(g_recv_buff, res_len);

exit:
  return ret;
}

int gacrux_cmd_uartconf(uint8_t baudrate, uint8_t flow_ctrl)
{
  int                  ret;
  FAR struct host_if_s *host;
  uint8_t              cmd[UARTCONF_CMD_SIZE] = {0};
  uint32_t             res_len;

  CHECKINIT();

  if (g_hif_type != HOST_IF_FCTRY_TYPE_UART)
    {
      printf("Permitted to execute only when selecting UART mode.\n");
      return -EPERM;
    }

  printf("baudrate:0x%02X, flow_ctrl:0x%02X\n",
         baudrate, flow_ctrl);

  if (0 < flow_ctrl ||
      baudrate == SPEED_UART_14400BPS ||
      SPEED_UART_MAX < baudrate)
    {
      printf("Warning!! Unsupported configuration. Behavior is undefined.\n");
    }

  host = host_if_fctry_get_obj(g_hif_type);

  ret = uartconf_cmd_create(cmd, UARTCONF_CMD_SIZE, baudrate, flow_ctrl);
  if (ret != 0)
    {
      printf("Cmd create error:%d\n", ret);
      goto exit;
    }

  /* Send command by old configuration */
  ret = host->write(host, cmd, UARTCONF_CMD_SIZE);
  if (ret < 0)
    {
      printf("Write error:%d\n", ret);
      goto exit;
    }

  usleep(20000); /* Wait completion transfer(temporary) */

  if (UARTCONF_OPR1_10000000BPS < baudrate ||
      UARTCONF_OPR2_FLOW_CTRL_ON < flow_ctrl)
    {
      printf("Invalid OPR, Not change the internal setting.\n");
    }
  else
    {
      /* Apply new configuration */
      ret = host->set_config(host, HOST_IF_SET_CONFIG_REQ_SETSLVCONF,
                            (void *)&baudrate);
      if (ret < 0)
        {
          printf("Change baudrate error:%d\n", ret);
        }
      else
        {
          printf("Apply new configuration.\n");
        }
    }

  /* Read response by new configuration */
  res_len = host->read(host, g_recv_buff, RECV_BUFF_SZ);
  if (res_len < 0)
    {
      printf("Read error:%d\n", res_len);
      ret = res_len;
      goto exit;
    }

  ret = uartconf_res_check(g_recv_buff, res_len);

exit:
  return ret;
}

int gacrux_cmd_i2cconf(uint8_t speed)
{
  int                  ret;
  FAR struct host_if_s *host;
  uint8_t              cmd[I2CCONF_CMD_SIZE] = {0};
  int                  res_len;

  CHECKINIT();

  if (g_hif_type != HOST_IF_FCTRY_TYPE_I2C)
    {
      printf("Permitted to execute only when selecting I2C mode.\n");
      return -EPERM;
    }

  printf("speed:0x%02X\n", speed);

  if (SPEED_I2C_MAX < speed)
    {
      printf("Warning!! Unsupported configuration. Behavior is undefined.\n");
    }

  host = host_if_fctry_get_obj(g_hif_type);

  ret = i2cconf_cmd_create(cmd, I2CCONF_CMD_SIZE, speed);
  if (ret != 0)
    {
      printf("Cmd create error:%d\n", ret);
      goto exit;
    }

  /* Send command by old configuration */
  ret = host->write(host, cmd, I2CCONF_CMD_SIZE);
  if (ret < 0)
    {
      printf("Write error:%d\n", ret);
      goto exit;
    }

  if (I2CCONF_OPR_3400000BPS < speed)
    {
      printf("Invalid OPR, Not change the internal setting.\n");
    }
  else
    {
      /* Apply new configuration */
      ret = host->set_config(host, HOST_IF_SET_CONFIG_REQ_SETSLVCONF,
                            (void *)&speed);
      if (ret < 0)
        {
          printf("Change speed error:%d\n", ret);
        }
      else
        {
          printf("Apply new configuration.\n");
        }
    }

  /* Read response by new configuration */
  res_len = host->read(host, g_recv_buff, RECV_BUFF_SZ);

  if (res_len < 0)
    {
      printf("Read error:%d\n", res_len);
      ret = res_len;
      goto exit;
    }

  ret = i2cconf_res_check(g_recv_buff, res_len);

exit:
  return ret;
}

int gacrux_cmd_i2cwrite(uint8_t bin_cmd, uint8_t *opr, uint16_t opr_len)
{
  int ret = 0;
  FAR struct host_if_s *host;

  printf("opr_len : %d\n", opr_len);
  for (int i = 0; i < opr_len; i++) {
    printf("opr[%d] : %d\n", i, opr[i]);
  }

  uint8_t buf[GHIFP_HEADER_SIZE + ((opr_len > 0)?(GHIFP_HEADER_SIZE + GHIFP_DATA_SIZE(opr_len)):GHIFP_HEADER_SIZE)];

  if (OprLength[bin_cmd] == opr_len) {
    cmd_create(buf, bin_cmd, opr, opr_len);
    for (int j = 0; j < ((opr_len > 0)?(GHIFP_HEADER_SIZE + GHIFP_DATA_SIZE(opr_len)):5); j++)
    {
      printf("%x ", buf[j]);
    }
    printf("\n");
  }else {
    printf("OPR Length does not match with the command\n");
    return -EINVAL;
  }
  host = host_if_fctry_get_obj(g_hif_type);

  if(opr_len == 0){
    ret = host->write(host, buf, GHIFP_HEADER_SIZE);
  } else {
    ret = host->write(host, buf, GHIFP_HEADER_SIZE+GHIFP_DATA_SIZE(opr_len));
  }
  printf("write ret : %d\n", ret);
  return ret;
}

int gacrux_cmd_i2cread(void)
{
  int res_len = 0;
  FAR struct host_if_s *host;

  host = host_if_fctry_get_obj(g_hif_type);
  res_len = host->read(host, g_recv_buff, RECV_BUFF_SZ);
  printf("read length : %d\n", res_len);
  for (int k = 0; k < res_len; k++)
  {
    printf("%x ", g_recv_buff[k]);
  }
   printf("\n");
  return res_len;
}

int gacrux_cmd_spiconf(uint8_t dfs)
{
  int                  ret;
  FAR struct host_if_s *host;
  uint8_t              cmd[SPICONF_CMD_SIZE] = {0};
  uint32_t             res_len;

  CHECKINIT();

  if (g_hif_type != HOST_IF_FCTRY_TYPE_SPI)
    {
      printf("Permitted to execute only when selecting SPI mode.\n");
      return -EPERM;
    }

  printf("dfs:0x%02X\n", dfs);

  if (DFS_SPI_MAX < dfs)
    {
      printf("Warning!! Unsupported configuration. Behavior is undefined.\n");
    }

  host = host_if_fctry_get_obj(g_hif_type);

  ret = spiconf_cmd_create(cmd, SPICONF_CMD_SIZE, dfs);
  if (ret != 0)
    {
      printf("Cmd create error:%d\n", ret);
      goto exit;
    }

  /* Send command by old configuration */
  ret = host->write(host, cmd, SPICONF_CMD_SIZE);
  if (ret < 0)
    {
      printf("Write error:%d\n", ret);
      goto exit;
    }

  if (SPICONF_OPR_DFS32 < dfs)
    {
      printf("Invalid OPR, Not change the internal setting.\n");
    }
  else
    {
      /* Apply new configuration */
      ret = host->set_config(host, HOST_IF_SET_CONFIG_REQ_SETSLVCONF,
                            (void *)&dfs);
      if (ret < 0)
        {
          printf("Change dfs error:%d\n", ret);
        }
      else
        {
          printf("Apply new configuration.\n");
        }
    }

  /* Read response by new configuration */
  res_len = host->read(host, g_recv_buff, RECV_BUFF_SZ);
  if (res_len < 0)
    {
      printf("Read error:%d\n", res_len);
      ret = res_len;
      goto exit;
    }

  ret = spiconf_res_check(g_recv_buff, res_len);

exit:
  return ret;
}
////////////////////////////////////////////////////////////////////////////////
int gacrux_cmd_spiwrite(uint8_t bin_cmd, uint8_t *opr, uint16_t opr_len)
{
  int ret = 0;
  FAR struct host_if_s *host;

  printf("opr_len : %d\n", opr_len);
  for (int i = 0; i < opr_len; i++) {
    printf("opr[%d] : %d\n", i, opr[i]);
  }

  uint8_t buf[GHIFP_HEADER_SIZE + ((opr_len > 0)?(GHIFP_HEADER_SIZE + GHIFP_DATA_SIZE(opr_len)):GHIFP_HEADER_SIZE)];

  if (OprLength[bin_cmd] == opr_len || OprLength[bin_cmd] < 0) {
    cmd_create(buf, bin_cmd, opr, opr_len);
    for (int j = 0; j < ((opr_len > 0)?(GHIFP_HEADER_SIZE + GHIFP_DATA_SIZE(opr_len)):5); j++)
    {
      printf("%x ", buf[j]);
    }
    printf("\n");
  }else {
    printf("OPR Length does not match with the command\n");
    return -EINVAL;
  }
  host = host_if_fctry_get_obj(g_hif_type);

  // if(opr_len == 0){
  //   ret = host->write(host, buf, GHIFP_HEADER_SIZE);
  // } else {
  //   ret = host->write(host, buf, GHIFP_HEADER_SIZE+GHIFP_DATA_SIZE(opr_len));
  // }

  ret = host->write(host, buf, GHIFP_HEADER_SIZE);
  up_mdelay(5);
  if (opr_len != 0) {
#ifdef Bit16Test
    int send_size = GHIFP_HEADER_SIZE + GHIFP_DATA_SIZE(opr_len) - ret;
    if (send_size) {
      ret = host->write(host, buf + ret, send_size);
    }
#else
    ret = host->write(host, buf + GHIFP_HEADER_SIZE, GHIFP_DATA_SIZE(opr_len));
#endif
  }
  // up_mdelay(5);
  return ret;
}

int gacrux_cmd_spiread(void)
{
  int res_len = 0;
  FAR struct host_if_s *host;

  host = host_if_fctry_get_obj(g_hif_type);
  res_len = host->read(host, g_recv_buff, RECV_BUFF_SZ);
  printf("read length : %d\n", res_len);
  for (int k = 0; k < res_len; k++)
  {
    printf("%x ", g_recv_buff[k]);
  }
   printf("\n");
  return res_len;
}
////////////////////////////////////////////////////////////////////////////////
int gacrux_cmd_debug_send(char *bin_str, int bin_len)
{
  int                  ret;
  FAR struct host_if_s *host;
  uint8_t              *cmd = NULL;

  CHECKINIT();

  if (!bin_str || !bin_len)
    {
      return -EINVAL;
    }

  if (strlen(bin_str) != bin_len * 2)
    {
      printf("Mismatch length. strlen:%d, bin_len:%d\n",
             strlen(bin_str), bin_len);
      return -EINVAL;
    }

  cmd = malloc(bin_len);
  if (!cmd)
    {
      printf("Failed to allocate debug binary buf.\n");
      ret = -errno;
      goto exit;
    }

  ret = str_to_bin(bin_str, strlen(bin_str),
                   cmd, bin_len);
  if (ret != 0)
    {
      printf("Failed to convert text to binary:%d\n", ret);
      goto exit;
    }

  host = host_if_fctry_get_obj(g_hif_type);

  ret = host->dbg_write(host, cmd, bin_len);
  if (ret < 0)
    {
      printf("Debug write error:%d\n", ret);
      goto exit;
    }

exit:

  if (cmd)
    {
      free(cmd);
    }

  return ret;
}

int gacrux_cmd_debug_file_send(const char *path)
{
  int                  ret;
  int                  fd;
  FAR struct host_if_s *host;
  FAR uint8_t          *cmd = NULL;
  int32_t              file_sz;

  CHECKINIT();

  if (!path)
    {
      return -EINVAL;
    }

  file_sz = calc_file_sz(path);
  if (file_sz < 0)
    {
      return file_sz;
    }

  fd = open(path, O_RDONLY);
  if (fd < 0)
    {
      printf("Failed to open file.\n");
      return fd;
    }

  cmd = malloc(file_sz);
  if (!cmd)
    {
      printf("Failed to allocate file data buf.\n");
      ret = -errno;
      goto exit;
    }

  ret = read(fd, cmd, file_sz);
  if (ret != file_sz)
    {
      printf("File read error. len:%d expected len:%u\n", ret, file_sz);
      ret = -EIO;
      goto exit;
    }

  host = host_if_fctry_get_obj(g_hif_type);

  ret = host->dbg_write(host, cmd, file_sz);
  if (ret < 0)
    {
      printf("Debug write error:%d\n", ret);
      goto exit;
    }

exit:

  close(fd);

  if (cmd)
    {
      free(cmd);
    }

  return ret;
}

int gacrux_cmd_set_if_type(int if_type)
{
  CHECKINIT();

  if (if_type < 0 || HOST_IF_FCTRY_TYPE_NUM <= if_type)
    {
      printf("Invalid I/F type:%d\n", if_type);
      return -EINVAL;
    }

  printf("Serial I/F type is changed. %d -> %d\n", g_hif_type, if_type);
  g_hif_type = (enum host_if_fctry_type_e)if_type;

  return 0;
}

int gacrux_cmd_get_if_type(void){
  return g_hif_type;
}

int gacrux_cmd_set_division_size(uint16_t sz)
{
  CHECKINIT();

  printf("Division size is changed. %d -> %d\n", g_tx_fw_one_packet_sz, sz);
  g_tx_fw_one_packet_sz = sz;

  if (GHIFP_OPR_LEN_MAX < TXFW_OPR_SIZE(g_tx_fw_one_packet_sz))
    {
      printf("Warning!! Over the max OPC length.\n");
    }

  return 0;
}

int gacrux_cmd_set_config(uint32_t req, FAR void *arg)
{
  FAR struct host_if_s *host;

  CHECKINIT();

  host = host_if_fctry_get_obj(g_hif_type);

  return host->set_config(host, req, arg);
}

int gacrux_cmd_debug_tx_fw(uint32_t virtual_file_sz, uint16_t div_sz,
                           uint8_t total_pkt_type, uint8_t pkt_no_type)
{
  int                  ret;
  int                  i;
  FAR struct host_if_s *host;
  FAR uint8_t          *cmd = NULL;
  int                  loop_num;
  uint32_t             pkt_len;
  uint16_t             total_pkt_num;
  uint32_t             total_tx_len = 0;
  uint32_t             res_len;

  CHECKINIT();

  printf("Start program transfer for debug.\n");
  printf("Virtual file size = %u\n"
         "Division size     = %u\n"
         "Total packet type = %u\n"
         "Packet No type    = %u\n",
          virtual_file_sz, div_sz, total_pkt_type, pkt_no_type);

  if (TOTAL_PKT_NUM_TYPE_MAX < total_pkt_type ||
      PKT_NO_TYPE_MAX < pkt_no_type)
    {
      return -EINVAL;
    }

  total_pkt_num =
    (virtual_file_sz % div_sz) == 0 ?
    (virtual_file_sz / div_sz) : (virtual_file_sz / div_sz) + 1;
  printf("total_pkt_num:%d\n", total_pkt_num);

  if (pkt_no_type == PKT_NO_TYPE_OVER_TOTAL_NUM)
    {
      loop_num = total_pkt_num + 2; /* Run excess loop two times */
    }
  else
    {
      loop_num = total_pkt_num;
    }
  printf("loop_num:%d\n", loop_num);

  cmd = malloc(TXFW_CMD_SIZE(div_sz));
  if (!cmd)
    {
      printf("Failed to allocate divided FW buf.\n");
      ret = -errno;
      goto exit;
    }

  host = host_if_fctry_get_obj(g_hif_type);

  for (i=0; i<loop_num; i++)
    {
      memset(cmd, 0, TXFW_CMD_SIZE(div_sz));

      pkt_len = div_sz < (virtual_file_sz - total_tx_len) ?
                div_sz : (virtual_file_sz - total_tx_len);

      /* ++ Create command ++ */
      cmd[GHIFP_SYNC_OFFSET]                  = GHIFP_SYNC;
      *(uint16_t *)&cmd[GHIFP_OPR_LEN_OFFSET] = TXFW_OPR_SIZE(pkt_len); /* Set OPC len by little endian. */
      cmd[GHIFP_OPC_OFFSET]                   = TXFW_OPC;
      cmd[GHIFP_H_CHECKSUM_OFFSET]            = calc_checksum(cmd, 4);

      switch (total_pkt_type)
        {
          case TOTAL_PKT_NUM_TYPE_NORMAL:
            *(uint16_t *)&cmd[TXFW_OPR_TOTAL_PKT_NUM_OFFSET] = total_pkt_num;
            break;
          case TOTAL_PKT_NUM_TYPE_ZERO:
            *(uint16_t *)&cmd[TXFW_OPR_TOTAL_PKT_NUM_OFFSET] = 0;
            break;
          case TOTAL_PKT_NUM_TYPE_INVALID:
            *(uint16_t *)&cmd[TXFW_OPR_TOTAL_PKT_NUM_OFFSET] = total_pkt_num + 1;
            break;
          case TOTAL_PKT_NUM_TYPE_SHIFT_PLUS:
            *(uint16_t *)&cmd[TXFW_OPR_TOTAL_PKT_NUM_OFFSET] =
              total_pkt_num <= i * 2 ? total_pkt_num + 1 : total_pkt_num;
            break;
          case TOTAL_PKT_NUM_TYPE_SHIFT_MINUS:
            *(uint16_t *)&cmd[TXFW_OPR_TOTAL_PKT_NUM_OFFSET] =
              total_pkt_num <= i * 2 ? total_pkt_num - 1 : total_pkt_num;
            break;
          default:
            break;
        }

      switch (pkt_no_type)
        {
          case PKT_NO_TYPE_NORMAL:
            *(uint16_t *)&cmd[TXFW_OPR_PKT_NUM_OFFSET] = i+1; /* 1, 2, 3, 4, ... */
            break;
          case PKT_NO_TYPE_INCREMENT_FROM_ZERO:
            *(uint16_t *)&cmd[TXFW_OPR_PKT_NUM_OFFSET] = i; /* 0, 1, 2, 3, ... */
            break;
          case PKT_NO_TYPE_NO_INCREMENT:
            *(uint16_t *)&cmd[TXFW_OPR_PKT_NUM_OFFSET] = 1; /* 1, 1, 1, 1, ... */
            break;
          case PKT_NO_TYPE_OVER_INCREMENT:
            *(uint16_t *)&cmd[TXFW_OPR_PKT_NUM_OFFSET] =
              (i * 2) + 1; /* 1, 3, 5, 7, ... */
            break;
          case PKT_NO_TYPE_OVER_TOTAL_NUM:
            /* Do nothing in here */
            *(uint16_t *)&cmd[TXFW_OPR_PKT_NUM_OFFSET] = i+1; /* 1, 2, 3, 4, ... */
            break;
          default:
            break;
        }

      total_tx_len += pkt_len;

      cmd[GHIFP_D_CHECKSUM_OFFSET(TXFW_OPR_SIZE(pkt_len))] =
        calc_checksum(&cmd[GHIFP_OPR_OFFSET], TXFW_OPR_SIZE(pkt_len));
      /* -- Create command -- */

      printf("Packet(%d/%d) fw part size:%d\n", i+1, total_pkt_num, pkt_len);

      ret = host->transaction(host, cmd, TXFW_CMD_SIZE(pkt_len),
                              g_recv_buff, RECV_BUFF_SZ, &res_len);
      if (ret != 0)
        {
          printf("Transaction error:%d\n", ret);
          break;
        }

      ret = tx_fw_res_check(g_recv_buff, res_len);
      if (ret != 0)
        {
          printf("Transaction error:%d\n", ret);
          break;
        }
      else
        {
          printf("Completed transfer of packet No.%d \n", i+1);
        }
    }

    if (ret == 0)
      {
        printf("Completed all transfers!!\n");
      }

exit:

  if (cmd)
    {
      free(cmd);
    }

  return ret;
}

int gacrux_cmd_bin_input(uint8_t input, const char *fw_path)
{
  int                  ret;
  int                  i;
  int                  fd;
  FAR struct host_if_s *host;
  FAR uint8_t          *cmd = NULL;
  int                  loop_num;
  uint32_t             pkt_len;
  uint32_t             total_tx_len = 0;
  int32_t              file_sz;
  uint32_t             res_len;

  CHECKINIT();
  char path[] = "/mnt/spif/";
  strcat(path, fw_path);
  if (!path)
  {
    return -EINVAL;
    }

  file_sz = calc_file_sz(path);
  if (file_sz < 0)
    {
      return file_sz;
    }

  printf("Division size = %d\n", g_bin_input_one_packet_sz);

  loop_num =
    (file_sz % g_bin_input_one_packet_sz) == 0 ?
    (file_sz / g_bin_input_one_packet_sz) : (file_sz / g_bin_input_one_packet_sz) + 1;
  printf("loop_num:%d\n", loop_num);

  fd = open(path, O_RDONLY);
  if (fd < 0)
    {
      printf("Failed to open FW file.\n");
      return fd;
    }

  cmd = malloc(TXFW_CMD_SIZE(g_bin_input_one_packet_sz));
  if (!cmd)
    {
      printf("Failed to allocate divided FW buf.\n");
      ret = -errno;
      goto exit;
    }

  host = host_if_fctry_get_obj(g_hif_type);

  for (i=0; i<loop_num; i++)
    {
      pkt_len = g_bin_input_one_packet_sz < (file_sz - total_tx_len) ?
                g_bin_input_one_packet_sz : (file_sz - total_tx_len);

      /* ++ Create command ++ */
      cmd[GHIFP_SYNC_OFFSET]                           = GHIFP_SYNC;
      *(uint16_t *)&cmd[GHIFP_OPR_LEN_OFFSET]          = pkt_len + 3;
      cmd[GHIFP_OPC_OFFSET]                            = TX_BIN_OPC;
      cmd[GHIFP_H_CHECKSUM_OFFSET]                     = calc_checksum(cmd, 4);

      /* Send header */
      ret = host->write(host, cmd, GHIFP_HEADER_SIZE);
      up_mdelay(5);

      /* Send data */
      *(uint8_t *)&cmd[5] = input;
      *(uint8_t *)&cmd[6] = loop_num;
      *(uint16_t *)&cmd[7] = i+1;

      ret = read(fd, &cmd[8], pkt_len);
       printf("pkt_len:%d \n", pkt_len);
      if (ret != pkt_len)
        {
          printf("File read error. len:%d expected len:%u\n", ret, pkt_len);
          ret = -EIO;
          break;
        }
      total_tx_len += ret;

      cmd[GHIFP_D_CHECKSUM_OFFSET(pkt_len+3)] =
        calc_checksum(&cmd[GHIFP_OPR_OFFSET], TXFW_OPR_SIZE(pkt_len)-1);
      /* -- Create command -- */

      printf("Packet(%d/%d) fw part size:%d\n", i+1, loop_num, pkt_len);
      ret = host->transaction(host, cmd + GHIFP_HEADER_SIZE, pkt_len + 4,
                          g_recv_buff, RECV_BUFF_SZ, &res_len);
      // ret = host->write(host, cmd + GHIFP_HEADER_SIZE,  pkt_len + 4);
      if (ret != 0)
        {
          printf("Transaction error:%d\n", ret);
          break;
        }

      // ret = bin_input_res_check(g_recv_buff, res_len);
      // if (ret != 0)
      //   {
      //     printf("Transaction error:%d\n", ret);
      //     break;
      //   }
        up_mdelay(5);
    }

    if (ret == 0)
      {
        printf("Completed all transfers!!\n");
      }

exit:

  close(fd);

  if (cmd)
    {
      free(cmd);
    }

  return ret;
}
