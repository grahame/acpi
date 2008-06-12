
/* provides a simple client program that reads ACPI status from the /proc 
 * filesystem
 *
 * Copyright (C) 2001  Grahame Bowland <grahame@angrygoats.net>
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

static struct field *parse_field(char *buf)
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
	p++;
	while (*(p++)) {
		if (*p != ' ') { break; }
	}
	strncpy(value, p, BUF_SIZE);
	rval->attr = attr;
	rval->value = value;
	return rval;
}

static struct list *parse_info_file(struct list *l, char *filename)
{
	FILE *fd;
	char buf[BUF_SIZE];

	fd = fopen(filename, "r");
	if (!fd) {
		return l;
	}
	while (fgets(buf, BUF_SIZE, fd)) {
		struct field *f;
		f = parse_field(buf);
		if (!f) { continue; }
		l = list_append(l, f);
	}
	fclose(fd);
	return l;
}

static struct list *get_device_info(char *device_name)
{
	struct list *rval = NULL;

	if (chdir(device_name) < 0) {
		return NULL;
	}
	rval = parse_info_file(rval, "state");
	rval = parse_info_file(rval, "status");
	rval = parse_info_file(rval, "info");
	rval = parse_info_file(rval, "temperature");
	rval = parse_info_file(rval, "cooling_mode");
	chdir("..");
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

struct list *find_devices(char *acpi_path, char *device_type, int showerr)
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
		device_info = get_device_info(de->d_name);
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

		fields = adapter->data;
		while (fields) {
			value = fields->data;
			if (!strcmp(value->attr, "state") || !strcmp(value->attr, "Status")) {
				state = value->value;
			}
			fields = list_next(fields);
		}
		if (!state) {
			if (show_empty_slots) {
				printf("%12s %d: slot empty\n", AC_ADAPTER_DESC, adapter_num);
			}
		} else {
			printf("%12s %d: %s\n", AC_ADAPTER_DESC, adapter_num, state);
		}
		adapter_num++;
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
		float temperature = -1;
		char *state = NULL, *scale;
		double real_temp;
		
		fields = sensor->data;
		while (fields) {
			value = fields->data;
			if (!strcmp(value->attr, "state")) {
				state = value->value;
			} else if (!strcmp(value->attr, "temperature")) {
				temperature = get_unit_value(value->value);
		if (strstr(value->value, "dK")) {
			temperature = (temperature / 10) - ABSOLUTE_ZERO;
		}
			}
			fields = list_next(fields);
		}
		if (temperature < 0 || !state) {
			if (show_empty_slots) {
				printf("%12s %d: slot empty\n", THERMAL_DESC, sensor_num);
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
			printf("%12s %d: %s, %.1f %s\n", THERMAL_DESC, sensor_num, state, real_temp, scale);
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
		int hours, minutes, seconds;
		int found_fields = 0;
		double pct = 0;
		int percentage;
		char *state = NULL, *poststr;

		fields = battery->data;
		while (fields) {
			value = fields->data;
			if (!strcasecmp(value->attr, "remaining capacity")) {
				remaining_capacity = get_unit_value(value->value);
				found_fields++;
			} else if (!strcasecmp(value->attr, "present rate")) {
				present_rate = get_unit_value(value->value);
				found_fields++;
			} else if (!strcasecmp(value->attr, "last full capacity")) {
				design_capacity = get_unit_value(value->value);
				found_fields++;
			} else if (!strcmp(value->attr, "charging state") ||
				   !strcmp(value->attr, "State")) {
				state = value->value;
				found_fields++;
			}
			/* have we found every field we need? */
			if (found_fields >= 4) {
				break;
			}
			fields = list_next(fields);
		}
		if (remaining_capacity < 0 || design_capacity < 0 || !state) {
			if (show_empty_slots) {
				printf("%12s %d: slot empty\n", BATTERY_DESC, battery_num);
			}
		} else {
			if (design_capacity < MIN_CAPACITY) {
				pct = 0;
			} else {
				pct = (double)remaining_capacity / design_capacity;
			}
			percentage = pct * 100;
			if (percentage > 100)
				percentage = 100;
			printf("%12s %d: %s, %d%%", BATTERY_DESC, battery_num, state, percentage);
			if (present_rate == -1) {
				poststr = "rate information unavailable.";
				seconds = -1;
			} else if (!strcmp(state, "charging")) {
				if (present_rate > MIN_PRESENT_RATE) {
					seconds = 3600 * (double)(design_capacity - remaining_capacity) / present_rate;
					poststr = " until charged";
				} else {
					poststr = "charging at zero rate - will never fully charge.";
					seconds = -1;
				}
			} else if (!strcmp(state, "discharging")) {
				seconds = 3600 * (float)remaining_capacity / present_rate;
				poststr = " remaining";
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
		battery = list_next(battery);
		battery_num++;
	}
}



