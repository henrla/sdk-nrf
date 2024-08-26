#include <zephyr/kernel.h>
#include <zephyr/sys/ring_buffer.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/addr.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/uuid.h>

//static bt_addr_le_t pending_conn_addr_cache[CONFIG_BT_MAX_CONN];

static bool connecting;
static uint32_t num_connections;


RING_BUF_ITEM_DECLARE(pending_devices_ring_buf,
		      CONFIG_BT_MAX_CONN * sizeof(uint32_t));

static uint8_t pending_conn_idx;
static bt_addr_le_t addr_cache[CONFIG_BT_MAX_CONN];
static bt_addr_le_t * pending_connections[CONFIG_BT_MAX_CONN];

//TODO: dont try to connect if already connected

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
    printk("disconnected reason %u\n", reason);
    pending_connections[pending_conn_idx] = NULL;
}

static void try_connect(const bt_addr_le_t *addr)
{
	int err;
	char addr_str[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(addr, addr_str, sizeof(addr_str));

	printk("connecting to %s\n", addr_str);
	struct bt_conn *unused_conn = NULL;
	const struct bt_conn_le_create_param create_param = {
		.options = BT_CONN_LE_OPT_NONE,
		.interval = 0x0030,
		.window = 0x0030,
		.interval_coded = 0,
		.window_coded = 0,
		.timeout = 0,
	};

	err = bt_conn_le_create(addr, &create_param, BT_LE_CONN_PARAM(8, 8, 0, 200),
						&unused_conn);
	if (err) {
		printk("Create conn to %s failed (%d)\n", addr_str, err);
	}

	if (unused_conn) {
		bt_conn_unref(unused_conn);
	}

	connecting = true;
}

static void connected(struct bt_conn *conn, uint8_t err)
{
	char addr_str[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr_str, sizeof(addr_str));
	printk("connected to %s\n", addr_str);
	connecting = false;
//	//TODO: disconnect
//
	if (err) {
		printk("Failed to connect to %s (%u)\n", addr_str, err);

		//try_connect(bt_conn_get_dst(conn));
		return;
	}

	num_connections++;

	err = bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
	if (err) {
		printk("Disconnection failed\n.");
	}

}

static struct bt_conn_cb conn_callbacks = {
	.connected = connected,
	.disconnected = disconnected,
};

static bool adv_data_parse_cb(struct bt_data *data, void *user_data)
{
	uint8_t * rcvd_uuid = user_data;

	switch (data->type)
	{
		case BT_DATA_UUID16_ALL:
			memcpy(rcvd_uuid, data->data, sizeof(uint16_t));
			return false;

		default:
			return true;
	}
}

static bool connection_to_addr_is_pending(const bt_addr_le_t *addr)
{
	for (uint8_t i = 0; i < CONFIG_BT_MAX_CONN; i++)
	{
		if (pending_connections[i])
		{
			if (!memcmp(pending_connections[i], addr, sizeof(*addr)))
			{
				return true;
			}
		}
	}

	return false;
}

static void store_addr_to_pending_conn(const bt_addr_le_t *addr)
{
	for (uint8_t i = 0; i < CONFIG_BT_MAX_CONN; i++)
	{
		if (!pending_connections[i])
		{
			memcpy(&addr_cache[i], addr, sizeof(*addr));
			pending_conn_idx = i;
			return;
		}
	}
}

static void device_found(const bt_addr_le_t *addr, int8_t rssi, uint8_t type,
			 struct net_buf_simple *ad)
{
	/* We're only interested in connectable advertisers */
	if (type != BT_GAP_ADV_TYPE_ADV_IND) {
		return;
	}

	/* connect only to devices in close proximity */
	if (rssi < -50) {
		return;
	}

	char addr_str[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(addr, addr_str, sizeof(addr_str));

	uint8_t hrs_uuid[] = {BT_UUID_16_ENCODE(BT_UUID_HRS_VAL)};
	uint8_t rcvd_uuid[2];

	bt_data_parse(ad, adv_data_parse_cb, rcvd_uuid);

	/* connect to peripherals advertising as a HRS device. */
	if (!memcmp(hrs_uuid, rcvd_uuid, sizeof(uint16_t)))
	{
		//int err;
		//bool connection_to_addr_not_pending = !connection_to_addr_is_pending(addr);
		//printk("got here: conn_not_pending %d\n", connection_to_addr_not_pending);

		if (!connecting /* && connection_to_addr_not_pending */)
		{
			//TODO: store addr

			printk("found addr %s\n", addr_str);

			//store_addr_to_pending_conn(addr);

			try_connect(addr);
		}
	}
}

static void scan_start(void)
{
	int err = bt_le_scan_start(BT_LE_SCAN_ACTIVE_CONTINUOUS, device_found);
	if (err) {
		printk("Scanning failed to start (err %d)\n", err);
	}

	printk("Scanning started\n");
}

int main(void)
{
	int err;

	err = bt_enable(NULL);
	if (err) {
	printk("Bluetooth init failed (err %d)\n", err);
		return 0;
	}

	bt_conn_cb_register(&conn_callbacks);

	scan_start();
}

