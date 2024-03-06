#ifndef __SX1278_H__
#define __SX1278_H__

#define PACKET_SIZE 60
#define TX_TIME_OUT 1000
#define RG_SIGNAL _IO('U', 11)
#define CTL_SIGNAL _IO('U', 12)

#define SLEEP_SET _IO('U', 13)
#define STANDBY_SET _IO('U', 14)
#define TRANSMIT_SET _IO('U', 15)
#define RXCONTINOUS_SET _IO('U', 16)
#define RXSINGLE_SET _IO('U', 17)

#define GET_RSSI            _IOR('a', '1', uint8_t *)
#define SPREADING_FACTOR    _IOW('a', '2', uint8_t *)
#define BAND_WIDTH          _IOW('a', '3', uint8_t *)
#define CODING_RATE         _IOW('a', '4', uint8_t *)
#define SYNC_WORD           _IOW('a', '5', uint8_t *)
#define FREQUENCY           _IOW('a', '6', int *)
#define POWER               _IOW('a', '7', uint8_t *)


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
#define SLEEP_MODE					0
#define STANDBY_MODE					1
#define FSTX						2
#define TRANSMIT_MODE					3
#define FSRX						4
#define RXCONTINUOUS_MODE				5
#define RXSINGLE_MODE					6
#define CAD						7
/*lora signal bandwidth */
#define BW_7_8_KHZ					0
#define BW_10_4_KHZ					1
#define BW_15_6_KHZ					2
#define BW_20_8_KHZ					3
#define BW_31_25_KHZ					4
#define BW_41_7_KHZ					5
#define BW_62_5_KHZ					6
#define BW_125_KHZ					7
#define BW_250_KHZ					8
#define BW_500_KHZ					9
/*lora codingrate */
#define CR_4_5						1
#define CR_4_6						2
#define CR_4_7						3
#define CR_4_8						4
/*lora spreadingFactor*/
#define SF_6						6
#define SF_7						7
#define SF_8						8
#define SF_9						9
#define SF_10						10
#define SF_11						11
#define SF_12						12
/*lora status */
#define LORA_OK						200
#define LORA_NOT_FOUND					404
#define LORA_LARGE_PAYLOAD				413
#define LORA_UNAVAILABLE				503
/*lora power gain */
#define POWER_11db					0xF6
#define POWER_14db					0xF9
#define POWER_17db					0xFC
#define POWER_20db					0xFF

#endif