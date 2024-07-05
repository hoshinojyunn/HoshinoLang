#include <cassert>
#include <cstdio>
#include <llvm/ExecutionEngine/JITSymbol.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/ExecutionEngine/Orc/ThreadSafeModule.h>
#include <memory>
#include <utility>
#include "jit/HoshinoJIT.h"
#include "lexer/token.h"
#include "code_gen/ir.h"
#include "tools/ir_tool.h"
#include "context.h"

static void HandleExtern(){
    if(auto protoAST = ParseExtern()){
        if(auto *fnIR = protoAST->ToLLvmValue(codeGenerator.get())){
            fprintf(stderr, "Read extern:\n");
            fnIR->print(llvm::errs());
            fprintf(stderr, "\n");
            // 函数声明注册到全局函数表中
            functionProtos[std::string{protoAST->GetFuncName()}] = std::move(protoAST);
        }
    }else{
        GetNextToken();
    }
}

static void HandleDefinition(){
    if(auto fnAST = ParseDefinition()){
        if(auto *fnIR = codeGenerator->CodeGen(fnAST.get())){
            // 一个函数定义放在一个module里
            exitOnErr(theJIT->addModule(
                llvm::orc::ThreadSafeModule(std::move(theModule), std::move(theContext))
            ));
            
            InitModuleAndManager();
        }
        
    } else{
        GetNextToken();
    }
}

static void HandleTopLevelExpr(){
    std::string anonFuncName;
    if(auto fnAST = ParseTopLevelExpr(anonFuncName)){
        if(auto fnIR = codeGenerator->CodeGen(fnAST.get())){
            auto res_tracker = theJIT->getMainJITDylib().createResourceTracker();
            // 将当前的module给顶级表达式的匿名函数使用
            auto thread_safe_mod = 
                llvm::orc::ThreadSafeModule(std::move(theModule), std::move(theContext));
            exitOnErr(theJIT->addModule(
                std::move(thread_safe_mod), res_tracker));
            // 前面将module给了匿名函数用 外层新建另外的module
            InitModuleAndManager();
            // jit中找匿名函数
            auto exprSymbol = exitOnErr(theJIT->lookup(anonFuncName));
            assert(exprSymbol && "function not found");
            
            auto funcAddr = exprSymbol.getAddress();
            auto fn = llvm::jitTargetAddressToPointer<double(*)()>(funcAddr);
            fprintf(stderr, "Evaluated to %f\n", fn());
            // 从JIT中删除匿名函数的module 所有之前添加到该module的函数定义都会消失
            exitOnErr(res_tracker->remove());
            // // 从符号表中移出匿名函数名字 即__anon_expr
            // fnIR->eraseFromParent();
        }
    }else{
        GetNextToken();
    }
}

void MainLoop(){
    while (true) {
        // fprintf(stderr, ">>> ");
        switch (curTok) {
        case TOK_EOF:
            return;
        case TOK_EXPR_END:
            GetNextToken();
            break;
        case TOK_DEF:
            HandleDefinition();
            break;
        case TOK_EXTERN:
            HandleExtern();
            break;
        default:
            HandleTopLevelExpr();
            break;
        }
    }
}


void SettingContext(std::string sourceFile){
    sourceInput = std::make_unique<std::ifstream>(sourceFile);
    InitBinOpPrecedence();
    InitValidBinOpSet();
    InitJIT();
    InitModuleAndManager();
    InitCodeVisitor();
    fprintf(stderr, ">>> ");
    GetNextToken();
}

void ContextClose(){
    theModule->print(llvm::errs(), nullptr);
}