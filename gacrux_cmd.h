#ifndef __APPS_EXAMPLES_GHIFP_GACRUX_CMD_H
#define __APPS_EXAMPLES_GHIFP_GACRUX_CMD_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <stdint.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/****************************************************************************
 * Public Types
 ****************************************************************************/

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int gacrux_cmd_init(void);
int gacrux_cmd_deinit(void);
int gacrux_cmd_change_sys_status(int stat);
int gacrux_cmd_tx_fw(const char *fw_path);
int gacrux_cmd_execute_fw(void);
int gacrux_cmd_uartconf(uint8_t baudrate, uint8_t flow_ctrl);
int gacrux_cmd_i2cconf(uint8_t speed);
int gacrux_cmd_i2cwrite(uint8_t bin_cmd, uint8_t *opr, uint16_t opr_len);
int gacrux_cmd_i2cread(void);
int gacrux_cmd_spiconf(uint8_t dfs);
int gacrux_cmd_spiwrite(uint8_t bin_cmd, uint8_t *opr, uint16_t opr_len);
int gacrux_cmd_spiread(void);
int gacrux_cmd_debug_send(char *bin_str, int bin_len);
int gacrux_cmd_debug_file_send(const char *path);
int gacrux_cmd_set_if_type(int if_type);
int gacrux_cmd_get_if_type(void);
int gacrux_cmd_set_division_size(uint16_t sz);
int gacrux_cmd_set_config(uint32_t req, FAR void *arg);
int gacrux_cmd_debug_tx_fw(uint32_t virtual_file_sz, uint16_t div_sz,
                           uint8_t total_pkt_type, uint8_t pkt_no_type);
int gacrux_cmd_bin_input(uint8_t input, const char *fw_path);

#endif /* __APPS_EXAMPLES_GHIFP_GACRUX_CMD_H */
