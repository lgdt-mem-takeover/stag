#include <stdio.h>
#include <string.h>
#include <stdint.h>

typedef uint64_t u64;
typedef struct { char *str; u64 len; } sstr;

static sstr strip_upto_delim(sstr s, char delim) {
	sstr out = {0};
	char *base = s.str;
	while (*s.str != delim && *s.str != '\0') s.str++;
	out.str = base;
	out.len = (u64)(s.str - base);
	return out;
}

int main(void) {
	char tok[] = "--dry-run"; // no '='
	sstr input = { tok, strlen(tok) };

	sstr cmd = strip_upto_delim(input, '=');
	char *args = input.str + cmd.len + 1; // one past '\0' when '=' missing
	size_t n = strlen(args);               // OOB read / UB

	printf("n=%zu\n", n);
	return 0;
}
