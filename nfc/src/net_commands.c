#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "debug.h"

#include "net_commands.h"
#include "pole.h"
#include "arm.h"
#include "parson.h"
#include "json_wp.h"
#include "settings.h"

#define PROTOCOL_VERSION  	"JSON_1.0"

bool stall_detection = true;

typedef struct {
	char *cmd_name;
	JSON_Value* (*cmd_function)(JSON_Value const *pars);
} cmd_entry;

JSON_Value* telemetria_cmd(JSON_Value const *pars)
{
	JSON_Value *ans = json_value_init_object();
	json_object_set_value(json_value_get_object(ans), "ARM", arm_json());
	json_object_set_value(json_value_get_object(ans), "POLE", pole_json());
	return ans;
}

JSON_Value* logs_cmd(JSON_Value const *pars)
{
	if (pars && json_value_get_type(pars) == JSONObject) {
		double quantity = json_object_get_number(json_value_get_object(pars),
				"quantity");

		JSON_Value *ans = json_value_init_object();
		JSON_Value *msg_array = json_value_init_array();

		int msgs_waiting = uxQueueMessagesWaiting(debug_queue);

		int extract = MIN(quantity, msgs_waiting);

		for (int x = 0; x < extract; x++) {
			char *dbg_msg = NULL;
			if (xQueueReceive(debug_queue, &dbg_msg, (TickType_t) 0) == pdPASS) {
				json_array_append_string(json_value_get_array(msg_array), dbg_msg);
				vPortFree(dbg_msg);
			}
		}

		json_object_set_value(json_value_get_object(ans), "DEBUG_MSGS",
				msg_array);

		return ans;

	}
	return NULL;
}

JSON_Value* lift_cmd(JSON_Value const *pars)
{
	lDebug(Info, "LLAMADA lift_up_cmd");
	JSON_Value *ans = json_value_init_object();
	return ans;
}

JSON_Value* protocol_version_cmd(JSON_Value const *pars)
{
	JSON_Value *ans = json_value_init_object();
	json_object_set_string(json_value_get_object(ans), "Version",
	PROTOCOL_VERSION);
	return ans;
}

JSON_Value* control_enable_cmd(JSON_Value const *pars)
{
	static bool enabled = false;
	enabled = !enabled;

	JSON_Value *ans = json_value_init_object();
	json_object_set_boolean(json_value_get_object(ans), "ACK", enabled);
	return ans;
}

JSON_Value* pole_closed_loop_cmd(JSON_Value const *pars)
{
	lDebug(Info, "LLAMADA pole_closed_loop_cmd");

	JSON_Value *ans = json_value_init_object();
	json_object_set_boolean(json_value_get_object(ans), "ACK", true);
	return ans;
}

JSON_Value* arm_free_run_cmd(JSON_Value const *pars)
{
	if (pars && json_value_get_type(pars) == JSONObject) {
		char const *dir = json_object_get_string(json_value_get_object(pars),
				"dir");
		double speed = json_object_get_number(json_value_get_object(pars),
				"speed");

		if (dir && speed != 0) {
			lDebug(Info, "ARM_FREE_RUN DIR: %s, SPEED: %d", dir, (int ) speed);
		}
		JSON_Value *ans = json_value_init_object();
		json_object_set_boolean(json_value_get_object(ans), "ACK", true);
		return ans;
	}
	return NULL;
}

JSON_Value* network_settings_cmd(JSON_Value const *pars)
{
	if (pars && json_value_get_type(pars) == JSONObject) {
		char const *gw = json_object_get_string(json_value_get_object(pars),
				"gw");
		char const *ipaddr = json_object_get_string(json_value_get_object(pars),
				"ipaddr");
		char const *netmask = json_object_get_string(
				json_value_get_object(pars), "netmask");
		uint16_t port = (uint16_t) json_object_get_number(
				json_value_get_object(pars), "port");

		if (gw && ipaddr && netmask && port != 0) {
			lDebug(Info,
					"Received network settings: gw:%s, ipaddr:%s, netmask:%s, port:%d",
					gw, ipaddr, netmask, port);

			struct settings settings;

			unsigned char *gw_bytes = (unsigned char*) &(settings.gw.addr);
			if (sscanf(gw, "%hu.%hu.%hu.%hu", &gw_bytes[0], &gw_bytes[1],
					&gw_bytes[2], &gw_bytes[3]) == 4) {
			}

			unsigned char *ipaddr_bytes =
					(unsigned char*) &(settings.ipaddr.addr);
			if (sscanf(ipaddr, "%hu.%hu.%hu.%hu", &ipaddr_bytes[0],
					&ipaddr_bytes[1], &ipaddr_bytes[2], &ipaddr_bytes[3])
					== 4) {
			}

			unsigned char *netmask_bytes =
					(unsigned char*) &(settings.netmask.addr);
			if (sscanf(netmask, "%hu.%hu.%hu.%hu", &netmask_bytes[0],
					&netmask_bytes[1], &netmask_bytes[2], &netmask_bytes[3])
					== 4) {
			}

			settings.port = port;

			settings_save(settings);
			lDebug(Info, "Settings saved. Restarting...");

			Chip_UART_SendBlocking(DEBUG_UART, "\n\n", 2);

			Chip_RGU_TriggerReset(RGU_CORE_RST);
		}

		JSON_Value *ans = json_value_init_object();
		json_object_set_boolean(json_value_get_object(ans), "ACK", true);
		return ans;
	}
	return NULL;
}

// @formatter:off
const cmd_entry cmds_table[] = {
		{
				"PROTOCOL_VERSION",		/* Command name */
				protocol_version_cmd,	/* Associated function */
		},
		{
				"CONTROL_ENABLE",
				control_enable_cmd,
		},
		{
				"LIFT_UP",
				lift_cmd,
		},
		{
				"POLE_CLOSED_LOOP",
				pole_closed_loop_cmd,
		},
		{
				"ARM_FREE_RUN",
				arm_free_run_cmd,
		},
		{
				"TELEMETRIA",
				telemetria_cmd,
		},
		{
				"LOGS",
				logs_cmd,
		},
		{
				"NETWORK_SETTINGS",
				network_settings_cmd,
		},

};
// @formatter:on
/**
 * @brief 	searchs for a matching command name in cmds_table[], passing the parameters
 * 			as a JSON object for the called function to parse them.
 * @param 	*cmd 	:name of the command to execute
 * @param   *pars   :JSON object containing the passed parameters to the called function
 */
JSON_Value* cmd_execute(char const *cmd, JSON_Value const *pars)
{
	bool cmd_found = false;
	for (int i = 0; i < (sizeof(cmds_table) / sizeof(cmds_table[0])); i++) {
		if (!strcmp(cmd, cmds_table[i].cmd_name)) {
			return cmds_table[i].cmd_function(pars);
		}
	}
	if (!cmd_found) {
		lDebug(Info, "No matching command found");
	}
	return NULL;
}