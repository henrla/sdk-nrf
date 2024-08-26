#include <zephyr/kernel.h>
#include <zephyr/sys/ring_buffer.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/addr.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/uuid.h>

static char adv_name[] = "Nordic_Peripheral";

//static bt_addr_le_t pending_conn_addr_cache[CONFIG_BT_MAX_CONN];
#define ADV_NAME_STR_MAX_LEN (sizeof(adv_name))

static bool connecting;
static uint32_t num_connections;


RING_BUF_ITEM_DECLARE(pending_devices_ring_buf,
		      CONFIG_BT_MAX_CONN * sizeof(uint32_t));

static uint8_t pending_conn_idx;
static bt_addr_le_t addr_cache[CONFIG_BT_MAX_CONN];

//TODO: dont try to connect if already connected

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
    printk("disconnected reason %u\n", reason);
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

	//err = bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
	//if (err) {
	//	printk("Disconnection failed\n.");
	//}
}

static struct bt_conn_cb conn_callbacks = {
	.connected = connected,
	.disconnected = disconnected,
};

static bool adv_data_parse_cb(struct bt_data *data, void *user_data)
{
	char * rcvd_name = user_data;
	uint8_t len;

	switch (data->type) {
		case BT_DATA_NAME_SHORTENED:
		case BT_DATA_NAME_COMPLETE:
			len = MIN(data->data_len, ADV_NAME_STR_MAX_LEN - 1);
			memcpy(rcvd_name, data->data, len);
			rcvd_name[len] = '\0';
			return false;
		default:
			return true;
	}
}

static void scan_recv(const struct bt_le_scan_recv_info *info, struct net_buf_simple *buf)
{
	/* We're only interested in connectable advertisers */
	if (info->adv_type != BT_GAP_ADV_TYPE_ADV_IND && info->adv_type != BT_GAP_ADV_TYPE_EXT_ADV) {
		return;
	}

	/* connect only to devices in close proximity */
	if (info->rssi < -55) {
		return;
	}

	char addr_str[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(info->addr, addr_str, sizeof(addr_str));

	//printk("found addr %s\n", addr_str);

	char name_str[ADV_NAME_STR_MAX_LEN] = {0};

	bt_data_parse(buf, adv_data_parse_cb, name_str);

	if (strncmp(name_str, adv_name, ADV_NAME_STR_MAX_LEN) == 0) {
		printk("found %s\n", name_str);
		return;
	}
}

static struct bt_le_scan_cb scan_callbacks = {
	.recv = scan_recv,
};

static void scan_start(void)
{
	int err = bt_le_scan_start(BT_LE_SCAN_ACTIVE_CONTINUOUS, NULL);
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

	printk("Bluetooth initialized\n");

	bt_le_scan_cb_register(&scan_callbacks);
	bt_conn_cb_register(&conn_callbacks);

	printk("adv_name %s\n", adv_name);

	scan_start();
}

