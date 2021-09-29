#ifndef __APPS_EXAMPLES_GHIFP_HOST_IF_BS_H
#define __APPS_EXAMPLES_GHIFP_HOST_IF_BS_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <stdint.h>
#include <sched.h>

/****************************************************************************
 * Public Types
 ****************************************************************************/

#define LOCAL_BUFF_SZ (4096 + 16)

/****************************************************************************
 * Public Types
 ****************************************************************************/

enum host_if_state_e
{
  HOST_IF_STATE_IDLE = 0,
  HOST_IF_STATE_WAIT_RESPONSE
};

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int push_dataframe(FAR uint8_t *df, uint32_t df_len);
int pop_dataframe(FAR uint8_t *buf, uint32_t sz, FAR uint32_t *df_len);
int create_df_queue(void);
int delete_df_queue(void);
int start_task(FAR const char *name, main_t entry, FAR const char *argv[]);
int stop_task(pid_t pid);
int restart_task(pid_t pid, FAR const char *name,
                 main_t entry, FAR const char *argv[]);
int bus_req_init(void);
int bus_req_deinit(void);
int bus_req_wait_i2c(void);
int bus_req_wait_spi(void);
int set_host_if_state(enum host_if_state_e state);
enum host_if_state_e get_host_if_state(void);

#endif /* __APPS_EXAMPLES_GHIFP_HOST_IF_BS_H */
