// HoshinoJIT
#pragma once

#include "llvm/ADT/StringRef.h"
#include "llvm/ExecutionEngine/JITSymbol.h"
#include "llvm/ExecutionEngine/Orc/CompileUtils.h"
#include "llvm/ExecutionEngine/Orc/IRTransformLayer.h"
#include "llvm/ExecutionEngine/Orc/Core.h"
#include "llvm/ExecutionEngine/Orc/ExecutionUtils.h"
#include "llvm/ExecutionEngine/Orc/ExecutorProcessControl.h"
#include "llvm/ExecutionEngine/Orc/IRCompileLayer.h"
#include "llvm/ExecutionEngine/Orc/JITTargetMachineBuilder.h"
#include "llvm/ExecutionEngine/Orc/RTDyldObjectLinkingLayer.h"
#include "llvm/ExecutionEngine/SectionMemoryManager.h"
#include "llvm/Transforms/InstCombine/InstCombine.h"
#include "llvm/Transforms/Scalar/GVN.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm-14/llvm/ExecutionEngine/Orc/ThreadSafeModule.h"
#include "llvm-14/llvm/IR/LegacyPassManager.h"
#include "llvm-14/llvm/Support/raw_ostream.h"
#include "llvm/ExecutionEngine/Orc/CompileOnDemandLayer.h"
#include "llvm/ExecutionEngine/Orc/EPCIndirectionUtils.h"
#include "llvm-14/llvm/Support/Error.h"
#include <cstdio>
#include <llvm-14/llvm/ExecutionEngine/Orc/IndirectionUtils.h>
#include <llvm-14/llvm/ExecutionEngine/Orc/LazyReexports.h>
#include <llvm-14/llvm/ExecutionEngine/Orc/Shared/ExecutorAddress.h>
#include <llvm-14/llvm/IR/Module.h>
#include <memory>
#include <utility>
#include "context.h"

namespace llvm {
namespace orc {

class HoshinoJIT {
private:
  // JIT上下文 包括string pool、global mutex、error reporting facilities等
  std::unique_ptr<ExecutionSession> ES;
  std::unique_ptr<EPCIndirectionUtils>EPCIU;
  // DataLayout和MangleAndInterner用于符号修改
  DataLayout DL;
  MangleAndInterner Mangle;
  // 这一层可以添加.o文件到JIT 不会直接使用它 
  RTDyldObjectLinkingLayer ObjectLayer;
  // 这一层可以添加LLVM Modules到JIT 并将Modules构建在ObjectLayer上
  IRCompileLayer CompileLayer;
  // 这一层在CompileLayer之上，构造时需要传入CompileLayer的引用，
  // 该层处理完后传入CompileLayer层处理
  IRTransformLayer OptimizeLayer;
  /* 
    当call指令进行编译时，会产生一个stub：jump to @Function
    回调编译，进行编译回调后，将原@Function的编译回调函数的地址
    更换为编译好的函数的地址
  */
  // std::unique_ptr<LazyCallThroughManager>LCTM;

  /* 
    构建CompileOnDemandLayer需要一个IndirectStubsManager，
    在JIT用addModule传入一个模块给CODLayer时，IndirectStubsManager会为Module
    里的每个Funtion产生一个stub。而在每次stub代表的Funtion被call时，
    CODLayer会使用LazyCallThroughManager对该Funtion进行编译，
    对Funtion进行编译的函数叫partitioning funtion，
    而事实上我们可以通过CompileOnDemandLayer::setPartitionFunction直接进行设置
  */
  CompileOnDemandLayer CODLayer;


  /*
    JITDylib模仿常规的动态链接库 通过向JITDylib中添加包含代码的Module(IR Code)
    可以做到函数调用操作
  */
  JITDylib &MainJD;


public:
  HoshinoJIT(std::unique_ptr<ExecutionSession> ES, std::unique_ptr<EPCIndirectionUtils>EPCIU,
                  JITTargetMachineBuilder JTMB, DataLayout DL)
      : ES(std::move(ES)), EPCIU(std::move(EPCIU)), DL(std::move(DL)), Mangle(*this->ES, this->DL),
        ObjectLayer(*this->ES,
                    []() { /*管理内存的分配、访问权限，添加的module会被其管理*/return std::make_unique<SectionMemoryManager>(); }),
        CompileLayer(*this->ES, ObjectLayer,
                     /*编译实例，用于将IR file编译为.o文件*/std::make_unique<ConcurrentIRCompiler>(std::move(JTMB))),
        OptimizeLayer(*this->ES, CompileLayer, optimizeModule),
        
        CODLayer(*this->ES, OptimizeLayer, this->EPCIU->getLazyCallThroughManager(), 
        [this]{return this->EPCIU->createIndirectStubsManager();}),
        MainJD(this->ES->createBareJITDylib("<main>")) {
    MainJD.addGenerator(
        cantFail(DynamicLibrarySearchGenerator::GetForCurrentProcess(
        DL.getGlobalPrefix())));
    if (JTMB.getTargetTriple().isOSBinFormatCOFF()) {
      // 设置可重定义，新的定义覆盖旧的
      ObjectLayer.setOverrideObjectFlagsWithResponsibilityFlags(true);
      // 设置自动声明，函数定义可自动声明
      ObjectLayer.setAutoClaimResponsibilityForObjectSymbols(true);
    }
  }

  ~HoshinoJIT() {
    if (auto Err = ES->endSession())
      ES->reportError(std::move(Err));
    if(auto Err = EPCIU->cleanup())
      ES->reportError(std::move(Err));
  }

  static Expected<std::unique_ptr<HoshinoJIT>> Create() {
    auto EPC = SelfExecutorProcessControl::Create();
    if (!EPC)
      return EPC.takeError();
    
    auto ES = std::make_unique<ExecutionSession>(std::move(*EPC));
    
    auto EPCIU = EPCIndirectionUtils::Create(ES->getExecutorProcessControl());
    if(!EPCIU)
      return EPCIU.takeError();
    (*EPCIU)->createLazyCallThroughManager(*ES, pointerToJITTargetAddress(&handleLazyCallThroughError));
    if(auto Err = setUpInProcessLCTMReentryViaEPCIU(**EPCIU))
      return std::move(Err);

    JITTargetMachineBuilder JTMB(
        ES->getExecutorProcessControl().getTargetTriple());

    auto DL = JTMB.getDefaultDataLayoutForTarget();
    if (!DL)
      return DL.takeError();

    return std::make_unique<HoshinoJIT>(std::move(ES), std::move(*EPCIU),
    std::move(JTMB), std::move(*DL));
  }

  const DataLayout &getDataLayout() const { return DL; }

  JITDylib &getMainJITDylib() { return MainJD; }

  Error addModule(ThreadSafeModule TSM, ResourceTrackerSP RT = nullptr) {
    if (!RT)
      RT = MainJD.getDefaultResourceTracker();
    /*
      将IR Module添加到JITDylib中，JITDylib会为Module中定义的每个函数创建一个符号表
      并且JITDylib会推迟编译该Module，直到Module中的任何一个函数定义被lookup，此时才编译Module
      注意：这并不是lazy compilation，lookup只是拿到函数定义的引用，并不是真正调用函数
    */
    return CODLayer.add(RT, std::move(TSM));
  }

  Expected<JITEvaluatedSymbol> lookup(StringRef Name) {
    /*
      lookup传入一系列dylib(这里只有一个)，并在这些dylib中找到指定的函数或变量的symbol
      而这个symbol不是IR代码中的symbol，而是在llvm ORC-jit内部使用mangle symbol
      mangle symbol也是c++静态编译器和链接器使用的符号 因此
      使用mangle symbol的好处是让JIT中的代码能够便捷地与应用程序或共享库的预编译代码进行交互
      mangle：重整，即符号重命名，重命名后的名称取决于DataLayout，而DataLayout取决于目标平台
    */
    return ES->lookup({&MainJD}, Mangle(Name.str()));
  }
private:
  /*
    将原来优化函数的操作改为加入jit时再对Module中的函数进行优化
    第二个参数MaterializationResponsibility用于查询进行模块优化的JIT的状态
    比如在JIT尝试调用函数时查询定义集
    返回优化后的Module，OptimizeLayer调用完这个函数后，传递给下层的CompileLayer进行操作
  */
  static Expected<orc::ThreadSafeModule>
  optimizeModule(orc::ThreadSafeModule M, const orc::MaterializationResponsibility &R){
      M.withModuleDo([](Module &Mod){
        // pass and analysis manager
        auto theFPM = std::make_unique<legacy::FunctionPassManager>(&Mod);
        /*
            添加Instruction Combine Pass 该Pass旨在简化、消除某些不必要的指令
            该Pass不会修改控制流图 且它的结果对DCE Pass(dead code pass)有重要作用
            如，原指令为: %Y = 1 + %X，%Z = 1 + %Y
            则经过该Pass后两条指令变为一条指令: %Z = 2 + %X
        */ 
        theFPM->add(createInstructionCombiningPass());
        /*
            该Pass用于重新关联可交换的表达式的顺序，为了更好促进常量表达式的传播
            如，4+(x+5) 经过该Pass后变为：x+(4+5)
        */
        theFPM->add(createReassociatePass());
        /*
            GVN: Global Value Numbering，为每一个计算得到的值分配一个唯一编号
            该Pass用于消除多余的values、loads以及指令
            如，一段程序中出现了多次操作数相同的乘法，那么编译器可以将这些乘法合并为一个
            一般GVN通过直接比较两个表达式的值是否相同来判断是否合并为一个表达式
        */
        theFPM->add(createGVNPass());
        /*
            CFG: Control Flow Graph 控制流图
            该Pass用于简化以及规范化函数的控制流图
        */
        theFPM->add(createCFGSimplificationPass());
        // 优化alloca变量的调用 将不必要的alloca load与store改为寄存器SSA form
        // theFPM->add(llvm::createPromoteMemoryToRegisterPass());
        theFPM->doInitialization();
        for(auto &F : Mod){
          theFPM->run(F);
#ifdef DEBUG
          fprintf(stderr, "Read Function optimized:\n");
          F.print(errs());
          fprintf(stderr, "\n");
#endif
        }
      });
      
      // fprintf(stderr, "\n");
      return M;
    }
    static void handleLazyCallThroughError() {
      errs() << "LazyCallThrough error: Could not find function body";
      exit(1);
    }

};

} // end namespace orc
} // end namespace llvm
inline llvm::ExitOnError exitOnErr;
inline std::unique_ptr<llvm::orc::HoshinoJIT>theJIT;

inline void InitJIT(){
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();
    llvm::InitializeNativeTargetAsmParser();

    theJIT = exitOnErr(llvm::orc::HoshinoJIT::Create());
    
}

