#pragma once
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/PassInstrumentation.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Passes/StandardInstrumentations.h>
#include <llvm/Transforms/Utils.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Transforms/InstCombine/InstCombine.h>
#include <llvm/Transforms/Scalar/Reassociate.h>
#include <llvm/Transforms/Scalar/GVN.h>
#include <llvm/Transforms/Scalar.h>
#include <llvm/Analysis/Passes.h>
#include <llvm/IR/PassManager.h>
#include <llvm/IR/Value.h>
#include <memory>
#include <llvm/Analysis/LoopAnalysisManager.h>
#include <llvm/Analysis/CGSCCPassManager.h>
#include <llvm/IR/LegacyPassManager.h>
#include "ast/basic_ast.h"
#include "jit/HoshinoJIT.h"

// 一个不透明的对象 包含许多llvm数据 不必管它 (
inline std::unique_ptr<llvm::LLVMContext> theContext;
// 用于生成llvm指令的一个全局builder
inline std::unique_ptr<llvm::IRBuilder<>> builder;

// module是llvm-ir用于包含代码的顶级结构 它拥有生成的所有ir的内存
// 因为module的某些操作原因 所以code-generator要返回Value*而不是std::unique_ptr<Value>
inline std::unique_ptr<llvm::Module>theModule;
// 包含当前作用范围内的变量的llvm表示
inline std::map<std::string, llvm::AllocaInst*>namedValues;
/*
    全局的函数注册表
    顶层表达式执行时会将当前module交给匿名函数使用 外层会新生成一个module
    且匿名函数执行完后会丢弃其module 导致之前注册的函数找不到
    因此需要一个全局的函数注册表保存以前注册的函数信息
    =================================================================
    *注意：对于map的key 不能用string_view 视图只保存底层字符串的指针和长度
           而底层字符串的数据随时有可能会被释放 考虑以下情况：
           functionProtos[ast->proto_->GetFuncName()] = std::move(ast->proto_);
           如果key是string_view，那么functionProtos中保存的为指向proto中funcName的
           视图，而如果有另一个proto的funcName与该保存的proto的funcName相同(比如顶层表达式的匿名函数)
           那么再次执行以上的语句时，先前保存的proto就被释放了，这时string_view的指针就指向了
           一个被释放的字符串地址。此时如果在functionProtos中find这个funcName，那么
           map中这个底层数据被释放的string_view就会与传入的参数进行比较，自然会引发内存错误
    =================================================================
*/
inline std::map<std::string, std::unique_ptr<hoshino::PrototypeAST>>functionProtos;


/*  
    Optimization的重要概念
    ==========================================================  
    1.Pass: 在IR之后，每个Pass都会基于自身功能或其他的Pass对IR进行一定信息提取或优化
            多个Pass形成PipeLine，对IR进行信息提取或优化
            llvm将Pass分为三类：Analysis Pass、Transform Pass和Utility Pass
    2.Analysis Pass: 即计算相关IR单元的高层信息，不会对IR进行修改，
        分析的信息供其他Pass使用，且会记录哪些信息已经过期，如相关IR的信息
        被其他Pass修改，则Analysis Pass会将其标记为invalidate告知原先存储的信息已失效
    3.Transform Pass: 可以使用Analysis Pass的分析结果，以某种方式修改优化IR
    4.Utility Pass: 一些功能性的程序，完成某些具体的任务
    ==========================================================
    IN:IR==>Pass1==>Pass2==>...==>OUT:optimized IR
*/
// 管理FunctionPass FunctionPass作用于每个函数上 优化函数指令 
// inline std::unique_ptr<llvm::legacy::FunctionPassManager>theFPM;
// // 循环分析管理
// inline std::unique_ptr<llvm::LoopAnalysisManager>theLAM;
// // 函数分析管理
// inline std::unique_ptr<llvm::FunctionAnalysisManager>theFAM;
// /* 
//     CGSCC: Call Graph Strongly Connected Component
//     这是一个函数调用图的强连通分量的分析管理
// */
// inline std::unique_ptr<llvm::CGSCCAnalysisManager>theCGAM;
// // module分析管理
// inline std::unique_ptr<llvm::ModuleAnalysisManager>theMAM;
// // 管理Callback的注册信息
// inline std::unique_ptr<llvm::PassInstrumentationCallbacks>thePIC;
// // 提供注册pass的接口
// inline std::unique_ptr<llvm::StandardInstrumentations>theSI;

inline void InitModuleAndManager(){
    // context and module
    theContext = std::make_unique<llvm::LLVMContext>();
    theModule = std::make_unique<llvm::Module>("jit module", *theContext);
    theModule->setDataLayout(theJIT->getDataLayout());
    
    // builder
    builder = std::make_unique<llvm::IRBuilder<>>(*theContext);

}




