#include <stdio.h>
#include <string.h>
#include "kmonkey.h"
#include "llvm/LLVMContext.h"
#include "llvm/Linker.h"
#include "llvm/Constants.h"
#include "llvm/Instructions.h"
#include "llvm/Support/IRReader.h"
#include "llvm/Support/InstIterator.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/ToolOutputFile.h"

km_program_t* km_program_new(const char** files, int num_files)
{
	llvm::LLVMContext& context = llvm::getGlobalContext();
	km_program_t* program = (km_program_t*)malloc(sizeof(km_program_t) + sizeof(km_module_t*) * num_files);
	program->num_modules = num_files;
	int i;
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
		program->modules[i] = (km_module_t*)malloc(sizeof(km_module_t) + sizeof(km_inst_t) * num_insts + slen);
		program->modules[i]->module = module;
		program->modules[i]->num_insts = num_insts;
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
				insts[inst_no].uid = ((uint64_t)i << 32) | inst_no;
				insts[inst_no].weight = 0.0;
				insts[inst_no].inst = &*jit;
				++inst_no;
			}
		}
		std::string errInfo, file = program->modules[i]->file;
		file += ".backup";
		llvm::tool_output_file out(file.c_str(), errInfo, llvm::raw_fd_ostream::F_Binary);
		llvm::WriteBitcodeToFile(module, out.os());
		out.os().close();
		out.keep();
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
	llvm::Linker::LinkModules(program->modules[0]->module, km_mapping_module, llvm::Linker::DestroySource, &errInfo);
	llvm::Function* km_mapping_init = program->modules[0]->module->getFunction("km_mapping_init");
	llvm::Function* km_mapping_out = program->modules[0]->module->getFunction("km_mapping_out");
	llvm::Function* km_mapping_for = program->modules[0]->module->getFunction("km_mapping_for");
	llvm::Function* km_mapping_close = program->modules[0]->module->getFunction("km_mapping_close");
	llvm::GlobalVariable* llvm_test_result = 0;
	// find global variable of this
	int i;
	for (i = 0; i < program->num_modules; i++)
	{
		llvm_test_result = program->modules[i]->module->getGlobalVariable("__llvm_test_result__");
		if (llvm_test_result != 0)
			break;
	}
	llvm::Value* km_mapping_for_args[2];
	llvm::Value* add_to_km_mapping_p = llvm::ConstantInt::getSigned(llvm::IntegerType::get(context, 32), 1);
	unsigned int num_insts = 0, inst_no = 0;
	for (i = 0; i < program->num_modules; i++)
		num_insts += program->modules[i]->num_insts;
	for (i = 0; i < program->num_modules; i++)
	{
		llvm::Module* module = program->modules[i]->module;
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
							km_mapping_for_args[1] = add_to_km_mapping_p;
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
		llvm::tool_output_file out(program->modules[i]->file, errInfo, llvm::raw_fd_ostream::F_Binary);
		llvm::WriteBitcodeToFile(module, out.os());
		out.os().close();
		out.keep();
	}
}

void km_weight_insts(km_program_t* program)
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

typedef struct {
} km_mutate_rule_t;


static km_program_t* _km_mutate_module(km_program_t* program)
{
	return program;
}

void km_emit_replacement(km_program_t* program)
{
}

void km_program_free(km_program_t* program)
{
}
