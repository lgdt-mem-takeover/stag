#include "/data/2026-projs/c/stormlibc/build/scbuild.h"





int main(int argc, char **argv)
{
	enum compilers compiler = C_GCC;
	if (argc > 1) {
		struct stc_string8 arg = STR_RUNTIME(argv[1]);
		printf("[%s]\n", arg.str);
		printf("[%lu]\n", arg.len);
		if (stc_string8_cmp(arg, STR("mingw"))) compiler = C_MINGW64;
		else if (stc_string8_cmp(arg, STR("clang"))) compiler = C_CLANG;
		else if (stc_string8_cmp(arg, STR("mingw32"))) compiler = C_MINGW32;
		else if (stc_string8_cmp(arg, STR("mingwpp"))) compiler = C_MINGW64PP;
		else if (stc_string8_cmp(arg, STR("gcc"))) compiler = C_GCC;
		else if (stc_string8_cmp(arg, STR("zig"))) compiler = C_ZIGCC;
		else if (stc_string8_cmp(arg, STR("clangpp"))) compiler = C_CLANGPP;
		else if (stc_string8_cmp(arg, STR("gccgpp"))) compiler = C_GCCPP;
	}

	proj.compiler = compiler;
	set_sources(STR("src/main.c"));
	set_flags(STD_C99, OPTIM_1, ALLSAN);
	set_out_file(STR("src/main"));

	bool linux_build = (compiler != C_MINGW64) && (compiler != C_MINGW32);
	build_proj();
	return 0;
}

