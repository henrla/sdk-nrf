#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>

struct advertiser_info {
	struct k_work work;
	struct bt_le_ext_adv *adv;
	uint8_t id;
};

static struct advertiser_info advertisers[CONFIG_BT_EXT_ADV_MAX_ADV_SET];

static void start_connectable_advertiser(struct k_work *work);

static const struct bt_data ad[] = {
	BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME, sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};

static void connected(struct bt_conn *conn, uint8_t err)
{
	char addr_str[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr_str, sizeof(addr_str));
	printk("connected to %s\n", addr_str);
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	char addr_str[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr_str, sizeof(addr_str));

	printk("disconnected %s, reason %u\n", addr_str, reason);

	struct bt_conn_info connection_info;
	int err;
	err = bt_conn_get_info(conn, &connection_info);

	if (err) {
		printk("Failed to get conn info (err %d)\n", err);
		return;
	}

	uint8_t id_current = connection_info.id;
	printk("Advertiser %d disconnected\n", id_current);

	k_work_submit(&advertisers[id_current].work);
}

static struct bt_conn_cb conn_callbacks = {
	.connected = connected,
	.disconnected = disconnected,
};

static void start_connectable_advertiser(struct k_work *work)
{
	int err;

	struct advertiser_info *current_adv_info =
		CONTAINER_OF(work, struct advertiser_info, work);

	if (current_adv_info->adv) {
		err = bt_le_ext_adv_start(current_adv_info->adv, BT_LE_EXT_ADV_START_DEFAULT);
		if (err) {
			printk("Failed to start advertising set (err %d)\n", err);
			return;
		}
	}

	printk("Advertiser %d successfully started\n", current_adv_info->id);
}

static int setup_advertiser(uint8_t id_adv)
{
	struct bt_le_adv_param adv_param =
		BT_LE_ADV_PARAM_INIT(BT_LE_ADV_OPT_CONNECTABLE,
				     BT_GAP_ADV_SLOW_INT_MIN,
				     BT_GAP_ADV_SLOW_INT_MAX,
				     NULL);
	
	size_t id_count = 0xFF;
	int err;
	
	bt_id_get(NULL, &id_count);
	if (id_adv == id_count) {
		int id;

		id = bt_id_create(NULL, NULL);
		if (id < 0) {
			printk("Create id failed (%d)\n", id);
			if (id_adv == 0) {
				id_adv = CONFIG_BT_EXT_ADV_MAX_ADV_SET;
			}
			id_adv--;
		} else {
			printk("New id: %d\n", id);
		}
	}

	printk("Using current id: %u\n", id_adv);
	adv_param.id = id_adv;
	advertisers[id_adv].id = id_adv;
	
	err = bt_le_ext_adv_create(&adv_param, NULL, &advertisers[id_adv].adv);
	if (err) {
		printk("Failed to create advertiser set (err %d)\n", err);
		return err;
	}
	printk("Created adv: %p\n", &advertisers[id_adv].adv);
	err = bt_le_ext_adv_set_data(advertisers[id_adv].adv, ad, ARRAY_SIZE(ad), NULL, 0);
	if (err) {
		printk("Failed to set advertising data (err %d)\n", err);
		return err;
	}

	return 0;
}

int main(void)
{
	int err;

	err = bt_enable(NULL);
	if (err)
	{
		printk("Bluetooth init failed (err %d)\n", err);
		return 0;
	}

	err = bt_conn_cb_register(&conn_callbacks);
	if (err) {
		printk("Conn callback register failed (err %d)\n", err);
		return 0;
	}

	printk("Bluetooth initialized\n");
	
	printk("Starting %d advertisers\n", CONFIG_BT_EXT_ADV_MAX_ADV_SET);
	for (uint8_t i = 0; i < CONFIG_BT_EXT_ADV_MAX_ADV_SET; i++)
	{
		err = setup_advertiser(i);
		if (err) {
			printk("Setup Advertiser failed (err %d)\n", err);
			return 0;
		}

		k_work_init(&advertisers[i].work, start_connectable_advertiser);
		k_work_submit(&advertisers[i].work);
	}
}
