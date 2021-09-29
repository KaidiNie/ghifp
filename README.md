# How to use ghifp(Gacrux Host I/F Protocol test).

- Prerequisites
  - SPRESENSE build environment.

## build

- Copy ghifp directory to examples directory of SPRESENSE.

~~~shell
(e.g.)
$ cp -r ./GacruxTest/GeneralTest/ghifp ./spresense/examples
~~~

- Enable ghifp application and peripheral interface.

~~~shell
(e.g.)
$ cd ./spresense/sdk
$ ./tools/config.py -m
~~~

- [x] Examples ---> Gacrux Host I/F protocol test
- [x] CXD56xx Configuration --> Peripheral Option
  - Check the peripheral interface used as Host I/F.

- Build and burn nuttx.spk to SPRESENSE.

## Execute

- Send "ghifp" to nuttx console, then you can see the help.

~~~shell
nsh> ghifp
[0]:ghifp
ghifp [arg1] ([arg2] [arg3] ...)
arg list
        - INIT
        - DEINIT
        - CHGSTAT [stat]
                e.g. "ghifp chgstat 1"
        - TXFW [fw_path]
                e.g. "ghifp txfw /mnt/spif/test.bin"
        - EXECFW
        - UARTCONF [baudrate No] [flow control No]
                e.g. "ghifp uartconf 6 0"
        - I2CCONF [speed No]
                e.g. "ghifp i2cconf 1"
        - SPICONF [data frame size No]
                e.g. "ghifp spiconf 1"
        - DBGSEND [len] [hex binary(text)]
                e.g. "ghifp dbgsend 4 001188FF"
        - DBGRECV [0 or 1]
                e.g. "ghifp dbgrecv 1"
        - DBGFSEND [path]
                e.g. "ghifp dbgfsend /mnt/spif/test.bin"
        - CHGIF [interface num]
                e.g. "ghifp chgif 1"
        - SETDIVSZ [one packet size]
                e.g. "ghifp setdivsz 4086"
        - SETADDR [hex target address]
                It is only effective when interface type is I2C.
                e.g. "ghifp setaddr 24"
        - SETDFS [data frame size]
                It is only effective when interface type is SPI.
                e.g. "ghifp setdfs 16"
        - SETSPICLK [clock No]
                It is only effective when interface type is SPI.
                e.g. "ghifp setspiclk 2"
        - DBGTXFW [virtual_file_sz] [div_sz] [total_pkt_type] [pkt_no_type]
                e.g. "ghifp dbgtxfw 20480 4086 0 0
~~~

- Command list

  - __INIT__
    - Initialize ghifp application.
      You must execute this command at first.

  - __DEINIT__
    - Terminate ghifp application.

  - __CHGSTAT [stat]__
    - Send command to switch Gacrux system state.
      - [stat]
        0 -> Request to delete FW on NVM and re-start.
        1 -> Request FW re-start.
        2 -> Request to wake up from deep sleep state.
        3 -> Request to change state to deep sleep.

  - __TXFW [fw_path]__
    - Send command to transfer firmware.
      - [fw_path]
        FW path name.

  - __EXECFW__
    - Send command to execute FW.

  - __UARTCONF [baudrate No] [flow control No]__
    - Change UART settings.
      - [baudrate No]
        0  -> 4800bps
        1  -> 9600bps
        2  -> 14400bps(Not support)
        3  -> 19200bps
        4  -> 38400bps
        5  -> 57600bps
        6  -> 115200bps
        7  -> 230400bps
        8  -> 460800bps
        9  -> 921600bps
        10 -> 1Mbps
      - [flow control No]
        0 -> Flow control OFF.
        1 -> Flow control ON.

  - __I2CCONF [speed No]__
    - Change I2C settings.
      - [speed No]
        0  -> -100Kbps
        1  -> -1Mbps(400Kbps in SPRESENSE)

  - __SPICONF [data frame size No]__
    - Change SPI settings.
      - [data frame size No]
        0  -> 8bit
        1  -> 16bit

  - __DBGSEND [len] [hex binary(text)]__
    - Send arbitrary binary for debug.
      - [len]
        Binary len.
      - [hex binary(text)]
        Hex binary format in text.

  - __DBGRECV [0 or 1]__
    - Enable debug receive mode.
      - [0 or 1]
        On or Off debug receive mode.

  - __DBGFSEND [path]__
    - Send arbitrary file for debug.
      - [path]
        File path to send.

  - __CHGIF [interface num]__
    - Change Host I/F type.
      - [interface num]
        0 -> UART
        1 -> I2C
        2 -> SPI

  - __SETDIVSZ [one packet size]__
    - Change division size when TXFW.
      - [one packet size]
        Division size.

  - __SETADDR [hex target address]__
    - Change I2C target address.
      It is only effective when interface type is I2C.
      - [hex target address]
        Target address in one byte hex.

  - __SETDFS [data frame size]__
    - Change SPI data frame size.
      It is only effective when interface type is SPI.
      - [data frame size]
        Data frame size(Only 8 or 16).

  - __SETSPICLK [clock No]__
    - Change SPI frequency.
      It is only effective when interface type is SPI.
      - [clock No]
        0  -> 1300000Hz
        1  -> 2600000Hz
        2  -> 5200000Hz
        3  -> 6500000Hz
        4  -> 7800000Hz
        5  -> 9750000Hz
        6  -> 13000000Hz

  - __DBGTXFW [virtual_file_sz] [div_sz] [total_pkt_type] [pkt_no_type]__
    - Transmit FW for test.
      - [virtual_file_sz]
        Virtual file size
      - [div_sz]
        File division size
      - [total_pkt_type]
        OPR configuration of total packet size.
        0  -> Normal
        1  -> Always zero
        2  -> Invalid(Normal + 1)
        3  -> Change value(+) in middle
        4  -> Change value(-) in middle
      - [pkt_no_type]
        0  -> Normal
        1  -> Increment from zero
        2  -> No increment
        3  -> Over increment
        4  -> Over total packet size
