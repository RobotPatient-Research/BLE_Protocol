#include <string.h>
#include <zephyr/kernel.h>
#include <manikin_ble.h>
#include <manikin_ble_serializer.h>

typedef struct {
    manikin_ble_cmd_t cmd;
    struct k_fifo *fifo;
} manikin_consumer_entry_t;

static manikin_consumer_entry_t consumers[CONFIG_MANIKIN_BLE_MAX_SUBSCRIBERS];

int
manikin_ble_init(manikin_ble_handle_t *handle)
{
    ring_buf_init(&handle->rx_ring, sizeof(handle->rx_ring_buf), handle->rx_ring_buf);
    ring_buf_init(&handle->tx_ring, sizeof(handle->tx_ring_buf), handle->tx_ring_buf);

    k_mutex_init(&handle->rx_mutex);
    k_mutex_init(&handle->tx_mutex);

    handle->num_of_subscribers = 0;
    memset(handle->subscribers, 0, sizeof(handle->subscribers));
    memset(consumers, 0, sizeof(consumers));

    return 0;
}

int
manikin_ble_send_command(manikin_ble_handle_t *handle, manikin_ble_cmd_t command, const uint8_t *data, size_t len)
{
    manikin_ble_msg_t msg = {
        .cmd = command,
        .payload_size = len < CONFIG_MANIKIN_BLE_MAX_TEMPORARY_BUFFER_SIZE ? len : CONFIG_MANIKIN_BLE_MAX_TEMPORARY_BUFFER_SIZE,
    };

    memcpy(msg.payload, data, msg.payload_size);

    uint8_t encoded[CONFIG_MANIKIN_BLE_MAX_TEMPORARY_BUFFER_SIZE + 8];
    int ret = manikin_ble_encode_msg(sizeof(encoded), &msg, encoded);
    if (ret <= 0) {
        return -EINVAL;
    }

    k_mutex_lock(&handle->tx_mutex, K_FOREVER);
    size_t written = ring_buf_put(&handle->tx_ring, encoded, ret);
    k_mutex_unlock(&handle->tx_mutex);

    return (written == ret) ? 0 : -ENOMEM;
}

int
manikin_ble_add_consumer(manikin_ble_handle_t *handle, manikin_ble_cmd_t command, struct k_fifo *fifo)
{
    if (handle->num_of_subscribers >= CONFIG_MANIKIN_BLE_MAX_SUBSCRIBERS) {
        return -ENOMEM;
    }

    for (int i = 0; i < CONFIG_MANIKIN_BLE_MAX_SUBSCRIBERS; i++) {
        if (consumers[i].fifo == NULL) {
            consumers[i].cmd = command;
            consumers[i].fifo = fifo;
            handle->subscribers[handle->num_of_subscribers++] = NULL;  // optional placeholder
            return 0;
        }
    }

    return -ENOMEM;
}

int
manikin_ble_remove_consumer(manikin_ble_handle_t *handle, manikin_ble_cmd_t command)
{
    for (int i = 0; i < CONFIG_MANIKIN_BLE_MAX_SUBSCRIBERS; i++) {
        if (consumers[i].fifo != NULL && consumers[i].cmd == command) {
            consumers[i].fifo = NULL;
            // Optional: compact the list
            return 0;
        }
    }

    return -ENOENT;
}

int
manikin_ble_process(manikin_ble_handle_t *handle)
{
    uint8_t temp[CONFIG_MANIKIN_BLE_MAX_TEMPORARY_BUFFER_SIZE + 8];
    int len;

    k_mutex_lock(&handle->rx_mutex, K_FOREVER);
    len = ring_buf_get(&handle->rx_ring, temp, sizeof(temp));
    k_mutex_unlock(&handle->rx_mutex);

    if (len <= 0) {
        return 0;
    }

    manikin_ble_msg_t decoded;
    int ret = manikin_ble_decode_msg(temp, len, &decoded);
    if (ret < 0) {
        return -EINVAL;
    }

    // Route to matching consumer
    for (int i = 0; i < CONFIG_MANIKIN_BLE_MAX_SUBSCRIBERS; i++) {
        if (consumers[i].fifo && consumers[i].cmd == decoded.cmd) {
            manikin_ble_msg_t *msg = k_malloc(sizeof(manikin_ble_msg_t));
            if (!msg) {
                return -ENOMEM;
            }
            memcpy(msg, &decoded, sizeof(manikin_ble_msg_t));
            k_fifo_put(consumers[i].fifo, msg);
            return 0;
        }
    }

    return -ENOSYS;  // No handler found
}

int
manikin_ble_deinit(manikin_ble_handle_t *handle)
{
    // Optional cleanup
    memset(handle, 0, sizeof(*handle));
    return 0;
}
