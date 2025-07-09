#include <zephyr/ztest.h>
#include <zephyr/sys/crc.h>
#include <zephyr/sys/byteorder.h>
#include <string.h>
#include "manikin_ble_serializer.h"

ZTEST_SUITE(manikin_ble_encode_tests, NULL, NULL, NULL, NULL, NULL);

ZTEST(manikin_ble_encode_tests, test_encode_valid_frame)
{
    manikin_ble_msg_t msg = {
        .cmd = MANIKIN_BLE_CMD_START,
        .payload = { 0xAA, 0xBB },
        .payload_size = 2
    };

    uint8_t encoded[32] = {0};

    int ret = manikin_ble_encode_msg(sizeof(encoded), &msg, encoded);
    zassert_equal(ret, 0, "Encoding failed");

    // Validate header
    zassert_equal(encoded[0], MANIKIN_BLE_PACKET_START_BYTE, "Start byte mismatch");
    zassert_equal(encoded[1], msg.payload_size, "Payload size field incorrect");
    zassert_equal(encoded[2], MANIKIN_BLE_PACKET_SOF_MSG, "SOF byte mismatch");
    zassert_equal(encoded[3], MANIKIN_BLE_CMD_START, "CMD byte mismatch");

    // Validate payload
    zassert_equal(encoded[4], 0xAA, "Payload[0] incorrect");
    zassert_equal(encoded[5], 0xBB, "Payload[1] incorrect");

    // Validate EOF
    zassert_equal(encoded[6], MANIKIN_BLE_PACKET_EOF_MSG, "EOF byte incorrect");

    // Validate CRC (LE): should be 0x59D7 → [0xD7, 0x59]
    const uint16_t expected_crc = 0x59D7;
    uint16_t crc = sys_get_le16(&encoded[7]);
    zassert_equal(crc, expected_crc, "CRC incorrect");

    // Validate stop
    zassert_equal(encoded[9], MANIKIN_BLE_PACKET_STOP_BYTE, "STOP byte incorrect");
}
