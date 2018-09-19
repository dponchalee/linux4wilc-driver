// SPDX-License-Identifier: GPL-2.0
/*
* Copyright (c) 2012 - 2018 Microchip Technology Inc., and its subsidiaries.
* All rights reserved.
*/

#include "wilc_wlan_if.h"
#include "wilc_wlan.h"
#include "wilc_wlan_cfg.h"
#include "coreconfigurator.h"
#include "wilc_wfi_netdevice.h"

enum cfg_cmd_type {
	CFG_BYTE_CMD	= 0,
	CFG_HWORD_CMD	= 1,
	CFG_WORD_CMD	= 2,
	CFG_STR_CMD	= 3,
	CFG_BIN_CMD	= 4
};

struct wilc_mac_cfg {
	u8 mac_address[7];
	u8 firmware_version[129];
	u8 assoc_rsp[256];
	u8 antenna_param[5];
};

static struct wilc_mac_cfg g_mac;

static struct wilc_cfg_byte g_cfg_byte[] = {
	{WID_STATUS, 0},
	{WID_RSSI, 0},
	{WID_LINKSPEED, 0},
	{WID_TX_POWER, 0},
	{WID_WOWLAN_TRIGGER, 0},
	{WID_NIL, 0}
};

static struct wilc_cfg_hword g_cfg_hword[] = {
	{WID_NIL, 0}
};

static struct wilc_cfg_word g_cfg_word[] = {
	{WID_FAILED_COUNT, 0},
	{WID_RECEIVED_FRAGMENT_COUNT, 0},
	{WID_SUCCESS_FRAME_COUNT, 0},
	{WID_GET_INACTIVE_TIME, 0},
	{WID_NIL, 0}

};

static struct wilc_cfg_str g_cfg_str[] = {
	{WID_FIRMWARE_VERSION, g_mac.firmware_version},
	{WID_MAC_ADDR, g_mac.mac_address},
	{WID_ASSOC_RES_INFO, g_mac.assoc_rsp},
	{WID_NIL, NULL}
};

static struct wilc_cfg_bin g_cfg_bin[] = {
	{WID_ANTENNA_SELECTION, g_mac.antenna_param}
};

/********************************************
 *
 *      Configuration Functions
 *
 ********************************************/

static int wilc_wlan_cfg_set_byte(u8 *frame, u32 offset, u16 id, u8 val8)
{
	u8 *buf;

	if ((offset + 4) >= MAX_CFG_FRAME_SIZE)
		return 0;

	buf = &frame[offset];

	buf[0] = (u8)id;
	buf[1] = (u8)(id >> 8);
	buf[2] = 1;
	buf[3] = 0;
	buf[4] = val8;
	return 5;
}

static int wilc_wlan_cfg_set_hword(u8 *frame, u32 offset, u16 id, u16 val16)
{
	u8 *buf;

	if ((offset + 5) >= MAX_CFG_FRAME_SIZE)
		return 0;

	buf = &frame[offset];

	buf[0] = (u8)id;
	buf[1] = (u8)(id >> 8);
	buf[2] = 2;
	buf[3] = 0;
	buf[4] = (u8)val16;
	buf[5] = (u8)(val16 >> 8);

	return 6;
}

static int wilc_wlan_cfg_set_word(u8 *frame, u32 offset, u16 id, u32 val32)
{
	u8 *buf;

	if ((offset + 7) >= MAX_CFG_FRAME_SIZE)
		return 0;

	buf = &frame[offset];

	buf[0] = (u8)id;
	buf[1] = (u8)(id >> 8);
	buf[2] = 4;
	buf[3] = 0;
	buf[4] = (u8)val32;
	buf[5] = (u8)(val32 >> 8);
	buf[6] = (u8)(val32 >> 16);
	buf[7] = (u8)(val32 >> 24);

	return 8;
}

static int wilc_wlan_cfg_set_str(u8 *frame, u32 offset, u16 id, u8 *str,
				 u32 size)
{
	u8 *buf;

	if ((offset + size + 4) >= MAX_CFG_FRAME_SIZE)
		return 0;

	buf = &frame[offset];

	buf[0] = (u8)id;
	buf[1] = (u8)(id >> 8);
	buf[2] = (u8)size;
	buf[3] = (u8)(size >> 8);

	if (str && size != 0)
		memcpy(&buf[4], str, size);

	return (size + 4);
}

static int wilc_wlan_cfg_set_bin(u8 *frame, u32 offset, u16 id, u8 *b, u32 size)
{
	u8 *buf;
	u32 i;
	u8 checksum = 0;

	if ((offset + size + 5) >= MAX_CFG_FRAME_SIZE)
		return 0;

	buf = &frame[offset];
	buf[0] = (u8)id;
	buf[1] = (u8)(id >> 8);
	buf[2] = (u8)size;
	buf[3] = (u8)(size >> 8);

	if ((b) && size != 0) {
		memcpy(&buf[4], b, size);
		for (i = 0; i < size; i++)
			checksum += buf[i + 4];
	}

	buf[size + 4] = checksum;

	return (size + 5);
}

/********************************************
 *
 *      Configuration Response Functions
 *
 ********************************************/

#define GET_WID_TYPE(wid)		(((wid) >> 12) & 0x7)
static void wilc_wlan_parse_response_frame(struct wilc *wilc, u8 *info,
					   int size)
{
	u16 wid;
	u32 len = 0, i = 0;
	struct wilc_vif *vif = wilc->vif[0];

	while (size > 0) {
		i = 0;
		wid = info[0] | (info[1] << 8);

		PRINT_D(vif->ndev, GENERIC_DBG,"Processing response for %d\n",
			wid);
		switch (GET_WID_TYPE(wid)) {
		case WID_CHAR:
			do {
				if (g_cfg_byte[i].id == WID_NIL)
					break;

				if (g_cfg_byte[i].id == wid) {
					g_cfg_byte[i].val = info[4];
					break;
				}
				i++;
			} while (1);
			len = 3;
			break;

		case WID_SHORT:
			do {
				if (g_cfg_hword[i].id == WID_NIL)
					break;

				if (g_cfg_hword[i].id == wid) {
					g_cfg_hword[i].val = (info[4] |
							      (info[5] << 8));
					break;
				}
				i++;
			} while (1);
			len = 4;
			break;

		case WID_INT:
			do {
				if (g_cfg_word[i].id == WID_NIL)
					break;

				if (g_cfg_word[i].id == wid) {
					g_cfg_word[i].val = (info[4] |
							     (info[5] << 8) |
							     (info[6] << 16) |
							     (info[7] << 24));
					break;
				}
				i++;
			} while (1);
			len = 6;
			break;

		case WID_STR:
			do {
				if (g_cfg_str[i].id == WID_NIL)
					break;

				if (g_cfg_str[i].id == wid) {
					memcpy(g_cfg_str[i].str, &info[2],
					       (2+((info[3] << 8) | info[2])));
					break;
				}
				i++;
			} while (1);
			len = 2+((info[3] << 8) | info[2]);
			break;
		case WID_BIN_DATA:
			do {
				if (g_cfg_bin[i].id == WID_NIL)
					break;

				if (g_cfg_bin[i].id == wid) {
					uint16_t length = (info[3] << 8) |
							  info[2];
					uint8_t  checksum = 0;
					uint16_t i = 0;

					/*
					 * Compute the Checksum of received 
					 * data field
					 */
					for(i = 0;i < length;i++)
						checksum += info[4 + i];
					/* 
					 * Verify the checksum of recieved BIN 
					 * DATA
					 */
					if (checksum == info[4 + length]) {
						memcpy(g_cfg_bin[i].bin, &info[2], length + 2);
						/*
						 * value length + data length +
						 * checksum 
						 */
						len = 2 + length + 1;   
						break;
					} else {
						PRINT_ER(vif->ndev,
							 "Checksum Failed!");
						return;
					}
				}
				i++;
			} while (1);
			break;
		default:
			break;
		}
		size -= (2 + len);
		info += (2 + len);
	}
}

static void wilc_wlan_parse_info_frame(struct wilc *wilc, u8 *info)
{
	struct wilc_vif *vif = wilc->vif[0];
	u32 wid, len;

	wid = info[0] | (info[1] << 8);

	len = info[2];
	PRINT_D(vif->ndev, GENERIC_DBG,"Status Len = %d Id= %d\n", len, wid);
	
	if (len == 1 && wid == WID_STATUS) {
		int i = 0;

		do {
			if (g_cfg_byte[i].id == WID_NIL)
				break;

			if (g_cfg_byte[i].id == wid) {
				g_cfg_byte[i].val = info[3];
				break;
			}
			i++;
		}while (1);
	}
}

/********************************************
 *
 *      Configuration Exported Functions
 *
 ********************************************/

int wilc_wlan_cfg_set_wid(struct wilc_vif *vif, u8 *frame, u32 offset, u16 id,
			  u8 *buf, int size)
{
	u8 type = (id >> 12) & 0xf;
	int ret = 0;

	switch (type) {
	case CFG_BYTE_CMD:
		if (size >= 1)
			ret = wilc_wlan_cfg_set_byte(frame, offset, id, *buf);
		break;

	case CFG_HWORD_CMD:
		if (size >= 2)
			ret = wilc_wlan_cfg_set_hword(frame, offset, id,
						      *((u16 *)buf));
		break;

	case CFG_WORD_CMD:
		if (size >= 4)
			ret = wilc_wlan_cfg_set_word(frame, offset, id,
						     *((u32 *)buf));
		break;

	case CFG_STR_CMD:
		ret = wilc_wlan_cfg_set_str(frame, offset, id, buf, size);
		break;

	case CFG_BIN_CMD:
		ret = wilc_wlan_cfg_set_bin(frame, offset, id, buf, size);
		break;
	default:
		PRINT_ER(vif->ndev, "illegal id\n");
	}

	return ret;
}

int wilc_wlan_cfg_get_wid(u8 *frame, u32 offset, u16 id)
{
	u8 *buf;

	if ((offset + 2) >= MAX_CFG_FRAME_SIZE)
		return 0;

	buf = &frame[offset];

	buf[0] = (u8)id;
	buf[1] = (u8)(id >> 8);

	return 2;
}

int wilc_wlan_cfg_get_wid_value(struct wilc_vif *vif, u16 wid, u8 *buffer,
				u32 buffer_size)
{
	u32 type = (wid >> 12) & 0xf;
	int i, ret = 0;

	i = 0;
	if (type == CFG_BYTE_CMD) {
		do {
			if (g_cfg_byte[i].id == WID_NIL)
				break;

			if (g_cfg_byte[i].id == wid) {
				memcpy(buffer,  &g_cfg_byte[i].val, 1);
				ret = 1;
				break;
			}
			i++;
		} while (1);
	} else if (type == CFG_HWORD_CMD) {
		do {
			if (g_cfg_hword[i].id == WID_NIL)
				break;

			if (g_cfg_hword[i].id == wid) {
				memcpy(buffer,  &g_cfg_hword[i].val, 2);
				ret = 2;
				break;
			}
			i++;
		} while (1);
	} else if (type == CFG_WORD_CMD) {
		do {
			if (g_cfg_word[i].id == WID_NIL)
				break;

			if (g_cfg_word[i].id == wid) {
				memcpy(buffer,  &g_cfg_word[i].val, 4);
				ret = 4;
				break;
			}
			i++;
		} while (1);
	} else if (type == CFG_STR_CMD) {
		do {
			u32 id = g_cfg_str[i].id;

			if (id == WID_NIL)
				break;

			if (id == wid) {
				u32 size = g_cfg_str[i].str[0] |
						(g_cfg_str[i].str[1] << 8);

				if (buffer_size >= size) {
					memcpy(buffer,  &g_cfg_str[i].str[2],
					       size);
					ret = size;
				}
				break;
			}
			i++;
		} while (1);
	} else if (type == CFG_BIN_CMD) { /* binary command */
		do {
			if (g_cfg_bin[i].id == WID_NIL)
				break;

			if (g_cfg_bin[i].id == wid) {
				uint32_t size = g_cfg_bin[i].bin[0] | 
					     (g_cfg_bin[i].bin[1]<<8);
				if (buffer_size >= size) {					
					memcpy(buffer, &g_cfg_bin[i].bin[2],
						size);
					ret = size;
				}
				break;
			}
			i++;
		} while (1);
	} else {
		PRINT_ER(vif->ndev, "[CFG]: illegal type (%08x)\n", wid);
	}

	return ret;
}

void wilc_wlan_cfg_indicate_rx(struct wilc *wilc, u8 *frame, int size,
			       struct wilc_cfg_rsp *rsp)
{
	u8 msg_type;
	u8 msg_id;

	msg_type = frame[0];
	msg_id = frame[1];      /* seq no */
	frame += 4;
	size -= 4;
	rsp->type = 0;

	/*
	 * The valid types of response messages are
	 * 'R' (Response),
	 * 'I' (Information), and
	 * 'N' (Network Information)
	 */

	switch (msg_type) {
	case 'R':
		wilc_wlan_parse_response_frame(wilc, frame, size);
		rsp->type = WILC_CFG_RSP;
		rsp->seq_no = msg_id;
		break;

	case 'I':
		wilc_wlan_parse_info_frame(wilc, frame);
		rsp->type = WILC_CFG_RSP_STATUS;
		rsp->seq_no = msg_id;
		/*call host interface info parse as well*/
		PRINT_D(wilc->vif[0]->ndev, RX_DBG,"Info message received\n");
		wilc_gnrl_async_info_received(wilc, frame - 4, size + 4);
		break;

	case 'N':
		wilc_network_info_received(wilc, frame - 4, size + 4);
		break;

	case 'S':
		PRINT_D(wilc->vif[0]->ndev, RX_DBG,"Scan Notification Received\n");
		wilc_scan_complete_received(wilc, frame - 4, size + 4);
		break;

	default:
		PRINT_D(wilc->vif[0]->ndev, RX_DBG,
			"Receive unknown message %d-%d-%d-%d-%d-%d-%d-%d\n",
			 frame[0], frame[1], frame[2], frame[3], frame[4],
			 frame[5], frame[6], frame[7]);
		rsp->seq_no = msg_id;
		break;
	}
}

int wilc_wlan_cfg_init(void)
{
	memset((void *)&g_mac, 0, sizeof(struct wilc_mac_cfg));
	return 1;
}
