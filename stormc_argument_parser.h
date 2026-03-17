#include <assert.h>

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
	struct stc_stack	*scratch;
	int			argc;
	char			**argv;


	struct stag_cmd_array		*cmd_array;
	stag_u64			cmd_array_len;
	stag_u64			cmd_array_dispatch_ct;
	stag_u64			cmtd;
	const struct stag_layout	*user_data;
};


/*FUNC DEFS*/
static stag_bool32 is_prime(stag_u64 n);
static stag_u64 next_prime(stag_u64 n);
static stag_bool32 is_pow2(stag_u64 n);
static stag_u64 next_pow2(stag_u64 n);
static void stag_parse(void);
struct stag_string stag_strip_arg_upto_delim(struct stag_string string, char delim);
static void stag_run(int argc, char **argv);
static stag_bool32 stag_strcmp(struct stag_string a, struct stag_string b);
static void stag_cmd_array_append(struct stag_cmd_array cmd);
struct stag_array_string stag_string_to_array_of_strings(struct stag_string input, char delim);
static stag_f64 stag_string_to_float(struct stag_string *string);
static stag_i64 stag_string_to_i64(struct stag_string *string);
static stag_u64 stag_string_to_u64(struct stag_string *string);
struct stag_string stag_strip_arg_at_delim(struct stag_string string, char delim); /*UNUSED*/

#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)


#include <stdlib.h>
#include <stdio.h>

/*============================================*/
/*@ALLOCATOR*/
#define MAX_UINT64 (stag_u64)(-1)
#include <stddef.h>


#define stc_stack_start(stack)\
	stack->checkpoint_offset = stack->base_offset

#define stc_stack_end(stack)\
	stack->base_offset = stack->checkpoint_offset

#define stc_stack_push(__stack, __type, __count)\
	_stc_stack_push((__stack), __alignof__(__type), (sizeof(__type) * (__count)))

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


#ifndef STAG_MAX_RESERVATION
#define STAG_MAX_RESERVATION (1llu << 30)
#endif



#define ILT_GROUPS_COUNT 64ULL
#define ILT_ML0_CT (ILT_GROUPS_COUNT * ILT_GROUPS_COUNT)
#define ILT_TOTAL_INDICES (ILT_ML0_CT * 64ULL)
#define ILT_NIL_REAL_IDX (~0ULL)


struct stc_ilt{
	stag_u64	ml0[ILT_ML0_CT];
	stag_u64	ml0_count;
	stag_u64	ml1[ILT_GROUPS_COUNT];
	stag_u64	ml1_count;
	stag_u64	ml2;
	stag_u64	used_count;
	stag_u64	max;
};



struct ilt_ctx_frame{
	stag_u64	ml1_idx;
	stag_u64	ml1_group;
	stag_u64	ml0_idx;
	stag_u64	ml0_bit;
};

static stag_u64 ilt_next_idx(struct stc_ilt *ilt);
static stag_u64 ilt_top_idx(struct stc_ilt *ilt);
static stag_bool32 ilt_is_empty(struct stc_ilt *ilt);
static stag_bool32 ilt_is_full(struct stc_ilt *ilt);
static stag_u64 ilt_get_ml0_idx(stag_u64 ml1_idx, stag_u64 ml1_current_full_groups);
static stag_u64 ilt_get_real_idx(stag_u64 ml1_idx, stag_u64 ml1_current_full_groups, stag_u64 ml0_bit_offset);
static void ilt_set_ml1_group(struct stc_ilt *ilt, stag_u64 ml1_idx, stag_u64 ml1_group);
static void ilt_unset_ml1_group(struct stc_ilt *ilt, stag_u64 ml1_idx, stag_u64 ml1_group);
static void ilt_count_inc(struct stc_ilt *ilt);
static void ilt_count_dec(struct stc_ilt *ilt);
static struct ilt_ctx_frame ilt_get_ctx_frame(struct stc_ilt *ilt);




struct free_list {
	struct stc_ilt	ilt;
	stag_u64		*ptr;
	stag_u64		*size;
};

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
	stag_u64 end_new = start_new + total_size;
	stag_u64 size_new = end_new - (stag_u64)s->base;


	if (!ilt_is_empty(&s->free_list.ilt)) {
		stag_u64 smallest_fit = MAX_UINT64;
		stag_u64 smallest_fit_position = MAX_UINT64;
		for (stag_u64 ml1_indx = 0; ml1_indx < 64; ++ml1_indx) {
			for (stag_u64 ml1_group = 0; ml1_group < 64; ++ml1_group) {
				stag_u64 ml0_idx = ilt_get_ml0_idx((stag_u64)ml1_indx, ml1_group);

				if (s->free_list.ilt.ml0[ml0_idx] != 0) {
					int bit_idx = 0;
					while (bit_idx < 64) {
						if (!(s->free_list.ilt.ml0[ml0_idx] & (1llu << bit_idx))) {
							++bit_idx;
							continue;
						}
						stag_u64 real_idx = ilt_get_real_idx(ml1_indx, ml1_group, (stag_u64)bit_idx);
						if (((stag_u64)(s->free_list.ptr[real_idx] & (alignment - 1)) == 0)) {
							if (s->free_list.size[real_idx] < smallest_fit && s->free_list.size[real_idx] >= total_size) {
								smallest_fit = s->free_list.size[real_idx];
								smallest_fit_position = real_idx;
							}
						}
						++bit_idx;
					}
				}
			}
		}

		if (smallest_fit != MAX_UINT64) {
			stag_u64 best_ml1_idx = smallest_fit_position / ILT_ML0_CT;
			stag_u64 best_ml1_group = (smallest_fit_position / ILT_GROUPS_COUNT) % ILT_GROUPS_COUNT;
			s->free_list.ilt.ml0[(smallest_fit_position / ILT_GROUPS_COUNT)] &= ~(1llu << (smallest_fit_position % ILT_GROUPS_COUNT));
			ilt_unset_ml1_group(&s->free_list.ilt, best_ml1_idx, best_ml1_group);
			s->free_list.ilt.used_count--;
			return (void*)s->free_list.ptr[smallest_fit_position];
		}
	}

	/*s->mem_cmt always stays aligned to page boundary*/
	if (size_new > s->mem_committed) {
		stag_u64 delta_commit = STC_ALIGN_UP(size_new - s->mem_committed, PAGESIZE);
		stag_u64 new_total_commit = delta_commit + s->mem_committed;
		assert((s->mem_committed % 4096) == 0);
		if (stc_os_mem_cmt((stc_byte*)s->base + s->mem_committed, delta_commit) == NULL) {
			perror("mprotect");
			exit(1);
		}
		assert((s->mem_committed % 4096) == 0);

		s->mem_committed = new_total_commit;
	}

	s->base_offset = size_new;

	return (void*)start_new;
}


void stc_stack_free(struct stc_stack *s, void* mem_addrs, stag_u64 size)
{
	stag_u64 next_index = ilt_next_idx(&s->free_list.ilt);
	s->free_list.ptr[next_index] = (stag_u64)mem_addrs;
	s->free_list.size[next_index] = size;
	ilt_count_inc(&s->free_list.ilt);
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



static struct stag_ctx stag_ctx = {0};






stag_u64 stag_string_to_u64(struct stag_string *string)
{
	stag_u64 pl = 0;
	for (char *start = (char*)string->str, *end = (char*)string->str + string->len; start!= end; ++start) {
		pl = (pl * 10) + ((*start) ^ '0');
	}

	return pl;
}


stag_i64 stag_string_to_i64(struct stag_string *string)
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


stag_bool32 stag_check_cmd_exists(struct stag_string target)
{
	for (stag_u64 i = STAG_HELP; i < USER_COMMANDS_COUNT; ++i) {
		struct stag_string name_long = stag_ctx.user_data[i].cmd_name;
		struct stag_string name_short = stag_ctx.user_data[i].cmd_short;
		stag_bool32 is_name_long = stag_strcmp(target, name_long);
		stag_bool32 is_name_short = stag_strcmp(target, name_short);
		if (is_name_long || is_name_short) {
			return true;
		}
	}
	return false;
}

void stag_parse(void)
{
	char **start = stag_ctx.argv + 1;
	char **end = stag_ctx.argv + stag_ctx.argc;

	if (*start == NULL) {
		fprintf(stderr, "No input detected. Try available commands:\n");
		for (stag_u64 j = 2; j < USER_COMMANDS_COUNT; ++j) {
			printf("%s %s : %s\n",
			       stag_ctx.user_data[j].cmd_name.str,
			       *stag_ctx.user_data[j].cmd_short.str ? stag_ctx.user_data[j].cmd_short.str : "",
			       stag_ctx.user_data[j].cmd_description.str
			      );
		}
		exit(1);
	}

	struct stag_string *input = (struct stag_string*)stc_stack_push(stag_ctx.scratch, struct stag_string, stag_ctx.argc);
	stag_u64 input_len = 0;


	while (start != end) {
		input[input_len].str = *start;
		input[input_len].len = __builtin_strlen(*start);
		input_len++;
		++start;
	}


	stag_u64 idx = 0;
	while (idx < input_len) {
		struct stag_string current = stag_strip_arg_at_delim(input[idx], '=');
		if (!stag_check_cmd_exists(current)) {
			fprintf(stderr, "[ERROR] Command %.*s not found\n", (int)current.len, current.str);
			exit(1);
		}
		if (stag_strcmp(input[idx], STAG_STR("--help")) || stag_strcmp(input[idx], STAG_STR("-h"))) {
			printf("Available commands:\n");
			for (stag_u64 j = 2; j < USER_COMMANDS_COUNT; ++j) {
				printf("%s %s : %s\n",
				       stag_ctx.user_data[j].cmd_name.str,
				       *stag_ctx.user_data[j].cmd_short.str ? stag_ctx.user_data[j].cmd_short.str : "",
				       stag_ctx.user_data[j].cmd_description.str
				      );
			}
			exit(0);
		}
		++idx;
	}

	idx = 0;
	while (idx < input_len) {
		for (stag_u64 cmd_idx = 2; cmd_idx < USER_COMMANDS_COUNT; ++cmd_idx) {
			struct stag_cmd_array cmd_current = {};
			cmd_current.cmd = (enum stag_user_commands)cmd_idx;
			stag_bool32 has_args = stag_ctx.user_data[cmd_idx].takes_args;
			struct stag_string current = stag_strip_arg_at_delim(input[idx], '=');


			stag_bool32 is_match_name_short = stag_strcmp(current, stag_ctx.user_data[cmd_idx].cmd_short);
			stag_bool32 is_match_name_long = stag_strcmp(current, stag_ctx.user_data[cmd_idx].cmd_name);
			stag_bool32 is_match = is_match_name_long || is_match_name_short;

			if (is_match) {
				if (has_args) {
					if (current.len >= input[idx].len) {
						fprintf(stderr, "[Error] Command '%.*s' : %s\n",
							(int)input[idx].len, input[idx].str,
							*stag_ctx.user_data[cmd_idx].arg_missing_err_msg.str
							? stag_ctx.user_data[cmd_idx].arg_missing_err_msg.str
							: "missing arg");
						exit(1);
					}
					cmd_current.args.str = input[idx].str + current.len + 1;
					cmd_current.args.len = input[idx].len - current.len - 1;
					if (cmd_current.args.len == 0) {
						fprintf(
							stderr,
							"[Error] Command '%.*s' : %s\n",
							(int)input[idx].len,
							input[idx].str,
							*stag_ctx.user_data[cmd_idx].arg_missing_err_msg.str ?
							stag_ctx.user_data[cmd_idx].arg_missing_err_msg.str : "missing arg"

						    );
						exit(1);
					}
				}
				stag_cmd_array_append(cmd_current);
				break;
			}
		}
		++idx;
	}

	stag_cmd_array_append((struct stag_cmd_array){.cmd = NIL});
}



/*@RUN*/
void stag_run(int argc, char **argv)
{

	stag_ctx.scratch = stc_stack_gen(STAG_MAX_RESERVATION);
	stag_ctx.user_data = stag_table;
	stag_ctx.argc = argc;
	stag_ctx.argv = argv;
	stag_ctx.cmd_array = (struct stag_cmd_array*)stc_os_mem_rsrv(STAG_MAX_RESERVATION);
	int initial_size = STC_ALIGN_UP(sizeof(*stag_ctx.cmd_array), PAGESIZE);
	stag_ctx.cmtd = initial_size;
	stc_os_mem_cmt(stag_ctx.cmd_array, initial_size);
	stag_parse();
}




void stag_cmd_array_append(struct stag_cmd_array cmd)
{
	stag_u64 element_count_from_size = stag_ctx.cmtd / sizeof(*stag_ctx.cmd_array);
	if (stag_ctx.cmd_array_len >= element_count_from_size) {
		stag_u64 new_size = STC_ALIGN_UP(sizeof(*stag_ctx.cmd_array), PAGESIZE);
		if (stc_os_mem_cmt((stag_u8*)stag_ctx.cmd_array + stag_ctx.cmtd, new_size) == NULL) {
			perror("mprotect");
			fprintf(stderr, "[STAG_ERROR]Mprotect failed in file %s at line %d", __FILE__, __LINE__);
			exit(1);
		}
		stag_ctx.cmtd += new_size;
	}
	stag_ctx.cmd_array[stag_ctx.cmd_array_len++] = cmd;
}


struct stag_cmd_array stag_next_cmd(void)
{
	return stag_ctx.cmd_array[stag_ctx.cmd_array_dispatch_ct++];
}



struct stag_string stag_strip_arg_at_delim(struct stag_string string, char delim)
{
	struct stag_string pl = {.str = string.str, .len = string.len};

	for (stag_u64 i = 0; i < string.len; ++i) {
		if  (string.str[i] == delim) {
			pl.len = i;
			break;
		}
	}

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


struct stag_array_string{
	struct stag_string	*strings;
	stag_u64		len;
};


struct stag_array_float{
	stag_f64		*values;
	stag_u64		len;
};

struct stag_array_unsigned{
	stag_u64		*values;
	stag_u64		len;
};

struct stag_array_signed{
	stag_i64		*values;
	stag_u64		len;
};



struct stag_array_signed stag_string_to_array_of_signed(struct stag_string input, char delim)
{
	struct stag_array_signed pl = {0};

	pl.values = (stag_i64 *)stc_stack_push(stag_ctx.scratch, stag_i64, input.len + 1);
	stag_u64 idx = 0;
	stag_u64 inner_offset = 0;
	while (idx < input.len) {
		if (input.str[idx] == delim) {
			struct stag_string string;
			string.str = (input.str + idx) - inner_offset;
			string.len = inner_offset;
			pl.values[pl.len] = stag_string_to_i64(&string);
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
	pl.values[pl.len] = stag_string_to_i64(&string);
	pl.len++;


	return pl;
}


struct stag_array_unsigned stag_string_to_array_of_unsigned(struct stag_string input, char delim)
{
	struct stag_array_unsigned pl = {0};

	pl.values = (stag_u64 *)stc_stack_push(stag_ctx.scratch, stag_u64, input.len + 1);
	stag_u64 idx = 0;
	stag_u64 inner_offset = 0;
	while (idx < input.len) {
		if (input.str[idx] == delim) {
			struct stag_string string;
			string.str = (input.str + idx) - inner_offset;
			string.len = inner_offset;
			pl.values[pl.len] = stag_string_to_u64(&string);
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
	pl.values[pl.len] = stag_string_to_u64(&string);
	pl.len++;


	return pl;
}

struct stag_array_float stag_string_to_array_of_floats(struct stag_string input, char delim)
{
	struct stag_array_float pl = {0};

	pl.values = (stag_f64 *)stc_stack_push(stag_ctx.scratch, stag_f64, input.len + 1);
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


struct stag_array_string stag_string_to_array_of_strings(struct stag_string input, char delim)
{
	struct stag_array_string pl = {0};
	pl.strings = (struct stag_string*)stc_stack_push(stag_ctx.scratch, struct stag_string, input.len + 1);
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




stag_u64 ilt_get_real_idx(stag_u64 ml1_idx, stag_u64 ml1_current_full_groups, stag_u64 ml0_bit_offset)
{
	return ml1_idx * ILT_ML0_CT + ml1_current_full_groups * ILT_GROUPS_COUNT + ml0_bit_offset;
}


void ilt_set_ml1_group(struct stc_ilt *ilt, stag_u64 ml1_idx, stag_u64 ml1_group)
{
	ilt->ml1[ml1_idx] |= (1llu << ml1_group);

	if (ilt->ml1[ml1_idx] == ~0ULL) {
		ilt->ml2 |= (1llu << ml1_idx);
	}
}


void ilt_unset_ml1_group(struct stc_ilt *ilt, stag_u64 ml1_idx, stag_u64 ml1_group)
{
	ilt->ml1[ml1_idx] &= ~(1llu << ml1_group);

	if (ilt->ml1[ml1_idx] != ~0ULL) {
		ilt->ml2 &= ~(1llu << ml1_idx);
	}
}


stag_u64 ilt_get_ml0_idx(stag_u64 ml1_idx, stag_u64 ml1_current_full_groups)
{
	return ml1_idx * ILT_GROUPS_COUNT + ml1_current_full_groups;
}

void ilt_count_inc(struct stc_ilt *ilt)
{
	ilt->used_count++;
}

void ilt_count_dec(struct stc_ilt *ilt)
{
	ilt->used_count--;
}


struct ilt_ctx_frame ilt_get_ctx_frame(struct stc_ilt *ilt)
{
	struct ilt_ctx_frame pl;
	stag_u64 ml2_free_mask = ~ilt->ml2;
	if (!ml2_free_mask) {
		printf("ILT is full\nAborting\n");
		pl.ml0_bit = ~0ULL;
		pl.ml1_idx = ~0ULL;
		pl.ml0_idx = ~0ULL;
		pl.ml1_group = ~0ULL;
		return pl;
	}
	stag_u64 ml1_idx = __builtin_ctzll(ml2_free_mask);
	stag_u64 ml1_free_mask = ~ilt->ml1[ml1_idx];
	assert(ml1_free_mask != 0);
	stag_u64 group = __builtin_ctzll(ml1_free_mask);
	stag_u64 ml0_idx = ilt_get_ml0_idx(ml1_idx, group);
	stag_u64 ml0_free_mask = ~ilt->ml0[ml0_idx];
	assert(ml0_free_mask != 0);
	stag_u64 bit = __builtin_ctzll(ml0_free_mask);
	pl.ml1_idx = ml1_idx;
	pl.ml0_idx = ml0_idx;
	pl.ml0_bit = bit;
	pl.ml1_group = group;

	return pl;
}


stag_bool32 ilt_is_full(struct stc_ilt *ilt)
{
	return ilt_top_idx(ilt) == ~0ULL;
}

inline stag_bool32 ilt_is_empty(struct stc_ilt *ilt)
{
	return ilt->used_count == 0;
}


stag_u64 ilt_top_idx(struct stc_ilt *ilt)
{
	struct ilt_ctx_frame frame = ilt_get_ctx_frame(ilt);
	if (frame.ml1_idx == ~0ULL)
		return ~0ULL;

	return ilt_get_real_idx(frame.ml1_idx, frame.ml1_group, frame.ml0_bit);
}

stag_u64 ilt_next_idx(struct stc_ilt *ilt)
{
	stag_u64 ml2_free = ~ilt->ml2;
	if (unlikely(ml2_free == 0)) {
		printf("ILT is full\nAborting\n");
		exit(1);
	}

	while(ml2_free) {
		stag_u64 ml1_idx = __builtin_ctzll(ml2_free);
		ml2_free &= ml2_free - 1;
		stag_u64 ml1_free = ~ilt->ml1[ml1_idx];
		if (!ml1_free) continue;

		stag_u64 found = __builtin_ctzll(ml1_free);

		stag_u64 ml0_idx = ilt_get_ml0_idx(ml1_idx, found);
		stag_u64 ml0_free = ~ilt->ml0[ml0_idx];
		assert(ml0_free != 0);
		stag_u64 next_ml0_idx = __builtin_ctzll(ml0_free);
		ilt->ml0[ml0_idx] |= (1ULL << next_ml0_idx);
		if (ilt->ml0[ml0_idx] == ~0ULL) {
			ilt_set_ml1_group(ilt, ml1_idx, found);
		}
		return ilt_get_real_idx(ml1_idx, found, next_ml0_idx);
	}

	return ~0ULL;
}
