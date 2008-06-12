
/* provides a simple client program that reads ACPI status from the /proc 
 * filesystem
 *
 * Copyright (C) 2001  Grahame Bowland <grahame@angrygoats.net>
 * Copyright (C) 2008  Michael Meskes <meskes@debian.org>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include "acpi.h"

static void do_show_batteries(char *acpi_path, int show_empty_slots)/*{{{*/
{
	struct list *batteries;

	batteries = find_devices(acpi_path, "battery", TRUE);
	print_battery_information(batteries, show_empty_slots);
	free_devices(batteries);
}

static void do_show_thermal(char *acpi_path, int show_empty_slots, int temperature_units) {/*{{{*/
	struct list *thermal;
	thermal = find_devices(acpi_path, "thermal_zone", FALSE);
	if (!thermal) {
		/* old acpi directory structure */
		thermal = find_devices(acpi_path, "thermal", TRUE); 
	}
	print_thermal_information(thermal, show_empty_slots, temperature_units);
	free_devices(thermal);
}

static void do_show_ac_adapter(char *acpi_path, int show_empty_slots)/*{{{*/
{
	struct list *ac_adapter;
	ac_adapter = find_devices(acpi_path, "ac_adapter", TRUE);
	print_ac_adapter_information(ac_adapter, show_empty_slots);
	free_devices(ac_adapter);
}

static int version(void)/*{{{*/
{
	printf(ACPI_VERSION_STRING "\n"
"\n"
"Copyright (C) 2001 Grahame Bowland.\n"
"This is free software; see the source for copying conditions.  There is NO\n"
"warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n"
);
	return 1;
}

static int usage(char *argv[])/*{{{*/
{
	printf(
"Usage: acpi [OPTION]...\n"
"Shows information from the /proc filesystem, such as battery status or\n"
"thermal information.\n"
"\n"
"  -b, --battery				battery information\n"
"  -B, --without-battery		suppress battery information\n"
"  -t, --thermal				thermal information\n"
"  -T, --without-thermal		suppress thermal information\n"
"  -a, --ac-adapter			 ac adapter information\n"
"  -A, --without-ac-adapter	 suppress ac-adapter information\n"
"  -V, --everything			 show every device, overrides above options\n"
"  -s, --show-empty			 show non-operational devices\n"
"  -S, --hide-empty			 hide non-operational devices\n"
"  -c, --celsius				use celsius as the temperature unit\n"
"  -f, --fahrenheit			 use fahrenheit as the temperature unit\n"
"  -k, --kelvin				 use kelvin as the temperature unit\n"
"  -d, --directory <dir>		path to ACPI info (/proc/acpi)\n"
"  -h, --help				   display this help and exit\n"
"  -v, --version				output version information and exit\n"
"\n"
"By default, acpi displays information on installed system batteries.\n"
"Non-operational devices, for example empty battery slots are hidden.\n"
"The default unit of temperature is degrees celsius.\n"
"\n"
"Report bugs to Grahame Bowland <grahame@angrygoats.net>.\n"
);
	return 1;
}

static struct option long_options[] = {/*{{{*/
	{ "help", 0, 0, 'h' }, 
	{ "version", 0, 0, 'v' }, 
	{ "verbose", 0, 0, 'V' }, 
	{ "battery", 0, 0, 'b' }, 
	{ "without-battery", 0, 0, 'B' }, 
	{ "thermal", 0, 0, 't' }, 
	{ "without-thermal", 0, 0, 'T' }, 
	{ "ac-adapter", 0, 0, 'a' }, 
	{ "without-ac-adapter", 0, 0, 'A' }, 
	{ "show-empty", 0, 0, 's' }, 
	{ "hide-empty", 0, 0, 'S' }, 
	{ "celcius", 0, 0, 'c' }, 
	{ "celsius", 0, 0, 'c' }, 
	{ "fahrenheit", 0, 0, 'f' }, 
	{ "kelvin", 0, 0, 'k' }, 
	{ "directory", 1, 0, 'd' },
	{ "everything", 0, 0, 'V' }, 
	{ 0, 0, 0, 0 }, 
};

int main(int argc, char *argv[])/*{{{*/
{
	int show_everything = 0;
	int show_batteries = 1;
	int show_thermal = 0;
	int show_ac_adapter = 0;
	int show_empty_slots = 0;
	int temperature_units = TEMP_CELSIUS;
	int ch, option_index;
	char *acpi_path = strdup(ACPI_PATH);

	if (!acpi_path) {
		fprintf(stderr, "Out of memory in main()\n");
		return -1;
	}

	while ((ch = getopt_long(argc, argv, "VbBtTaAsShvfkcd:", long_options, &option_index)) != -1) {
		switch (ch) {
			case 'V':
				show_everything = 1;
				break;
			case 'b':
				show_batteries = 1;
				break;
			case 'B':
				show_batteries = 0;
				break;
			case 't':
				show_thermal = 1;
				break;
			case 'T':
				show_thermal = 0;
				break;
			case 'a':
				show_ac_adapter = 1;
				break;
			case 'A':
				show_ac_adapter = 0;
				break;
			case 's':
				show_empty_slots = 1;
				break;
			case 'S':
				show_empty_slots = 0;
				break;
			case 'v':
				return version();
				break;
			case 'f':
				temperature_units = TEMP_FAHRENHEIT;
				break;
			case 'k':
				temperature_units = TEMP_KELVIN;
				break;
			case 'c':
				temperature_units = TEMP_CELSIUS;
				break;
			case 'd':
				free(acpi_path);
				acpi_path = strdup(optarg);
				if (!acpi_path) {
					fprintf(stderr, "Out of memory in main()\n");
					return -1;
				}
				break;
			case 'h':
			default:
				return usage(argv);
		}
	}

	if (show_everything || show_batteries) {
		do_show_batteries(acpi_path, show_empty_slots);
	}
	if (show_everything || show_thermal) {
		do_show_thermal(acpi_path, show_empty_slots, temperature_units);
	}
	if (show_everything || show_ac_adapter) {
		do_show_ac_adapter(acpi_path, show_empty_slots);
	}
	return 0;
}

