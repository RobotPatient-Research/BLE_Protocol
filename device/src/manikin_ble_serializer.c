#include "manikin_ble_serializer.h"
#include "private/manikin_ble_protocol_definitions.h"
#include <zephyr/sys/crc.h>
#include <zephyr/sys/byteorder.h>
#include <errno.h>
#include <string.h>

static inline manikin_ble_cmd_t
get_ble_cmd_from_msg (const uint8_t *in_buff)
{
    return in_buff[MANIKIN_BLE_CMD_POS];
}

static int
decode_msg_from_buffer (const uint8_t *in_buff, const size_t len, manikin_ble_msg_t *out_buff)
{
    /* Get command from message */
    manikin_ble_cmd_t cmd = 0;
    cmd                   = get_ble_cmd_from_msg(in_buff);
    if (cmd == 0 || cmd >= MANIKIN_BLE_CMD_INVALID)
    {
        return 1;
    }

    size_t msg_len = in_buff[MANIKIN_BLE_LENGTH_BYTE_POS]-1;
    if (msg_len > 0)
    {
        /* Copy payload from message to the output-buffer */
        memcpy(out_buff->payload, &in_buff[MANIKIN_BLE_PAYLOAD_POS], msg_len);
        out_buff->payload_size = msg_len;
        uint8_t crc_pos        = MANIKIN_BLE_CRC_POS(msg_len);
        /* Check CRC-16 */
        uint16_t received_crc = sys_get_le16(&in_buff[crc_pos]);
        uint16_t calc_crc     = crc16_ccitt(MANIKIN_BLE_CRC_SEED, out_buff->payload, out_buff->payload_size);
        if (received_crc != calc_crc)
        {
            return 1;
        }
    }
    out_buff->cmd = cmd;

    return 0;
}

static inline int
packet_is_invalid (const uint8_t *in_buff, size_t len)
{
    uint8_t  length  = in_buff[MANIKIN_BLE_LENGTH_BYTE_POS];
    uint16_t eof_pos = MANIKIN_BLE_EOF_MSG_POS(length-1);
    if (eof_pos > len)
    {
        return 1;
    }
    else if (in_buff[MANIKIN_BLE_START_BYTE_POS] == MANIKIN_BLE_PACKET_START_BYTE
             && in_buff[MANIKIN_BLE_SOF_MSG_POS] == MANIKIN_BLE_PACKET_SOF_MSG
             && (in_buff[eof_pos] == MANIKIN_BLE_PACKET_EOF_MSG))
    {
        return 0;
    }
    return 1;
}

int
find_valid_frame_offset (const uint8_t *data, size_t len)
{
    for (size_t i = 0; i + 2 < len; i++)
    {
        if (data[i] == MANIKIN_BLE_PACKET_START_BYTE)
        {
            uint8_t length = data[i + 1];

            // Ensure the full frame is within bounds
            if (i + 1 + length < len)
            {
                if (packet_is_invalid(&data[i], len - i) == 0)
                {
                    return (int)i; // Return the start-byte position
                }
            }
        }
    }
    return -1; // No valid frame found
}

int
manikin_ble_decode_msg (const uint8_t *in_buff, const size_t len, manikin_ble_msg_t *out_buff)
{
    if ((len < MANIKIN_BLE_PACKET_MINIMAL_SIZE) || (in_buff == NULL || out_buff == NULL))
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
    if (length < 0 || length >= MANIKIN_BLE_EOF_MSG_POS(length-1))
    {
        return EINVAL;
    }

    int ret = decode_msg_from_buffer((in_buff + offset), (len - offset), out_buff);
    if (ret)
    {
        /* Malformed package */
        return EINVAL;
    }
    return 0;
}

int
manikin_ble_encode_msg (const size_t max_len, const manikin_ble_msg_t *in_buff, uint8_t *out_buff)
{
    /* First check for null parameters */
    if ((in_buff == NULL || out_buff == NULL))
    {
        return EINVAL;
    }

    /* Make sure payload fits within buffer which is passed as output buffer */
    if ((max_len < (MANIKIN_BLE_PACKET_MINIMAL_SIZE + in_buff->payload_size))
        || in_buff->payload_size > MANIKIN_BLE_ATT_MTU)
    {
        return EINVAL;
    }

    /* Construct packet header */
    out_buff[MANIKIN_BLE_START_BYTE_POS]                       = MANIKIN_BLE_PACKET_START_BYTE;
    out_buff[MANIKIN_BLE_LENGTH_BYTE_POS]                      = in_buff->payload_size+1; /* +1 for command byte */
    out_buff[MANIKIN_BLE_SOF_MSG_POS]                          = MANIKIN_BLE_PACKET_SOF_MSG;
    out_buff[MANIKIN_BLE_CMD_POS]                              = in_buff->cmd;
    out_buff[MANIKIN_BLE_EOF_MSG_POS(in_buff->payload_size)]   = MANIKIN_BLE_PACKET_EOF_MSG;
    out_buff[MANIKIN_BLE_STOP_BYTE_POS(in_buff->payload_size)] = MANIKIN_BLE_PACKET_STOP_BYTE;

    /* Copy payload into buffer */
    memcpy(&out_buff[MANIKIN_BLE_PAYLOAD_POS], in_buff->payload, in_buff->payload_size);

    /* Calculate CRC16 and insert into buffer */
    uint16_t crc     = crc16_ccitt(MANIKIN_BLE_CRC_SEED, in_buff->payload, in_buff->payload_size);
    uint8_t *crc_ptr = &out_buff[MANIKIN_BLE_CRC_POS(in_buff->payload_size)];
    crc_ptr[0]       = crc & 0xFF;        // LSB
    crc_ptr[1]       = (crc >> 8) & 0xFF; // MSB

    return 0;
}