#ifndef MANIKIN_BLE_SERIALIZER_H
#define MANIKIN_BLE_SERIALIZER_H
#include <stdint.h>
#include <stddef.h>
#include "private/manikin_ble_protocol_definitions.h"

typedef struct
{
    manikin_ble_cmd_t cmd;
    uint8_t           payload[CONFIG_MANIKIN_BLE_MAX_TEMPORARY_BUFFER_SIZE];
    uint8_t           payload_size;
} manikin_ble_msg_t;

int manikin_ble_decode_msg(const uint8_t *in_buff, const size_t len, manikin_ble_msg_t *out_buff);

int manikin_ble_encode_msg(const size_t max_len, const manikin_ble_msg_t *in_buff, uint8_t *out_buff);

#endif /* MANIKIN_BLE_SERIALIZER_H */