/*
 * Copyright (C) 2014 Intel Mobile Communications GmbH
 * Copyright (C) 2015 FuZhou Rockchip Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _IRQ_VECTORS_H
#define _IRQ_VECTORS_H
/* AUTO GENERATED - DO NOT MODIFY */
/* This MUST be sync across full system or system could break! */

#define IRQ_MOVE_CLEANUP_VECTOR 32
#define HIRQ 33
#define VMM_IPI_SCHEDULE 34
#define VMM_PERF_CTR_IRQ 117
#define VMM_SOCWATCH_IRQ 118
#define LINUX_OS_LOCAL_TIMER 119
#define LINUX_OS_REBOOT_IPI 120
#define LINUX_OS_CALL_FUNC_SINGLE 121
#define LINUX_OS_CALL_FUNC 122
#define LINUX_OS_RESCHEDULE_IPI 123
#define VMM_LAPIC_SPURIOUS 255
#define NOC_TRACEALARM 0
#define NOC_ERR_AUDIO_IDI 0
#define SDIO_INT 0
#define AFE_LS_ALERT 0
#define VM_ID_VIOLATION 0
#define OCP_DATA_ABORT 0
#define NSEC2 0
#define NSEC_CEU2_EOP_IRQ 0
#define NSEC_CEU2_GLOBAL_IRQ 0
#define NSEC_CEU2_RX_IRQ 0
#define NSEC_CEU2_TX_IRQ 0
#define SEC_INT 0
#define NSEC_CEU1_RX_IRQ 0
#define NSEC_CEU1_TX_IRQ 0
#define NSEC_CEU1_GLOBAL_IRQ 0
#define NSEC_CEU1_EOP_IRQ 0
#define SEC_CEU1_RX_IRQ 0
#define SEC_CEU1_TX_IRQ 0
#define MC_CONTROLLER_INT 0
#define MC_INT 0
#define WDT1 0
#define PROF_CORE3_INT 0
#define PROF_CORE2_INT 0
#define PROF_CORE1_INT 0
#define PROF_CORE0_INT 0
#define GT1_INT5 0
#define GT1_INT6 0
#define GT1_INT7 0
#define CGU_INT 0
#define OTG_SRP_INT 0
#define ACI_INT 0
#define SDIO_DAT3 0
#define SDIO_DAT1 0
#define SEC_CEU1_GLOBAL_IRQ 0
#define SEC_CEU1_EOP_IRQ 0
#define DIF_TX_XREQ_INT 0
#define DIF_RX_XREQ_INT 0
#define DIF_RX_BREQ_INT 0
#define USIF1_WK 0
#define USIF2_WK 0
#define USIF1_INT 35
#define USIF2_INT 36
#define DIF_ERR_INT 37
#define HEVC_DEC_INT 38
#define PWM_INT 39
#define RGA_INT 40
#define NOC_ERR_APS_L1 41
#define NOC_ERR_APS_L2 42
#define CIF_ISP_INT 43
#define CIF_JPEG_ERR_INT 44
#define CIF_JPEG_STAT_INT 45
#define CIF_MI_INT 46
#define CIF_MIPI_INT 47
#define FMRX_EV_TUNE 48
#define FMTX_EV_CALIB 49
#define FMTX_EV_TUNE 50
#define USB_INT 51
#define I2C_RAWIRQ 58
#define USB_HS_RESUME 62
#define SDMMC_DETECT 64
#define BTIF_ERR_SRQ 65
#define USB_ID 66
#define BTIF_REQ_IRQ 67
#define BTIF_MULT_SRC_SRQ 68
#define PMU_INT1 69
#define KPD_INT 70
#define SCU_INT3 73
#define MRDY_INT 74
#define WLAN_INT 75
#define IDIDBB_RX_REQ_IRQ 76
#define IDIDBB_RX2_REQ_IRQ 77
#define IDIDBB_RX3_REQ_IRQ 78
#define IDIDBB_TX_REQ_IRQ 79
#define IDIDBB_TX2_REQ_IRQ 80
#define IDIDBB_TX3_REQ_IRQ 81
#define IDIDBB_MULT_SRC_SRQ 82
#define IDIDBB_ERR_SRQ 83
#define GNSS_WUP 84
#define GNSS_NOTIFICATION 85
#define GNSS_ERR 86
#define USIF_ALL_INT 87
#define IDIABB_ERR_SRQ0 88
#define IDIABB_TX_REQ_IRQ0 89
#define IDIABB_RX_REQ_IRQ0 90
#define IDIABB_ERR_SRQ1 91
#define IDIABB_TX_REQ_IRQ1 92
#define IDIABB_RX_REQ_IRQ1 93
#define IDIABB_MULT_SRC_SRQ 94
#define FMR_INT 95
#define FMRX_EV_ONESHOT 96
#define AUDIODSP_INT7 97
#define AUDIODSP_INT6 98
#define AUDIODSP_INT3 99
#define SDMMC_INT 100
#define EMMC_INT 101
#define OCT_INT 102
#define NOC_STATALARM 103
#define GPU_INT 104
#define DMA1_ERR_INT 105
#define DMA1_CH0_7_INT 106
#define AUDIODSP_INT2 107
#define AUDIODSP_INT1 108
#define AUDIODSP_INT0 109
#define VID_HINT_ENC 110
#define VID_HINT_DEC 111
#define VOP_INT 112
#define NANDCTRL_INT 113
#define I2C1_INT 114
#define I2C3_INT 115
#define I2C4_INT 116
#define STM_INT0 124
#define STM_INT1 125
#define CC1_T0INT 126
#define CC1_T1INT 127
#define SEC_T_INT 134
#define I2C2_INT 136
#define LINE_GT1_0 137
#define SCU_INT0 138
#define PMU_INT0 139
#define PROF_ENV_INT 140
#define WDT0 141
#define RTC_INT 142
#define SPCU_INT 143
#define WUP_DBB 144
#define USIM1_IN_INT 145
#define USIM2_IN_INT 146
#define DMA4_CH0_3_INT 148
#define DMA4_ERR_INT 149
#define CC0_CCINT0_7 150
#define _2GDSP_INT2 151
#define _2GDSP_INT3 152
#define GSI_GP2_INT 153
#define GSI_GP3_INT 154
#define GSI_GP4_INT 156
#define GSI_TLINT 157
#define GUC_ERR_INT 158
#define GUC_INT0 159
#define GUC_INT1 160
#define SWDT_INT0 161
#define SWDT_INT1 162
#define SWDT_INT2 163
#define SCU_INT1 164
#define U2H 168
#define DTXS 170
#define DRXS 171
#define GT0_INT0 173
#define GT0_INT1 174
#define GT0_INT2 175
#define GT0_INT3 176
#define GT0_INT4 177
#define GT0_INT5 178
#define GT0_INT6 179
#define GT0_INT7 180
#define GT1_INT0 181
#define GT1_INT1 182
#define GT1_INT2 183
#define GT1_INT3 184
#define GT1_INT4 185
#define MPUL_INT 186
#define G3FP 187
#define MPDL_INT 188
#define U2H1 189
#define U2H2 190
#define CC0_T0INT 192
#define CC0_T1INT 193
#define COMMTX 197
#define COMMRX 198
#define DRF_RXSTAT2G 199
#define DRF_ERR2G 204
#define DRF_LPINT2G 205
#define _2GDSP_INT0 217
#define _2GDSP_INT1 218
#define SCU_INT2 219
#define SCU_INT4 220
#define GSI_GP0_INT 222
#define GSI_T_INT2 223
#define AUDIODSP_INT4 236
#define AUDIODSP_INT5 237
#define USIM1_ERR_INT 238
#define USIM2_ERR_INT 239
#define GSI_T_INT1 246
#define GSI_GP1_INT 247
#define USIM1_FIFO_INT 249
#define USIM1_OK_INT 250
#define USIM2_FIFO_INT 251
#define USIM2_OK_INT 252

#endif /* _IRQ_VECTORS_H */
