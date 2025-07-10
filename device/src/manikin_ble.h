#ifndef MANIKIN_BLE_H
#define MANIKIN_BLE_H

#include <zephyr/bluetooth/conn.h>
#include <zephyr/kernel.h>
#include <manikin_ble_serializer.h>
#include <private/manikin_ble_protocol_definitions.h>

/**
 * @brief BLE command parser handle.
 */
typedef struct
{
    struct ring_buf rx_ring;                          /**< RX ring buffer */
    uint8_t rx_ring_buf[MANIKIN_BLE_BUFFER_SPACE];    /**< RX buffer space */
    struct k_mutex rx_mutex;                          /**< RX mutex */

    struct ring_buf tx_ring;                          /**< TX ring buffer */
    uint8_t tx_ring_buf[MANIKIN_BLE_BUFFER_SPACE];    /**< TX buffer space */
    struct k_mutex tx_mutex;                          /**< TX mutex */

    manikin_ble_subscription_t subscriptions[CONFIG_MANIKIN_BLE_MAX_SUBSCRIBERS]; /**< Command listeners */
    uint8_t num_of_subscribers;                       /**< Number of listeners */
    struct k_mutex subscriber_mutex;                  /**< Subscriber list mutex */
} manikin_ble_handle_t;

/**
 * @brief Initialize the BLE parser.
 *
 * @param handle Pointer to the BLE handle (must be allocated by the user).
 * @return 0 on success, or negative errno on failure.
 */
int manikin_ble_init(manikin_ble_handle_t *handle);

/**
 * @brief Register a listener for a specific command.
 *
 * @param handle BLE handle.
 * @param command Command to listen for.
 * @param listener_sem Semaphore to signal when command is received.
 * @return 0 on success, or negative errno on failure.
 */
int manikin_ble_wait_for_command(manikin_ble_handle_t *handle, manikin_ble_cmd_t command, struct k_sem* listener_sem);

/**
 * @brief Send a command to the BLE peer.
 *
 * @param handle BLE handle.
 * @param command Command ID to send.
 * @param data Pointer to payload buffer.
 * @param len Length of payload in bytes.
 * @return 0 on success, or negative errno on failure.
 */
int manikin_ble_send_message(manikin_ble_handle_t *handle, manikin_ble_cmd_t command, const uint8_t *data, size_t len);

/**
 * @brief Retrieve the next message for a command.
 *
 * @param handle BLE handle.
 * @param cmd Command to read from buffer.
 * @param msg Pointer to store received message.
 * @return 0 on success, -ENODATA if no message available, or -EINVAL on error.
 */
int manikin_ble_receive_message(manikin_ble_handle_t *handle, manikin_ble_cmd_t cmd, manikin_ble_msg_t *msg);

/**
 * @brief Decode and dispatch a raw incoming message.
 *
 * @param handle BLE handle.
 * @param raw_message Pointer to complete raw message (with header).
 * @return 0 on success, -ENOENT if no subscriber, or -EINVAL on error.
 */
int manikin_ble_on_ble_message(manikin_ble_handle_t *handle, uint8_t *raw_message);

/**
 * @brief Process RX and TX queues.
 *
 * @param handle BLE handle.
 * @return 0 on success, or negative errno on failure.
 */
int manikin_ble_poll(manikin_ble_handle_t *handle);

/**
 * @brief Deinitialize BLE parser.
 *
 * @param handle BLE handle.
 * @return 0 on success, or negative errno on failure.
 */
int manikin_ble_deinit(manikin_ble_handle_t *handle);

/**
 * @brief Send raw BLE data (must be implemented by user).
 *
 * @param handle BLE handle.
 * @param data Pointer to serialized message.
 * @param size Size of data in bytes.
 * @return 0 on success, or negative errno on failure.
 */
int manikin_ble_send_ble(manikin_ble_handle_t *handle, const uint8_t *data, const uint8_t size);

#endif /* MANIKIN_BLE_H */
