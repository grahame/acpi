
/* provides a simple client program that reads ACPI status from the /proc 
 * filesystem
 *
 * Copyright (C) 2001  Grahame Bowland <grahame@angrygoats.net>
 * 	     (C) 2008  Michael Meskes  <meskes@debian.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <errno.h>

#include "list.h"
#include "acpi.h"

#define DEVICE_LEN	  20
#define BATTERY_DESC	"Battery"
#define AC_ADAPTER_DESC "AC Adapter"
#define THERMAL_DESC	"Thermal"

#define MIN_PRESENT_RATE 0.01
#define MIN_CAPACITY	 0.01

static int ignore_directory_entry(struct dirent *de)
{
	return !strcmp(de->d_name, ".") || \
		!strcmp(de->d_name, "..");
}

static struct field *parse_field(char *buf, char *given_attr)
{
	struct field *rval;
	char *p;
	char *attr;
	char *value;
	int has_attr = 0, len;

	attr = calloc(BUF_SIZE, sizeof(char));
	value = calloc(BUF_SIZE, sizeof(char));
	rval = malloc(sizeof(struct field));
	len = strlen(buf);
	buf[len - 1] = '\0';
	if (!rval || !attr || !value) {
		fprintf(stderr, "Out of memory. Could not allocate memory in parse_field.\n");
		exit(1);
	}

	p = buf;
	if (!given_attr) {
		while (*(p++)) {
			if (*p == ':') {
				strncpy(attr, buf, p - buf);
				has_attr = 1;
				break;
			}
		}
		if (!has_attr) {
			free(attr); free(value); free(rval);
			return NULL;
		}
		if (*p == ' ') p++;
		while (*(p++)) {
			if (*p != ' ') { break; }
		}
	} else {
		strncpy(attr, given_attr, BUF_SIZE);
	}
	strncpy(value, p, BUF_SIZE);
	rval->attr = attr;
	rval->value = value;
	return rval;
}

static struct list *parse_info_file(struct list *l, char *filename, char *given_attr)
{
	FILE *fd;
	char buf[BUF_SIZE];

	fd = fopen(filename, "r");
	if (!fd) {
		return l;
	}
	while (fgets(buf, BUF_SIZE, fd)) {
		struct field *f;
		f = parse_field(buf, given_attr);
		if (!f) { continue; }
		l = list_append(l, f);
	}
	fclose(fd);
	return l;
}

static struct list *get_sys_info(char *device_name, char *acpi_path, char *device_type)
{
	struct list *rval = NULL;

	if (!strncmp(device_name, "cooling_device", strlen("cooling_device")))
		return NULL;
	if (chdir(device_name) < 0)
		return NULL;

	rval = parse_info_file(rval, "current_now", "current_now");
	rval = parse_info_file(rval, "charge_now", "charge_now");
	rval = parse_info_file(rval, "energy_now", "charge_now");
	rval = parse_info_file(rval, "charge_full", "charge_full");
	rval = parse_info_file(rval, "energy_full", "charge_full");
	rval = parse_info_file(rval, "charge_full_design", "charge_full_design");
	rval = parse_info_file(rval, "energy_full_design", "charge_full_design");
	rval = parse_info_file(rval, "online", "online");
	rval = parse_info_file(rval, "status", "charging state");
	rval = parse_info_file(rval, "type", "type");
	rval = parse_info_file(rval, "trip_point_0_type", "sys_trip_type");
	rval = parse_info_file(rval, "trip_point_0_temp", "sys_trip_temp");
	rval = parse_info_file(rval, "temp", "sys_temp");

	/* we cannot do chdir("..") here because some dirs are just symlinks */
	if (chdir(acpi_path) < 0)
		return NULL;
	if (chdir(device_type) < 0) 
		return NULL;
	return rval;
}

static struct list *get_proc_info(char *device_name)
{
	struct list *rval = NULL;

	if (chdir(device_name) < 0) {
		return NULL;
	}
	rval = parse_info_file(rval, "state", NULL);
	rval = parse_info_file(rval, "status", NULL);
	rval = parse_info_file(rval, "info", NULL);
	rval = parse_info_file(rval, "temperature", NULL);
	rval = parse_info_file(rval, "cooling_mode", NULL);
	chdir ("..");
	return rval;
}

void free_devices(struct list *devices)
{
	struct list *p, *r, *s;
	struct field *f;

	p = devices;
	while (p) {
		r = s = p->data;
		while (r) {
			f = r->data;
			free(f->attr);
			free(f->value);
			free(f);
			r = r->next;
		}
		list_free(s);
		p = p->next;
	}
	list_free(devices);
}

struct list *find_devices(char *acpi_path, char *device_type, int showerr, int proc_interface)
{
	DIR *d;
	struct dirent *de;
	struct list *device_info;
	struct list *rval = NULL;

	if (chdir(acpi_path) < 0) {
		if (showerr) {
			fprintf(stderr, "No ACPI support in kernel, or incorrect acpi_path (\"%s\").\n", acpi_path);
		}
		exit(1);
	}
	if (chdir(device_type) < 0) {
		if (showerr) {
			fprintf(stderr, "No support for device type: %s\n", device_type);
		}
		return NULL;
	}
	d = opendir(".");
	if (!d) {
		return NULL;
	}
	while ((de = readdir(d))) {
		if (ignore_directory_entry(de)) {
			continue;
		}

		if (proc_interface)
			device_info = get_proc_info(de->d_name);
		else 
			device_info = get_sys_info(de->d_name, acpi_path, device_type);

		if (device_info)
			rval = list_append(rval, device_info);
	}
	closedir(d);
	return rval;
}

static unsigned int get_unit_value(char *value)
{
	int n = -1;
	sscanf(value, "%d", &n);
	return n;
}

void print_ac_adapter_information(struct list *ac_adapters, int show_empty_slots)
{
	struct list *adapter = ac_adapters;
	struct list *fields;
	struct field *value;
	int adapter_num = 1;

	while (adapter) {
		char *state = NULL;
		int type_ac = 0;

		fields = adapter->data;
		while (fields) {
			value = fields->data;
			if (!strcmp(value->attr, "state") || !strcmp(value->attr, "Status")) {
				state = value->value;
			}
			else if (!strcmp(value->attr, "online")) {
				state = get_unit_value(value->value) ? "on-line" : "off-line";
			} else if (!strcmp(value->attr, "type")) {
				type_ac = strcasecmp(value->value, "mains");
			}

			fields = list_next(fields);
		}
		if (type_ac == 0) { /* or else this is a battery */
			if (!state) {
				if (show_empty_slots) {
					printf("%12s %d: slot empty\n", AC_ADAPTER_DESC, adapter_num);
				}
			} else {
				printf("%12s %d: %s\n", AC_ADAPTER_DESC, adapter_num, state);
			}
			adapter_num++;
		}
		adapter = list_next(adapter);
	}
}

void print_thermal_information(struct list *thermal, int show_empty_slots, int temp_units)
{
	struct list *sensor = thermal;
	struct list *fields;
	struct field *value;
	int sensor_num = 1;

	while (sensor) {
		float temperature = -1, trip_temp = -1;
		char *state = NULL, *scale;
		double real_temp;
		
		fields = sensor->data;
		while (fields) {
			value = fields->data;
			if (!strcmp(value->attr, "state") ||
			    !strcmp(value->attr, "sys_trip_type")) {
				state = value->value;
			} else if (!strcmp(value->attr, "temperature")) {
				temperature = get_unit_value(value->value);
				if (strstr(value->value, "dK")) {
					temperature = (temperature / 10) - ABSOLUTE_ZERO;
				}
			} else if (!strcmp(value->attr, "sys_temp")) {
				temperature = get_unit_value(value->value) / 1000.0;
			} else if (!strcmp(value->attr, "sys_trip_temp")) {
				trip_temp = get_unit_value(value->value) / 1000.0;
			}
			fields = list_next(fields);
		}
		if (temperature < trip_temp) 
			state = "ok";
		if (temperature < 0 || !state) {
			if (show_empty_slots) {
				printf("%12s %d: slot empty\n", THERMAL_DESC, sensor_num - 1);
			}
		} else {
			real_temp = (double)temperature;
			switch (temp_units) {
				case TEMP_CELSIUS:
					scale = "degrees C";
					break;
				case TEMP_FAHRENHEIT:
					real_temp = (real_temp * 1.8) + 32;
					scale = "degrees F";
					break;
				case TEMP_KELVIN:
				default:
					real_temp += ABSOLUTE_ZERO;
					scale = "kelvin";
					break;
			}
			printf("%12s %d: %s, %.1f %s\n", THERMAL_DESC, sensor_num - 1, state, real_temp, scale);
		}
		sensor_num++;
		sensor = list_next(sensor);
	} 
}

void print_battery_information(struct list *batteries, int show_empty_slots)
{
	struct list *battery = batteries;
	struct list *fields;
	struct field *value;
	int battery_num = 1;

	while (battery) {
		int remaining_capacity = -1;
		int present_rate = -1;
		int design_capacity = -1;
		int real_capacity = -1;
		int hours, minutes, seconds;
		int percentage;
		char *state = NULL, *poststr;
		int type_battery = 0;

		fields = battery->data;
		while (fields) {
			value = fields->data;
			if (!strcasecmp(value->attr, "remaining capacity")) {
				remaining_capacity = get_unit_value(value->value);
			} else if (!strcmp(value->attr, "charge_now")) {
				remaining_capacity = get_unit_value(value->value)/1000;
			} else if (!strcasecmp(value->attr, "present_rate")) {
				present_rate = get_unit_value(value->value);
			} else if (!strcmp(value->attr, "current_now")) {
				present_rate = abs(get_unit_value(value->value))/1000;
			} else if (!strcasecmp(value->attr, "last full capacity")) {
				design_capacity = get_unit_value(value->value);
			} else if (!strcmp(value->attr, "charge_full")) {
				design_capacity = get_unit_value(value->value)/1000;
			} else if (!strcmp(value->attr, "charge_full_design")) {
				real_capacity = get_unit_value(value->value)/1000;
			} else if (!strcmp(value->attr, "type")) {
				type_battery = strcasecmp(value->value, "battery");
			} else if (!strcmp(value->attr, "charging state") ||
				   !strcmp(value->attr, "State")) {
				state = value->value;
			}
			fields = list_next(fields);
		}
		if (type_battery == 0) { /* or else this is the ac_adapter */
			if (remaining_capacity < 0 || design_capacity < 0 || !state) {
				if (show_empty_slots) {
					printf("%12s %d: slot empty\n", BATTERY_DESC, battery_num);
				}
			} else {
				if (design_capacity < MIN_CAPACITY) {
					percentage = 0;
				} else {
					percentage = remaining_capacity * 100 / design_capacity;
				}
				if (percentage > 100)
					percentage = 100;
				printf("%12s %d: %s, %d%%", BATTERY_DESC, battery_num, state, percentage);
				if (present_rate == -1) {
					poststr = "rate information unavailable.";
					seconds = -1;
				} else if (!strcasecmp(state, "charging")) {
					if (present_rate > MIN_PRESENT_RATE) {
						seconds = 60 * (design_capacity - remaining_capacity) / present_rate;
						poststr = " until charged";
					} else {
						poststr = "charging at zero rate - will never fully charge.";
						seconds = -1;
					}
				} else if (!strcasecmp(state, "discharging")) {
					if (present_rate > MIN_PRESENT_RATE) {
						seconds = 60 * remaining_capacity / present_rate;
						poststr = " remaining";
					} else {
						poststr = "discharging at zero rate - will never fully discharge.";
						seconds = -1;
					}
				} else {
					poststr = NULL;
					seconds = -1;
				}

				if (seconds > 0) {
					hours = seconds / 3600;
					seconds -= 3600 * hours;
					minutes = seconds / 60;
					seconds -= 60 * minutes;
					printf(", %02d:%02d:%02d%s", hours, minutes, seconds, poststr);
				} else if ((seconds < 0) && (poststr != NULL)) {
					printf(", %s", poststr);
				}
				printf("\n");
			}
			battery_num++;
		}
		battery = list_next(battery);
	}
}



