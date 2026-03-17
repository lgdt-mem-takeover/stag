#define STAG_USER_COMMANDS(ENTRY) \
	ENTRY(MOVE,     "--move",    "-m", "the file to be moved", "",  true, '=') \
	ENTRY(TO,       "--to",      "-t", "the destination the file will be moved to", "",  true, '=') \
	ENTRY(DRY,      "--dry-run", "-d", "test output, no execution", "",  false, 0) \
	ENTRY(VERBOSE,  "--verbose", "-v", "verbose", "",  false, 0)

#include "../../stormc_argument_parser.h"


int main(int argc, char **argv)
{
	stag_run(argc, argv);
	return 0;
}
