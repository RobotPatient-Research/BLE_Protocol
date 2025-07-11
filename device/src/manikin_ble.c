#include "manikin_ble.h"
#include <zephyr/sys/ring_buffer.h>
#include <zephyr/sys/printk.h>
#include <string.h>

static int store_to_ring(struct ring_buf *ring, struct k_mutex *mutex, const uint8_t *data, size_t len);

static int fetch_from_ring(struct ring_buf *ring, struct k_mutex *mutex, uint8_t *out, size_t len);

static int peek_from_ring(struct ring_buf *ring, struct k_mutex *mutex, uint8_t *out, size_t len);

static int ring_is_empty(struct ring_buf *ring, struct k_mutex *mutex);
/**
 * @brief Get number of complete manikin_ble_msg_t messages in ring buffer.
 *
 * @param ring Pointer to ring buffer.
 * @return Number of messages in buffer.
 */
static inline size_t manikin_ble_ring_msg_count(struct ring_buf *ring, struct k_mutex *mutex);

int
manikin_ble_init (manikin_ble_handle_t *handle)
{
    if (!handle)
    {
        return -EINVAL;
    }
    ring_buf_init(&handle->rx_ring, sizeof(handle->rx_ring_buf), handle->rx_ring_buf);
    ring_buf_init(&handle->tx_ring, sizeof(handle->tx_ring_buf), handle->tx_ring_buf);
    k_mutex_init(&handle->rx_mutex);
    k_mutex_init(&handle->tx_mutex);
    k_mutex_init(&handle->subscriber_mutex);

    handle->num_of_subscribers = 0;
    memset(handle->subscriptions, 0, sizeof(handle->subscriptions));
    for (int i = 0; i < CONFIG_MANIKIN_BLE_MAX_SUBSCRIBERS; i++)
    {
        k_sem_init(&(handle->subscriptions[i].sem), 0, 1);
        k_sem_take(&(handle->subscriptions[i].sem), K_FOREVER);
    }
    return 0;
}

int
manikin_ble_wait_for_command (manikin_ble_handle_t *handle, manikin_ble_cmd_t command)
{
    if (!handle && handle->num_of_subscribers != 4)
    {
        return -EINVAL;
    }
    k_mutex_lock(&handle->subscriber_mutex, K_FOREVER);

    if (handle->num_of_subscribers >= CONFIG_MANIKIN_BLE_MAX_SUBSCRIBERS)
    {
        k_mutex_unlock(&handle->subscriber_mutex);
        return -ENOMEM;
    }

    handle->subscriptions[handle->num_of_subscribers].command = command;
    handle->num_of_subscribers++;
    int idx = handle->num_of_subscribers;
    k_mutex_unlock(&handle->subscriber_mutex);
    // Block until the command is received and the semaphore is given
    k_sem_take(&handle->subscriptions[idx].sem, K_FOREVER);
    handle->num_of_subscribers--;
    return 0;
}

int
manikin_ble_send_message (manikin_ble_handle_t *handle, manikin_ble_cmd_t command, const uint8_t *data, size_t len)
{
    if (!handle || !data || len == 0)
    {
        return -EINVAL;
    }
    uint8_t encoded[CONFIG_MANIKIN_BLE_MAX_ATT_SIZE];

    manikin_ble_msg_t msg
        = { .cmd          = command,
            .payload_size = len > CONFIG_MANIKIN_BLE_MAX_ATT_SIZE ? CONFIG_MANIKIN_BLE_MAX_ATT_SIZE : len };
    memcpy(msg.payload, data, msg.payload_size);

    int encoded_len = manikin_ble_encode_msg(sizeof(encoded), &msg, encoded);
    if (encoded_len < 0)
    {
        return -EIO;
    }

    return manikin_ble_send_ble(handle, encoded, encoded_len);
}

int
manikin_ble_receive_message (manikin_ble_handle_t *handle, manikin_ble_cmd_t cmd, manikin_ble_msg_t *msg)
{
    if (!handle || !msg)
    {
        return -EINVAL;
    }

    return -ENODATA;
}

int
manikin_ble_on_ble_message (manikin_ble_handle_t *handle, uint8_t *raw_message)
{
    if (!handle || !raw_message)
    {
        return -EINVAL;
    }
    manikin_ble_msg_t temp;
    int               decode_res = manikin_ble_decode_msg(raw_message, CONFIG_MANIKIN_BLE_MAX_ATT_SIZE, &temp);
    if (decode_res != 0)
    {
        return -EINVAL;
    }

    if (temp.cmd != handle->latest_ack_cmd)
    {
        if (store_to_ring(&handle->rx_ring, &handle->rx_mutex, (uint8_t *)&temp, sizeof(temp)) != 0)
        {
            return -ENOSPC;
        }

        uint8_t encoded_ack_buf[32];
        int     encoded_ack_res = manikin_ble_encode_msg(sizeof(encoded_ack_buf), &temp, encoded_ack_buf);
        if (encoded_ack_res != 0)
        {
            return -EINVAL;
        }
        temp.payload_size = 1;
        if (store_to_ring(&handle->tx_ring, &handle->tx_mutex, (uint8_t *)&temp, sizeof(temp)) != 0)
        {
            return -ENOSPC;
        }
    }
    return 0;
}

int
manikin_ble_poll (manikin_ble_handle_t *handle)
{
    if (!handle)
    {
        return -EINVAL;
    }

    uint8_t tx_buf[CONFIG_MANIKIN_BLE_MAX_ATT_SIZE];
    if (ring_is_empty(&handle->tx_ring, &handle->tx_mutex) == 0)
    {
        if (peek_from_ring(&handle->tx_ring, &handle->tx_mutex, tx_buf, sizeof(tx_buf)) != 0) {
            manikin_ble_send_ble(handle, tx_buf, sizeof(tx_buf));
        }
    }
    
if (ring_is_empty(&handle->rx_ring, &handle->rx_mutex) == 0)
{
    k_mutex_lock(&handle->rx_mutex, K_FOREVER);

    uint32_t claimed_len;
    uint8_t *data;

    while ((claimed_len = ring_buf_get_claim(&handle->rx_ring, &data, sizeof(manikin_ble_msg_t))) == sizeof(manikin_ble_msg_t)) {
        manikin_ble_msg_t *msg = (manikin_ble_msg_t *)data;

        for(uint8_t subscriber = 0; subscriber < CONFIG_MANIKIN_BLE_MAX_SUBSCRIBERS; subscriber++) {
            if(handle->subscriptions[subscriber].command == msg->cmd) {
                k_sem_give(&(handle->subscriptions[subscriber].sem));
            }
        }

        // Mark message as consumed
        ring_buf_get_finish(&handle->rx_ring, 0);
    }

    k_mutex_unlock(&handle->rx_mutex);
}

    // k_mutex_lock(&handle->subscriber_mutex, K_FOREVER);
    // for (int i = 0; i < handle->num_of_subscribers; ++i) {
    //     if (handle->subscriptions[i].command == decoded.cmd) {
    //         k_sem_give(handle->subscriptions[i].listener_sem);
    //     }
    // }
    // k_mutex_unlock(&handle->subscriber_mutex);

    return 0;
}

int
manikin_ble_deinit (manikin_ble_handle_t *handle)
{
    if (!handle)
    {
        return -EINVAL;
    }
    k_mutex_lock(&handle->subscriber_mutex, K_FOREVER);
    handle->num_of_subscribers = 0;
    k_mutex_unlock(&handle->subscriber_mutex);

    return 0;
}

static int
store_to_ring (struct ring_buf *ring, struct k_mutex *mutex, const uint8_t *data, size_t len)
{
    if (!data || len == 0)
    {
        return -EINVAL;
    }
    k_mutex_lock(mutex, K_FOREVER);
    uint32_t written = ring_buf_put(ring, data, len);
    k_mutex_unlock(mutex);

    return (written == len) ? 0 : -ENOSPC;
}

static int
fetch_from_ring (struct ring_buf *ring, struct k_mutex *mutex, uint8_t *out, size_t len)
{
    if (!out || len == 0)
    {
        return -EINVAL;
    }
    k_mutex_lock(mutex, K_FOREVER);
    uint32_t fetched = ring_buf_get(ring, out, len);
    k_mutex_unlock(mutex);

    return (fetched == len) ? 0 : -ENODATA;
}

static int
peek_from_ring (struct ring_buf *ring, struct k_mutex *mutex, uint8_t *out, size_t len)
{
    if (!out || len == 0)
    {
        return -EINVAL;
    }

    k_mutex_lock(mutex, K_FOREVER);
    uint32_t fetched = ring_buf_peek(ring, out, len);
    k_mutex_unlock(mutex);

    return (fetched == len) ? 0 : -ENODATA;
}

static int
ring_is_empty (struct ring_buf *ring, struct k_mutex *mutex)
{
    k_mutex_lock(mutex, K_FOREVER);
    bool is_empty = ring_buf_is_empty(ring);
    k_mutex_unlock(mutex);
    return is_empty;
}


static inline size_t manikin_ble_ring_msg_count(struct ring_buf *ring, struct k_mutex *mutex)
{  
    k_mutex_lock(mutex, K_FOREVER);
    size_t used = ring_buf_size_get(ring);
    k_mutex_unlock(mutex);
    return used / sizeof(manikin_ble_msg_t);
}