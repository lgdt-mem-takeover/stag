#define STAG_USER_COMMANDS(ENTRY) \
	ENTRY(MOVE,     "--move",    "-m", "the file to be moved", "",  true, '=') \
	ENTRY(TO,       "--to",      "-t", "the destination the file will be moved to", "",  true, '=') \
	ENTRY(DRY,      "--dry-run", "-d", "test output, no execution", "",  false, 0) \
	ENTRY(VERBOSE,  "--verbose", "-v", "verbose", "",  false, 0)

#include "../stormc_argument_parser.h"

int main(int argc, char **argv)
{
	stag_run(argc, argv);

	struct stag_string src = {0};
	struct stag_string dst = {0};
	stag_bool32 dry_run = false;
	stag_bool32 verbose = false;

	stag_bool32 running = true;
	struct stag_cmd_array cmd;
	while (running) {
		cmd = stag_next_cmd();
		switch (cmd.cmd) {
		default: break;
		case MOVE:    src = cmd.args; break;
		case TO:      dst = cmd.args; break;
		case DRY:     dry_run = true; break;
		case VERBOSE: verbose = true; break;
		case NIL:     running = false; break;
		}
	}

	if (!src.str || !dst.str) {
		printf("Usage: organize --move=<path> --to=<path> [--dry-run] [--verbose]\n");
		return 1;
	}

	if (verbose) printf("Moving '%.*s' to '%.*s'%s\n",
			    (int)src.len, src.str,
			    (int)dst.len, dst.str,
			    dry_run ? " (dry run)" : "");

	if (!dry_run) {
		/* actually move files here */
		printf("Done!\n");
	}

	return 0;
}
