#ifndef __SX1278_H__
#define __SX1278_H__

#define PACKET_SIZE 60
#define TX_TIME_OUT 1000
#define RG_SIGNAL _IO('U', 11)
#define CTL_SIGNAL _IO('U', 12)

#define DEV_NAME		"lora"
/* lora register address */
#define RegFifo						0x00
#define RegOpMode					0x01
#define	RegFrMsb					0x06
#define	RegFrMid					0x07
#define	RegFrLsb					0x08
#define	RegPaConfig					0x09
#define RegPaRamp					0x0A
#define RegOcp						0x0B
#define RegLna						0x0C
#define RegFiFoAddPtr					0x0D
#define RegFiFoTxBaseAddr				0x0E
#define RegFiFoRxBaseAddr				0x0F
#define RegFiFoRxCurrentAddr				0x10
#define RegIrqFlags					0x12
#define RegRxNbBytes					0x13
#define RegPktRssiValue					0x1A
#define	RegModemConfig1					0x1D
#define RegModemConfig2					0x1E
#define RegModemConfig3                 0x26
#define RegSymbTimeoutL					0x1F
#define RegPreambleMsb					0x20
#define RegPreambleLsb					0x21
#define RegPayloadLength				0x22
#define RegDioMapping1					0x40
#define RegDioMapping2					0x41
#define RegSyncWord                     0x39
#define RegVersion					0x42
/*lora operation mode */
typedef enum {
    SLEEP_MODE,
    STANDBY_MODE,
    FSTX,
    TRANSMIT_MODE,
    FSRX,
    RXCONTINUOUS_MODE,
    RXSINGLE_MODE,
    CAD
} lora_mode_t;
/*lora signal bandwidth */
typedef enum {
    BW_7_8_KHZ,
    BW_10_4_KHZ,
    BW_15_6_KHZ,
    BW_20_8_KHZ,
    BW_31_25_KHZ,
    BW_41_7_KHZ,
    BW_62_5_KHZ,
    BW_125_KHZ,
    BW_250_KHZ,
    BW_500_KHZ
} bandwidth_t;
/*lora codingrate */
typedef enum {
    CR_4_5 = 1,
    CR_4_6,
    CR_4_7,
    CR_4_8
} codingrate_t;
/*lora spreadingFactor*/
typedef enum {
    SF_6 = 6,
    SF_7,
    SF_8,
    SF_9,
    SF_10,
    SF_11,
    SF_12
} SF_t;
/*lora status */
typedef enum {
    LORA_OK = 200,
    LORA_NOT_FOUND = 404,
    LORA_LARGE_PAYLOAD = 413,
    LORA_UNAVAILABLE = 503
} status_t;
/*lora power gain */
typedef enum {
    POWER_11db = 0xF6,
    POWER_14db = 0xF9,
    POWER_17db = 0xFC,
    POWER_20db = 0xFF
} power_t;


#define GOTO_MODE           _IOW('m', '1', lora_mode_t *)
#define GET_RSSI            _IOR('a', '1', uint8_t *)
#define SPREADING_FACTOR    _IOW('a', '2', SF_t *)
#define BAND_WIDTH          _IOW('a', '3', bandwidth_t *)
#define CODING_RATE         _IOW('a', '4', codingrate_t *)
#define SYNC_WORD           _IOW('a', '5', uint8_t *)
#define FREQUENCY           _IOW('a', '6', int *)
#define POWER               _IOW('a', '7', power_t *)
#define GET_STATUS          _IOR('a', '8', status_t *)

#endif