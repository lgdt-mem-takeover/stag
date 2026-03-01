typedef float	stag_f32;
typedef double	stag_f64;

typedef unsigned char		stag_u8;
typedef unsigned short		stag_u16;
typedef unsigned int		stag_u32;
typedef unsigned int long	stag_u64;

typedef char		stag_i8;
typedef short		stag_i16;
typedef int		stag_i32;
typedef int long	stag_i64;

typedef stag_u8		stag_bool8;
typedef stag_u16	stag_bool16;
typedef stag_u32	stag_bool32;
typedef stag_u64	stag_bool64;

static stag_bool32 is_prime(stag_u64 n);
static stag_u64 next_prime(stag_u64 n);
static stag_bool32 is_pow2(stag_u64 n);
static stag_u64 next_pow2(stag_u64 n);


#ifndef STAG_MAX_RESERVATION
#define STAG_MAX_RESERVATION (1llu << 30)
#endif

#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)


#include <stdlib.h>
#include <stdio.h>


/*============================================*/
/*@ALLOCATOR*/
#define MAX_UINT64 (stag_u64)(-1)
#include <stddef.h>



#define stc_byte char
#define STC_ALIGN_UP(x, align) (((x) + ((align)-1)) & ~((align)-1))
#define SELECT(cond, when_true, when_false) ((when_true) * (cond) | (when_false) * !(cond))
#define PAGESIZE 4096



#ifdef _WIN32
#include <windows.h>
static void *stc_os_alloc_default(stag_u64 size)
{
	return VirtualAlloc(NULL, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
}

void *stc_os_mem_rsrv(stag_u64 size)
{
	return VirtualAlloc(NULL, size, MEM_RESERVE, PAGE_NOACCESS);
}

void *stc_os_mem_cmt(void* addrs, stag_u64 size)
{
	return VirtualAlloc(addrs, size, MEM_COMMIT, PAGE_READWRITE);
}
#else
#include <sys/mman.h>

void *stc_os_alloc_default(stag_u64 size)
{
	return mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1 , 0);

}

void *stc_os_mem_rsrv(stag_u64 size)
{
	return mmap(NULL, size, PROT_NONE, MAP_ANON | MAP_PRIVATE, -1 , 0);
}

void *stc_os_mem_cmt(void* addrs, stag_u64 size)
{
	size = STC_ALIGN_UP(size, PAGESIZE);
	int res = mprotect(addrs, size, PROT_READ | PROT_WRITE);
	if (unlikely(res != 0)) {
		stc_byte buff[64];
		printf("mprotect code: %d\n", res);
		return NULL;
	}
	return addrs;

}
#endif


#define MAX_FREE_LIST_ELEMENTS 65535
struct free_list {
	void		*ptr[MAX_FREE_LIST_ELEMENTS];
	stag_u64	size[MAX_FREE_LIST_ELEMENTS];
	stag_u64	ct_ptrs;
};

//@STACK STC RUNTIME
struct stc_stack {
	struct free_list	free_list;
	stag_u64		mem_rsrv;
	stag_u64		mem_committed;
	stag_u64		base_offset;
	stag_u64		checkpoint_offset;
	stc_byte		*base;
};
#define STACK_HEADER_SIZE sizeof(struct stc_stack)

struct stc_stack *stc_stack_gen(stag_u64 rsrv)
{

	if (!is_pow2(rsrv))
		rsrv = STC_ALIGN_UP(next_pow2(rsrv), PAGESIZE);

	stc_byte *block = (stc_byte *)stc_os_mem_rsrv(rsrv);
	stc_byte *stack_ptr_start = (stc_byte *)STC_ALIGN_UP((stag_u64)block + STACK_HEADER_SIZE, PAGESIZE);
	if (unlikely(stc_os_mem_cmt(block, STACK_HEADER_SIZE) == NULL)) {
		printf("Failed block header size\n");
		perror("mprotect");
	}
	rsrv = ((stag_u64)block + rsrv) - (stag_u64)stack_ptr_start;
	if (unlikely(stc_os_mem_cmt(stack_ptr_start, PAGESIZE) == NULL)) {
		printf("Failed at stack ptr start\n");
		perror("mprotect");
	}

	struct stc_stack *pl = (struct stc_stack *)block;
	pl->base = stack_ptr_start;
	pl->mem_rsrv = rsrv;
	pl->mem_committed = PAGESIZE;
	pl->base_offset = 0;
	return pl;
}


void *_stc_stack_push(struct stc_stack *s, stag_u64 alignment, stag_u64 total_size)
{
	if (!is_pow2(alignment))
		alignment = next_pow2(alignment);


	stag_u64 start_old = (stag_u64)s->base + s->base_offset;
	stag_u64 start_new = STC_ALIGN_UP(start_old, alignment);
	stag_u64 end_new = STC_ALIGN_UP(start_new + total_size, alignment);
	stag_u64 delta_offset = end_new - (stag_u64)s->base;


	if (s->free_list.ct_ptrs > 0 && s->free_list.ct_ptrs < MAX_FREE_LIST_ELEMENTS) {
		void *return_ptr = NULL;
		stag_u64 found_idx = 0;
		stag_u64 smallest_fit_size = MAX_UINT64;
		for (stag_u64 i = 0; i < s->free_list.ct_ptrs; ++i) {
			stag_u64 ptr_base_aligned = STC_ALIGN_UP((stag_u64)s->free_list.ptr[i], alignment);
			stag_u64 ptr_aligned_dist_relative_to_base_pointer = ptr_base_aligned - (stag_u64)s->free_list.ptr[i];
			stag_u64 size_after_aligning = s->free_list.size[i] - ptr_aligned_dist_relative_to_base_pointer;
			if (smallest_fit_size >= size_after_aligning && size_after_aligning >= total_size) {
				smallest_fit_size = size_after_aligning;
				return_ptr = (void*)ptr_base_aligned;
				found_idx = i;
			}
		}

		if (return_ptr != NULL) {
			void *temp_ptr = s->free_list.ptr[s->free_list.ct_ptrs - 1];
			s->free_list.ptr[s->free_list.ct_ptrs - 1] = s->free_list.ptr[found_idx];
			s->free_list.ptr[found_idx] = temp_ptr;

			stag_u64 temp_size = s->free_list.size[s->free_list.ct_ptrs - 1];
			s->free_list.size[s->free_list.ct_ptrs - 1] = s->free_list.size[found_idx];
			s->free_list.size[found_idx] = temp_size;
			s->free_list.ct_ptrs--;
			return return_ptr;
		}
	}

	/*s->mem_cmt always stays aligned to page boundary*/
	if (delta_offset > s->mem_committed) {
		stag_u64 delta_commit = STC_ALIGN_UP(delta_offset - s->mem_committed, PAGESIZE);
		stag_u64 new_total_commit = delta_commit + s->mem_committed;
		if (stc_os_mem_cmt((stc_byte*)s->base + s->mem_committed, delta_commit) == NULL) {
			perror("mprotect");
		}

		s->mem_committed = new_total_commit;
	}

	s->base_offset = delta_offset;

	return (void*)start_new;
}


void stc_stack_free(struct stc_stack *s, void* mem_addrs, stag_u64 size)
{

	if (s->free_list.ct_ptrs < MAX_FREE_LIST_ELEMENTS) {
		s->free_list.ptr[s->free_list.ct_ptrs] = mem_addrs;
		s->free_list.size[s->free_list.ct_ptrs] = size;
		s->free_list.ct_ptrs++;
	}
}

void stc_stack_pop(struct stc_stack *stack, stag_u64 size)
{
	stag_bool32 cond = stack->base_offset >= size;
	if (unlikely(!cond)) {
		fprintf(stdout, "Tried to pop %lu bytes, but offset is %lu\n", size, stack->base_offset);
	}
	stag_u64 count_checked = SELECT(cond, stack->base_offset - size, stack->base_offset);
	stack->base_offset = count_checked;
}


void stack_begin(struct stc_stack *s)
{
	s->checkpoint_offset = s->base_offset;
}


void stack_end(struct stc_stack *s)
{
	s->base_offset = s->checkpoint_offset;
}

/*============================================*/


#ifndef true
#define true 1
#endif

#ifndef false
#define false 0
#endif

#define STAG_STR(x) (struct stag_string){.str = (x), .len = sizeof((x)) - 1}

// #define STAG_REMOVE_PREFIX

#ifdef STAG_STRIP_PREFIX
#define string (struct stag_string)
#define ctx (struct stag_ctx)
#define layout (struct stag_layout)


#define bool8  stag_bool8
#define bool16 stag_bool16
#define bool32 stag_bool32
#define bool64 stag_bool64

#define u8 stag_u8
#define u16 stag_u16
#define u32 stag_u32
#define u64 stag_u64

#define i8  stag_i8
#define i16 stag_i16
#define i32 stag_i32
#define i64 stag_i64


#define f32 stag_f32
#define f64 stag_f64
#endif


struct stag_string{
	char	*str;
	stag_u64 len;
};


struct stag_layout{
	struct stag_string	cmd_name;
	struct stag_string	cmd_short;
	struct stag_string	cmd_description;
	struct stag_string	arg_missing_err_msg;
	stag_bool64		takes_args;
	char			arg_delimiter;
};



#ifndef STAG_USER_COMMANDS
#error "Define STAG_USER_COMMANDS with a list of commands"
#endif
#define STAG_ENUM_ENTRY(name, ...) name,
enum stag_user_commands{
	NIL,
	STAG_HELP,
	STAG_USER_COMMANDS(STAG_ENUM_ENTRY)
#undef STAG_ENUM_ENTRY
	USER_COMMANDS_COUNT
};

#define STAG_TABLE_ENTRY(_enum, _cmd_name, _cmd_short, _cmd_desc, _err_msg_missing_arg, _takes_args, _arg_delim, ...) \
	[(_enum)] = {(STAG_STR(_cmd_name)), (STAG_STR(_cmd_short)), (STAG_STR(_cmd_desc)), (STAG_STR(_err_msg_missing_arg)), (_takes_args), (_arg_delim)},

const struct stag_layout stag_table[] = {
	[NIL] = {STAG_STR(""), STAG_STR(""), false},
	[STAG_HELP] = {STAG_STR("--help"), STAG_STR("-h")},
	STAG_USER_COMMANDS(STAG_TABLE_ENTRY)
#undef STAG_TABLE_ENTRY
};



struct stag_cmd_array{
	enum stag_user_commands		cmd;
	struct stag_string		args;

};

struct stag_ctx{
	int		argc;
	char		**argv;


	struct stag_cmd_array		*cmd_array;
	stag_u64			cmd_array_len;
	stag_u64			cmd_array_dispatch_ct;
	stag_u64			cmtd;
	const struct stag_layout	*user_data;
};


static struct stag_ctx stag_ctx = {0};



/*FUNC DEFS*/
void stag_parse(void);
struct stag_string stag_strip_arg_upto_delim(struct stag_string string, char delim);
void stag_run(int argc, char **argv);
stag_bool32 stag_strcmp(struct stag_string a, struct stag_string b);
void stag_cmd_array_append(struct stag_cmd_array cmd);
struct stag_string_array stag_string_to_array_of_strings(struct stag_string input, char delim);
static stag_f64 stag_string_to_float(struct stag_string *string);
static stag_i64 stag_string_to_i64(struct stag_string *string);
static stag_u64 stag_string_to_u64(struct stag_string *string);
struct stag_string stag_strip_arg_at_delim(char *string, char delim); /*UNUSED*/



stag_u64 stag_string_to_stag_u64(struct stag_string *string)
{
	stag_u64 pl = 0;
	for (char *start = (char*)string->str, *end = (char*)string->str + string->len; start!= end; ++start) {
		pl = (pl * 10) + ((*start) ^ '0');
	}

	return pl;
}


stag_i64 stag_string_to_stag_i64(struct stag_string *string)
{
	stag_i64 pl = 0;
	stag_bool32 is_negative = string->str[0] == '-';
	char *start;
	if (is_negative) {
		start = (char*)string->str + 1;
	} else {
		start = (char*)string->str;
	}
	for (char *end = (char *)string->str + string->len; start!= end; ++start) {
		pl = (pl * 10) + ((*start) ^ '0');
	}

	return is_negative ? -pl : pl;
}

stag_f64 stag_string_to_float(struct stag_string *string)
{

	stag_f64 pl = 0.0f;

	stag_bool32 is_negative = string->str[0] == '-';
	char *start;
	if (is_negative) {
		start = (char*)(string->str + 1);
	} else {
		start = (char*)string->str;
	}

	char *end = (char*)(string->str + string->len);

	for (; (*start) != '.' && start != end; ++start) {
		pl = (pl * 10) + ((*start) ^ '0');
	}
	++start; /*eat dot '.' */

	stag_f64 divisor = 10.0f;
	for (; start != end; ++start) {
		stag_f64 fractional = (*start) ^ '0';
		pl = pl + fractional / divisor;
		divisor *= 10.0f;
	}


	return is_negative ? -pl : pl;
}


void stag_parse(void)
{
	char **start = stag_ctx.argv + 1;
	char **end = stag_ctx.argv + stag_ctx.argc;

	int ct = 0;

	struct stag_string *input = (struct stag_string*)stc_os_alloc_default(sizeof(struct stag_string) * stag_ctx.argc);
	stag_u64 input_len = 0;


	while (start != end) {
		input[input_len].str = *start;
		input[input_len].len = __builtin_strlen(*start);
		input_len++;
		++start;
	}


	stag_u64 idx = 0;
	while (idx < input_len) {
		for (stag_u64 i = 2; i < USER_COMMANDS_COUNT; ++i) {
			struct stag_cmd_array current = {0};
			current.cmd = i;
			struct stag_string cmd = {0};
			stag_bool32 has_args = stag_ctx.user_data[i].takes_args;

			if (stag_strcmp(input[idx], STAG_STR("--help")) || stag_strcmp(input[idx], STAG_STR("-h"))) {
				printf("Available commands:\n");
				for (stag_u64 j = 2; j < USER_COMMANDS_COUNT; ++j) {
					printf("%s %s : %s\n",
					       stag_ctx.user_data[j].cmd_name.str,
					       *stag_ctx.user_data[j].cmd_short.str ? stag_ctx.user_data[j].cmd_short.str : "",
					       stag_ctx.user_data[j].cmd_description.str
					);
				}
				exit(1);
			}

			if (has_args) {
				char delim = stag_ctx.user_data[i].arg_delimiter;
				if (delim == ' ') {
					cmd = input[idx];
					++idx;
					current.args = input[idx];
				} else {
					cmd = stag_strip_arg_upto_delim(input[idx], delim);
					current.args.str = input[idx].str + cmd.len + 1;
					current.args.len = __builtin_strlen(current.args.str);

					if (stag_strcmp(stag_ctx.user_data[i].cmd_name, cmd) || stag_strcmp(stag_ctx.user_data[i].cmd_short, cmd)) {
					}
				}
			} else {
				cmd = input[idx];
			}

			stag_bool32 is_string_match = stag_strcmp(stag_ctx.user_data[i].cmd_name, cmd) || stag_strcmp(stag_ctx.user_data[i].cmd_short, cmd);

			if (is_string_match) {
				if (has_args) {
					if (current.args.len == 0) {
						fprintf(
							stderr,
							"[Error] Command '%.*s' : %s\n",
							(int)input[idx].len,
							input[idx].str,
							*stag_ctx.user_data[i].arg_missing_err_msg.str ?
							stag_ctx.user_data[i].arg_missing_err_msg.str : "missing arg"

						    );
						exit(1);
					}
				}

				stag_cmd_array_append(current);
				break;
			}
		}
		++idx;
	}


	stag_cmd_array_append((struct stag_cmd_array){.cmd = NIL});
}

void stag_run(int argc, char **argv)
{

	stag_ctx.user_data = stag_table;
	stag_ctx.argc = argc;
	stag_ctx.argv = argv;
	stag_ctx.cmd_array = stc_os_mem_rsrv(STAG_MAX_RESERVATION);
	int initial_size = STC_ALIGN_UP(sizeof(*stag_ctx.cmd_array), PAGESIZE);
	stag_ctx.cmtd = initial_size;
	stc_os_mem_cmt(stag_ctx.cmd_array, initial_size);
	stag_parse();
}




void stag_cmd_array_append(struct stag_cmd_array cmd)
{
	stag_u64 element_count_from_size = stag_ctx.cmtd / sizeof(*stag_ctx.cmd_array);
	if (stag_ctx.cmd_array_len == 0 || stag_ctx.cmd_array_len >= element_count_from_size) {
		int new_size = STC_ALIGN_UP(sizeof(*stag_ctx.cmd_array), PAGESIZE);
		stc_os_mem_cmt(stag_ctx.cmd_array + stag_ctx.cmtd, new_size);
		stag_ctx.cmtd += new_size;
	}
	stag_ctx.cmd_array[stag_ctx.cmd_array_len++] = cmd;
}


struct stag_cmd_array stag_next_cmd(void)
{
	return stag_ctx.cmd_array[stag_ctx.cmd_array_dispatch_ct++];
}



struct stag_string stag_strip_arg_at_delim(char *string, char delim)
{
	struct stag_string pl = {0};

	while(string++) {
		if (*string == delim) {
			string++;
			pl.str = string;
			break;
		}
	}
	pl.len = __builtin_strlen(pl.str);
	return pl;
}

struct stag_string stag_strip_arg_upto_delim(struct stag_string string, char delim)
{
	struct stag_string pl = {0};

	char *base = string.str;

	while (*string.str != delim && *string.str != '\0')
		string.str++;

	pl.str = base;
	pl.len = string.str - base;

	return pl;
}


stag_bool32 stag_strcmp(struct stag_string a, struct stag_string b)
{
	if (a.len != b.len)
		return false;

	char *start = a.str;
	while (*a.str == *b.str && ((a.str - start) < a.len)) {
		a.str++;
		b.str++;
	}


	return !((a.str - start) - a.len);
}






struct stag_string_array{
	struct stag_string	*strings;
	stag_u64		len;
};


struct stag_float_array{
	stag_f64		*values;
	stag_u64	len;
};

struct stag_float_array_u{
	stag_u64		*values;
	stag_u64	len;
};

struct stag_float_array_s{
	stag_i64		*values;
	stag_u64	len;
};



struct stag_float_array stag_string_to_array_of_floats(struct stag_string input, char delim)
{
	struct stag_float_array pl = {0};

	pl.values = (stag_f64*)stc_os_alloc_default(sizeof(stag_f64) * 1024);
	stag_u64 idx = 0;
	stag_u64 inner_offset = 0;
	while (idx < input.len) {
		if (input.str[idx] == delim) {
			struct stag_string string;
			string.str = (input.str + idx) - inner_offset;
			string.len = inner_offset;
			pl.values[pl.len] = stag_string_to_float(&string);
			pl.len++;
			inner_offset = 0;
		} else {
			inner_offset++;
		}
		++idx;
	}
	struct stag_string string;
	string.str = (input.str + idx) - inner_offset;
	string.len = inner_offset;
	pl.values[pl.len] = stag_string_to_float(&string);
	pl.len++;


	return pl;
}


struct stag_string_array stag_string_to_array_of_strings(struct stag_string input, char delim)
{
	struct stag_string_array pl = {0};
	pl.strings = stc_os_alloc_default(sizeof(struct stag_string) * 1024);
	stag_u64 idx = 0;
	stag_u64 inner_offset = 0;
	while (idx < input.len) {
		if (input.str[idx] == delim) {
			pl.strings[pl.len].str = (input.str + idx) - inner_offset;
			pl.strings[pl.len].len = inner_offset;
			pl.len++;
			inner_offset = 0;
		} else {
			inner_offset++;
		}
		++idx;
	}
	pl.strings[pl.len].str = (input.str + idx) - inner_offset;
	pl.strings[pl.len].len = inner_offset;
	pl.len++;

	return pl;
}



stag_bool32 is_prime(stag_u64 n) {
    if (n < 2) return false;
    if ((n & 1) == 0) return n == 2;
    for (stag_u64 i = 3; i * i <= n; i += 2)
        if (n % i == 0) return false;
    return (stag_bool32)true;
}

stag_u64 next_prime(stag_u64 n) {
    if (n <= 2) return 2;
    if ((n & 1) == 0) n++;
    while (!is_prime(n)) n += 2;
    return n;
}

stag_bool32 is_pow2(stag_u64 n)
{
	return (n != 0) && (n & (n-1)) == 0;
}

stag_u64 next_pow2(stag_u64 n)
{
	n--;
	n |= n >> 1;
	n |= n >> 2;
	n |= n >> 4;
	n |= n >> 8;
	n |= n >> 16;
	n |= n >> 32;
	n++;
	return n;
}
