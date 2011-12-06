#ifndef GUARD_kmonkey_h
#define GUARD_kmonkey_h

#include <stdlib.h>
#include "llvm/Module.h"
#include "llvm/Instruction.h"

typedef struct {
	uint64_t uid;
	double weight;
	llvm::Instruction* inst;
} km_inst_t;

typedef struct {
	llvm::Module* module;
	int num_insts;
	char* file;
	km_inst_t insts[0];
} km_module_t;

typedef struct {
	int num_modules;
	km_module_t* modules[0];
} km_program_t;

km_program_t* km_program_new(const char** files, int num_files);
void km_emit_coverage_reporter(km_program_t* program);
void km_weight_insts(km_program_t* program);
void km_emit_replacement(km_program_t* program);
void km_program_free(km_program_t* program);

#endif
