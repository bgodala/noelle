/*
 * Copyright 2016 - 2019  Angelo Matni, Simone Campanoni
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. 
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#include "DataFlow.hpp"

using namespace llvm ;

DataFlowAnalysis::DataFlowAnalysis (){
  return ;
}

DataFlowResult * DataFlowAnalysis::runReachableAnalysis (Function *f){

  /*
   * Allocate the engine
   */
  auto dfa = DataFlowEngine{};

  /*
   * Define the data-flow equations
   */
  auto computeGEN = [](Instruction *i, DataFlowResult *df) {
    auto& gen = df->GEN(i);
    gen.insert(i);
    return ;
  };
  auto computeKILL = [](Instruction *, DataFlowResult *) {
    return ;
  };
  auto computeOUT = [](std::set<Value *>& OUT, Instruction *succ, DataFlowResult *df) {
    auto& inS = df->IN(succ);
    OUT.insert(inS.begin(), inS.end());
    return ;
  } ;
  auto computeIN = [](std::set<Value *>& IN, Instruction *inst, DataFlowResult *df) {
    auto& genI = df->GEN(inst);
    auto& outI = df->OUT(inst);

    /*
     * IN[i] = GEN[i] U OUT[i]
     */
    IN.insert(genI.begin(), genI.end());
    IN.insert(outI.begin(), outI.end());

    return ;
  };

  /*
   * Run the data flow analysis needed to identify the instructions that could be executed from a given point.
   */
  auto df = dfa.applyBackward(f, computeGEN, computeKILL, computeIN, computeOUT);

  return df;
}
