#ifndef __APPS_EXAMPLES_GHIFP_GACRUX_PROTOCOL_DEF_H
#define __APPS_EXAMPLES_GHIFP_GACRUX_PROTOCOL_DEF_H

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Gacrux Host I/F protocol definition */

#define GHIFP_CHECKSUM_SIZE       (1)
#define GHIFP_HEADER_SIZE         (4 + GHIFP_CHECKSUM_SIZE)
#define GHIFP_DATA_SIZE(opr_len)  (opr_len + GHIFP_CHECKSUM_SIZE)
#define GHIFP_FRAME_SIZE(opr_len) (opr_len == 0 ?      \
                                   GHIFP_HEADER_SIZE : \
                                   GHIFP_HEADER_SIZE + \
                                   GHIFP_DATA_SIZE(opr_len))

#define GHIFP_SYNC_OFFSET                (0)
#define GHIFP_OPR_LEN_OFFSET             (1)
#define GHIFP_OPC_OFFSET                 (3)
#define GHIFP_H_CHECKSUM_OFFSET          (4)
#define GHIFP_OPR_OFFSET                 (5)
#define GHIFP_D_CHECKSUM_OFFSET(opr_len) (GHIFP_HEADER_SIZE + opr_len)

#define GHIFP_SYNC (0x7f)

#define GHIFP_STDERR_OK (0)

#define GHIFP_OPR_LEN_MAX (4090)

/* Change system status */

#define CHGSTAT_OPC           (0x0)
#define CHGSTAT_OPR_SIZE      (1)
#define CHGSTAT_CMD_SIZE      (GHIFP_HEADER_SIZE + \
                               GHIFP_DATA_SIZE(CHGSTAT_OPR_SIZE))
#define CHGSTAT_OPR_ROM_RESTART (0x0)
#define CHGSTAT_OPR_RESTART     (0x1)
#define CHGSTAT_OPR_WAKEUP      (0x2)
#define CHGSTAT_OPR_SUSPEND     (0x3)
#define CHGSTAT_OPR_BROKEN_DOWM (0xff)

#define CHGSTAT_RES_OPR_SIZE  (1)
#define CHGSTAT_RES_SIZE      (GHIFP_HEADER_SIZE + \
                               GHIFP_DATA_SIZE(CHGSTAT_RES_OPR_SIZE))

/* Transmit FW */

#define TXFW_OPC               (0x1)
#define TX_BIN_OPC               (0x7)
#define TXFW_OPR_SIZE(pkt_len) ((pkt_len) + 4)
#define TXFW_CMD_SIZE(pkt_len) (GHIFP_HEADER_SIZE + \
                                GHIFP_DATA_SIZE(TXFW_OPR_SIZE(pkt_len)))

#define TXFW_OPR_TOTAL_PKT_NUM_OFFSET (5)
#define TXFW_OPR_PKT_NUM_OFFSET       (7)
#define TXFW_OPR_PKT_OFFSET           (9)

#define TXFW_RES_OPR_SIZE             (1)
#define TXFW_RES_SIZE                 (GHIFP_HEADER_SIZE + \
                                       GHIFP_DATA_SIZE(TXFW_RES_OPR_SIZE))

/* Execute FW */

#define EXECFW_OPC          (0x2)
#define EXECFW_OPR_SIZE     (0)
#define EXECFW_CMD_SIZE     (GHIFP_HEADER_SIZE)

#define EXECFW_RES_OPR_SIZE (1)
#define EXECFW_RES_SIZE     (GHIFP_HEADER_SIZE + \
                             GHIFP_DATA_SIZE(EXECFW_RES_OPR_SIZE))

/* Change UART config */

#define UARTCONF_OPC                (0x3)
#define UARTCONF_OPR_SIZE           (2)
#define UARTCONF_CMD_SIZE           (GHIFP_HEADER_SIZE + \
                                     GHIFP_DATA_SIZE(UARTCONF_OPR_SIZE))
#define UARTCONF_OPR1_4800BPS       (0x0)
#define UARTCONF_OPR1_9600BPS       (0x1)
#define UARTCONF_OPR1_14400BPS      (0x2)
#define UARTCONF_OPR1_19200BPS      (0x3)
#define UARTCONF_OPR1_38400BPS      (0x4)
#define UARTCONF_OPR1_57600BPS      (0x5)
#define UARTCONF_OPR1_115200BPS     (0x6)
#define UARTCONF_OPR1_230400BPS     (0x7)
#define UARTCONF_OPR1_460800BPS     (0x8)
#define UARTCONF_OPR1_921600BPS     (0x9)
#define UARTCONF_OPR1_1000000BPS    (0xa)
#define UARTCONF_OPR1_2000000BPS    (0xb)
#define UARTCONF_OPR1_3000000BPS    (0xc)
#define UARTCONF_OPR1_4000000BPS    (0xd)
#define UARTCONF_OPR1_5000000BPS    (0xe)
#define UARTCONF_OPR1_6000000BPS    (0xf)
#define UARTCONF_OPR1_7000000BPS    (0x10)
#define UARTCONF_OPR1_8000000BPS    (0x11)
#define UARTCONF_OPR1_9000000BPS    (0x12)
#define UARTCONF_OPR1_10000000BPS   (0x13)

#define UARTCONF_OPR2_FLOW_CTRL_OFF (0x0)
#define UARTCONF_OPR2_FLOW_CTRL_ON  (0x1)

#define UARTCONF_RES_OPR_SIZE       (1)
#define UARTCONF_RES_SIZE           (GHIFP_HEADER_SIZE + \
                                     GHIFP_DATA_SIZE(UARTCONF_RES_OPR_SIZE))

/* Change I2C config */

#define I2CCONF_OPC            (0x4)
#define I2CCONF_OPR_SIZE       (1)
#define I2CCONF_CMD_SIZE       (GHIFP_HEADER_SIZE + \
                                GHIFP_DATA_SIZE(I2CCONF_OPR_SIZE))
#define I2CCONF_OPR_100000BPS  (0x0)
#define I2CCONF_OPR_1000000BPS (0x1) /* 400Kbps in SPRESENSE*/
#define I2CCONF_OPR_3400000BPS (0x2)

#define I2CCONF_RES_OPR_SIZE  (1)
#define I2CCONF_RES_SIZE      (GHIFP_HEADER_SIZE + \
                               GHIFP_DATA_SIZE(I2CCONF_RES_OPR_SIZE))

/* Change SPI config */

#define SPICONF_OPC          (0x5)
#define SPICONF_OPR_SIZE     (1)
#define SPICONF_CMD_SIZE     (GHIFP_HEADER_SIZE + \
                              GHIFP_DATA_SIZE(SPICONF_OPR_SIZE))
#define SPICONF_OPR_DFS8     (0x0)
#define SPICONF_OPR_DFS16    (0x1)
#define SPICONF_OPR_DFS32    (0x2)

#define SPICONF_RES_OPR_SIZE (1)
#define SPICONF_RES_SIZE     (GHIFP_HEADER_SIZE + \
                              GHIFP_DATA_SIZE(SPICONF_RES_OPR_SIZE))

/* Frame check error */

#define FRAMECHKERR_OPC         (0xFF)
#define FRAMECHKERR_OPR_SIZE    (2)
#define FRAMECHKERR_SIZE        (GHIFP_HEADER_SIZE + \
                                 GHIFP_DATA_SIZE(FRAMECHKERR_OPR_SIZE))
#define FRAMECHKERR_OPR1_OFFSET (5)
#define FRAMECHKERR_OPR2_OFFSET (6)

/****************************************************************************
 * Inline Functions
 ****************************************************************************/

static inline uint8_t calc_checksum(FAR uint8_t *target_buf, uint32_t sz)
{
  int i;
  uint8_t checksum = 0;

  for (i=0; i<sz; i++)
    {
      checksum += target_buf[i];
    }

  return checksum;
}

static inline int check_header(FAR uint8_t *header,
                 FAR uint8_t *opc, FAR uint16_t *opr_len)
{
  if (!header || !opc || !opr_len)
    {
      return -EINVAL;
    }

  if (header[0] != GHIFP_SYNC)
    {
      return -EINVAL;
    }

  if (header[GHIFP_H_CHECKSUM_OFFSET] != calc_checksum(header, 4))
    {
      return -EINVAL;
    }

  *opr_len = *(FAR uint16_t *)&header[GHIFP_OPR_LEN_OFFSET];
  *opc = header[GHIFP_OPC_OFFSET];

  return 0;
}

static inline int check_data(FAR uint8_t *data, uint16_t opr_len)
{
  if (opr_len == 0)
    {
      return 0;
    }

  if (data[opr_len] != calc_checksum(data, opr_len))
    {
      return -EINVAL;
    }

  return 0;
}

static inline bool is_evt_data(uint8_t opc)
{
  if ( /* opc == CHGSTAT_OPC || */ opc == FRAMECHKERR_OPC)
    {
      return true;
    }
  else
    {
      return false;
    }
}

#endif /* __APPS_EXAMPLES_GHIFP_GACRUX_PROTOCOL_DEF_H */
