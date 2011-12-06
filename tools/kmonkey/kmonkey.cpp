#include "kmonkey.h"
#include "llvm/Support/CommandLine.h"

llvm::cl::list<std::string> files(llvm::cl::Positional, llvm::cl::desc("LLVM bitcode files that may contain bugs"), llvm::cl::OneOrMore);
llvm::cl::opt<std::string> mainfile("mainfile", llvm::cl::desc("Specify LLVM bitcode file that contains the main function"), llvm::cl::value_desc("mainfile"), llvm::cl::Required);
llvm::cl::opt<std::string> makeBitcode("i", llvm::cl::desc("Specify command that generates LLVM bitcode"), llvm::cl::value_desc("make bitcode"), llvm::cl::Required);
llvm::cl::opt<std::string> makeBinary("c", llvm::cl::desc("Specify command that compiles LLVM bitcode into binaries"), llvm::cl::value_desc("make binary"), llvm::cl::Required);
llvm::cl::opt<std::string> test("t", llvm::cl::desc("Specify command that tests the program"), llvm::cl::value_desc("make test"), llvm::cl::Required);

int main (int argc, char** argv)
{
	llvm::cl::ParseCommandLineOptions(argc, argv);
	const char** list = (const char**)malloc((1 + files.size()) * sizeof(char*));
	int* skips = (int*)malloc((1 + files.size()) * sizeof(int));
	int num_files = files.size() + 1;
	list[0] = mainfile.c_str();
	skips[0] = 1;
	unsigned int i;
	for (i = 0; i < files.size(); i++)
	{
		if (files[i] == mainfile)
		{
			skips[0] = 0;
			--num_files;
		} else {
			list[i + 1] = files[i].c_str();
			skips[i + 1] = 0;
		}
	}
	system(makeBitcode.c_str());
	km_program_t* program = km_program_new(list, skips, num_files);
	km_program_genetic_optimize(program, makeBinary.c_str(), test.c_str(), 50, 10);
	km_program_free(program);
	free(list);
	free(skips);
	/*
	tool_output_file out("report.bc", errInfo, raw_fd_ostream::F_Binary);
	WriteBitcodeToFile(module, out.os());
	out.os().close();
	out.keep();
	*/
	return 0;
}

