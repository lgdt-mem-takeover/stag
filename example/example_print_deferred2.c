#define STAG_USER_COMMANDS(ENTRY) \
	ENTRY(CMD_A, "--a", "-a", "print a args", "", true, ' ') \
	ENTRY(CMD_B, "--b", "-b", "print b args", "", true, ' ')

#include "../stormc_argument_parser.h"
#include <stdio.h>

struct print_words_payload {
	struct stag_array_string words;
	struct stag_string original;
};

static struct stag_stack *user_stack;

static struct stag_array_string
copy_split_words_to_user_stack(struct stag_string input, char delim)
{
	struct stag_array_string split = stag_string_to_array_of_strings(input, delim);
	struct stag_array_string out = {0};

	out.len = split.len;
	out.strings = stag_stack_push(user_stack, struct stag_string, out.len);

	for (stag_u64 i = 0; i < split.len; ++i) {
		out.strings[i] = split.strings[i];
	}

	return out;
}

void *print_a(struct stag_cmd_call *call)
{
	struct print_words_payload *pl = (struct print_words_payload *)call->args;

	printf("A callback:\n");
	printf("  original: %.*s\n", (int)pl->original.len, pl->original.str);
	printf("  split words:\n");

	for (stag_u64 i = 0; i < pl->words.len; ++i) {
		printf("    A[%llu] = %.*s\n",
		       (unsigned long long)i,
		       (int)pl->words.strings[i].len,
		       pl->words.strings[i].str);
	}
	printf("\n");

	return NULL;
}

void *print_b(struct stag_cmd_call *call)
{
	struct print_words_payload *pl = (struct print_words_payload *)call->args;

	printf("B callback:\n");
	printf("  original: %.*s\n", (int)pl->original.len, pl->original.str);
	printf("  split words:\n");

	for (stag_u64 i = 0; i < pl->words.len; ++i) {
		printf("    B[%llu] = %.*s\n",
		       (unsigned long long)i,
		       (int)pl->words.strings[i].len,
		       pl->words.strings[i].str);
	}
	printf("\n");

	return NULL;
}

static void register_callbacks(void)
{
	stag_ctx.user_data[CMD_A].fptr = print_a;
	stag_ctx.user_data[CMD_A].immediate_dispatch = false;

	stag_ctx.user_data[CMD_B].fptr = print_b;
	stag_ctx.user_data[CMD_B].immediate_dispatch = false;
}

static void build_deferred_calls(void)
{
	while (1) {
		struct stag_cmd_array cmd = stag_next_cmd();
		if (cmd.cmd == NIL) {
			break;
		}

		if (cmd.cmd == CMD_A || cmd.cmd == CMD_B) {
			struct print_words_payload *payload =
				stag_stack_push(user_stack, struct print_words_payload, 1);

			struct stag_cmd_call *call =
				stag_stack_push(user_stack, struct stag_cmd_call, 1);

			payload->original = cmd.args;
			payload->words = copy_split_words_to_user_stack(cmd.args, ' ');

			call->arg_count = payload->words.len;
			call->args = payload;

			stag_register_deferred_args(cmd.cmd, call);
		}
	}
}

int main(int argc, char **argv)
{
	user_stack = stag_stack_gen(1llu << 20);

	stag_run(argc, argv);
	register_callbacks();
	build_deferred_calls();

	printf("=== deferred flush ===\n\n");
	stag_deferred_flush_for(CMD_A);
	stag_deferred_flush_for(CMD_B);

	return 0;
}
