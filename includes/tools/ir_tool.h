#pragma once
#include <llvm-14/llvm/IR/Function.h>
#include <llvm-14/llvm/IR/IRBuilder.h>
#include <llvm-14/llvm/IR/Instructions.h>
#include <llvm-14/llvm/IR/Type.h>
#include <string>
#include <string_view>
#include "code_gen/ir.h"

inline std::unique_ptr<kalei::CodeGenVisitor>codeGenerator;
inline void InitCodeVisitor(){
    codeGenerator = std::make_unique<kalei::CodeGenVisitor>();
}

inline auto getFunction(std::string_view name) -> llvm::Function* {
    // 当前模块有 直接返回
    if(auto *func = theModule->getFunction(name))
        return func;
    // 当前模块没有 查找全局的函数注册表 找到就再次生成代码 注册到当前module中
    if(auto it = functionProtos.find(std::string{name}); it!=functionProtos.end())
        return it->second->ToLLvmValue(codeGenerator.get());
    return nullptr;
}
/*
    在theFunc的开头处建立一个名为varName的alloca变量
*/
inline auto CreateEntryBlockAlloca(llvm::Function *theFunc, 
    const std::string&varName) -> llvm::AllocaInst* {
    // 在一个function块的开头创建alloca变量
    llvm::IRBuilder<>tempBuilder{&theFunc->getEntryBlock(), 
        theFunc->getEntryBlock().begin()};
    return tempBuilder.CreateAlloca(llvm::Type::getDoubleTy(*theContext), 0, 
        varName.c_str());
}