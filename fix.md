# 修改cc2530的PA设置
 1. hal/target/cc2530eb/config/hal_board_cfg.h文件中打开HAL_PA_LNA宏定义
 2. mac/low level/system/mac_radio_defs.c:575
 ```c
    OBSSEL4       = OBSSEL_OBS_CTRL1;
```
改为
```c
    OBSSEL0       = OBSSEL_OBS_CTRL1;
```
 3. mac/high level/mac_pib.c文件中static CODE const macPib_t macPibDefaults
 ```c
    20,                                          /* phyTransmitPower chenjing */
 ```
