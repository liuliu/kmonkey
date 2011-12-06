#include "kmonkey.h"

int main (int argc, char** argv)
{
	km_program_t* program = km_program_new((const char**)&argv[1], 1);
	km_weight_insts(program);
	// km_emit_coverage_reporter(program);
	km_program_free(program);
	/*
	tool_output_file out("report.bc", errInfo, raw_fd_ostream::F_Binary);
	WriteBitcodeToFile(module, out.os());
	out.os().close();
	out.keep();
	*/
	return 0;
}

