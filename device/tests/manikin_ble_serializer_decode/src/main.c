#include <zephyr/ztest.h>
#include "manikin_ble_serializer.h"
#include <string.h>

ZTEST_SUITE(manikin_ble_tests, NULL, NULL, NULL, NULL, NULL);

ZTEST(manikin_ble_tests, test_valid_msg_parsing)
{
    // Message: | 0x01 | 0x02 | 0x3A | 0x02 | 0xAA 0xBB | 0x3B |
    uint8_t buffer[] = {
        0x01, 0x02, 0x3A, 0x02, 0xAA, 0xBB, 0x3B, 0xD7, 0x59, 0x17
    };
    manikin_ble_msg_t msg = {0};
    int ret = manikin_ble_decode_msg(buffer, sizeof(buffer), &msg);

    zassert_equal(ret, 0, "Failed to decode valid BLE frame");
    zassert_equal(msg.cmd, MANIKIN_BLE_CMD_START, "Incorrect command parsed");
    zassert_equal(msg.payload_size, 2, "Incorrect payload size");
    zassert_equal(msg.payload[0], 0xAA, "Payload[0] mismatch");
    zassert_equal(msg.payload[1], 0xBB, "Payload[1] mismatch");
}

ZTEST(manikin_ble_tests, test_invalid_start_byte)
{
    uint8_t buffer[] = {
        0x00, 0x02, 0x3A, 0x02, 0xAA, 0xBB, 0x3B
    };
    manikin_ble_msg_t msg = {0};
    int ret = manikin_ble_decode_msg(buffer, sizeof(buffer), &msg);
    zassert_equal(ret, EINVAL, "Message with invalid start byte should fail");
}

ZTEST(manikin_ble_tests, test_missing_eof_byte)
{
    uint8_t buffer[] = {
        0x01, 0x02, 0x3A, 0x02, 0xAA, 0xBB /* missing 0x3B */
    };
    manikin_ble_msg_t msg = {0};
    int ret = manikin_ble_decode_msg(buffer, sizeof(buffer), &msg);
    zassert_equal(ret, EINVAL, "Message without EOF should fail");
}

ZTEST(manikin_ble_tests, test_shifted_valid_frame)
{
    uint8_t buffer[] = {
        0x00, 0x00, 0x01, 0x02, 0x3A, 0x03, 0xDE, 0xAD, 0x3B, 0xC4, 0xBB, 0x17
    };
    manikin_ble_msg_t msg = {0};
    int ret = manikin_ble_decode_msg(buffer, sizeof(buffer), &msg);

    zassert_equal(ret, 0, "Failed to decode shifted valid frame");
    zassert_equal(msg.cmd, MANIKIN_BLE_CMD_STOP, "Incorrect command");
    zassert_equal(msg.payload_size, 2, "Payload size mismatch");
    zassert_equal(msg.payload[0], 0xDE, "Payload[0] mismatch");
    zassert_equal(msg.payload[1], 0xAD, "Payload[1] mismatch");
}

ZTEST(manikin_ble_tests, test_null_input)
{
    manikin_ble_msg_t msg;
    int ret = manikin_ble_decode_msg(NULL, 10, &msg);
    zassert_equal(ret, EINVAL, "NULL input buffer should return EINVAL");

    ret = manikin_ble_decode_msg((const uint8_t*)"abc", 3, NULL);
    zassert_equal(ret, EINVAL, "NULL output buffer should return EINVAL");
}

ZTEST(manikin_ble_tests, test_invalid_command)
{
    uint8_t buffer[] = {
        0x01, 0x01, 0x3A, 0x06, 0xFF, 0x3B // CMD = 0x06 (INVALID)
    };
    manikin_ble_msg_t msg = {0};
    int ret = manikin_ble_decode_msg(buffer, sizeof(buffer), &msg);
    zassert_equal(ret, EINVAL, "Invalid command should return EINVAL");
}

ZTEST(manikin_ble_tests, test_invalid_crc)
{
    // Message with incorrect CRC: actual CRC for 0xAA 0xBB is 0x59D7 (little-endian: D7 59)
    // Here we intentionally corrupt the CRC (e.g., 0x00 0x00 instead of D7 59)
    uint8_t buffer[] = {
        0x01, 0x02, 0x3A, 0x02, 0xAA, 0xBB, 0x3B, 0x00, 0x00, 0x17
    };
    manikin_ble_msg_t msg = {0};
    int ret = manikin_ble_decode_msg(buffer, sizeof(buffer), &msg);

    zassert_not_equal(ret, 0, "Invalid CRC should result in error");
}
