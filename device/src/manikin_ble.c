#include "manikin_ble.h"
#include <zephyr/sys/ring_buffer.h>
#include <zephyr/sys/printk.h>
#include <string.h>

static int store_to_ring(struct ring_buf *ring, struct k_mutex *mutex, const uint8_t *data, size_t len)
{
    if (!data || len == 0)
        return -EINVAL;

    k_mutex_lock(mutex, K_FOREVER);
    uint32_t written = ring_buf_put(ring, data, len);
    k_mutex_unlock(mutex);

    return (written == len) ? 0 : -ENOSPC;
}

static int fetch_from_ring(struct ring_buf *ring, struct k_mutex *mutex, uint8_t *out, size_t len)
{
    if (!out || len == 0)
        return -EINVAL;

    k_mutex_lock(mutex, K_FOREVER);
    uint32_t fetched = ring_buf_get(ring, out, len);
    k_mutex_unlock(mutex);

    return (fetched == len) ? 0 : -ENODATA;
}

static int peek_from_ring(struct ring_buf *ring, struct k_mutex *mutex, uint8_t *out, size_t len) {

}

static int ring_is_empty(struct ring_buf *ring, struct k_mutex *mutex) {
    k_mutex_lock(mutex, K_FOREVER);
    bool is_empty = ring_buf_is_empty(ring);
    k_mutex_unlock(mutex);
    return is_empty;
}

int manikin_ble_init(manikin_ble_handle_t *handle)
{
    if (!handle)
        return -EINVAL;

    ring_buf_init(&handle->rx_ring, sizeof(handle->rx_ring_buf), handle->rx_ring_buf);
    ring_buf_init(&handle->tx_ring, sizeof(handle->tx_ring_buf), handle->tx_ring_buf);
    k_mutex_init(&handle->rx_mutex);
    k_mutex_init(&handle->tx_mutex);
    k_mutex_init(&handle->subscriber_mutex);

    handle->num_of_subscribers = 0;
    memset(handle->subscriptions, 0, sizeof(handle->subscriptions));

    return 0;
}

int manikin_ble_wait_for_command(manikin_ble_handle_t *handle, manikin_ble_cmd_t command, struct k_sem *listener_sem)
{
    if (!handle || !listener_sem)
        return -EINVAL;

    k_mutex_lock(&handle->subscriber_mutex, K_FOREVER);

    if (handle->num_of_subscribers >= CONFIG_MANIKIN_BLE_MAX_SUBSCRIBERS) {
        k_mutex_unlock(&handle->subscriber_mutex);
        return -ENOMEM;
    }

    handle->subscriptions[handle->num_of_subscribers].command = command;
    handle->subscriptions[handle->num_of_subscribers].listener_sem = listener_sem;
    handle->num_of_subscribers++;

    k_mutex_unlock(&handle->subscriber_mutex);
    return 0;
}

int manikin_ble_send_message(manikin_ble_handle_t *handle, manikin_ble_cmd_t command, const uint8_t *data, size_t len)
{
    if (!handle || !data || len == 0)
        return -EINVAL;

    uint8_t encoded[CONFIG_MANIKIN_BLE_MAX_ATT_SIZE];

    manikin_ble_msg_t msg = {
        .cmd = command,
        .payload_size = len > CONFIG_MANIKIN_BLE_MAX_ATT_SIZE ? CONFIG_MANIKIN_BLE_MAX_ATT_SIZE : len
    };
    memcpy(msg.payload, data, msg.payload_size);

    int encoded_len = manikin_ble_encode_msg(sizeof(encoded), &msg, encoded);
    if (encoded_len < 0)
        return -EIO;

    return manikin_ble_send_ble(handle, encoded, encoded_len);
}

int manikin_ble_receive_message(manikin_ble_handle_t *handle, manikin_ble_cmd_t cmd, manikin_ble_msg_t *msg)
{
    if (!handle || !msg)
        return -EINVAL;



    return -ENODATA;
}

int manikin_ble_on_ble_message(manikin_ble_handle_t *handle, uint8_t *raw_message)
{
    if (!handle || !raw_message)
        return -EINVAL;

    manikin_ble_msg_t decoded;
    int decode_res = manikin_ble_decode_msg(raw_message, CONFIG_MANIKIN_BLE_MAX_ATT_SIZE, &decoded);
    if (decode_res != 0)
        return -EINVAL;

    if (store_to_ring(&handle->rx_ring, &handle->rx_mutex, (uint8_t *)&decoded, sizeof(decoded)) != 0)
        return -ENOSPC;

    return 0;
}

int manikin_ble_poll(manikin_ble_handle_t *handle)
{
    if (!handle)
        return -EINVAL;

    
    // Currently no background processing needed.

    // k_mutex_lock(&handle->subscriber_mutex, K_FOREVER);
    // for (int i = 0; i < handle->num_of_subscribers; ++i) {
    //     if (handle->subscriptions[i].command == decoded.cmd) {
    //         k_sem_give(handle->subscriptions[i].listener_sem);
    //     }
    // }
    // k_mutex_unlock(&handle->subscriber_mutex);

    return 0;
}

int manikin_ble_deinit(manikin_ble_handle_t *handle)
{
    if (!handle)
        return -EINVAL;

    k_mutex_lock(&handle->subscriber_mutex, K_FOREVER);
    handle->num_of_subscribers = 0;
    k_mutex_unlock(&handle->subscriber_mutex);

    return 0;
}
