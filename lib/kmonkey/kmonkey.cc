#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <gsl/gsl_rng.h>
#include <gsl/gsl_randist.h>
#include <string.h>
#include "kmonkey.h"
#include "llvm/LLVMContext.h"
#include "llvm/Linker.h"
#include "llvm/Constants.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Instructions.h"
#include "llvm/Support/IRReader.h"
#include "llvm/Support/InstIterator.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/ToolOutputFile.h"

km_program_t* km_program_new(const char** files, int* skips, int num_files)
{
	llvm::LLVMContext& context = llvm::getGlobalContext();
	km_program_t* program = (km_program_t*)malloc(sizeof(km_program_t) + sizeof(km_module_t*) * num_files);
	program->num_insts = 0;
	program->num_modules = num_files;
	int i, j;
	for (i = 0; i < num_files; i++)
	{
		size_t slen = strlen(files[i]) + 1;
		if (slen > 1024) // something wrong with the file name
			continue;
		llvm::SMDiagnostic err;
		llvm::Module* module = ParseIRFile(files[i], err, context);
		int num_insts = 0;
		llvm::Module::iterator iit;
		for (iit = module->begin(); iit != module->end(); ++iit)
		{
			llvm::Function* func = &*iit;
			llvm::inst_iterator jit, je;
			for (jit = inst_begin(func), je = inst_end(func); jit != je; ++jit)
				++num_insts;
		}
		program->num_insts += num_insts;
		program->modules[i] = (km_module_t*)malloc(sizeof(km_module_t) + sizeof(km_inst_t) * num_insts + slen);
		program->modules[i]->module = module;
		program->modules[i]->num_insts = num_insts;
		program->modules[i]->skip = skips[i];
		program->modules[i]->file = (char*)(program->modules[i]->insts + num_insts);
		strncpy(program->modules[i]->file, files[i], slen);
		km_inst_t* insts = program->modules[i]->insts;
		// add inst to module
		int inst_no = 0;
		for (iit = module->begin(); iit != module->end(); ++iit)
		{
			llvm::Function* func = &*iit;
			llvm::inst_iterator jit, je;
			for (jit = inst_begin(func), je = inst_end(func); jit != je; ++jit)
			{
				insts[inst_no].weight = 0.0;
				insts[inst_no].inst = &*jit;
				insts[inst_no].module = program->modules[i];
				++inst_no;
			}
		}
		std::string errInfo, file = program->modules[i]->file;
		file += ".kmb";
		llvm::tool_output_file out(file.c_str(), errInfo, llvm::raw_fd_ostream::F_Binary);
		llvm::WriteBitcodeToFile(module, out.os());
		out.os().close();
		out.keep();
	}
	int inst_no = 0;
	program->insts = (km_inst_t**)malloc(sizeof(km_inst_t*) * program->num_insts);
	for (i = 0; i < program->num_modules; i++)
	{
		for (j = 0; j < program->modules[i]->num_insts; j++)
		{
			program->insts[inst_no] = program->modules[i]->insts + j;
			program->insts[inst_no]->uid = inst_no;
			++inst_no;
		}
	}
	return program;
}

void km_emit_coverage_reporter(km_program_t* program)
{
	llvm::LLVMContext& context = llvm::getGlobalContext();
	llvm::SMDiagnostic err;
	// I will move this to somewhere else, but for now, it is in ext directory and is a text asm file
	llvm::Module* km_mapping_module = ParseIRFile("ext/km_mapping.ll", err, context);
	std::string errInfo;
	int i;
	// clone modules for emitting
	km_module_t* modules = (km_module_t*)malloc(sizeof(km_module_t) * program->num_modules);
	for (i = 0; i < program->num_modules; i++)
	{
		modules[i] = *(program->modules[i]);
		modules[i].module = llvm::CloneModule(modules[i].module);
	}
	llvm::Linker::LinkModules(modules[0].module, km_mapping_module, llvm::Linker::DestroySource, &errInfo);
	llvm::Function* km_mapping_init = modules[0].module->getFunction("km_mapping_init");
	llvm::Function* km_mapping_out = modules[0].module->getFunction("km_mapping_out");
	llvm::Function* km_mapping_for = modules[0].module->getFunction("km_mapping_for");
	llvm::Function* km_mapping_close = modules[0].module->getFunction("km_mapping_close");
	llvm::GlobalVariable* llvm_test_result = 0;
	// find global variable of this
	for (i = 0; i < program->num_modules; i++)
	{
		llvm_test_result = modules[i].module->getGlobalVariable("__llvm_test_result__");
		if (llvm_test_result != 0)
			break;
	}
	llvm::Value* km_mapping_for_args[2];
	llvm::Value* add_to_km_mapping_n = llvm::ConstantInt::getSigned(llvm::IntegerType::get(context, 32), -1);
	int num_insts = 0, inst_no = 0;
	for (i = 0; i < program->num_modules; i++)
		num_insts += modules[i].num_insts;
	for (i = 0; i < program->num_modules; i++)
	{
		llvm::Module* module = modules[i].module;
		llvm::Module::iterator iit;
		for (iit = module->begin(); iit != module->end(); ++iit)
		{
			llvm::Function* func = &*iit;
			if (func != km_mapping_init && func != km_mapping_out && func != km_mapping_for && func != km_mapping_close)
			{
				llvm::inst_iterator jit, je;
				for (jit = inst_begin(func), je = inst_end(func); jit != je; ++jit)
				{
					llvm::Instruction* inst = &*jit;
					if (!llvm::PHINode::classof(inst))
					{
						if (llvm_test_result == 0)
						{
							km_mapping_for_args[0] = llvm::ConstantInt::get(llvm::IntegerType::get(context, 32), inst_no);
							km_mapping_for_args[1] = add_to_km_mapping_n;
							llvm::CallInst* km_mapping_for_call = llvm::CallInst::Create(km_mapping_for, km_mapping_for_args);
							km_mapping_for_call->insertBefore(inst);
						} else {
							llvm::LoadInst* km_mapping_load_gv = new llvm::LoadInst(llvm_test_result);
							km_mapping_for_args[0] = llvm::ConstantInt::get(llvm::IntegerType::get(context, 32), inst_no);
							km_mapping_for_args[1] = km_mapping_load_gv;
							llvm::CallInst* km_mapping_for_call = llvm::CallInst::Create(km_mapping_for, km_mapping_for_args);
							km_mapping_for_call->insertBefore(inst);
							km_mapping_load_gv->insertBefore(km_mapping_for_call);
						}
					}
					++inst_no;
				}
			}
		}
		llvm::Function *entry = module->getFunction("main");
		if (entry != 0)
		{
			llvm::Value* km_mapping_init_args[1];
			km_mapping_init_args[0] = llvm::ConstantInt::get(llvm::IntegerType::get(context, 32), num_insts);
			llvm::CallInst* km_mapping_init_call = llvm::CallInst::Create(km_mapping_init, km_mapping_init_args);
			km_mapping_init_call->insertBefore(entry->begin()->begin());
			llvm::CallInst* km_mapping_close_call = llvm::CallInst::Create(km_mapping_close);
			llvm::Instruction* entry_end = 0;
			llvm::inst_iterator eit, ee;
			for (eit = inst_begin(entry), ee = inst_end(entry); eit != ee; ++eit)
			{
				entry_end = &*eit;
				if (llvm::ReturnInst::classof(entry_end))
					km_mapping_close_call->clone()->insertBefore(entry_end);
			}
			if (entry_end != 0 && !llvm::ReturnInst::classof(entry_end))
					km_mapping_close_call->insertAfter(entry_end);
		}
		llvm::tool_output_file out(modules[i].file, errInfo, llvm::raw_fd_ostream::F_Binary);
		llvm::WriteBitcodeToFile(module, out.os());
		out.os().close();
		out.keep();
	}
	free(modules);
}

static void _km_weight_insts(km_program_t* program)
{
	FILE* out = fopen("/tmp/km.out", "r");
	if (out == 0)
		return;
	int i, j;
	int num_insts = 0;
	for (i = 0; i < program->num_modules; i++)
		num_insts += program->modules[i]->num_insts;
	fscanf(out, "%d", &i);
	if (i != num_insts)
		return;
	double* km_mapping_pf = (double*)malloc(sizeof(double) * num_insts);
	unsigned int* km_mapping_pu = (unsigned int*)malloc(sizeof(unsigned int) * num_insts);
	double* km_mapping_nf = (double*)malloc(sizeof(double) * num_insts);
	unsigned int* km_mapping_nu = (unsigned int*)malloc(sizeof(unsigned int) * num_insts);
	double km_mapping_pt = 0;
	double km_mapping_nt = 0;
	int pnum_insts = 0, nnum_insts = 0;
	for (i = 0; i < num_insts; i++)
	{
		unsigned int km_mapping_tu;
		fscanf(out, "%u %u %u", &km_mapping_pu[i], &km_mapping_nu[i], &km_mapping_tu);
		km_mapping_nu[i] += km_mapping_tu;
		km_mapping_pt += km_mapping_pu[i];
		km_mapping_nt += km_mapping_nu[i];
		if (km_mapping_pu[i] > 0)
			++pnum_insts;
		if (km_mapping_nu[i] > 0)
			++nnum_insts;
	}
	fclose(out);
	remove("/tmp/km.out");
	// normalize weight
	km_mapping_pt = (double)pnum_insts / km_mapping_pt;
	km_mapping_nt = (double)nnum_insts / km_mapping_nt;
	for (i = 0; i < num_insts; i++)
	{
		if (km_mapping_pu[i] > 0)
		{
			km_mapping_pf[i] = (double)km_mapping_pu[i] * km_mapping_pt;
			if (km_mapping_pf[i] < 0.2)
				km_mapping_pf[i] = 0.2;
			if (km_mapping_pf[i] > 5.0)
				km_mapping_pf[i] = 5.0;
		} else {
			km_mapping_pf[i] = 0;
		}
		if (km_mapping_nu[i] > 0)
		{
			km_mapping_nf[i] = (double)km_mapping_nu[i] * km_mapping_nt;
			if (km_mapping_nf[i] < 0.2)
				km_mapping_nf[i] = 0.2;
			if (km_mapping_nf[i] > 5.0)
				km_mapping_nf[i] = 5.0;
		} else {
			km_mapping_nf[i] = 0;
		}
	}
	for (i = 0; i < num_insts; i++)
	{
		km_mapping_pt += km_mapping_pf[i];
		km_mapping_nt += km_mapping_nf[i];
	}
	km_mapping_pt = 1.0 / km_mapping_pt;
	km_mapping_nt = 1.0 / km_mapping_nt;
	for (i = 0; i < num_insts; i++)
	{
		if (km_mapping_pu[i] > 0)
			km_mapping_pf[i] = (double)km_mapping_pf[i] * km_mapping_pt;
		if (km_mapping_nu[i] > 0)
			km_mapping_nf[i] = (double)km_mapping_nf[i] * km_mapping_nt;
	}
	unsigned int inst_no = 0;
	for (i = 0; i < program->num_modules; i++)
	{
		km_module_t* module = program->modules[i];
		for (j = 0; j < module->num_insts; j++)
		{
			// bayesian theorem P(A | B) = P(B | A) * P(A) / P(B)
			if (km_mapping_nu[inst_no] > 0 || km_mapping_pu[inst_no] > 0)
				module->insts[j].weight = km_mapping_nf[inst_no] / (km_mapping_nf[inst_no] + km_mapping_pf[inst_no]);
			else
				module->insts[j].weight = 0;
			++inst_no;
		}
	}
	free(km_mapping_pu);
	free(km_mapping_nu);
	free(km_mapping_pf);
	free(km_mapping_nf);
}

static km_program_t* _km_copy_program(km_program_t* _program)
{
	km_program_t* program = (km_program_t*)malloc(sizeof(km_program_t) + sizeof(km_module_t*) * _program->num_modules);
	program->num_modules = _program->num_modules;
	program->num_insts = _program->num_insts;
	int i, j;
	for (i = 0; i < program->num_modules; i++)
	{
		size_t slen = strlen(_program->modules[i]->file) + 1;
		if (slen > 1024) // something wrong with the file name
			continue;
		int num_insts = _program->modules[i]->num_insts;
		llvm::Module* module = llvm::CloneModule(_program->modules[i]->module);
		program->modules[i] = (km_module_t*)malloc(sizeof(km_module_t) + sizeof(km_inst_t) * num_insts + slen);
		program->modules[i]->module = module;
		program->modules[i]->num_insts = num_insts;
		program->modules[i]->skip = _program->modules[i]->skip;
		program->modules[i]->file = (char*)(program->modules[i]->insts + num_insts);
		strncpy(program->modules[i]->file, _program->modules[i]->file, slen);
		km_inst_t* insts = program->modules[i]->insts;
		// add inst to module
		int inst_no = 0;
		llvm::Module::iterator iit;
		for (iit = module->begin(); iit != module->end(); ++iit)
		{
			llvm::Function* func = &*iit;
			llvm::inst_iterator jit, je;
			for (jit = inst_begin(func), je = inst_end(func); jit != je; ++jit)
			{
				insts[inst_no].weight = 0.0;
				insts[inst_no].inst = &*jit;
				insts[inst_no].module = program->modules[i];
				++inst_no;
			}
		}
	}
	int inst_no = 0;
	program->insts = (km_inst_t**)malloc(sizeof(km_inst_t*) * program->num_insts);
	for (i = 0; i < program->num_modules; i++)
	{
		for (j = 0; j < program->modules[i]->num_insts; j++)
		{
			program->insts[inst_no] = program->modules[i]->insts + j;
			program->insts[inst_no]->uid = inst_no;
			++inst_no;
		}
	}
	return program;
}

enum {
	KM_MUTATE_ADD,
	KM_MUTATE_REMOVE,
	KM_MUTATE_REPLACE,
};

typedef struct {
	int op;
	int from;
	int* operands;
	int target;
	int to;
} km_mutate_rule_t;

static void _km_mutate_program(km_program_t* program, km_mutate_rule_t* rule)
{
	switch (rule->op)
	{
		case KM_MUTATE_ADD:
		{
			llvm::Instruction* new_inst = program->insts[rule->from]->inst->clone();
			llvm::Instruction* insertPos = program->insts[rule->to]->inst;
			new_inst->insertBefore(insertPos);
			break;
		}
		case KM_MUTATE_REMOVE:
		{
			llvm::Instruction* inst = program->insts[rule->from]->inst;
			llvm::Instruction* deps = program->insts[rule->to]->inst;
			inst->replaceAllUsesWith(deps);
			inst->removeFromParent();
			break;
		}
		case KM_MUTATE_REPLACE:
		{
			llvm::Instruction* new_inst = program->insts[rule->from]->inst->clone();
			llvm::Instruction* old_inst = program->insts[rule->to]->inst;
			new_inst->insertBefore(old_inst);
			old_inst->removeFromParent();
			break;
		}
	}
}

static void _km_wait_timeout(int pid, int64_t milliseconds)
{
	setpgid(pid, pid);
	while (milliseconds > 0)
	{
		usleep(10000);
		int stat;
		if (waitpid(pid, &stat, WNOHANG) == pid)
			if (WIFEXITED(stat) || WIFSIGNALED(stat))
				break;
		milliseconds -= 10;
	}
	if (milliseconds <= 0)
	{
		killpg(pid, SIGTERM);
		int stat;
		waitpid(pid, &stat, 0);
	}
}

static double _km_run_test_with_timeout(const char* test_command, int64_t timeout)
{
	int pid = fork();
	if (pid == 0)
	{
		execl("/bin/sh", "/bin/sh", "-c", test_command, 0);
		exit(0);
	} else if (pid > 0) {
		_km_wait_timeout(pid, timeout);
	}
	FILE* out = fopen("/tmp/case.out", "r");
	if (out == 0)
		return 0;
	int pass, total;
	fscanf(out, "%d %d", &pass, &total);
	fclose(out);
	remove("/tmp/case.out");
	return total > 0 ? (double)pass / (double)total : 0;
}

typedef struct {
	double fitness;
	double pass;
	int age;
	int num_rules;
	km_mutate_rule_t rules[20];
} km_gene_t;

static int _km_add_rule_new(int from, km_mutate_rule_t* rule)
{
}

static int _km_remove_rule_new(int from, km_mutate_rule_t* rule)
{
	rule->op = KM_MUTATE_REMOVE;
}

static int _km_replace_rule_new(int from, int to, km_mutate_rule_t* rule)
{
}

static void _km_randomize_gene(gsl_rng* rng, km_program_t* program, km_gene_t* gene)
{
	gene->fitness = 0;
	gene->num_rules = 0;
	int i;
	double s = 0;
	for (i = 0; i < program->num_insts; i++)
		s += program->insts[i]->weight;
	for (i = 0; i < program->num_insts; i++)
	{
		if (gsl_rng_uniform(rng) * s < program->insts[i]->weight)
		{
			int y = gsl_rng_uniform_int(rng, 3);
			switch (y)
			{
				case 0:
				{
					gene->rules[gene->num_rules].op = KM_MUTATE_REMOVE;
					gene->rules[gene->num_rules].from = i;
					++gene->num_rules;
					break;
				}
				case 1:
				{
					break;
				}
				case 2:
				{
					break;
				}
			}
			if (gene->num_rules >= 20)
				break;
		}
	}
}

static void _km_genetic_pass_rate(km_program_t* _program, const char* test_command, km_gene_t* gene)
{
	km_program_t* program = _km_copy_program(_program);
	int i;
	for (i = 0; i < gene->num_rules; i++)
		_km_mutate_program(program, &gene->rules[i]);
	gene->pass = _km_run_test_with_timeout(test_command, 5000);
	km_program_free(program);
}

static void _km_genetic_fitness(km_gene_t* gene)
{
	gene->fitness = gene->pass * exp(-0.01 * gene->age) * exp(gene->num_rules * log(1.015));
}

#define greater_than(fit1, fit2, aux) ((fit1).fitness >= (fit2).fitness)
static KM_IMPLEMENT_QSORT(_km_genetic_qsort, km_gene_t, greater_than)
#undef greater_than

void km_program_genetic_optimize(km_program_t* program, const char* compile_command, const char* test_command, int size, int generation)
{
	double result;
	km_emit_coverage_reporter(program);
	system(compile_command);
	result = _km_run_test_with_timeout(test_command, 5000);
	_km_weight_insts(program);
	if (result >= 1.0 - 1e-6)
		return;
	gsl_rng_env_setup();
	gsl_rng* rng = gsl_rng_alloc(gsl_rng_default);
	km_gene_t* genes = (km_gene_t*)malloc(sizeof(km_gene_t) * size * 10);
	km_gene_t best;
	gsl_rng_set(rng, (unsigned long int)(genes));
	int i, j, k;
	for (i = 0; i < size * 10; i++)
		_km_randomize_gene(rng, program, &genes[i]);
	best.pass = result;
	best.num_rules = 0;
	for (i = 0; i < size * 10; i++)
	{
		_km_genetic_pass_rate(program, test_command, &genes[i]);
		_km_genetic_fitness(&genes[i]);
		if (genes[i].pass > best.pass)
			best = genes[i];
	}
	_km_genetic_qsort(genes, size * 10, 0);
	for (k = 0; k < generation; k++)
	{
		// crossover
		for (i = size * 3;  i < size * 6; i++)
		{
			int dad, mum;
			do {
				dad = gsl_rng_uniform_int(rng, size * 3);
				mum = gsl_rng_uniform_int(rng, size * 3);
			} while (dad == mum);
		}
		// mutation
		for (i = size; i < size * 3; i++)
		{
			int me = gsl_rng_uniform_int(rng, size);
			genes[i] = genes[me];
			int decay = 1;
			do {
				switch (gsl_rng_uniform_int(rng, 3))
				{
					case 0: // remove
						if (genes[i].num_rules > 1)
						{
							int victim = gsl_rng_uniform_int(rng, genes[i].num_rules);
							for (j = victim; j < genes[i].num_rules - 1; j++)
								genes[i].rules[j] = genes[i].rules[j + 1];
							--genes[i].num_rules;
						}
						decay = 0;
						break;
					case 1: // add
						if (genes[i].num_rules < 20)
						{
						}
						decay = 0;
						break;
					case 2: // replace
						decay = 0;
						break;
				}
			} while (decay);
		}
		// preserve
		for (i = 0; i < size; i++)
			++genes[i].age;
		for (i = 0; i < size; i++)
			_km_genetic_fitness(&genes[i]);
		for (i = size; i < size * 10; i++)
		{
			_km_genetic_pass_rate(program, test_command, &genes[i]);
			_km_genetic_fitness(&genes[i]);
			if (genes[i].pass > best.pass)
				best = genes[i];
		}
	}
	printf("%lf\n", best.pass);
	/*
	for (k = 0; k < generation; k++)
	{
		_km_genetic_qsort(genes, size * 10, 0);
	}
	*/
	free(genes);
	gsl_rng_free(rng);
}

void km_emit_program(km_program_t* program)
{
	int i;
	for (i = 0; i < program->num_modules; i++)
	{
		std::string errInfo;
		llvm::tool_output_file out(program->modules[i]->file, errInfo, llvm::raw_fd_ostream::F_Binary);
		llvm::WriteBitcodeToFile(program->modules[i]->module, out.os());
		out.os().close();
		out.keep();
	}
}

void km_program_free(km_program_t* program)
{
	int i;
	for (i = 0; i < program->num_modules; i++)
	{
		delete program->modules[i]->module;
		free(program->modules[i]);
	}
	free(program->insts);
	free(program);
}
