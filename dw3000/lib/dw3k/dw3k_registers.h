#pragma once

struct DW3KFastCommand { uint8_t bits; };

struct DW3KRegisterAddress {
  uint8_t file;
  uint16_t offset;
};

struct DW3KOTPAddress { uint16_t index; };

// DW3000 User Manual 9. "Fast Commands"
static constexpr DW3KFastCommand
  DW3K_TXRXOFF     = {0x00},
  DW3K_TX          = {0x01},
  DW3K_RX          = {0x02},
  DW3K_DTX         = {0x03},
  DW3K_DRX         = {0x04},
  DW3K_DTX_TS      = {0x05},
  DW3K_DRX_TS      = {0x06},
  DW3K_DTX_RS      = {0x07},
  DW3K_DRX_RS      = {0x08},
  DW3K_DTX_REF     = {0x09},
  DW3K_DRX_REF     = {0x0A},
  DW3K_CCA_TX      = {0x0B},
  DW3K_TX_W4R      = {0x0C},
  DW3K_DTX_W4R     = {0x0D},
  DW3K_DTX_TS_W4R  = {0x0E},
  DW3K_DTX_RS_W4R  = {0x0F},
  DW3K_DTX_REF_W4R = {0x10},
  DW3K_CCA_TX_W4R  = {0x11},
  DW3K_CLR_IRQS    = {0x12},
  DW3K_DB_TOGGLE   = {0x13};

// DW3000 User Manual 8.1. "Register map overview"
static constexpr DW3KRegisterAddress
  DW3K_DEV_ID        = {0x00, 0x00},
  DW3K_EUI_64        = {0x00, 0x04},
  DW3K_PANADR        = {0x00, 0x0C},
  DW3K_SYS_CFG       = {0x00, 0x10},
  DW3K_FF_CFG        = {0x00, 0x14},
  DW3K_SPI_RD_CRC    = {0x00, 0x18},
  DW3K_SYS_TIME      = {0x00, 0x1C},
  DW3K_TX_FCTRL_64   = {0x00, 0x24},
  DW3K_DX_TIME       = {0x00, 0x2C},
  DW3K_DREF_TIME     = {0x00, 0x30},
  DW3K_RX_FWTO       = {0x00, 0x34},
  DW3K_SYS_CTRL      = {0x00, 0x38},
  DW3K_SYS_ENABLE_64 = {0x00, 0x3C},
  DW3K_SYS_STATUS_64 = {0x00, 0x44},
  DW3K_RX_FINFO      = {0x00, 0x4C},
  DW3K_RX_STAMP_64   = {0x00, 0x64},
  DW3K_RX_RAWST      = {0x00, 0x70},
  DW3K_TX_STAMP_64   = {0x00, 0x74},
  DW3K_TX_RAWST      = {0x01, 0x00},
  DW3K_TX_ANTD       = {0x01, 0x04},
  DW3K_ACK_RESP_T    = {0x01, 0x08},
  DW3K_TX_POWER      = {0x01, 0x0C},
  DW3K_CHAN_CTRL     = {0x01, 0x14},
  DW3K_LE_PEND01     = {0x01, 0x18},
  DW3K_LE_PEND23     = {0x01, 0x1C},
  DW3K_SPI_COLLISION = {0x01, 0x20},
  DW3K_RDB_STATUS    = {0x01, 0x24},
  DW3K_RDB_DIAG      = {0x01, 0x28},
  DW3K_AES_CFG       = {0x01, 0x30},
  DW3K_AES_IV0       = {0x01, 0x34},
  DW3K_AES_IV1       = {0x01, 0x38},
  DW3K_AES_IV2       = {0x01, 0x3C},
  DW3K_AES_IV3       = {0x01, 0x40},
  DW3K_DMA_CFG_64    = {0x01, 0x44},
  DW3K_AES_START     = {0x01, 0x4C},
  DW3K_AES_STS       = {0x01, 0x50},
  DW3K_AES_KEY_128   = {0x01, 0x54},

  DW3K_STS_CFG       = {0x02, 0x00},
  DW3K_STS_CTRL      = {0x02, 0x04},
  DW3K_STS_STS       = {0x02, 0x08},
  DW3K_STS_KEY_128   = {0x02, 0x0C},
  DW3K_STS_IV_128    = {0x02, 0x1C},

  DW3K_DGC_CFG       = {0x03, 0x18},
  DW3K_DGC_CFG0      = {0x03, 0x1C},  // from deca_driver
  DW3K_DGC_CFG1      = {0x03, 0x20},  // from deca_driver
  DW3K_DGC_LUT0      = {0x03, 0x38},  // from deca_driver
  DW3K_DGC_LUT1      = {0x03, 0x3C},  // from deca_driver
  DW3K_DGC_LUT2      = {0x03, 0x40},  // from deca_driver
  DW3K_DGC_LUT3      = {0x03, 0x44},  // from deca_driver
  DW3K_DGC_LUT4      = {0x03, 0x48},  // from deca_driver
  DW3K_DGC_LUT5      = {0x03, 0x4C},  // from deca_driver
  DW3K_DGC_LUT6      = {0x03, 0x50},  // from deca_driver
  DW3K_DGC_DBG       = {0x03, 0x60},

  DW3K_EC_CTRL       = {0x04, 0x00},
  DW3K_RX_CAL        = {0x04, 0x0C},
  DW3K_RX_CAL_RESI   = {0x04, 0x14},
  DW3K_RX_CAL_RESQ   = {0x04, 0x1C},
  DW3K_RX_CAL_STS    = {0x04, 0x20},

  DW3K_GPIO_MODE     = {0x05, 0x00},
  DW3K_GPIO_PULL_EN  = {0x05, 0x04},
  DW3K_GPIO_DIR      = {0x05, 0x08},
  DW3K_GPIO_OUT      = {0x05, 0x0C},
  DW3K_GPIO_IRQE     = {0x05, 0x10},
  DW3K_GPIO_ISTS     = {0x05, 0x14},
  DW3K_GPIO_ISEN     = {0x05, 0x18},
  DW3K_GPIO_IMODE    = {0x05, 0x1C},
  DW3K_GPIO_IBES     = {0x05, 0x20},
  DW3K_GPIO_ICLR     = {0x05, 0x24},
  DW3K_GPIO_IDBE     = {0x05, 0x28},
  DW3K_GPIO_RAW      = {0x05, 0x2C},

  DW3K_DTUNE0        = {0x06, 0x00},
  DW3K_RX_SFD_TOC    = {0x06, 0x02},
  DW3K_PRE_TOC       = {0x06, 0x04},
  DW3K_DTUNE3        = {0x06, 0x0C},
  DW3K_DTUNE5        = {0x06, 0x14},
  DW3K_DRX_CAR_INT   = {0x06, 0x29},

  DW3K_RF_ENABLE     = {0x07, 0x00},
  DW3K_RF_CTRL_MASK  = {0x07, 0x04},
  DW3K_RF_RX_CTRL2   = {0x07, 0x10},  // from deca_driver
  DW3K_RF_SWITCH     = {0x07, 0x14},
  DW3K_RF_TX_CTRL1   = {0x07, 0x1A},
  DW3K_RF_TX_CTRL2   = {0x07, 0x1C},
  DW3K_TX_TEST       = {0x07, 0x28},
  DW3K_SAR_TEST      = {0x07, 0x34},
  DW3K_LDO_TUNE_64   = {0x07, 0x40},
  DW3K_LDO_CTRL      = {0x07, 0x48},
  DW3K_LDO_RLOAD     = {0x07, 0x51},  // double check??

  DW3K_SAR_CTRL      = {0x08, 0x00},
  DW3K_SAR_STATUS    = {0x08, 0x04},
  DW3K_SAR_READING   = {0x08, 0x08},
  DW3K_SAR_WAKE_RD   = {0x08, 0x0C},
  DW3K_PGC_CTRL      = {0x08, 0x10},
  DW3K_PGC_STATUS    = {0x08, 0x14},
  DW3K_PG_TEST       = {0x08, 0x18},
  DW3K_PG_CAL_TARGET = {0x08, 0x1C},

  DW3K_PLL_CFG       = {0x09, 0x00},
  DW3K_PLL_CC        = {0x09, 0x04},
  DW3K_PLL_CAL       = {0x09, 0x08},
  DW3K_XTAL          = {0x09, 0x14},

  DW3K_AON_DIG_CFG   = {0x0A, 0x00},
  DW3K_AON_CTRL      = {0x0A, 0x04},
  DW3K_AON_RDATA     = {0x0A, 0x08},
  DW3K_AON_ADDR      = {0x0A, 0x0C},
  DW3K_AON_WDATA     = {0x0A, 0x10},
  DW3K_AON_CFG       = {0x0A, 0x14},

  DW3K_OTP_WDATA     = {0x0B, 0x00},
  DW3K_OTP_ADDR      = {0x0B, 0x04},
  DW3K_OTP_CFG       = {0x0B, 0x08},
  DW3K_OTP_STAT      = {0x0B, 0x0C},
  DW3K_OTP_RDATA     = {0x0B, 0x10},
  DW3K_OTP_SRDATA    = {0x0B, 0x14},

  DW3K_IP_TS_64      = {0x0C, 0x00},
  DW3K_STS_TS_64     = {0x0C, 0x08},
  DW3K_STS1_TS_64    = {0x0C, 0x10},
  DW3K_TDOA          = {0x0C, 0x18},
  DW3K_PDOA          = {0x0C, 0x1E},  // double check?
  DW3K_CIA_DIAG0     = {0x0C, 0x20},
  DW3K_CIA_DIAG1     = {0x0C, 0x24},
  DW3K_IP_DIAG0      = {0x0C, 0x28},
  DW3K_IP_DIAG1      = {0x0C, 0x2C},
  DW3K_IP_DIAG2      = {0x0C, 0x30},
  DW3K_IP_DIAG3      = {0x0C, 0x34},
  DW3K_IP_DIAG4      = {0x0C, 0x38},
  DW3K_IP_DIAG8      = {0x0C, 0x48},
  DW3K_IP_DIAG12     = {0x0C, 0x58},
  DW3K_STS_DIAG0     = {0x0C, 0x5C},
  DW3K_STS_DIAG1     = {0x0C, 0x60},
  DW3K_STS_DIAG2     = {0x0C, 0x64},
  DW3K_STS_DIAG3     = {0x0C, 0x68},
  DW3K_STS_DIAG4     = {0x0D, 0x00},
  DW3K_STS_DIAG8     = {0x0D, 0x10},
  DW3K_STS_DIAG12    = {0x0D, 0x20},
  DW3K_STS1_DIAG0    = {0x0D, 0x38},
  DW3K_STS1_DIAG1    = {0x0D, 0x3C},
  DW3K_STS1_DIAG2    = {0x0D, 0x40},
  DW3K_STS1_DIAG3    = {0x0D, 0x44},
  DW3K_STS1_DIAG4    = {0x0D, 0x48},
  DW3K_STS1_DIAG8    = {0x0D, 0x58},
  DW3K_STS1_DIAG12   = {0x0D, 0x68},
  DW3K_CIA_CONF      = {0x0E, 0x00},
  DW3K_FP_CONF       = {0x0E, 0x04},
  DW3K_IP_CONF_64    = {0x0E, 0x0C},
  DW3K_STS_CONF0     = {0x0E, 0x12},
  DW3K_STS_CONF1     = {0x0E, 0x16},
  DW3K_CIA_ADJUST    = {0x0E, 0x1A},
  DW3K_PGF_DELAY_COMP_64 = {0x0E, 0x1E},

  DW3K_EVC_CTRL      = {0x0F, 0x00},
  DW3K_EVC_PHE       = {0x0F, 0x04},
  DW3K_EVC_RSE       = {0x0F, 0x06},
  DW3K_EVC_FCG       = {0x0F, 0x08},
  DW3K_EVC_FCE       = {0x0F, 0x0A},
  DW3K_EVC_FFR       = {0x0F, 0x0C},
  DW3K_EVC_OVR       = {0x0F, 0x0E},
  DW3K_EVC_STO       = {0x0F, 0x10},
  DW3K_EVC_PTO       = {0x0F, 0x12},
  DW3K_EVC_FWTO      = {0x0F, 0x14},
  DW3K_EVC_TXFS      = {0x0F, 0x16},
  DW3K_EVC_HPW       = {0x0F, 0x18},
  DW3K_EVC_SWCE      = {0x0F, 0x1A},
  DW3K_EVC_RES1      = {0x0F, 0x1C},
  DW3K_DIAG_TMC      = {0x0F, 0x24},
  DW3K_EVC_CPQE      = {0x0F, 0x28},
  DW3K_EVC_VWARN     = {0x0F, 0x2A},
  DW3K_SPI_MODE      = {0x0F, 0x2C},
  DW3K_SYS_STATE     = {0x0F, 0x30},
  DW3K_FCMD_STAT     = {0x0F, 0x3C},
  DW3K_CTR_DBG       = {0x0F, 0x48},
  DW3K_SPICRCINIT    = {0x0F, 0x4C},

  DW3K_SOFT_RST      = {0x11, 0x00},
  DW3K_CLK_CTRL      = {0x11, 0x04},
  DW3K_SEQ_CTRL      = {0x11, 0x08},
  DW3K_TXFSEQ        = {0x11, 0x12},  // double check??
  DW3K_LED_CTRL      = {0x11, 0x16},
  DW3K_RX_SNIFF      = {0x11, 0x1A},
  DW3K_BIAS_CTRL     = {0x11, 0x1F},  // From deca_driver

  DW3K_RX_BUFFER0    = {0x12, 0x00},

  DW3K_RX_BUFFER1    = {0x13, 0x00},

  DW3K_TX_BUFFER     = {0x14, 0x00},

  DW3K_ACC_MEM       = {0x15, 0x00},

  DW3K_SCRATCH_RAM   = {0x16, 0x00},

  DW3K_AES_KEY1      = {0x17, 0x00},
  DW3K_AES_KEY2      = {0x17, 0x10},
  DW3K_AES_KEY3      = {0x17, 0x20},
  DW3K_AES_KEY4      = {0x17, 0x30},
  DW3K_AES_KEY5      = {0x17, 0x40},
  DW3K_AES_KEY6      = {0x17, 0x50},
  DW3K_AES_KEY7      = {0x17, 0x60},
  DW3K_AES_KEY8      = {0x17, 0x70},

  DW3K_INDIRECT_PTR_A = {0x1D, 0},
  DW3K_INDIRECT_PTR_B = {0x1E, 0},

  DW3K_FINT_STAT     = {0x1F, 0x00},
  DW3K_PTR_ADDR_A    = {0x1F, 0x04},
  DW3K_PTR_OFFSET_A  = {0x1F, 0x08},
  DW3K_PTR_ADDR_B    = {0x1F, 0x0C},
  DW3K_PTR_OFFSET_B  = {0x1F, 0x10};

static constexpr DW3KOTPAddress
  DW3K_OTP_EUID_LO             = {0x00},
  DW3K_OTP_EUID_HI             = {0x01},
  DW3K_OTP_EUID_ALT_LO         = {0x02},
  DW3K_OTP_EUID_ALT_HI         = {0x03},
  DW3K_OTP_LDO_TUNE_LO         = {0x04},
  DW3K_OTP_LDO_TUNE_HI         = {0x05},
  DW3K_OTP_CHIP_ID             = {0x06},
  DW3K_OTP_LOT_ID              = {0x07},
  DW3K_OTP_VBAT                = {0x08},
  DW3K_OTP_TEMP                = {0x09},
  DW3K_OTP_BIAS_TUNE           = {0x0A},
  DW3K_OTP_RFLOOP_DELAY        = {0x0B},
  DW3K_OTP_AOA_ISO             = {0x0C},
  DW3K_OTP_WS_LOT_LO           = {0x0D},
  DW3K_OTP_WS_LOT_HI           = {0x0E},
  DW3K_OTP_WS_WAFER            = {0x0F},
  DW3K_OTP_XTAL_TRIM           = {0x1E},
  DW3K_OTP_REVISION            = {0x1F},
  DW3K_OTP_RX_DGC_CFG_BASE     = {0x20},
  DW3K_OTP_RX_DGC_LUT_CH5_BASE = {0x27},
  DW3K_OTP_RX_DGC_LUT_CH9_BASE = {0x2E},
  DW3K_OTP_QSR                 = {0x60},
  DW3K_OTP_Q_RR                = {0x61},
  DW3K_OTP_PLL_LOCK_CODE       = {0x35},
  DW3K_OTP_AES_KEY_BASE        = {0x78};
