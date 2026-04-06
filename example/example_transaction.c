#define STAG_USER_COMMANDS(ENTRY) \
	ENTRY(BEGIN,    "begin",    "-b",  "begin transaction", "", false, 0) \
	ENTRY(ADD,      "add",      "-a",  "queue add",         "", true,  ' ') \
	ENTRY(REMOVE,   "remove",   "-rm", "queue remove",      "", true,  ' ') \
	ENTRY(COMMIT,   "commit",   "-c",  "commit transaction","", false, 0) \
	ENTRY(ROLLBACK, "rollback", "-rb", "rollback transaction","", false, 0)



#define STORMC_ALLOCATOR
#include "/data/2026-projs/c/stormlibc/stormc_header.h"
#include "../stormc_argument_parser.h"
#include <stdio.h>

struct filetx_state {
	stag_bool32 in_tx;
	stag_bool32 should_commit;
	stag_bool32 should_rollback;
};

struct filetx_payload {
	struct stag_string path;
};

static struct filetx_state g_tx = {0};
static struct stc_stack *stack_cmd_call = NULL;
static struct stc_stack *stack_payloads = NULL;

static void filetx_init_stacks(void)
{
	stack_cmd_call = stc_stack_gen(1llu << 20);
	stack_payloads = stc_stack_gen(1llu << 20);
}

void *cmd_begin_cb(struct stag_cmd_call *call)
{
	(void)call;

	if (g_tx.in_tx) {
		fprintf(stderr, "[ERR] transaction already open\n");
		return NULL;
	}

	g_tx.in_tx = true;
	g_tx.should_commit = false;
	g_tx.should_rollback = false;
	printf("[BEGIN]\n");
	return NULL;
}

void *cmd_add_cb(struct stag_cmd_call *call)
{
	struct filetx_payload *pl = (struct filetx_payload *)call->args;

	if (!g_tx.in_tx) {
		fprintf(stderr, "[ERR] add used outside transaction: %.*s\n",
		        (int)pl->path.len, pl->path.str);
		return NULL;
	}

	printf("[ADD] %.*s\n", (int)pl->path.len, pl->path.str);
	return NULL;
}

void *cmd_remove_cb(struct stag_cmd_call *call)
{
	struct filetx_payload *pl = (struct filetx_payload *)call->args;

	if (!g_tx.in_tx) {
		fprintf(stderr, "[ERR] remove used outside transaction: %.*s\n",
		        (int)pl->path.len, pl->path.str);
		return NULL;
	}

	printf("[REMOVE] %.*s\n", (int)pl->path.len, pl->path.str);
	return NULL;
}

void *cmd_commit_cb(struct stag_cmd_call *call)
{
	(void)call;

	if (!g_tx.in_tx) {
		fprintf(stderr, "[ERR] commit used without begin\n");
		return NULL;
	}

	g_tx.should_commit = true;
	printf("[COMMIT]\n");
	return NULL;
}

void *cmd_rollback_cb(struct stag_cmd_call *call)
{
	(void)call;

	if (!g_tx.in_tx) {
		fprintf(stderr, "[ERR] rollback used without begin\n");
		return NULL;
	}

	g_tx.should_rollback = true;
	printf("[ROLLBACK]\n");
	return NULL;
}

static void register_callbacks(void)
{
	struct stag_callback_desc descs[] = {
		{ .cmd = BEGIN,    .func = cmd_begin_cb,    .immediate = false },
		{ .cmd = ADD,      .func = cmd_add_cb,      .immediate = false },
		{ .cmd = REMOVE,   .func = cmd_remove_cb,   .immediate = false },
		{ .cmd = COMMIT,   .func = cmd_commit_cb,   .immediate = false },
		{ .cmd = ROLLBACK, .func = cmd_rollback_cb, .immediate = false },
	};

	stag_register_callback_batch(descs, STAG_ARRCOUNT(descs));
}

static void build_calls(void)
{
	for (;;) {
		struct stag_cmd_array cmd = stag_next_cmd();
		if (cmd.cmd == NIL) {
			break;
		}

		switch (cmd.cmd) {
		case BEGIN:
		case COMMIT:
		case ROLLBACK:
		{
			struct stag_cmd_call *call =
				(struct stag_cmd_call *)stc_stack_push(stack_cmd_call, struct stag_cmd_call, 1);
			call->arg_count = 0;
			call->args = NULL;
			stag_register_deferred_args(cmd.cmd, call);
			break;
		}

		case ADD:
		case REMOVE:
		{
			struct filetx_payload *pl =
				(struct filetx_payload *)stc_stack_push(stack_payloads, struct filetx_payload, 1);
			pl->path = cmd.args;

			struct stag_cmd_call *call =
				(struct stag_cmd_call *)stc_stack_push(stack_cmd_call, struct stag_cmd_call, 1);
			call->arg_count = 1;
			call->args = pl;

			stag_register_deferred_args(cmd.cmd, call);
			break;
		}

		default:
			break;
		}
	}
}

int main(int argc, char **argv)
{
	stag_run(argc, argv);
	filetx_init_stacks();
	register_callbacks();
	build_calls();

	/* transaction validation before execution */
	stag_bool32 saw_begin = false;
	stag_bool32 saw_commit = false;
	stag_bool32 saw_rollback = false;

	for (stag_u64 i = 0; i < stag_ctx.cmd_array_dispatch_ct; ++i) {
		struct stag_cmd_array cmd = stag_ctx.cmd_array[i];
		if (cmd.cmd == BEGIN) saw_begin = true;
		if (cmd.cmd == COMMIT) saw_commit = true;
		if (cmd.cmd == ROLLBACK) saw_rollback = true;
	}

	if (!saw_begin) {
		fprintf(stderr, "[ERR] missing begin\n");
		return 1;
	}
	if (saw_commit && saw_rollback) {
		fprintf(stderr, "[ERR] cannot use commit and rollback together\n");
		return 1;
	}
	if (!saw_commit && !saw_rollback) {
		fprintf(stderr, "[ERR] transaction must end with commit or rollback\n");
		return 1;
	}

	if (saw_rollback) {
		printf("[INFO] rollback requested, queued ops discarded\n");
		return 0;
	}

	stag_deferred_flush();
	return 0;
}
