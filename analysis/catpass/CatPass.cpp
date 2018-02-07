#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Analysis/AliasAnalysis.h"

#include "../include/PDGAnalysis.hpp"

using namespace llvm;

bool llvm::PDGAnalysis::doInitialization (Module &M){
  errs() << "PDGAnalysis at \"doInitialization\"\n" ;
  return false;
}

void llvm::PDGAnalysis::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequired<AAResultsWrapperPass>();
  AU.setPreservesAll();
  return ;
}

bool llvm::PDGAnalysis::runOnModule (Module &M){
  errs() << "PDGAnalysis at \"runOnModule\"\n" ;
  this->programDependenceGraph = std::unique_ptr<PDG>(new PDG());
  for (auto &F : M) {
    if (F.empty()) continue ;
    this->aaResults[&F] = &(getAnalysis<AAResultsWrapperPass>(F).getAAResults());
  }

  this->programDependenceGraph->constructNodes(M);
  constructEdgesFromUseDefs(M);
  constructEdgesFromAliases(M);

  return false;
}

llvm::PDGAnalysis::PDGAnalysis() : ModulePass{ID}{
  return ;
}

llvm::PDG & llvm::PDGAnalysis::getPDG (){
  return *this->programDependenceGraph;
}

// Next there is code to register your pass to "opt"
char llvm::PDGAnalysis::ID = 0;
static RegisterPass<PDGAnalysis> X("PDGAnalysis", "Computing the Program Dependence Graph");

// Next there is code to register your pass to "clang"
static PDGAnalysis * _PassMaker = NULL;
static RegisterStandardPasses _RegPass1(PassManagerBuilder::EP_OptimizerLast,
    [](const PassManagerBuilder&, legacy::PassManagerBase& PM) {
        if(!_PassMaker){ PM.add(_PassMaker = new PDGAnalysis());}}); // ** for -Ox
static RegisterStandardPasses _RegPass2(PassManagerBuilder::EP_EnabledOnOptLevel0,
    [](const PassManagerBuilder&, legacy::PassManagerBase& PM) {
        if(!_PassMaker){ PM.add(_PassMaker = new PDGAnalysis());}});// ** for -O0

void llvm::PDGAnalysis::constructEdgesFromUseDefs (Module &M){
  for (auto iNodePair : programDependenceGraph->instructionNodePairs()) {
    Instruction *I = iNodePair.first;
    if (I->getNumUses() == 0)
      continue;
    for (auto& U : I->uses()) {
      auto user = U.getUser();
      if (auto userInst = dyn_cast<Instruction>(user)) {
        programDependenceGraph->addEdgeFromTo(I, userInst);
      }
    }
  }

  return ;
}

void llvm::PDGAnalysis::addEdgeFromMemoryAlias (Function &F, Instruction &memI, Instruction &memJ){
  switch (aaResults[&F]->alias(MemoryLocation::get(&memI), MemoryLocation::get(&memJ))) {
    case PartialAlias:
    case MayAlias:
    case MustAlias:
      programDependenceGraph->addEdgeFromTo(&memI, &memJ);
      break;
  }
}

void llvm::PDGAnalysis::addEdgeFromFunctionModRef (Function &F, Instruction &memI, CallInst &call){
  switch (aaResults[&F]->getModRefInfo(&call, MemoryLocation::get(&memI))) {
    case MRI_Ref:
    case MRI_Mod:
    case MRI_ModRef:
      programDependenceGraph->addEdgeFromTo(&memI, &call);
      break;
  }
}

template <class iType>
void llvm::PDGAnalysis::iterateInstForAliases(Function &F, Instruction &J) {
  for (auto &B : F) {
    for (auto &I : B) {
      if (dyn_cast<iType>(&I)) {
        addEdgeFromMemoryAlias(F, I, J);
      }
    }
  }
}

void llvm::PDGAnalysis::iterateInstForModRef(Function &F, CallInst &J) {
  for (auto &B : F) {
    for (auto &I : B) {
      if (auto *load = dyn_cast<LoadInst>(&I)) {
        addEdgeFromFunctionModRef(F, I, J);
      } else if (auto *store = dyn_cast<StoreInst>(&I)) {
        addEdgeFromFunctionModRef(F, I, J);
      }
    }
  }
}

void llvm::PDGAnalysis::constructEdgesFromAliases (Module &M){
  /*
   * Use alias analysis on stores, loads, and function calls to construct PDG edges
   */
  for (auto &F : M) {
    for (auto &B : F) {
      for (auto &I : B) {
        if (auto* store = dyn_cast<StoreInst>(&I)) {
          iterateInstForAliases<LoadInst>(F, I);
        } else if (auto *load = dyn_cast<LoadInst>(&I)) {
          iterateInstForAliases<StoreInst>(F, I);
        } else if (auto *call = dyn_cast<CallInst>(&I)) {
          iterateInstForModRef(F, *call);
        }
      }
    }
  }
}
  