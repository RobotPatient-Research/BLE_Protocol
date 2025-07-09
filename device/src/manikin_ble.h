#ifndef MANIKIN_BLE_H
#define MANIKIN_BLE_H

#include <zephyr/bluetooth/conn.h>
#include <zephyr/kernel.h>
#include <private/manikin_ble_protocol_definitions.h>

/**
 * @brief Handle structure for the Manikin BLE command parser.
 */
typedef struct
{
    struct bt_conn *conn; /**< Pointer to the active BLE connection*/

    /* RX ring buffer */
    struct ring_buf rx_ring;
    uint8_t rx_ring_buf[MANIKIN_BLE_BUFFER_SPACE];
    struct k_mutex rx_mutex;

    struct ring_buf tx_ring;
    uint8_t tx_ring_buf[MANIKIN_BLE_BUFFER_SPACE];
    struct k_mutex tx_mutex;

    struct k_sem* subscribers[CONFIG_MANIKIN_BLE_MAX_SUBSCRIBERS];
    uint8_t num_of_subscribers;

} manikin_ble_handle_t;

/**
 * @brief Initialize the Manikin BLE command parser.
 *
 * Sets up internal state for BLE command handling. Must be called before
 * using other functions in this module.
 *
 * @param handle Pointer to the BLE handle (must be user-allocated).
 * @return 0 on success, or a negative errno code on failure.
 */
int manikin_ble_init(manikin_ble_handle_t *handle);

/**
 * @brief Send a BLE command or response to the peer device.
 *
 * This function transmits a command or response back to the connected BLE peer.
 * Typically used to reply to incoming requests or to send unsolicited updates.
 *
 * The implementation may use a GATT notification, write, or indication depending
 * on configuration.
 *
 * @param handle Pointer to the BLE session handle.
 * @param command The command ID to send
 * @param data Pointer to payload data to send.
 * @param len Length of payload data in bytes.
 *
 * @return 0 on success,
 *         -ENOTCONN if no BLE connection is active,
 *         -EINVAL if arguments are invalid,
 *         -EIO or other negative errno on transmission failure.
 */
int manikin_ble_send_command(manikin_ble_handle_t *handle, manikin_ble_cmd_t command, const uint8_t *data, size_t len);

/**
 * @brief Register or replace the FIFO consumer for a specific BLE command.
 *
 * When the specified BLE command is received, its data will be delivered to
 * the given FIFO. If the command was already registered, the FIFO is replaced.
 *
 * The caller is responsible for ensuring the FIFO remains valid for the duration
 * of the BLE session or until explicitly removed via `manikin_ble_remove_consumer()`.
 *
 * @param handle Pointer to the BLE handle.
 * @param command BLE command to associate with this FIFO.
 * @param fifo Pointer to a Zephyr FIFO structure (must be statically allocated).
 *
 * @return 0 on success,
 *         -EINVAL if arguments are invalid.
 */
int manikin_ble_add_consumer(manikin_ble_handle_t *handle, manikin_ble_cmd_t command, struct k_fifo *fifo);

/**
 * @brief Remove a previously registered consumer for a BLE command.
 *
 * After calling this, the command will no longer deliver data to any FIFO.
 *
 * @param handle Pointer to the BLE handle.
 * @param command BLE command to remove.
 *
 * @return 0 on success,
 *         -ENOENT if no consumer was registered for the command,
 *         -EINVAL if arguments are invalid.
 */
int manikin_ble_remove_consumer(manikin_ble_handle_t *handle, manikin_ble_cmd_t command);

/**
 * @brief Process incoming BLE command messages.
 *
 * Call this periodically to handle received BLE command data and route it
 * to the appropriate FIFO (if registered).
 *
 * @param handle Pointer to the BLE handle.
 *
 * @return 0 on success, or negative errno on processing error.
 */
int manikin_ble_process(manikin_ble_handle_t *handle);

/**
 * @brief Deinitialize the Manikin BLE command parser.
 *
 * Cleans up internal state. Call this when BLE command processing is no longer needed.
 *
 * @param handle Pointer to the BLE handle.
 * @return 0 on success, or negative errno on failure.
 */
int manikin_ble_deinit(manikin_ble_handle_t *handle);

#endif /* MANIKIN_BLE_H */
