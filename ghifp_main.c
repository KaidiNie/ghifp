/****************************************************************************
 * ghifp/ghifp_main.c
 *
 *   Copyright (C) 2008, 2011-2012 Gregory Nutt. All rights reserved.
 *   Author: Gregory Nutt <gnutt@nuttx.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name NuttX nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/arch.h>
#include <sdk/config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdbool.h>
#include <errno.h>

#include "gacrux_cmd.h"
#include "host_if_fctry.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Test command format */

#define CMD_KEY_INIT              "INIT"
#define CMD_KEY_DEINIT            "DEINIT"
#define CMD_KEY_CHANGE_SYS_STATUS "CHGSTAT"
#define CMD_KEY_TRANSMIT_FW       "TXFW"
#define CMD_KEY_EXECUTE_FW        "EXECFW"
#define CMD_KEY_UARTCONF          "UARTCONF"
#define CMD_KEY_I2CCONF           "I2CCONF"
#define CMD_KEY_I2CWRITE          "I2CWRITE"
#define CMD_KEY_TEST              "CMD"
#define CMD_KEY_SPICONF           "SPICONF"
#define CMD_KEY_DEBUG_SEND        "DBGSEND"
#define CMD_KEY_DEBUG_RECV        "DBGRECV"
#define CMD_KEY_DEBUG_FILE_SEND   "DBGFSEND"
#define CMD_KEY_CHANGE_IF         "CHGIF"
#define CMD_KEY_SET_DIVISION_SZ   "SETDIVSZ"
#define CMD_KEY_SETADDR           "SETADDR"
#define CMD_KEY_SETDFS            "SETDFS"
#define CMD_KEY_SETSPICLK         "SETSPICLK"
#define CMD_KEY_DEBUG_TRANSMIT_FW "DBGTXFW"
#define CMD_KEY_BINARY_INPUT      "BININ"

/****************************************************************************
 * Private Types
 ****************************************************************************/

/****************************************************************************
 * Private Data
 ****************************************************************************/

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static void help(void)
{
  printf("ghifp [arg1] ([arg2] [arg3] ...)\n");

  printf("arg list\n");
  printf("\t- %s\n", CMD_KEY_INIT);
  printf("\t- %s\n", CMD_KEY_DEINIT);
  printf("\t- %s [stat]\n", CMD_KEY_CHANGE_SYS_STATUS);
  printf("\t\te.g. \"ghifp chgstat 1\"\n");
  printf("\t- %s [fw_path]\n", CMD_KEY_TRANSMIT_FW);
  printf("\t\te.g. \"ghifp txfw /mnt/spif/test.bin\"\n");
  printf("\t- %s\n", CMD_KEY_EXECUTE_FW);
  printf("\t- %s [baudrate No] [flow control No]\n", CMD_KEY_UARTCONF);
  printf("\t\te.g. \"ghifp uartconf 6 0\"\n");
  printf("\t- %s [speed No]\n", CMD_KEY_I2CCONF);
  printf("\t- %s [binary cpmmand No]\n", CMD_KEY_I2CWRITE);
  printf("\t\te.g. \"ghifp i2cwrite 1 2 3 4 5\"\n");
  printf("\t- %s [binary cpmmand No]\n", CMD_KEY_TEST);
  printf("\t\te.g. \"ghifp cmd 1 2 3 4 5\"\n");
  printf("\t- %s [data frame size No]\n", CMD_KEY_SPICONF);
  printf("\t\te.g. \"ghifp spiconf 1\"\n");
  printf("\t- %s [len] [hex binary(text)]\n", CMD_KEY_DEBUG_SEND);
  printf("\t\te.g. \"ghifp dbgsend 4 001188FF\"\n");
  printf("\t- %s [0 or 1]\n", CMD_KEY_DEBUG_RECV);
  printf("\t\te.g. \"ghifp dbgrecv 1\"\n");
  printf("\t- %s [path]\n", CMD_KEY_DEBUG_FILE_SEND);
  printf("\t\te.g. \"ghifp dbgfsend /mnt/spif/test.bin\"\n");
  printf("\t- %s [interface num]\n", CMD_KEY_CHANGE_IF);
  printf("\t\te.g. \"ghifp chgif 1\"\n");
  printf("\t- %s [one packet size]\n", CMD_KEY_SET_DIVISION_SZ);
  printf("\t\te.g. \"ghifp setdivsz 4086\"\n");
  printf("\t- %s [hex target address]\n", CMD_KEY_SETADDR);
  printf("\t\tIt is only effective when interface type is I2C.\n");
  printf("\t\te.g. \"ghifp setaddr 24\"\n");
  printf("\t- %s [data frame size]\n", CMD_KEY_SETDFS);
  printf("\t\tIt is only effective when interface type is SPI.\n");
  printf("\t\te.g. \"ghifp setdfs 16\"\n");
  printf("\t- %s [clock No]\n", CMD_KEY_SETSPICLK);
  printf("\t\tIt is only effective when interface type is SPI.\n");
  printf("\t\te.g. \"ghifp setspiclk 2\"\n");
  printf("\t- %s [virtual_file_sz] [div_sz] [total_pkt_type] [pkt_no_type]\n",
         CMD_KEY_DEBUG_TRANSMIT_FW);
  printf("\t\te.g. \"ghifp dbgtxfw 20480 4086 0 0\n");
  printf("\t- %s [input][path]\n", CMD_KEY_BINARY_INPUT);
  printf("\t\te.g. \"ghifp binin 0 test.bin\"\n");

  return;
}

static int dump_arg(int argc, FAR char *argv[])
{
  int i;

  for (i=0; i<argc; i++)
    {
      printf("[%d]:%s\n", i, argv[i]);
    }

  return 0;
}

int ghifp_cmd_entry(int argc, FAR char *argv[])
{
  int ret = 0;

  if (argc <= 0)
    {
      help();
      return -EINVAL;
    }

  if (0 == strcasecmp(argv[0], CMD_KEY_INIT))
    {
      /* Initialize */
      if (argc == 1)
        {
          ret = gacrux_cmd_init();
        }
      else
        {
          printf("No need arguments.\n");
          ret = -EINVAL;
        }
    }
  else if (0 == strcasecmp(argv[0], CMD_KEY_DEINIT))
    {
      /* De-Initialize */
      if (argc == 1)
        {
          ret = gacrux_cmd_deinit();
        }
      else
        {
          printf("No need arguments.\n");
          ret = -EINVAL;
        }
    }
  else if (0 == strcasecmp(argv[0], CMD_KEY_CHANGE_SYS_STATUS))
    {
       /* Change system status */
      if (argc == 2)
        {
          ret = gacrux_cmd_change_sys_status(atoi(argv[1]));
        }
      else
        {
          printf("The number of arguments is incorrect.\n");
          ret = -EINVAL;
        }
    }
  else if (0 == strcasecmp(argv[0], CMD_KEY_TRANSMIT_FW))
    {
       /* Change system status */
      if (argc == 2)
        {
          ret = gacrux_cmd_tx_fw(argv[1]);
        }
      else
        {
          printf("The number of arguments is incorrect.\n");
          ret = -EINVAL;
        }
    }
  else if (0 == strcasecmp(argv[0], CMD_KEY_EXECUTE_FW))
    {
      /* Execute FW */
      if (argc == 1)
        {
          ret = gacrux_cmd_execute_fw();
        }
      else
        {
          printf("No need arguments.\n");
          ret = -EINVAL;
        }
    }
  else if (0 == strcasecmp(argv[0], CMD_KEY_UARTCONF))
    {
      /* Change UART configuration */
      if (argc == 3)
        {
          ret = gacrux_cmd_uartconf((uint8_t)atoi(argv[1]),
                                    (uint8_t)atoi(argv[2]));
        }
      else
        {
          printf("No need arguments.\n");
          ret = -EINVAL;
        }
    }
  else if (0 == strcasecmp(argv[0], CMD_KEY_I2CCONF))
    {
      /* Change I2C configuration */
      if (argc == 2)
        {
          ret = gacrux_cmd_i2cconf((uint8_t)atoi(argv[1]));
        }
      else
        {
          printf("No need arguments.\n");
          ret = -EINVAL;
        }
    }
  else if (0 == strcasecmp(argv[0], CMD_KEY_I2CWRITE))
    {
      printf("argc = %d.\n", argc);
      /* I2C write */
      if (argc >= 2)
        {
          uint8_t opr[argc - 2];
          for (int i = 0; i < (argc - 2); i++)
          {
            opr[i] = (uint8_t)atoi(argv[2 + i]);
          }
          ret = gacrux_cmd_i2cwrite((uint8_t)atoi(argv[1]), opr,
                                    (uint16_t)(argc - 2));
          gacrux_cmd_i2cread();
        }
        else
        {
          printf("No need arguments.\n");
          ret = -EINVAL;
        }
    }
  else if (0 == strcasecmp(argv[0], CMD_KEY_TEST))
    {
      /* I2C write */
      if (argc >= 2)
        {
          uint8_t opr[argc - 2];
          for (int i = 0; i < (argc - 2); i++)
          {
            opr[i] = (uint8_t)atoi(argv[2 + i]);
          }
          switch (gacrux_cmd_get_if_type())
          {
          case HOST_IF_FCTRY_TYPE_UART:
            gacrux_cmd_debug_send((char *)opr, (uint16_t)(argc - 2));
            break;
          case HOST_IF_FCTRY_TYPE_I2C:
            ret = gacrux_cmd_i2cwrite((uint8_t)atoi(argv[1]), opr,
                                    (uint16_t)(argc - 2));
            gacrux_cmd_i2cread();
            break;
          case HOST_IF_FCTRY_TYPE_SPI: {
            uint8_t bin_cmd = (uint8_t)atoi(argv[1]);
            ret = gacrux_cmd_spiwrite(bin_cmd, opr, (uint16_t)(argc - 2));
            int res_len = gacrux_cmd_spiread();
            up_mdelay(5);

            if (res_len < 0 && bin_cmd == 0 && (opr[0] == 2))
            {
              int retry = 1;
              while (res_len < 0 && retry < 10)
              {
                ret = gacrux_cmd_spiwrite(bin_cmd, opr, (uint16_t)(argc - 2));
                res_len = gacrux_cmd_spiread();
                printf("RETRY: %d\n", retry);
                retry++;
              }
            }
            break;
          }
          default:
            break;
          }
        }
      else
        {
          printf("No need arguments.\n");
          ret = -EINVAL;
        }
    }
  else if (0 == strcasecmp(argv[0], CMD_KEY_SPICONF))
    {
      /* Change SPI configuration */
      if (argc == 2)
        {
          ret = gacrux_cmd_spiconf((uint8_t)atoi(argv[1]));
        }
      else
        {
          printf("No need arguments.\n");
          ret = -EINVAL;
        }
    }
  else if (0 == strcasecmp(argv[0], CMD_KEY_DEBUG_SEND))
    {
      /* Send arbitrary binary */
      if (argc == 3)
        {
          ret = gacrux_cmd_debug_send(argv[2], atoi(argv[1]));
        }
      else
        {
          printf("The number of arguments is incorrect.\n");
          ret = -EINVAL;
        }
    }
  else if (0 == strcasecmp(argv[0], CMD_KEY_DEBUG_RECV))
    {
      /* Enable debug receive mode */
      if (argc == 2)
        {
          ret = gacrux_cmd_set_config(HOST_IF_SET_CONFIG_REQ_DBGRECV,
                                      (void *)argv[1]);
        }
      else
        {
          printf("The number of arguments is incorrect.\n");
          ret = -EINVAL;
        }
    }
  else if (0 == strcasecmp(argv[0], CMD_KEY_DEBUG_FILE_SEND))
    {
      /* Send arbitrary file */
      if (argc == 2)
        {
          ret = gacrux_cmd_debug_file_send(argv[1]);
        }
      else
        {
          printf("The number of arguments is incorrect.\n");
          ret = -EINVAL;
        }
    }
  else if (0 == strcasecmp(argv[0], CMD_KEY_CHANGE_IF))
    {
      /* Change serial I/F */
      if (argc == 2)
        {
          ret = gacrux_cmd_set_if_type(atoi(argv[1]));
        }
      else
        {
          printf("The number of arguments is incorrect.\n");
          ret = -EINVAL;
        }
    }
  else if (0 == strcasecmp(argv[0], CMD_KEY_SET_DIVISION_SZ))
    {
      /* Change division size when xfer program */
      if (argc == 2)
        {
          ret = gacrux_cmd_set_division_size((uint16_t)atoi(argv[1]));
        }
      else
        {
          printf("The number of arguments is incorrect.\n");
          ret = -EINVAL;
        }
    }
  else if (0 == strcasecmp(argv[0], CMD_KEY_SETADDR))
    {
      /* Change I2C target address */
      if (argc == 2)
        {
          ret = gacrux_cmd_set_config(HOST_IF_SET_CONFIG_REQ_SETADDR,
                                      (void *)argv[1]);
        }
      else
        {
          printf("The number of arguments is incorrect.\n");
          ret = -EINVAL;
        }
    }
  else if (0 == strcasecmp(argv[0], CMD_KEY_SETDFS))
    {
      printf("Deprecated command. Recommend to use \"SPICONF\"\n");

      /* Change SPI data frame size */
      if (argc == 2)
        {
          ret = gacrux_cmd_set_config(HOST_IF_SET_CONFIG_REQ_SETDFS,
                                      (void *)argv[1]);
        }
      else
        {
          printf("The number of arguments is incorrect.\n");
          ret = -EINVAL;
        }
    }
  else if (0 == strcasecmp(argv[0], CMD_KEY_SETSPICLK))
    {
      /* Change SPI clock */
      if (argc == 2)
        {
          ret = gacrux_cmd_set_config(HOST_IF_SET_CONFIG_REQ_SETSPICLK,
                                      (void *)argv[1]);
        }
      else
        {
          printf("The number of arguments is incorrect.\n");
          ret = -EINVAL;
        }
    }
  else if (0 == strcasecmp(argv[0], CMD_KEY_DEBUG_TRANSMIT_FW))
    {
      /* Transmit FW for test */
      if (argc == 5)
        {
          ret = gacrux_cmd_debug_tx_fw(
                  (uint32_t)atoi(argv[1]), (uint16_t)atoi(argv[2]),
                  (uint8_t)atoi(argv[3]), (uint8_t)atoi(argv[4]));
        }
      else
        {
          printf("The number of arguments is incorrect.\n");
          ret = -EINVAL;
        }
    }
  else if (0 == strcasecmp(argv[0], CMD_KEY_BINARY_INPUT))
    {
       /* Change system status */
      if (argc == 3)
        {
          printf("argv[1] : %d.\n", (uint8_t)atoi(argv[1]));
          ret = gacrux_cmd_bin_input((uint8_t)atoi(argv[1]), argv[2]);
        }
      else
        {
          printf("The number of arguments is incorrect.\n");
          ret = -EINVAL;
        }
    }
  else
    {
      help();
      ret = -EINVAL;
    }

  return ret;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * ghifp_main
 ****************************************************************************/

#ifdef CONFIG_BUILD_KERNEL
int main(int argc, FAR char *argv[])
#else
int ghifp_main(int argc, char *argv[])
#endif
{
  int ret;
  // dump_arg(argc, argv);
  ret = ghifp_cmd_entry(argc-1, &argv[1]); /* Except "ghifp". */
  printf("ghifp command ret=%d\n", ret);

  return 0;
}
