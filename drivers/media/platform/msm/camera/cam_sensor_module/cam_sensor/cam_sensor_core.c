/* Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <cam_sensor_cmn_header.h>
#if VENDOR_EDIT
/*add by hongbo.dai@camera 20190505, for OIS firmware update*/
#include <linux/firmware.h>
#endif
#include "cam_sensor_core.h"
#include "cam_sensor_util.h"
#include "cam_soc_util.h"
#include "cam_trace.h"
#include "cam_common_util.h"
#include "cam_packet_util.h"
#ifdef VENDOR_EDIT
/* fangyan@Camera, 2019/08/31, Add for diff */
#include <soc/oppo/oppo_project.h>
#endif /* DBMDX_SOUND_TRIGGER_SUPPORT */

#ifdef VENDOR_EDIT
/*add by hongbo.dai@camera 20190221, get DPC Data for IMX471*/
#define FD_DFCT_NUM_ADDR 0x7678
#define SG_DFCT_NUM_ADDR 0x767A
#define FD_DFCT_ADDR 0x8B00
#define SG_DFCT_ADDR 0x8B10

#define V_ADDR_SHIFT 12
#define H_DATA_MASK 0xFFF80000
#define V_DATA_MASK 0x0007FF80

/* chen.li@RM.Cam, 20190806, add for get sensor version */
#define S5kGW1_SENSOR_ID   (0x971)   //s5kgw1 sensor id(0x971)
#define S5KGW1_VERSION_REG (0x0002)  //s5kgw1 version register address(0x0002)
#define SAMSUNG_SENSOR_MP1 (0xA101)  //s5kgw1 evt0.1(0xA101)
#define SAMSUNG_SENSOR_MP2 (0xA201)  //s5kgw1 evt0.2(0xA201)

#define MAX_LENGTH 128

struct sony_dfct_tbl_t imx471_dfct_tbl;

static int sensor_imx471_get_dpc_data(struct cam_sensor_ctrl_t *s_ctrl)
{
    int i = 0, j = 0;
    int rc = 0;
    int check_reg_val, dfct_data_h, dfct_data_l;
    int dfct_data = 0;
    int fd_dfct_num = 0, sg_dfct_num = 0;
    int retry_cnt = 5;
    int data_h = 0, data_v = 0;
    int fd_dfct_addr = FD_DFCT_ADDR;
    int sg_dfct_addr = SG_DFCT_ADDR;

    CAM_INFO(CAM_SENSOR, "sensor_imx471_get_dpc_data enter");
    if (s_ctrl == NULL) {
        CAM_ERR(CAM_SENSOR, "Invalid Args");
        return -EINVAL;
    }

    memset(&imx471_dfct_tbl, 0, sizeof(struct sony_dfct_tbl_t));

    for (i = 0; i < retry_cnt; i++) {
        check_reg_val = 0;
        rc = camera_io_dev_read(&(s_ctrl->io_master_info),
            FD_DFCT_NUM_ADDR, &check_reg_val,
            CAMERA_SENSOR_I2C_TYPE_WORD,
            CAMERA_SENSOR_I2C_TYPE_BYTE);

        if (0 == rc) {
            fd_dfct_num = check_reg_val & 0x07;
            if (fd_dfct_num > FD_DFCT_MAX_NUM)
                fd_dfct_num = FD_DFCT_MAX_NUM;
            break;
        }
    }

    for (i = 0; i < retry_cnt; i++) {
        check_reg_val = 0;
        rc = camera_io_dev_read(&(s_ctrl->io_master_info),
            SG_DFCT_NUM_ADDR, &check_reg_val,
            CAMERA_SENSOR_I2C_TYPE_WORD,
            CAMERA_SENSOR_I2C_TYPE_WORD);

        if (0 == rc) {
            sg_dfct_num = check_reg_val & 0x01FF;
            if (sg_dfct_num > SG_DFCT_MAX_NUM)
                sg_dfct_num = SG_DFCT_MAX_NUM;
            break;
        }
    }

    CAM_INFO(CAM_SENSOR, " fd_dfct_num = %d, sg_dfct_num = %d", fd_dfct_num, sg_dfct_num);
    imx471_dfct_tbl.fd_dfct_num = fd_dfct_num;
    imx471_dfct_tbl.sg_dfct_num = sg_dfct_num;

    if (fd_dfct_num > 0) {
        for (j = 0; j < fd_dfct_num; j++) {
            dfct_data = 0;
            for (i = 0; i < retry_cnt; i++) {
                dfct_data_h = 0;
                rc = camera_io_dev_read(&(s_ctrl->io_master_info),
                        fd_dfct_addr, &dfct_data_h,
                        CAMERA_SENSOR_I2C_TYPE_WORD,
                        CAMERA_SENSOR_I2C_TYPE_WORD);
                if (0 == rc) {
                    break;
                }
            }
            for (i = 0; i < retry_cnt; i++) {
                dfct_data_l = 0;
                rc = camera_io_dev_read(&(s_ctrl->io_master_info),
                        fd_dfct_addr+2, &dfct_data_l,
                        CAMERA_SENSOR_I2C_TYPE_WORD,
                        CAMERA_SENSOR_I2C_TYPE_WORD);
                if (0 == rc) {
                    break;
                }
            }
            CAM_DBG(CAM_SENSOR, " dfct_data_h = 0x%x, dfct_data_l = 0x%x", dfct_data_h, dfct_data_l);
            dfct_data = (dfct_data_h << 16) | dfct_data_l;
            data_h = 0;
            data_v = 0;
            data_h = (dfct_data & (H_DATA_MASK >> j%8)) >> (19 - j%8); //19 = 32 -13;
            data_v = (dfct_data & (V_DATA_MASK >> j%8)) >> (7 - j%8);  // 7 = 32 -13 -12;
            CAM_DBG(CAM_SENSOR, "j = %d, H = %d, V = %d", j, data_h, data_v);
            imx471_dfct_tbl.fd_dfct_addr[j] = ((data_h & 0x1FFF) << V_ADDR_SHIFT) | (data_v & 0x0FFF);
            CAM_DBG(CAM_SENSOR, "fd_dfct_data[%d] = 0x%08x", j, imx471_dfct_tbl.fd_dfct_addr[j]);
            fd_dfct_addr = fd_dfct_addr + 3 + ((j+1)%8 == 0);
        }
    }
    if (sg_dfct_num > 0) {
        for (j = 0; j < sg_dfct_num; j++) {
            dfct_data = 0;
            for (i = 0; i < retry_cnt; i++) {
                dfct_data_h = 0;
                rc = camera_io_dev_read(&(s_ctrl->io_master_info),
                        sg_dfct_addr, &dfct_data_h,
                        CAMERA_SENSOR_I2C_TYPE_WORD,
                        CAMERA_SENSOR_I2C_TYPE_WORD);
                if (0 == rc) {
                    break;
                }
            }
            for (i = 0; i < retry_cnt; i++) {
                dfct_data_l = 0;
                rc = camera_io_dev_read(&(s_ctrl->io_master_info),
                        sg_dfct_addr+2, &dfct_data_l,
                        CAMERA_SENSOR_I2C_TYPE_WORD,
                        CAMERA_SENSOR_I2C_TYPE_WORD);
                if (0 == rc) {
                    break;
                }
            }
            CAM_DBG(CAM_SENSOR, " dfct_data_h = 0x%x, dfct_data_l = 0x%x", dfct_data_h, dfct_data_l);
            dfct_data = (dfct_data_h << 16) | dfct_data_l;
            data_h = 0;
            data_v = 0;
            data_h = (dfct_data & (H_DATA_MASK >> j%8)) >> (19 - j%8); //19 = 32 -13;
            data_v = (dfct_data & (V_DATA_MASK >> j%8)) >> (7 - j%8);  // 7 = 32 -13 -12;
            CAM_DBG(CAM_SENSOR, "j = %d, H = %d, V = %d", j, data_h, data_v);
            imx471_dfct_tbl.sg_dfct_addr[j] = ((data_h & 0x1FFF) << V_ADDR_SHIFT) | (data_v & 0x0FFF);
            CAM_DBG(CAM_SENSOR, "sg_dfct_data[%d] = 0x%08x", j, imx471_dfct_tbl.sg_dfct_addr[j]);
            sg_dfct_addr = sg_dfct_addr + 3 + ((j+1)%8 == 0);
        }
    }

    CAM_INFO(CAM_SENSOR, "exit");
    return rc;
}
#endif

static void cam_sensor_update_req_mgr(
	struct cam_sensor_ctrl_t *s_ctrl,
	struct cam_packet *csl_packet)
{
	struct cam_req_mgr_add_request add_req;

	add_req.link_hdl = s_ctrl->bridge_intf.link_hdl;
	add_req.req_id = csl_packet->header.request_id;
	CAM_DBG(CAM_SENSOR, " Rxed Req Id: %lld",
		csl_packet->header.request_id);
	add_req.dev_hdl = s_ctrl->bridge_intf.device_hdl;
	add_req.skip_before_applying = 0;
	if (s_ctrl->bridge_intf.crm_cb &&
		s_ctrl->bridge_intf.crm_cb->add_req)
		s_ctrl->bridge_intf.crm_cb->add_req(&add_req);

	CAM_DBG(CAM_SENSOR, " add req to req mgr: %lld",
			add_req.req_id);
}

static void cam_sensor_release_stream_rsc(
	struct cam_sensor_ctrl_t *s_ctrl)
{
	struct i2c_settings_array *i2c_set = NULL;
	int rc;

	i2c_set = &(s_ctrl->i2c_data.streamoff_settings);
	if (i2c_set->is_settings_valid == 1) {
		i2c_set->is_settings_valid = -1;
		rc = delete_request(i2c_set);
		if (rc < 0)
			CAM_ERR(CAM_SENSOR,
				"failed while deleting Streamoff settings");
	}

	i2c_set = &(s_ctrl->i2c_data.streamon_settings);
	if (i2c_set->is_settings_valid == 1) {
		i2c_set->is_settings_valid = -1;
		rc = delete_request(i2c_set);
		if (rc < 0)
			CAM_ERR(CAM_SENSOR,
				"failed while deleting Streamon settings");
	}
}

static void cam_sensor_release_per_frame_resource(
	struct cam_sensor_ctrl_t *s_ctrl)
{
	struct i2c_settings_array *i2c_set = NULL;
	int i, rc;

	if (s_ctrl->i2c_data.per_frame != NULL) {
		for (i = 0; i < MAX_PER_FRAME_ARRAY; i++) {
			i2c_set = &(s_ctrl->i2c_data.per_frame[i]);
			if (i2c_set->is_settings_valid == 1) {
				i2c_set->is_settings_valid = -1;
				rc = delete_request(i2c_set);
				if (rc < 0)
					CAM_ERR(CAM_SENSOR,
						"delete request: %lld rc: %d",
						i2c_set->request_id, rc);
			}
		}
	}
}

static int32_t cam_sensor_i2c_pkt_parse(struct cam_sensor_ctrl_t *s_ctrl,
	void *arg)
{
	int32_t rc = 0;
	uintptr_t generic_ptr;
	struct cam_control *ioctl_ctrl = NULL;
	struct cam_packet *csl_packet = NULL;
	struct cam_cmd_buf_desc *cmd_desc = NULL;
	struct i2c_settings_array *i2c_reg_settings = NULL;
	size_t len_of_buff = 0;
	size_t remain_len = 0;
	uint32_t *offset = NULL;
	struct cam_config_dev_cmd config;
	struct i2c_data_settings *i2c_data = NULL;

	ioctl_ctrl = (struct cam_control *)arg;

	if (ioctl_ctrl->handle_type != CAM_HANDLE_USER_POINTER) {
		CAM_ERR(CAM_SENSOR, "Invalid Handle Type");
		return -EINVAL;
	}

	if (copy_from_user(&config,
		u64_to_user_ptr(ioctl_ctrl->handle),
		sizeof(config)))
		return -EFAULT;

	rc = cam_mem_get_cpu_buf(
		config.packet_handle,
		&generic_ptr,
		&len_of_buff);
	if (rc < 0) {
		CAM_ERR(CAM_SENSOR, "Failed in getting the packet: %d", rc);
		return rc;
	}

	remain_len = len_of_buff;
	if ((sizeof(struct cam_packet) > len_of_buff) ||
		((size_t)config.offset >= len_of_buff -
		sizeof(struct cam_packet))) {
		CAM_ERR(CAM_SENSOR,
			"Inval cam_packet strut size: %zu, len_of_buff: %zu",
			 sizeof(struct cam_packet), len_of_buff);
		rc = -EINVAL;
		goto rel_pkt_buf;
	}

	remain_len -= (size_t)config.offset;
	csl_packet = (struct cam_packet *)(generic_ptr +
		(uint32_t)config.offset);

	if (cam_packet_util_validate_packet(csl_packet,
		remain_len)) {
		CAM_ERR(CAM_SENSOR, "Invalid packet params");
		rc = -EINVAL;
		goto rel_pkt_buf;

	}

	if ((csl_packet->header.op_code & 0xFFFFFF) !=
		CAM_SENSOR_PACKET_OPCODE_SENSOR_INITIAL_CONFIG &&
		csl_packet->header.request_id <= s_ctrl->last_flush_req
		&& s_ctrl->last_flush_req != 0) {
		CAM_ERR(CAM_SENSOR,
			"reject request %lld, last request to flush %lld",
			csl_packet->header.request_id, s_ctrl->last_flush_req);
		rc = -EINVAL;
		goto rel_pkt_buf;
	}

	if (csl_packet->header.request_id > s_ctrl->last_flush_req)
		s_ctrl->last_flush_req = 0;

	i2c_data = &(s_ctrl->i2c_data);
	CAM_DBG(CAM_SENSOR, "Header OpCode: %d", csl_packet->header.op_code);
	switch (csl_packet->header.op_code & 0xFFFFFF) {
	case CAM_SENSOR_PACKET_OPCODE_SENSOR_INITIAL_CONFIG: {
		i2c_reg_settings = &i2c_data->init_settings;
		i2c_reg_settings->request_id = 0;
		i2c_reg_settings->is_settings_valid = 1;
		break;
	}
	case CAM_SENSOR_PACKET_OPCODE_SENSOR_CONFIG: {
		i2c_reg_settings = &i2c_data->config_settings;
		i2c_reg_settings->request_id = 0;
		i2c_reg_settings->is_settings_valid = 1;
		break;
	}
	case CAM_SENSOR_PACKET_OPCODE_SENSOR_STREAMON: {
		if (s_ctrl->streamon_count > 0)
			goto rel_pkt_buf;

		s_ctrl->streamon_count = s_ctrl->streamon_count + 1;
		i2c_reg_settings = &i2c_data->streamon_settings;
		i2c_reg_settings->request_id = 0;
		i2c_reg_settings->is_settings_valid = 1;
		break;
	}
	case CAM_SENSOR_PACKET_OPCODE_SENSOR_STREAMOFF: {
		if (s_ctrl->streamoff_count > 0)
			goto rel_pkt_buf;

		s_ctrl->streamoff_count = s_ctrl->streamoff_count + 1;
		i2c_reg_settings = &i2c_data->streamoff_settings;
		i2c_reg_settings->request_id = 0;
		i2c_reg_settings->is_settings_valid = 1;
		break;
	}

	case CAM_SENSOR_PACKET_OPCODE_SENSOR_UPDATE: {
		if ((s_ctrl->sensor_state == CAM_SENSOR_INIT) ||
			(s_ctrl->sensor_state == CAM_SENSOR_ACQUIRE)) {
			CAM_WARN(CAM_SENSOR,
				"Rxed Update packets without linking");
			goto rel_pkt_buf;
		}

		i2c_reg_settings =
			&i2c_data->per_frame[csl_packet->header.request_id %
				MAX_PER_FRAME_ARRAY];
		CAM_DBG(CAM_SENSOR, "Received Packet: %lld req: %lld",
			csl_packet->header.request_id % MAX_PER_FRAME_ARRAY,
			csl_packet->header.request_id);
		if (i2c_reg_settings->is_settings_valid == 1) {
			CAM_ERR(CAM_SENSOR,
				"Already some pkt in offset req : %lld",
				csl_packet->header.request_id);
			/*
			 * Update req mgr even in case of failure.
			 * This will help not to wait indefinitely
			 * and freeze. If this log is triggered then
			 * fix it.
			 */
			cam_sensor_update_req_mgr(s_ctrl, csl_packet);
			goto rel_pkt_buf;
		}
		break;
	}
	case CAM_SENSOR_PACKET_OPCODE_SENSOR_NOP: {
		if ((s_ctrl->sensor_state == CAM_SENSOR_INIT) ||
			(s_ctrl->sensor_state == CAM_SENSOR_ACQUIRE)) {
			CAM_WARN(CAM_SENSOR,
				"Rxed NOP packets without linking");
			goto rel_pkt_buf;
		}

		cam_sensor_update_req_mgr(s_ctrl, csl_packet);
		goto rel_pkt_buf;
	}
	default:
		CAM_ERR(CAM_SENSOR, "Invalid Packet Header");
		rc = -EINVAL;
		goto rel_pkt_buf;
	}

	offset = (uint32_t *)&csl_packet->payload;
	offset += csl_packet->cmd_buf_offset / 4;
	cmd_desc = (struct cam_cmd_buf_desc *)(offset);

	rc = cam_sensor_i2c_command_parser(&s_ctrl->io_master_info,
			i2c_reg_settings, cmd_desc, 1);
	if (rc < 0) {
		CAM_ERR(CAM_SENSOR, "Fail parsing I2C Pkt: %d", rc);
		goto rel_pkt_buf;
	}

	if ((csl_packet->header.op_code & 0xFFFFFF) ==
		CAM_SENSOR_PACKET_OPCODE_SENSOR_UPDATE) {
		i2c_reg_settings->request_id =
			csl_packet->header.request_id;
		cam_sensor_update_req_mgr(s_ctrl, csl_packet);
	}

rel_pkt_buf:
	if (cam_mem_put_cpu_buf(config.packet_handle))
		CAM_WARN(CAM_SENSOR, "Failed in put the buffer: 0x%x",
			config.packet_handle);

	return rc;
}

static int32_t cam_sensor_i2c_modes_util(
	struct camera_io_master *io_master_info,
	struct i2c_settings_list *i2c_list)
{
	int32_t rc = 0;
	uint32_t i, size;

	if (i2c_list->op_code == CAM_SENSOR_I2C_WRITE_RANDOM) {
		rc = camera_io_dev_write(io_master_info,
			&(i2c_list->i2c_settings));
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR,
				"Failed to random write I2C settings: %d",
				rc);
			return rc;
		}
	} else if (i2c_list->op_code == CAM_SENSOR_I2C_WRITE_SEQ) {
		rc = camera_io_dev_write_continuous(
			io_master_info,
			&(i2c_list->i2c_settings),
			0);
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR,
				"Failed to seq write I2C settings: %d",
				rc);
			return rc;
		}
	} else if (i2c_list->op_code == CAM_SENSOR_I2C_WRITE_BURST) {
		rc = camera_io_dev_write_continuous(
			io_master_info,
			&(i2c_list->i2c_settings),
			1);
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR,
				"Failed to burst write I2C settings: %d",
				rc);
			return rc;
		}
	} else if (i2c_list->op_code == CAM_SENSOR_I2C_POLL) {
		size = i2c_list->i2c_settings.size;
		for (i = 0; i < size; i++) {
			rc = camera_io_dev_poll(
			io_master_info,
			i2c_list->i2c_settings.reg_setting[i].reg_addr,
			i2c_list->i2c_settings.reg_setting[i].reg_data,
			i2c_list->i2c_settings.reg_setting[i].data_mask,
			i2c_list->i2c_settings.addr_type,
			i2c_list->i2c_settings.data_type,
			i2c_list->i2c_settings.reg_setting[i].delay);
			if (rc < 0) {
				CAM_ERR(CAM_SENSOR,
					"i2c poll apply setting Fail: %d", rc);
				return rc;
			}
		}
	}

	return rc;
}

int32_t cam_sensor_update_i2c_info(struct cam_cmd_i2c_info *i2c_info,
	struct cam_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	struct cam_sensor_cci_client   *cci_client = NULL;

	if (s_ctrl->io_master_info.master_type == CCI_MASTER) {
		cci_client = s_ctrl->io_master_info.cci_client;
		if (!cci_client) {
			CAM_ERR(CAM_SENSOR, "failed: cci_client %pK",
				cci_client);
			return -EINVAL;
		}
		cci_client->cci_i2c_master = s_ctrl->cci_i2c_master;
		cci_client->sid = i2c_info->slave_addr >> 1;
		cci_client->retries = 3;
		cci_client->id_map = 0;
		cci_client->i2c_freq_mode = i2c_info->i2c_freq_mode;
		CAM_DBG(CAM_SENSOR, " Master: %d sid: %d freq_mode: %d",
			cci_client->cci_i2c_master, i2c_info->slave_addr,
			i2c_info->i2c_freq_mode);
	}

	s_ctrl->sensordata->slave_info.sensor_slave_addr =
		i2c_info->slave_addr;
	return rc;
}

int32_t cam_sensor_update_slave_info(struct cam_cmd_probe *probe_info,
	struct cam_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;

	s_ctrl->sensordata->slave_info.sensor_id_reg_addr =
		probe_info->reg_addr;
	s_ctrl->sensordata->slave_info.sensor_id =
		probe_info->expected_data;
	s_ctrl->sensordata->slave_info.sensor_id_mask =
		probe_info->data_mask;
	/* Userspace passes the pipeline delay in reserved field */
	s_ctrl->pipeline_delay =
		probe_info->reserved;

	s_ctrl->sensor_probe_addr_type =  probe_info->addr_type;
	s_ctrl->sensor_probe_data_type =  probe_info->data_type;
	CAM_DBG(CAM_SENSOR,
		"Sensor Addr: 0x%x sensor_id: 0x%x sensor_mask: 0x%x sensor_pipeline_delay:0x%x",
		s_ctrl->sensordata->slave_info.sensor_id_reg_addr,
		s_ctrl->sensordata->slave_info.sensor_id,
		s_ctrl->sensordata->slave_info.sensor_id_mask,
		s_ctrl->pipeline_delay);
	return rc;
}

int32_t cam_handle_cmd_buffers_for_probe(void *cmd_buf,
	struct cam_sensor_ctrl_t *s_ctrl,
	int32_t cmd_buf_num, uint32_t cmd_buf_length, size_t remain_len)
{
	int32_t rc = 0;

	switch (cmd_buf_num) {
	case 0: {
		struct cam_cmd_i2c_info *i2c_info = NULL;
		struct cam_cmd_probe *probe_info;

		if (remain_len <
			(sizeof(struct cam_cmd_i2c_info) +
			sizeof(struct cam_cmd_probe))) {
			CAM_ERR(CAM_SENSOR,
				"not enough buffer for cam_cmd_i2c_info");
			return -EINVAL;
		}
		i2c_info = (struct cam_cmd_i2c_info *)cmd_buf;
		rc = cam_sensor_update_i2c_info(i2c_info, s_ctrl);
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR, "Failed in Updating the i2c Info");
			return rc;
		}
		probe_info = (struct cam_cmd_probe *)
			(cmd_buf + sizeof(struct cam_cmd_i2c_info));
		rc = cam_sensor_update_slave_info(probe_info, s_ctrl);
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR, "Updating the slave Info");
			return rc;
		}
		cmd_buf = probe_info;
	}
		break;
	case 1: {
		rc = cam_sensor_update_power_settings(cmd_buf,
			cmd_buf_length, &s_ctrl->sensordata->power_info,
			remain_len);
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR,
				"Failed in updating power settings");
			return rc;
		}
	}
		break;
	default:
		CAM_ERR(CAM_SENSOR, "Invalid command buffer");
		break;
	}
	return rc;
}

int32_t cam_handle_mem_ptr(uint64_t handle, struct cam_sensor_ctrl_t *s_ctrl)
{
	int rc = 0, i;
	uint32_t *cmd_buf;
	void *ptr;
	size_t len;
	struct cam_packet *pkt = NULL;
	struct cam_cmd_buf_desc *cmd_desc = NULL;
	uintptr_t cmd_buf1 = 0;
	uintptr_t packet = 0;
	size_t    remain_len = 0;

	rc = cam_mem_get_cpu_buf(handle,
		&packet, &len);
	if (rc < 0) {
		CAM_ERR(CAM_SENSOR, "Failed to get the command Buffer");
		return -EINVAL;
	}

	pkt = (struct cam_packet *)packet;
	if (pkt == NULL) {
		CAM_ERR(CAM_SENSOR, "packet pos is invalid");
		rc = -EINVAL;
		goto rel_pkt_buf;
	}

	if ((len < sizeof(struct cam_packet)) ||
		(pkt->cmd_buf_offset >= (len - sizeof(struct cam_packet)))) {
		CAM_ERR(CAM_SENSOR, "Not enough buf provided");
		rc = -EINVAL;
		goto rel_pkt_buf;
	}

	cmd_desc = (struct cam_cmd_buf_desc *)
		((uint32_t *)&pkt->payload + pkt->cmd_buf_offset/4);
	if (cmd_desc == NULL) {
		CAM_ERR(CAM_SENSOR, "command descriptor pos is invalid");
		rc = -EINVAL;
		goto rel_pkt_buf;
	}
	if (pkt->num_cmd_buf != 2) {
		CAM_ERR(CAM_SENSOR, "Expected More Command Buffers : %d",
			 pkt->num_cmd_buf);
		rc = -EINVAL;
		goto rel_pkt_buf;
	}

	for (i = 0; i < pkt->num_cmd_buf; i++) {
		if (!(cmd_desc[i].length))
			continue;
		rc = cam_mem_get_cpu_buf(cmd_desc[i].mem_handle,
			&cmd_buf1, &len);
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR,
				"Failed to parse the command Buffer Header");
			goto rel_pkt_buf;
		}
		if (cmd_desc[i].offset >= len) {
			CAM_ERR(CAM_SENSOR,
				"offset past length of buffer");
			rc = -EINVAL;
			goto rel_pkt_buf;
		}
		remain_len = len - cmd_desc[i].offset;
		if (cmd_desc[i].length > remain_len) {
			CAM_ERR(CAM_SENSOR,
				"Not enough buffer provided for cmd");
			rc = -EINVAL;
			goto rel_pkt_buf;
		}
		cmd_buf = (uint32_t *)cmd_buf1;
		cmd_buf += cmd_desc[i].offset/4;
		ptr = (void *) cmd_buf;

		rc = cam_handle_cmd_buffers_for_probe(ptr, s_ctrl,
			i, cmd_desc[i].length, remain_len);
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR,
				"Failed to parse the command Buffer Header");
			goto rel_cmd_buf;
		}

		if (cam_mem_put_cpu_buf(cmd_desc[i].mem_handle))
			CAM_WARN(CAM_SENSOR,
				"Failed to put command Buffer : 0x%x",
				cmd_desc[i].mem_handle);
	}

	if (cam_mem_put_cpu_buf(handle))
		CAM_WARN(CAM_SENSOR, "Failed to put the command Buffer: 0x%x",
			handle);

	return rc;

rel_cmd_buf:
	if (cam_mem_put_cpu_buf(cmd_desc[i].mem_handle))
		CAM_WARN(CAM_SENSOR, "Failed to put command Buffer : 0x%x",
			cmd_desc[i].mem_handle);
rel_pkt_buf:
	if (cam_mem_put_cpu_buf(handle))
		CAM_WARN(CAM_SENSOR, "Failed to put the command Buffer: 0x%x",
			handle);

	return rc;
}

void cam_sensor_query_cap(struct cam_sensor_ctrl_t *s_ctrl,
	struct  cam_sensor_query_cap *query_cap)
{
	query_cap->pos_roll = s_ctrl->sensordata->pos_roll;
	query_cap->pos_pitch = s_ctrl->sensordata->pos_pitch;
	query_cap->pos_yaw = s_ctrl->sensordata->pos_yaw;
	query_cap->secure_camera = 0;
	query_cap->actuator_slot_id =
		s_ctrl->sensordata->subdev_id[SUB_MODULE_ACTUATOR];
	query_cap->csiphy_slot_id =
		s_ctrl->sensordata->subdev_id[SUB_MODULE_CSIPHY];
	query_cap->eeprom_slot_id =
		s_ctrl->sensordata->subdev_id[SUB_MODULE_EEPROM];
	query_cap->flash_slot_id =
		s_ctrl->sensordata->subdev_id[SUB_MODULE_LED_FLASH];
	query_cap->ois_slot_id =
		s_ctrl->sensordata->subdev_id[SUB_MODULE_OIS];
	query_cap->slot_info =
		s_ctrl->soc_info.index;
}

static uint16_t cam_sensor_id_by_mask(struct cam_sensor_ctrl_t *s_ctrl,
	uint32_t chipid)
{
	uint16_t sensor_id = (uint16_t)(chipid & 0xFFFF);
	int16_t sensor_id_mask = s_ctrl->sensordata->slave_info.sensor_id_mask;

	if (!sensor_id_mask)
		sensor_id_mask = ~sensor_id_mask;

	sensor_id &= sensor_id_mask;
	sensor_id_mask &= -sensor_id_mask;
	sensor_id_mask -= 1;
	while (sensor_id_mask) {
		sensor_id_mask >>= 1;
		sensor_id >>= 1;
	}
	return sensor_id;
}

void cam_sensor_shutdown(struct cam_sensor_ctrl_t *s_ctrl)
{
	struct cam_sensor_power_ctrl_t *power_info =
		&s_ctrl->sensordata->power_info;
	int rc = 0;

	if ((s_ctrl->sensor_state == CAM_SENSOR_INIT) &&
		(s_ctrl->is_probe_succeed == 0))
		return;

	cam_sensor_release_stream_rsc(s_ctrl);
	cam_sensor_release_per_frame_resource(s_ctrl);

	if (s_ctrl->sensor_state != CAM_SENSOR_INIT)
		cam_sensor_power_down(s_ctrl);

	rc = cam_destroy_device_hdl(s_ctrl->bridge_intf.device_hdl);
	if (rc < 0)
		CAM_ERR(CAM_SENSOR, "dhdl already destroyed: rc = %d", rc);
	s_ctrl->bridge_intf.device_hdl = -1;
	s_ctrl->bridge_intf.link_hdl = -1;
	s_ctrl->bridge_intf.session_hdl = -1;
	kfree(power_info->power_setting);
	kfree(power_info->power_down_setting);
	power_info->power_setting = NULL;
	power_info->power_down_setting = NULL;
	power_info->power_setting_size = 0;
	power_info->power_down_setting_size = 0;
	s_ctrl->streamon_count = 0;
	s_ctrl->streamoff_count = 0;
	s_ctrl->is_probe_succeed = 0;
	s_ctrl->last_flush_req = 0;
	s_ctrl->sensor_state = CAM_SENSOR_INIT;
}

int cam_sensor_match_id(struct cam_sensor_ctrl_t *s_ctrl)
{
	int rc = 0;
	uint32_t chipid = 0;
	struct cam_camera_slave_info *slave_info;
#ifdef VENDOR_EDIT
        /* wanghaoran@camera.driver. 2018/11/2, add for read sensor gc02m0 of camera */
	uint32_t gc02m0_high = 0;
	uint32_t gc02m0_low = 0;
	uint32_t chipid_high = 0;
	uint32_t chipid_low = 0;
    /* chen.li@RM.Cam, 20190806, add for get sensor version */
	uint32_t sensor_version = 0;
	/*add by zhixian.mai@camera 20190717, for distinguish the second source camera module*/
	struct cam_sensor_cci_client ee_cci_client ;
	uint32_t ee_vcmid = 0 ;
	const uint8_t IMX586_EEPROM_SID = (0xA0 >> 1);
	const uint8_t IMX586_EEPROM_VCMID_ADDR = 0x0A;
	const uint8_t IMX586_FIRST_SOURCE_VCMID = 0xC2;
	const uint8_t IMX586_SECOND_SOURCE_VCMID = 0x3A;
	const uint32_t IMX586_FIRST_SOURCE_CHIPID = 0xFFFF;
	const uint32_t IMX586_SECOND_SOURCE_CHIPID = 0xFFFE;
#endif

	slave_info = &(s_ctrl->sensordata->slave_info);

	if (!slave_info) {
		CAM_ERR(CAM_SENSOR, " failed: %pK",
			 slave_info);
		return -EINVAL;
	}
#ifndef VENDOR_EDIT
/* wanghaoran@camera.driver. 2018/11/2, add for read sensor gc02m0 of camera */
/* fangyan@camera.driver 2019/06/15, add for read sensor gc2375 of camera */
/* zhixian.Mai@camera.driver. 2019/9/25, modify for read sensor gc02m0 of camera */
	rc = camera_io_dev_read(
		&(s_ctrl->io_master_info),
		slave_info->sensor_id_reg_addr,
		&chipid, CAMERA_SENSOR_I2C_TYPE_WORD,
		CAMERA_SENSOR_I2C_TYPE_WORD);
#else
       if (slave_info->sensor_id == 0x02d0
		|| slave_info->sensor_id == 0x2375) {
		gc02m0_high = slave_info->sensor_id_reg_addr & 0xff00;
		gc02m0_high = gc02m0_high >> 8;
		gc02m0_low = slave_info->sensor_id_reg_addr & 0x00ff;
		rc = camera_io_dev_read(
		     &(s_ctrl->io_master_info),
		     gc02m0_high,
		     &chipid_high, CAMERA_SENSOR_I2C_TYPE_BYTE,
		     CAMERA_SENSOR_I2C_TYPE_BYTE);

		if (rc) {
			goto match_id_err ;
		}
		CAM_ERR(CAM_SENSOR, "gc02m0_high: 0x%x chipid_high id 0x%x:",
                        gc02m0_high, chipid_high);

		rc = camera_io_dev_read(
		     &(s_ctrl->io_master_info),
		     gc02m0_low,
		     &chipid_low, CAMERA_SENSOR_I2C_TYPE_BYTE,
		     CAMERA_SENSOR_I2C_TYPE_BYTE);

		if (rc) {
			goto match_id_err ;
		}

		CAM_ERR(CAM_SENSOR, "gc02m0_low: 0x%x chipid_low id 0x%x:",
	               gc02m0_low, chipid_low);

		chipid = ((chipid_high << 8) & 0xff00) | (chipid_low & 0x00ff);

	}else {
		   rc = camera_io_dev_read(
			   &(s_ctrl->io_master_info),
			   slave_info->sensor_id_reg_addr,
			   &chipid, CAMERA_SENSOR_I2C_TYPE_WORD,
			   CAMERA_SENSOR_I2C_TYPE_WORD);
	}

	/* chen.li@RM.Cam, 20190806, add for get sensor version */
	if (chipid == S5kGW1_SENSOR_ID) {
		rc = camera_io_dev_read(
			&(s_ctrl->io_master_info),
			S5KGW1_VERSION_REG,
			&sensor_version, CAMERA_SENSOR_I2C_TYPE_WORD,
			CAMERA_SENSOR_I2C_TYPE_WORD);

		CAM_INFO(CAM_SENSOR, "s5kgw1 sensor_version: 0x%x",
				sensor_version);
		if (sensor_version == SAMSUNG_SENSOR_MP1) {
			s_ctrl->sensordata->slave_info.sensor_version = 0;
		} else if (sensor_version == SAMSUNG_SENSOR_MP2){
			s_ctrl->sensordata->slave_info.sensor_version = 1;
		}
		CAM_INFO(CAM_SENSOR, "s5kgw1 slave_info.sensor_version: %d:",
				s_ctrl->sensordata->slave_info.sensor_version);
	}
	/*add by zhixian.mai@camera 20190715, add for distinguish  second source camera module*/
	if (slave_info->sensor_id == IMX586_FIRST_SOURCE_CHIPID || \
			slave_info->sensor_id == IMX586_SECOND_SOURCE_CHIPID) {
		memcpy(&ee_cci_client, s_ctrl->io_master_info.cci_client,sizeof(struct cam_sensor_cci_client));
		ee_cci_client.sid = IMX586_EEPROM_SID;
		rc = cam_cci_i2c_read(&ee_cci_client,
					IMX586_EEPROM_VCMID_ADDR,
					&ee_vcmid, CAMERA_SENSOR_I2C_TYPE_WORD,
					CAMERA_SENSOR_I2C_TYPE_BYTE);

		 CAM_ERR(CAM_SENSOR, "distinguish imx586 camera module, vcm id : 0x%x ",ee_vcmid);
		if (IMX586_FIRST_SOURCE_VCMID == ee_vcmid) {
			chipid = IMX586_FIRST_SOURCE_CHIPID;
		} else if (IMX586_SECOND_SOURCE_VCMID == ee_vcmid) {
			chipid = IMX586_SECOND_SOURCE_CHIPID;
		} else {
			chipid = IMX586_FIRST_SOURCE_CHIPID;
		}
	}
match_id_err:
#endif
	CAM_DBG(CAM_SENSOR, "read id: 0x%x expected id 0x%x:",
			 chipid, slave_info->sensor_id);
	if (cam_sensor_id_by_mask(s_ctrl, chipid) != slave_info->sensor_id) {
		CAM_ERR(CAM_SENSOR, "chip id %x does not match %x",
				chipid, slave_info->sensor_id);
		return -ENODEV;
	}
	#ifdef VENDOR_EDIT
	/*add by hongbo.dai@camera 20190221, get DPC Data for IMX471*/
	if (slave_info->sensor_id == 0x0471) {
		sensor_imx471_get_dpc_data(s_ctrl);
	}
	#endif
	return rc;
}

int32_t cam_sensor_driver_cmd(struct cam_sensor_ctrl_t *s_ctrl,
	void *arg)
{
	int rc = 0;
	struct cam_control *cmd = (struct cam_control *)arg;
	struct cam_sensor_power_ctrl_t *power_info =
		&s_ctrl->sensordata->power_info;
	if (!s_ctrl || !arg) {
		CAM_ERR(CAM_SENSOR, "s_ctrl is NULL");
		return -EINVAL;
	}

	if (cmd->op_code != CAM_SENSOR_PROBE_CMD) {
		if (cmd->handle_type != CAM_HANDLE_USER_POINTER) {
			CAM_ERR(CAM_SENSOR, "Invalid handle type: %d",
				cmd->handle_type);
			return -EINVAL;
		}
	}

	mutex_lock(&(s_ctrl->cam_sensor_mutex));
	switch (cmd->op_code) {
	case CAM_SENSOR_PROBE_CMD: {
		if (s_ctrl->is_probe_succeed == 1) {
			CAM_ERR(CAM_SENSOR,
				"Already Sensor Probed in the slot");
			break;
		}

		if (cmd->handle_type ==
			CAM_HANDLE_MEM_HANDLE) {
			rc = cam_handle_mem_ptr(cmd->handle, s_ctrl);
			if (rc < 0) {
				CAM_ERR(CAM_SENSOR, "Get Buffer Handle Failed");
				goto release_mutex;
			}
		} else {
			CAM_ERR(CAM_SENSOR, "Invalid Command Type: %d",
				 cmd->handle_type);
			rc = -EINVAL;
			goto release_mutex;
		}

		/* Parse and fill vreg params for powerup settings */
		rc = msm_camera_fill_vreg_params(
			&s_ctrl->soc_info,
			s_ctrl->sensordata->power_info.power_setting,
			s_ctrl->sensordata->power_info.power_setting_size);
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR,
				"Fail in filling vreg params for PUP rc %d",
				 rc);
			goto free_power_settings;
		}

		/* Parse and fill vreg params for powerdown settings*/
		rc = msm_camera_fill_vreg_params(
			&s_ctrl->soc_info,
			s_ctrl->sensordata->power_info.power_down_setting,
			s_ctrl->sensordata->power_info.power_down_setting_size);
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR,
				"Fail in filling vreg params for PDOWN rc %d",
				 rc);
			goto free_power_settings;
		}

		/* Power up and probe sensor */
		rc = cam_sensor_power_up(s_ctrl);
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR, "power up failed");
			goto free_power_settings;
		}

		/* Match sensor ID */
		rc = cam_sensor_match_id(s_ctrl);
		if (rc < 0) {
			cam_sensor_power_down(s_ctrl);
			msleep(20);
			goto free_power_settings;
		}

		#ifdef VENDOR_EDIT
        /*add by chen.li@camera, 2019/09/04 for get sensor version*/
		cmd->reserved = s_ctrl->sensordata->slave_info.sensor_version;
		#endif

		CAM_INFO(CAM_SENSOR,
			"Probe success,slot:%d,slave_addr:0x%x,sensor_id:0x%x",
			s_ctrl->soc_info.index,
			s_ctrl->sensordata->slave_info.sensor_slave_addr,
			s_ctrl->sensordata->slave_info.sensor_id);

		rc = cam_sensor_power_down(s_ctrl);
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR, "fail in Sensor Power Down");
			goto free_power_settings;
		}
		/*
		 * Set probe succeeded flag to 1 so that no other camera shall
		 * probed on this slot
		 */
		s_ctrl->is_probe_succeed = 1;
		s_ctrl->sensor_state = CAM_SENSOR_INIT;
	}
		break;
	case CAM_ACQUIRE_DEV: {
		struct cam_sensor_acquire_dev sensor_acq_dev;
		struct cam_create_dev_hdl bridge_params;

		if ((s_ctrl->is_probe_succeed == 0) ||
			(s_ctrl->sensor_state != CAM_SENSOR_INIT)) {
			CAM_WARN(CAM_SENSOR,
				"Not in right state to aquire %d",
				s_ctrl->sensor_state);
			rc = -EINVAL;
			goto release_mutex;
		}

		if (s_ctrl->bridge_intf.device_hdl != -1) {
			CAM_ERR(CAM_SENSOR, "Device is already acquired");
			rc = -EINVAL;
			goto release_mutex;
		}
		rc = copy_from_user(&sensor_acq_dev,
			u64_to_user_ptr(cmd->handle),
			sizeof(sensor_acq_dev));
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR, "Failed Copying from user");
			goto release_mutex;
		}

		bridge_params.session_hdl = sensor_acq_dev.session_handle;
		bridge_params.ops = &s_ctrl->bridge_intf.ops;
		bridge_params.v4l2_sub_dev_flag = 0;
		bridge_params.media_entity_flag = 0;
		bridge_params.priv = s_ctrl;
		bridge_params.dev_id = CAM_SENSOR;
		sensor_acq_dev.device_handle =
			cam_create_device_hdl(&bridge_params);
		s_ctrl->bridge_intf.device_hdl = sensor_acq_dev.device_handle;
		s_ctrl->bridge_intf.session_hdl = sensor_acq_dev.session_handle;

		CAM_DBG(CAM_SENSOR, "Device Handle: %d",
			sensor_acq_dev.device_handle);
		if (copy_to_user(u64_to_user_ptr(cmd->handle),
			&sensor_acq_dev,
			sizeof(struct cam_sensor_acquire_dev))) {
			CAM_ERR(CAM_SENSOR, "Failed Copy to User");
			rc = -EFAULT;
			goto release_mutex;
		}

		rc = cam_sensor_power_up(s_ctrl);
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR, "Sensor Power up failed");
			goto release_mutex;
		}

		s_ctrl->sensor_state = CAM_SENSOR_ACQUIRE;
		s_ctrl->last_flush_req = 0;
		CAM_INFO(CAM_SENSOR,
			"CAM_ACQUIRE_DEV Success, sensor_id:0x%x,sensor_slave_addr:0x%x",
			s_ctrl->sensordata->slave_info.sensor_id,
			s_ctrl->sensordata->slave_info.sensor_slave_addr);
	}
		break;
	case CAM_RELEASE_DEV: {
		if ((s_ctrl->sensor_state == CAM_SENSOR_INIT) ||
			(s_ctrl->sensor_state == CAM_SENSOR_START)) {
			rc = -EINVAL;
			CAM_WARN(CAM_SENSOR,
			"Not in right state to release : %d",
			s_ctrl->sensor_state);
			goto release_mutex;
		}

		if (s_ctrl->bridge_intf.link_hdl != -1) {
			CAM_ERR(CAM_SENSOR,
				"Device [%d] still active on link 0x%x",
				s_ctrl->sensor_state,
				s_ctrl->bridge_intf.link_hdl);
			rc = -EAGAIN;
			goto release_mutex;
		}

		rc = cam_sensor_power_down(s_ctrl);
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR, "Sensor Power Down failed");
			goto release_mutex;
		}

		cam_sensor_release_per_frame_resource(s_ctrl);
		cam_sensor_release_stream_rsc(s_ctrl);
		if (s_ctrl->bridge_intf.device_hdl == -1) {
			CAM_ERR(CAM_SENSOR,
				"Invalid Handles: link hdl: %d device hdl: %d",
				s_ctrl->bridge_intf.device_hdl,
				s_ctrl->bridge_intf.link_hdl);
			rc = -EINVAL;
			goto release_mutex;
		}
		rc = cam_destroy_device_hdl(s_ctrl->bridge_intf.device_hdl);
		if (rc < 0)
			CAM_ERR(CAM_SENSOR,
				"failed in destroying the device hdl");
		s_ctrl->bridge_intf.device_hdl = -1;
		s_ctrl->bridge_intf.link_hdl = -1;
		s_ctrl->bridge_intf.session_hdl = -1;

		s_ctrl->sensor_state = CAM_SENSOR_INIT;
		CAM_INFO(CAM_SENSOR,
			"CAM_RELEASE_DEV Success, sensor_id:0x%x,sensor_slave_addr:0x%x",
			s_ctrl->sensordata->slave_info.sensor_id,
			s_ctrl->sensordata->slave_info.sensor_slave_addr);
		s_ctrl->streamon_count = 0;
		s_ctrl->streamoff_count = 0;
		s_ctrl->last_flush_req = 0;
	}
		break;
	case CAM_QUERY_CAP: {
		struct  cam_sensor_query_cap sensor_cap;

		cam_sensor_query_cap(s_ctrl, &sensor_cap);
		if (copy_to_user(u64_to_user_ptr(cmd->handle),
			&sensor_cap, sizeof(struct  cam_sensor_query_cap))) {
			CAM_ERR(CAM_SENSOR, "Failed Copy to User");
			rc = -EFAULT;
			goto release_mutex;
		}
		break;
	}
	case CAM_START_DEV: {
		if ((s_ctrl->sensor_state == CAM_SENSOR_INIT) ||
			(s_ctrl->sensor_state == CAM_SENSOR_START)) {
			rc = -EINVAL;
			CAM_WARN(CAM_SENSOR,
			"Not in right state to start : %d",
			s_ctrl->sensor_state);
			goto release_mutex;
		}

		if (s_ctrl->i2c_data.streamon_settings.is_settings_valid &&
			(s_ctrl->i2c_data.streamon_settings.request_id == 0)) {
			rc = cam_sensor_apply_settings(s_ctrl, 0,
				CAM_SENSOR_PACKET_OPCODE_SENSOR_STREAMON);
			if (rc < 0) {
				CAM_ERR(CAM_SENSOR,
					"cannot apply streamon settings");
				goto release_mutex;
			}
		}
		s_ctrl->sensor_state = CAM_SENSOR_START;
		CAM_INFO(CAM_SENSOR,
			"CAM_START_DEV Success, sensor_id:0x%x,sensor_slave_addr:0x%x",
			s_ctrl->sensordata->slave_info.sensor_id,
			s_ctrl->sensordata->slave_info.sensor_slave_addr);
	}
		break;
	case CAM_STOP_DEV: {
		if (s_ctrl->sensor_state != CAM_SENSOR_START) {
			rc = -EINVAL;
			CAM_WARN(CAM_SENSOR,
			"Not in right state to stop : %d",
			s_ctrl->sensor_state);
			goto release_mutex;
		}

		if (s_ctrl->i2c_data.streamoff_settings.is_settings_valid &&
			(s_ctrl->i2c_data.streamoff_settings.request_id == 0)) {
			rc = cam_sensor_apply_settings(s_ctrl, 0,
				CAM_SENSOR_PACKET_OPCODE_SENSOR_STREAMOFF);
			if (rc < 0) {
				CAM_ERR(CAM_SENSOR,
				"cannot apply streamoff settings");
			}
		}

		cam_sensor_release_per_frame_resource(s_ctrl);
		s_ctrl->last_flush_req = 0;
		s_ctrl->sensor_state = CAM_SENSOR_ACQUIRE;
		CAM_INFO(CAM_SENSOR,
			"CAM_STOP_DEV Success, sensor_id:0x%x,sensor_slave_addr:0x%x",
			s_ctrl->sensordata->slave_info.sensor_id,
			s_ctrl->sensordata->slave_info.sensor_slave_addr);
	}
		break;
	case CAM_CONFIG_DEV: {
		rc = cam_sensor_i2c_pkt_parse(s_ctrl, arg);
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR, "Failed i2c pkt parse: %d", rc);
			goto release_mutex;
		}
		if (s_ctrl->i2c_data.init_settings.is_settings_valid &&
			(s_ctrl->i2c_data.init_settings.request_id == 0)) {

			rc = cam_sensor_apply_settings(s_ctrl, 0,
				CAM_SENSOR_PACKET_OPCODE_SENSOR_INITIAL_CONFIG);

			s_ctrl->i2c_data.init_settings.request_id = -1;

			if (rc < 0) {
				CAM_ERR(CAM_SENSOR,
					"cannot apply init settings");
				delete_request(&s_ctrl->i2c_data.init_settings);
				goto release_mutex;
			}
			rc = delete_request(&s_ctrl->i2c_data.init_settings);
			if (rc < 0) {
				CAM_ERR(CAM_SENSOR,
					"Fail in deleting the Init settings");
				goto release_mutex;
			}
		}

		if (s_ctrl->i2c_data.config_settings.is_settings_valid &&
			(s_ctrl->i2c_data.config_settings.request_id == 0)) {
			rc = cam_sensor_apply_settings(s_ctrl, 0,
				CAM_SENSOR_PACKET_OPCODE_SENSOR_CONFIG);

			s_ctrl->i2c_data.config_settings.request_id = -1;

			if (rc < 0) {
				CAM_ERR(CAM_SENSOR,
					"cannot apply config settings");
				delete_request(
					&s_ctrl->i2c_data.config_settings);
				goto release_mutex;
			}
			rc = delete_request(&s_ctrl->i2c_data.config_settings);
			if (rc < 0) {
				CAM_ERR(CAM_SENSOR,
					"Fail in deleting the config settings");
				goto release_mutex;
			}
			s_ctrl->sensor_state = CAM_SENSOR_CONFIG;
		}
	}
		break;
	#ifdef VENDOR_EDIT
	/*add by hongbo.dai@camera 20190221, get DPC Data for IMX471*/
	case CAM_GET_DPC_DATA: {
		if (0x0471 != s_ctrl->sensordata->slave_info.sensor_id) {
			rc = -EFAULT;
			goto release_mutex;
		}
		CAM_INFO(CAM_SENSOR, "imx471_dfct_tbl: fd_dfct_num=%d, sg_dfct_num=%d",
			imx471_dfct_tbl.fd_dfct_num, imx471_dfct_tbl.sg_dfct_num);
		if (copy_to_user((void __user *) cmd->handle, &imx471_dfct_tbl,
			sizeof(struct  sony_dfct_tbl_t))) {
			CAM_ERR(CAM_SENSOR, "Failed Copy to User");
			rc = -EFAULT;
			goto release_mutex;
		}
	}
		break;
	#endif
	default:
		CAM_ERR(CAM_SENSOR, "Invalid Opcode: %d", cmd->op_code);
		rc = -EINVAL;
		goto release_mutex;
	}

release_mutex:
	mutex_unlock(&(s_ctrl->cam_sensor_mutex));
	return rc;

free_power_settings:
	kfree(power_info->power_setting);
	kfree(power_info->power_down_setting);
	power_info->power_setting = NULL;
	power_info->power_down_setting = NULL;
	power_info->power_down_setting_size = 0;
	power_info->power_setting_size = 0;
	mutex_unlock(&(s_ctrl->cam_sensor_mutex));
	return rc;
}

int cam_sensor_publish_dev_info(struct cam_req_mgr_device_info *info)
{
	int rc = 0;
	struct cam_sensor_ctrl_t *s_ctrl = NULL;

	if (!info)
		return -EINVAL;

	s_ctrl = (struct cam_sensor_ctrl_t *)
		cam_get_device_priv(info->dev_hdl);

	if (!s_ctrl) {
		CAM_ERR(CAM_SENSOR, "Device data is NULL");
		return -EINVAL;
	}

	info->dev_id = CAM_REQ_MGR_DEVICE_SENSOR;
	strlcpy(info->name, CAM_SENSOR_NAME, sizeof(info->name));
	if (s_ctrl->pipeline_delay >= 1 && s_ctrl->pipeline_delay <= 3)
		info->p_delay = s_ctrl->pipeline_delay;
	else
		info->p_delay = 2;
	info->trigger = CAM_TRIGGER_POINT_SOF;

	return rc;
}

int cam_sensor_establish_link(struct cam_req_mgr_core_dev_link_setup *link)
{
	struct cam_sensor_ctrl_t *s_ctrl = NULL;

	if (!link)
		return -EINVAL;

	s_ctrl = (struct cam_sensor_ctrl_t *)
		cam_get_device_priv(link->dev_hdl);
	if (!s_ctrl) {
		CAM_ERR(CAM_SENSOR, "Device data is NULL");
		return -EINVAL;
	}

	mutex_lock(&s_ctrl->cam_sensor_mutex);
	if (link->link_enable) {
		s_ctrl->bridge_intf.link_hdl = link->link_hdl;
		s_ctrl->bridge_intf.crm_cb = link->crm_cb;
	} else {
		s_ctrl->bridge_intf.link_hdl = -1;
		s_ctrl->bridge_intf.crm_cb = NULL;
	}
	mutex_unlock(&s_ctrl->cam_sensor_mutex);

	return 0;
}

int cam_sensor_power(struct v4l2_subdev *sd, int on)
{
	struct cam_sensor_ctrl_t *s_ctrl = v4l2_get_subdevdata(sd);

	mutex_lock(&(s_ctrl->cam_sensor_mutex));
	if (!on && s_ctrl->sensor_state == CAM_SENSOR_START) {
		cam_sensor_power_down(s_ctrl);
		s_ctrl->sensor_state = CAM_SENSOR_ACQUIRE;
	}
	mutex_unlock(&(s_ctrl->cam_sensor_mutex));

	return 0;
}

int cam_sensor_power_up(struct cam_sensor_ctrl_t *s_ctrl)
{
	int rc;
	struct cam_sensor_power_ctrl_t *power_info;
	struct cam_camera_slave_info *slave_info;
	struct cam_hw_soc_info *soc_info =
		&s_ctrl->soc_info;

	if (!s_ctrl) {
		CAM_ERR(CAM_SENSOR, "failed: %pK", s_ctrl);
		return -EINVAL;
	}

	power_info = &s_ctrl->sensordata->power_info;
	slave_info = &(s_ctrl->sensordata->slave_info);

	if (!power_info || !slave_info) {
		CAM_ERR(CAM_SENSOR, "failed: %pK %pK", power_info, slave_info);
		return -EINVAL;
	}

	if (s_ctrl->bob_pwm_switch) {
		rc = cam_sensor_bob_pwm_mode_switch(soc_info,
			s_ctrl->bob_reg_index, true);
		if (rc) {
			CAM_WARN(CAM_SENSOR,
			"BoB PWM setup failed rc: %d", rc);
			rc = 0;
		}
	}

	rc = cam_sensor_core_power_up(power_info, soc_info);
	if (rc < 0) {
		CAM_ERR(CAM_SENSOR, "power up the core is failed:%d", rc);
		return rc;
	}

	rc = camera_io_init(&(s_ctrl->io_master_info));
	if (rc < 0)
		CAM_ERR(CAM_SENSOR, "cci_init failed: rc: %d", rc);

	return rc;
}

int cam_sensor_power_down(struct cam_sensor_ctrl_t *s_ctrl)
{
	struct cam_sensor_power_ctrl_t *power_info;
	struct cam_hw_soc_info *soc_info;
	int rc = 0;

	if (!s_ctrl) {
		CAM_ERR(CAM_SENSOR, "failed: s_ctrl %pK", s_ctrl);
		return -EINVAL;
	}

	power_info = &s_ctrl->sensordata->power_info;
	soc_info = &s_ctrl->soc_info;

	if (!power_info) {
		CAM_ERR(CAM_SENSOR, "failed: power_info %pK", power_info);
		return -EINVAL;
	}
	rc = cam_sensor_util_power_down(power_info, soc_info);
	if (rc < 0) {
		CAM_ERR(CAM_SENSOR, "power down the core is failed:%d", rc);
		return rc;
	}

	if (s_ctrl->bob_pwm_switch) {
		rc = cam_sensor_bob_pwm_mode_switch(soc_info,
			s_ctrl->bob_reg_index, false);
		if (rc) {
			CAM_WARN(CAM_SENSOR,
				"BoB PWM setup failed rc: %d", rc);
			rc = 0;
		}
	}

	camera_io_release(&(s_ctrl->io_master_info));

	return rc;
}

int cam_sensor_apply_settings(struct cam_sensor_ctrl_t *s_ctrl,
	int64_t req_id, enum cam_sensor_packet_opcodes opcode)
{
	int rc = 0, offset, i;
	uint64_t top = 0, del_req_id = 0;
	struct i2c_settings_array *i2c_set = NULL;
	struct i2c_settings_list *i2c_list;

	if (req_id == 0) {
		switch (opcode) {
		case CAM_SENSOR_PACKET_OPCODE_SENSOR_STREAMON: {
			i2c_set = &s_ctrl->i2c_data.streamon_settings;
			break;
		}
		case CAM_SENSOR_PACKET_OPCODE_SENSOR_INITIAL_CONFIG: {
			i2c_set = &s_ctrl->i2c_data.init_settings;
			break;
		}
		case CAM_SENSOR_PACKET_OPCODE_SENSOR_CONFIG: {
			i2c_set = &s_ctrl->i2c_data.config_settings;
			break;
		}
		case CAM_SENSOR_PACKET_OPCODE_SENSOR_STREAMOFF: {
			i2c_set = &s_ctrl->i2c_data.streamoff_settings;
			break;
		}
		case CAM_SENSOR_PACKET_OPCODE_SENSOR_UPDATE:
		case CAM_SENSOR_PACKET_OPCODE_SENSOR_PROBE:
		default:
			return 0;
		}
		if (i2c_set->is_settings_valid == 1) {
			list_for_each_entry(i2c_list,
				&(i2c_set->list_head), list) {
#ifdef VENDOR_EDIT
				/* wanghaoran@camera.driver. 2018/11/2, add for read sensor gc02m0 of camera */
				if (s_ctrl->sensordata->slave_info.sensor_id == 0x02d0) {
					i2c_list->i2c_settings.addr_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
					CAM_ERR(CAM_SENSOR,
						"i2c_list->i2c_settings.addr_type: %d",
						i2c_list->i2c_settings.addr_type);
				}
#endif
				rc = cam_sensor_i2c_modes_util(
					&(s_ctrl->io_master_info),
					i2c_list);
				if (rc < 0) {
					CAM_ERR(CAM_SENSOR,
						"Failed to apply settings: %d",
						rc);
					return rc;
				}
			}
		}
	} else {
		offset = req_id % MAX_PER_FRAME_ARRAY;
		i2c_set = &(s_ctrl->i2c_data.per_frame[offset]);
		if (i2c_set->is_settings_valid == 1 &&
			i2c_set->request_id == req_id) {
			list_for_each_entry(i2c_list,
				&(i2c_set->list_head), list) {
#ifdef VENDOR_EDIT
				/* wanghaoran@camera.driver. 2018/11/2, add for read sensor gc02m0 of camera */
				if (s_ctrl->sensordata->slave_info.sensor_id == 0x02d0) {
				   i2c_list->i2c_settings.addr_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
				   CAM_DBG(CAM_SENSOR,
						  "i2c_list->i2c_settings.addr_type: %d",
						   i2c_list->i2c_settings.addr_type);
			        }
#endif
				rc = cam_sensor_i2c_modes_util(
					&(s_ctrl->io_master_info),
					i2c_list);
				if (rc < 0) {
					CAM_ERR(CAM_SENSOR,
						"Failed to apply settings: %d",
						rc);
					return rc;
				}
			}
		} else {
			CAM_DBG(CAM_SENSOR,
				"Invalid/NOP request to apply: %lld", req_id);
		}

		/* Change the logic dynamically */
		for (i = 0; i < MAX_PER_FRAME_ARRAY; i++) {
			if ((req_id >=
				s_ctrl->i2c_data.per_frame[i].request_id) &&
				(top <
				s_ctrl->i2c_data.per_frame[i].request_id) &&
				(s_ctrl->i2c_data.per_frame[i].
				is_settings_valid == 1)) {
				del_req_id = top;
				top = s_ctrl->i2c_data.per_frame[i].request_id;
			}
		}

		if (top < req_id) {
			if ((((top % MAX_PER_FRAME_ARRAY) - (req_id %
				MAX_PER_FRAME_ARRAY)) >= BATCH_SIZE_MAX) ||
				(((top % MAX_PER_FRAME_ARRAY) - (req_id %
				MAX_PER_FRAME_ARRAY)) <= -BATCH_SIZE_MAX))
				del_req_id = req_id;
		}

		if (!del_req_id)
			return rc;

		CAM_DBG(CAM_SENSOR, "top: %llu, del_req_id:%llu",
			top, del_req_id);

		for (i = 0; i < MAX_PER_FRAME_ARRAY; i++) {
			if ((del_req_id >
				 s_ctrl->i2c_data.per_frame[i].request_id) && (
				 s_ctrl->i2c_data.per_frame[i].is_settings_valid
					== 1)) {
				s_ctrl->i2c_data.per_frame[i].request_id = 0;
				rc = delete_request(
					&(s_ctrl->i2c_data.per_frame[i]));
				if (rc < 0)
					CAM_ERR(CAM_SENSOR,
						"Delete request Fail:%lld rc:%d",
						del_req_id, rc);
			}
		}
	}

	return rc;
}

int32_t cam_sensor_apply_request(struct cam_req_mgr_apply_request *apply)
{
	int32_t rc = 0;
	struct cam_sensor_ctrl_t *s_ctrl = NULL;

	if (!apply)
		return -EINVAL;

	s_ctrl = (struct cam_sensor_ctrl_t *)
		cam_get_device_priv(apply->dev_hdl);
	if (!s_ctrl) {
		CAM_ERR(CAM_SENSOR, "Device data is NULL");
		return -EINVAL;
	}
	CAM_DBG(CAM_REQ, " Sensor update req id: %lld", apply->request_id);
	trace_cam_apply_req("Sensor", apply->request_id);
	mutex_lock(&(s_ctrl->cam_sensor_mutex));
	rc = cam_sensor_apply_settings(s_ctrl, apply->request_id,
		CAM_SENSOR_PACKET_OPCODE_SENSOR_UPDATE);
	mutex_unlock(&(s_ctrl->cam_sensor_mutex));
	return rc;
}

int32_t cam_sensor_flush_request(struct cam_req_mgr_flush_request *flush_req)
{
	int32_t rc = 0, i;
	uint32_t cancel_req_id_found = 0;
	struct cam_sensor_ctrl_t *s_ctrl = NULL;
	struct i2c_settings_array *i2c_set = NULL;

	if (!flush_req)
		return -EINVAL;

	s_ctrl = (struct cam_sensor_ctrl_t *)
		cam_get_device_priv(flush_req->dev_hdl);
	if (!s_ctrl) {
		CAM_ERR(CAM_SENSOR, "Device data is NULL");
		return -EINVAL;
	}

	mutex_lock(&(s_ctrl->cam_sensor_mutex));
	if (s_ctrl->sensor_state != CAM_SENSOR_START ||
		s_ctrl->sensor_state != CAM_SENSOR_CONFIG) {
		mutex_unlock(&(s_ctrl->cam_sensor_mutex));
		return rc;
	}

	if (s_ctrl->i2c_data.per_frame == NULL) {
		CAM_ERR(CAM_SENSOR, "i2c frame data is NULL");
		mutex_unlock(&(s_ctrl->cam_sensor_mutex));
		return -EINVAL;
	}

	if (flush_req->type == CAM_REQ_MGR_FLUSH_TYPE_ALL) {
		s_ctrl->last_flush_req = flush_req->req_id;
		CAM_DBG(CAM_SENSOR, "last reqest to flush is %lld",
			flush_req->req_id);
	}

	for (i = 0; i < MAX_PER_FRAME_ARRAY; i++) {
		i2c_set = &(s_ctrl->i2c_data.per_frame[i]);

		if ((flush_req->type == CAM_REQ_MGR_FLUSH_TYPE_CANCEL_REQ)
				&& (i2c_set->request_id != flush_req->req_id))
			continue;

		if (i2c_set->is_settings_valid == 1) {
			rc = delete_request(i2c_set);
			if (rc < 0)
				CAM_ERR(CAM_SENSOR,
					"delete request: %lld rc: %d",
					i2c_set->request_id, rc);

			if (flush_req->type ==
				CAM_REQ_MGR_FLUSH_TYPE_CANCEL_REQ) {
				cancel_req_id_found = 1;
				break;
			}
		}
	}

	if (flush_req->type == CAM_REQ_MGR_FLUSH_TYPE_CANCEL_REQ &&
		!cancel_req_id_found)
		CAM_DBG(CAM_SENSOR,
			"Flush request id:%lld not found in the pending list",
			flush_req->req_id);
	mutex_unlock(&(s_ctrl->cam_sensor_mutex));
	return rc;
}
