
#include "klang/AST/ASTNodes.h"
#include "klang/Builtin/Tutorial.h"
#include "klang/Lex/Lexer.h"
#include "klang/Parse/Parser.h"
#include "llvm/Analysis/Passes.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/ExecutionEngine/JIT.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/PassManager.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Transforms/Scalar.h"
#include <cstdio>
#include <map>


//===----------------------------------------------------------------------===//
// Main driver code.
//===----------------------------------------------------------------------===//

namespace klang {

  std::map<char, int> Token::BinopPrecedence;

  llvm::Module *TheModule;
  llvm::IRBuilder<> Builder(llvm::getGlobalContext());
  std::map<std::string, llvm::AllocaInst*> NamedValues;

  llvm::FunctionPassManager *TheFPM;
  llvm::ExecutionEngine *TheExecutionEngine;

}


int main() {

  llvm::InitializeNativeTarget();
  llvm::LLVMContext &Context = llvm::getGlobalContext();

  klang::Lexer myLexer;
  klang::Parser myParser(myLexer);

  // Install standard binary operators.
  // 1 is lowest precedence.
  klang::Token::BinopPrecedence['='] = 2;
  klang::Token::BinopPrecedence['<'] = 10;
  klang::Token::BinopPrecedence['+'] = 20;
  klang::Token::BinopPrecedence['-'] = 20;
  klang::Token::BinopPrecedence['*'] = 40;  // highest.

  // Make the module, which holds all the code.
  klang::TheModule = new llvm::Module("my cool jit", Context);

  //-----------------------------------------------------
  // Create the JIT.  This takes ownership of the module.
  std::string ErrStr;
  klang::TheExecutionEngine =
    llvm::EngineBuilder(klang::TheModule).setErrorStr(&ErrStr).create();
  if (!klang::TheExecutionEngine) {
    fprintf(stderr, "Could not create ExecutionEngine: %s\n", ErrStr.c_str());
    exit(1);
  }

  llvm::FunctionPassManager OurFPM(klang::TheModule);

  // Set up the optimizer pipeline.  Start with registering info about how the
  // target lays out data structures.
  OurFPM.add(new llvm::DataLayout(*klang::TheExecutionEngine->getDataLayout()));
  // Provide basic AliasAnalysis support for GVN.
  OurFPM.add(llvm::createBasicAliasAnalysisPass());
  // Promote allocas to registers.
  OurFPM.add(llvm::createPromoteMemoryToRegisterPass());
  // Do simple "peephole" optimizations and bit-twiddling optzns.
  OurFPM.add(llvm::createInstructionCombiningPass());
  // Reassociate expressions.
  OurFPM.add(llvm::createReassociatePass());
  // Eliminate Common SubExpressions.
  OurFPM.add(llvm::createGVNPass());
  // Simplify the control flow graph (deleting unreachable blocks, etc).
  OurFPM.add(llvm::createCFGSimplificationPass());

  OurFPM.doInitialization();

  // Set the global so the code gen can use this.
  klang::TheFPM = &OurFPM;
  //-----------------------------------------------------

  // Run the main "interpreter loop" now.
  myParser.Go();

  klang::TheFPM = 0;

  // Print out all of the generated code.
  klang::TheModule->dump();

	// Calls an unused function just not to lose it in the final binary
	// Without this call klangBuiltin.a is just ignored during linking
	putchard(100);

  return 0;
}

