#define STAG_USER_COMMANDS(ENTRY) \
	ENTRY(FETCH,  "fetch",  "-f",  "load a file",        "", true, ' ') \
	ENTRY(FILTER, "filter", "-fi", "filter rows",        "", true, ' ') \
	ENTRY(MAP,    "map",    "-m",  "select columns",     "", true, ' ') \
	ENTRY(SAVE,   "save",   "-s",  "save output file",   "", true, ' ')




#include "../stormc_argument_parser.h"

struct pipeline_state {
	struct stag_string input_file;
	struct stag_string filter_expr;
	struct stag_string map_expr;
	struct stag_string output_file;
};

int main(int argc, char **argv)
{
	stag_run(argc, argv);

	struct pipeline_state st = {0};
	stag_bool32 running = true;

	while (running) {
		struct stag_cmd_array cmd = stag_next_cmd();
		switch (cmd.cmd) {
		case FETCH:  st.input_file = cmd.args; break;
		case FILTER: st.filter_expr = cmd.args; break;
		case MAP:    st.map_expr = cmd.args; break;
		case SAVE:   st.output_file = cmd.args; break;
		case NIL:    running = false; break;
		default:     break;
		}
	}

	printf("FETCH  : %.*s\n", (int)st.input_file.len, st.input_file.str);
	printf("FILTER : %.*s\n", (int)st.filter_expr.len, st.filter_expr.str);
	printf("MAP    : %.*s\n", (int)st.map_expr.len, st.map_expr.str);
	printf("SAVE   : %.*s\n", (int)st.output_file.len, st.output_file.str);

	return 0;
}
