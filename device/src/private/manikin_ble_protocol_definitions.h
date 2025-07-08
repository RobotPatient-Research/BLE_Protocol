#ifndef MANIKIN_BLE_PROTOCOL_DEFINITIONS
#define MANIKIN_BLE_PROTOCOL_DEFINITIONS

/*
 * The polynomial used to calculate the CRC16 checksum
 */
#define MANIKIN_BLE_CRC_POLYNOMIAL 0x8d95U

/*
 * Given the message structure:
 * | START | LENGTH | SOF_MSG | CMD | MSG | EOF_MSG | CRC | STOP |   
 * 
 * These defines specify the contants START, SOF_MSG, EOF_MSG and STOP
 */
#define MANIKIN_BLE_PACKET_START_BYTE 0x01U
#define MANIKIN_BLE_PACKET_START_OF_MSG 0x3AU
#define MANIKIN_BLE_PACKET_END_OF_MSG 0x3BU
#define MANIKIN_BLE_PACKET_STOP_BYTE 0x17U

typedef enum {
    MANIKIN_BLE_CMD_START = 2U,
    MANIKIN_BLE_CMD_STOP = 3U,
    MANIKIN_BLE_CMD_DATA = 4U,
    MANIKIN_BLE_CMD_TIMEDATA = 5U
} manikin_ble_cmd_t;



#endif /* BLE_GATT_PROTOCOL_DEFINITIONS */