#define STAG_USER_COMMANDS(ENTRY)\
	ENTRY(STRINGS, "--strings", "-s", "list of strings separated by user-picked delimiter", "", true, '=')\
	ENTRY(FLOATS, "--floats", "-f", "list of floats separated by user-picked delimiter", "", true, '=')\
	ENTRY(UNSIGNED, "--unsigned", "-us", "list of integers separated by user-picked delimiter", "", true, '=')\
	ENTRY(SIGNED, "--signed", "-si", "list of integers separated by user-picked delimiter", "", true, '=')\

#include "stormc_argument_parser.h"




int main(int argc, char **argv)
{
	stag_run(argc, argv);

	stag_bool32 running = true;
	struct stag_cmd_array cmd;
	struct stag_array_string strings;
	struct stag_array_float floats;
	struct stag_array_unsigned us;
	struct stag_array_signed ss;
	while(running) {
		cmd = stag_next_cmd();
		switch (cmd.cmd) {
		case STRINGS:
		{
			strings = stag_string_to_array_of_strings(cmd.args, ';');
			break;
		}
		case FLOATS:
			floats = stag_string_to_array_of_floats(cmd.args, ';');
			break;
		case UNSIGNED:
			us = stag_string_to_array_of_unsigned(cmd.args, ';');
			break;
		case SIGNED:
			ss = stag_string_to_array_of_signed(cmd.args, ';');
			break;
		case NIL:
			running = false;
			break;
		}
	}


	printf("Strings:\n");
	for (int i = 0; i < strings.len; ++i) {
		printf("%.*s\n", (int)strings.strings[i].len, strings.strings[i].str);
	}
	printf("\n\n");


	printf("Floats:\n");
	for (int i = 0; i < floats.len; ++i) {
		printf("%f\n", floats.values[i]);
	}
	printf("\n\n");

	printf("Unsigned values:\n");
	for (int i = 0; i < us.len; ++i) {
		printf("%lu\n", us.values[i]);
	}
	printf("\n\n");

	printf("Signed values:\n");
	for (int i = 0; i < ss.len; ++i) {
		printf("%ld\n", ss.values[i]);
	}

	return 0;
}
