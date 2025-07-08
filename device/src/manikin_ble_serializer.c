#include "manikin_ble_serializer.h"
#include "private/manikin_ble_protocol_definitions.h"
#include <errno.h>
#include <string.h>

static inline manikin_ble_cmd_t get_ble_cmd_from_msg(const uint8_t *in_buff)
{
    return in_buff[MANIKIN_BLE_CMD_POS];
}

static int decode_msg_from_buffer(const uint8_t *in_buff, const size_t len, manikin_ble_msg_t *out_buff)
{
    manikin_ble_cmd_t cmd = 0;
    cmd = get_ble_cmd_from_msg(in_buff);
    if(cmd == 0 || cmd >= MANIKIN_BLE_CMD_INVALID) {
        return 1;
    }
    size_t msg_len = in_buff[MANIKIN_BLE_LENGTH_BYTE_POS];
    if(msg_len != 0) {
        memcpy(out_buff->payload, &in_buff[MANIKIN_BLE_PAYLOAD_POS], msg_len);
    }
    out_buff->cmd = cmd;
    out_buff->payload_size = msg_len;
    return 0;
}

static inline int packet_is_invalid(const uint8_t *in_buff, size_t len)
{
    uint8_t length = in_buff[MANIKIN_BLE_LENGTH_BYTE_POS];
    uint16_t eof_pos = MANIKIN_BLE_EOF_MSG_POS(length);
    if (eof_pos > len)
    {
        return 1;
    }
    else if (in_buff[MANIKIN_BLE_START_BYTE_POS] == MANIKIN_BLE_PACKET_START_BYTE &&
             in_buff[MANIKIN_BLE_SOF_MSG_POS] == MANIKIN_BLE_PACKET_SOF_MSG &&
             (in_buff[eof_pos-1] == MANIKIN_BLE_PACKET_EOF_MSG))
    {
        return 0;
    }
    return 1;
}

int find_valid_frame_offset(const uint8_t *data, size_t len)
{
    for (size_t i = 0; i + 2 < len; i++)
    {
        if (data[i] == 0x01)
        {
            uint8_t length = data[i + 1];

            // Ensure the full frame is within bounds
            if (i + 1 + length < len)
            {
                if (packet_is_invalid(&data[i], len-i) == 0)
                {
                    return (int)i; // Return the start-byte position
                }
            }
        }
    }
    return -1; // No valid frame found
}

int manikin_ble_decode_msg(const uint8_t *in_buff, const size_t len, manikin_ble_msg_t *out_buff)
{
    if ((len < 7) || (in_buff == NULL || out_buff == NULL))
    {
        return EINVAL;
    }

    int offset = 0;

    /* Check if the buffer is not-aligned */
    if (packet_is_invalid(in_buff, len))
    {
        /* Search for the next two start-bytes */
        offset = find_valid_frame_offset(in_buff, len);
        if (offset < 0)
        {
            return EINVAL;
        }
    }

    /* Check if length of payload is larger than provided packet */
    uint8_t length = in_buff[offset + MANIKIN_BLE_LENGTH_BYTE_POS];
    if (length < 0 || length >= MANIKIN_BLE_EOF_MSG_POS(length))
    {
        return EINVAL;
    }

    int ret = decode_msg_from_buffer((in_buff+offset), (len-offset), out_buff);
    if(ret) {
        /* Malformed package */
        return EINVAL;
    }
    return 0;
}

int manikin_ble_encode_msg(const size_t max_len, const manikin_ble_msg_t *in_buff, uint8_t *out_buff)
{
    if (max_len < 8 || (in_buff || out_buff == NULL))
    {
        return EINVAL;
    }

    return 0;
}