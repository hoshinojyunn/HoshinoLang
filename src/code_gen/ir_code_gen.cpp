#include "ast/basic_ast.h"
#include "code_gen/ir.h"
#include "lexer/token.h"
#include "tools/basic_tool.h"
#include "tools/ir_tool.h"
#include <cassert>
#include <cstddef>
#include <llvm/ADT/APFloat.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constant.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Verifier.h>
#include <memory>
#include <utility>
#include <vector>
using namespace kalei;


auto CodeGenVisitor::CodeGen(NumberExprAST *ast) -> llvm::Value* {
    return llvm::ConstantFP::get(*theContext, 
            llvm::APFloat(ast->val_));
}

auto CodeGenVisitor::CodeGen(VariableExprAST *ast) -> llvm::Value * {
    llvm::AllocaInst* val = namedValues[ast->name_];
    if(!val)
        LOG_ERROR_V("unknow variable name");
    return builder->CreateLoad(val->getAllocatedType()
        , val, ast->name_);
}

// llvm生成的指令的两个操作数必须类型相同 返回的结果也与操作数类型相同
// (kalei所有操作数都是double 所以不必在意这个问题)
auto CodeGenVisitor::CodeGen(BinaryExprAST *ast) -> llvm::Value* {
    // 
    if(ast->op_ == "="){
        auto lhsExpr = dynamic_cast<kalei::VariableExprAST*>(ast->lhs_.get());
        // 若=运算左边不是一个变量 则返回错误
        if(!lhsExpr)
            return LOG_ERROR_V("destination of '=' must be a variable");
        // 右边的值 可以是Variable或普通的Number
        auto rVal = ast->rhs_->ToLLvmValue(this);
        if(!rVal)
            return nullptr;
        llvm::AllocaInst *variable = namedValues[lhsExpr->name_];
        if(!variable)
            return LOG_ERROR_V("unknow variable name");
        builder->CreateStore(rVal, variable);
        return rVal;
    }
    llvm::Value *l = ast->lhs_->ToLLvmValue(this);
    llvm::Value *r = ast->rhs_->ToLLvmValue(this);
    if(!l || !r)
        return nullptr;
    if(ast->op_ == "+"){
        return builder->CreateFAdd(l, r, "addtmp");
    }else if(ast->op_ == "-"){
        return builder->CreateFSub(l, r, "subtmp");
    }else if(ast->op_ == "*"){
        return builder->CreateFMul(l, r, "multmp");
    }else if(ast->op_ == "<"){
        /* 
            FCmpULT(float point compare, result is unsigned, less than)
            llvm的fcmp指令始终返回一位整数 但是因为kalei只有double一种类型
            所以我们希望将返回的数转换为0.0或1.0
            因此第二行用UIToFP(unsigned int to float point)将其转为浮点数
            如果用SIToFP(signed int to float point)则会转为0.0或-1.0
        */
        l = builder->CreateFCmpULT(l, r, "cmptmp");
        return builder->CreateUIToFP(l, llvm::Type::getDoubleTy(*theContext), "booltmp");
    }
    // switch (ast->op_) {
    // case '+':
    //     // 第三个参数Name是一个可选的参数 表示生成指令的名称
    //     // 如果生成了多条类型相同的ir指令 llvm会在名称后加上唯一的递增的数字区分指令
    //     // FAdd、FSub为浮点数加减法 因为kalei只有double一种类型 所以用F开头的指令
    //     return builder->CreateFAdd(l, r, "addtmp");
    // case '-':
    //     return builder->CreateFSub(l, r, "subtmp");
    // case '*':
    //     return builder->CreateFMul(l, r, "multmp");
    // case '<':
    //     /* 
    //         FCmpULT(float point compare, result is unsigned, less than)
    //         llvm的fcmp指令始终返回一位整数 但是因为kalei只有double一种类型
    //         所以我们希望将返回的数转换为0.0或1.0
    //         因此第二行用UIToFP(unsigned int to float point)将其转为浮点数
    //         如果用SIToFP(signed int to float point)则会转为0.0或-1.0
    //     */
    //     l = builder->CreateFCmpULT(l, r, "cmptmp");
    //     return builder->CreateUIToFP(l, llvm::Type::getDoubleTy(*theContext), "booltmp");
    // default:
    //     // return LOG_ERROR_V("invalid binary operator");
    //     break;
    // }
    // 如果二元运算符不是内建运算符 则查找用户定义运算符
    llvm::Function *func = getFunction(std::string{"binary@"} + ast->op_);
    assert(func && "binary operator function not found");
    return builder->CreateCall(func, {l, r}, "binop");
}

auto CodeGenVisitor::CodeGen(UnaryExprAST *ast) -> llvm::Value* {
    // 操作数
    llvm::Value *operandVal = ast->operand_->ToLLvmValue(this);
    if(!operandVal)
        return nullptr;
    auto func = getFunction(std::string{"unary@"} + ast->op_);
    if(!func)
        return LOG_ERROR_V("unknow unary operator");
    return builder->CreateCall(func, operandVal, "unop");
}

auto CodeGenVisitor::CodeGen(VarExprAST *ast) -> llvm::Value* {

    auto theFunc = builder->GetInsertBlock()->getParent();
    const std::string&varName = ast->varNames_.first;
    // 检查当前是否存在同名变量
    if(namedValues.find(varName) != namedValues.end())
        return nullptr;
    ExprAST *init = ast->varNames_.second.get();
    llvm::Value *initVal;
    if(init){
        initVal = init->ToLLvmValue(this);
        if(!initVal)
            return nullptr;
    }else{
        // 如果没有初始化 默认为0.0
        initVal = llvm::ConstantFP::get(*theContext, llvm::APFloat(0.0));
    }
    llvm::AllocaInst *alloca = CreateEntryBlockAlloca(theFunc, varName);
    builder->CreateStore(initVal, alloca);
    namedValues[varName] = alloca;
    return alloca;
}

/*
    if分支结构：
    条件为true 跳到then分支 false跳到else分支 然后再往下合并到ifcont
    根据静态单一赋值原则 到ifcont时 需要决定之前的变量用哪个分支的 即Φ函数
    iftmp = Φ(then, else)
entry:
    %ifcond = fcmp one double %condition, 0.0
    br i1 %ifcond, label %then, label %else
then:
    %calltmp1 = xxx
    ......
    goto ifcont
else:
    %calltmp2 = xxx
    ......
    goto ifcont
ifcont:
    %iftmp = phi [%calltmp1, %then], [%calltmp2, %else]
    ......
*/

/*
    在函数中有这样的代码：thenBB = builder->GetInsertBlock(); 很奇怪 有什么作用？
    llvm中 phi节点是从控制流图的最新的block中合并的 考虑以下代码：
    def test(x)
        if x then
            if x then foo()
    在没有开启IR优化时 得到的IR如下：
===============================================================
define double @test(double %x) {
entry:
  %ifcond = fcmp one double %x, 0.000000e+00
  br double %x, label %then-out, label %else-out

then-out:                                         ; preds = %entry
  %ifcond1 = fcmp one double %x, 0.000000e+00
  br double %x, label %then-in, label %else-in

then-in:                                          ; preds = %then-out
  %calltmp = call double @foo()
  br label %ifcont-in

else-in:                                          ; preds = %then-out
  br label %ifcont-in

ifcont-in:                                        ; preds = %else-in, %then-in
  %iftmp = phi double [ %calltmp, %then2 ], [ 0.000000e+00, %else ]
  br label %ifcont-out

else-out:                                         ; preds = %entry
  br label %ifcont-out

ifcont-out:                                       ; preds = %else-in, %ifcont-in
  %iftmp5 = phi double [ %iftmp, %ifcont ], [ 0.000000e+00, %else3 ]
  ret double %iftmp5
}
===============================================================
可以知道ifcont-out是从最新的block 即内层的if语句的ifcont-in和外层的else-out合并的
下面逐句分析IR生成过程：
1. 在处理外层if时，then表达式为内层if，else表达式为外层else，因此在then-out
    生成关于内层if的条件跳转语句br double %x, label %then-in, label %else-in
2. 接下来处理内层if，内层if的then语句为call foo()，而没有else语句，故then-in
    生成%calltmp = call double @foo()，且then-in与else-in都在ifcont-in合并，
    即生成无条件跳转指令br label %ifcont-in
3. 到了关键的一步，此时内层if已经处理完成了，关于外层if的以下这条语句结束：
    llvm::Value *then_val = ast->then_->ToLLvmValue(this);
    注意：此时的builder内的插入点在ifcont-in这里，ifcont-in块在控制流图上即为
    ifcont-out的前置块(else-out也是前置块)。
    而后面要将phi节点的前置块设置为thenBB与elseBB，意味着在处理完外层then块后
    需要将thenBB设置为then-out内的最新的一块，也就是ifcont-in
    这就是为什么需要: thenBB = builder->GetInsertBlock()
4. 之后的elseBB的更新设置同理
*/
auto CodeGenVisitor::CodeGen(IfExprAST *ast) -> llvm::Value* {
    auto ifRet = CreateEntryBlockAlloca(builder->GetInsertBlock()->getParent(), "ifRet");
    llvm::Value *condition_val = ast->condition_->ToLLvmValue(this);
    if(!condition_val)
        return nullptr;
    
    // float compare ordered not equal 将if后条件表达式的值与0.0比较
    // 不相等返回true 相等返回false
    condition_val = builder->CreateFCmpONE(condition_val, 
        llvm::ConstantFP::get(*theContext, llvm::APFloat(0.0)), 
        "ifcond");
    // 当前if所在函数
    llvm::Function *theFunc = builder->GetInsertBlock()->getParent();
    
    // 生成then块 并且将其添加到thFunc的blocksList中 
    llvm::BasicBlock *thenBB = llvm::BasicBlock::Create(*theContext,
         "if.then", theFunc);
    llvm::BasicBlock *elseBB = llvm::BasicBlock::Create(*theContext,
         "if.else");
    llvm::BasicBlock *mergeBB = llvm::BasicBlock::Create(*theContext,
         "if.end");
    // 创建conditional compare分支(then与else)
    builder->CreateCondBr(condition_val, thenBB, elseBB);
    // 往then分支插入指令
    builder->SetInsertPoint(thenBB);
    llvm::Value *then_val = ast->then_->ToLLvmValue(this);
    if(!then_val)
        return nullptr;
    builder->CreateStore(then_val, ifRet);
    /* 
        给thenblock创建branch指令 表示该块执行完后跳转到mergeBB块
        llvm要求每一个块都必须使用控制流图终止指令结尾 如return、branch等
    */
    builder->CreateBr(mergeBB);
    /* 
        then分支的代码生成llvm::Value时可能会转换到其他的Block 比如多层if嵌套
        builder中的basicblock就可能变成其他block而不是原来的thenBB
        而在最后合并时 ifcont要从最新的块中合并 那么就需要将thenBB设置为最新的块
        详细查看函数顶部的示例
    */
    thenBB = builder->GetInsertBlock();
    // ======= 生成else块 =======
    // 注意前面的elseBB与mergeBB都与thenBB不同 并没有添加到theFunc的blockList中
    theFunc->getBasicBlockList().push_back(elseBB);
    builder->SetInsertPoint(elseBB);
    llvm::Value *else_val = nullptr;
    // else可能没有
    if(ast->else_){
        // TODO: BlockExpr始终返回0
        else_val = ast->else_->ToLLvmValue(this);
        if(!else_val)
            return nullptr;
        builder->CreateStore(else_val, ifRet);
        builder->CreateBr(mergeBB);
        // 同thenBB一样 elseBB也要更新
        elseBB = builder->GetInsertBlock();
    }else{
        builder->CreateBr(mergeBB);
        elseBB = builder->GetInsertBlock();
    }
    if(!else_val)
        else_val = llvm::ConstantFP::get(llvm::Type::getDoubleTy(*theContext), llvm::APFloat{0.0});
    // 生成merge块 
    theFunc->getBasicBlockList().push_back(mergeBB);
    builder->SetInsertPoint(mergeBB);
    // // 创建phi节点
    // llvm::PHINode *phiNode = builder->CreatePHI(
    //     llvm::Type::getDoubleTy(*theContext),
    //     2, "iftmp");
    // 向phi节点设置block/value pair
    // phiNode->addIncoming(then_val, thenBB);
    // phiNode->addIncoming(else_val, elseBB);
    // return phiNode;
    return builder->CreateLoad(ifRet->getAllocatedType(), ifRet, "ifRet");
}

auto CodeGenVisitor::CodeGen(ForExprAST *ast) -> llvm::Value* {
    llvm::Function*theFunction = builder->GetInsertBlock()->getParent();
    // 创建alloca局部变量
    llvm::AllocaInst *alloca = CreateEntryBlockAlloca(theFunction, ast->varName_);
    llvm::Value*startVal = ast->start_->ToLLvmValue(this);
    if(!startVal)
        return nullptr;
    // 将startVal保存到alloca变量中
    builder->CreateStore(startVal, alloca);
    auto forCount = llvm::BasicBlock::Create(
        *theContext, "for_count", theFunction); 
    auto forBody = llvm::BasicBlock::Create(
        *theContext, "for_body", theFunction);
    // 跳出循环块 parent为for的外层块
    auto afterLoopBB = llvm::BasicBlock::Create(*theContext,
        "after_loop", theFunction);
    
    // 创建phi节点 且最开始时 初始变量是从preHeaderBB中来的
    // auto initVar = builder->CreatePHI(llvm::Type::getDoubleTy(*theContext), 
    //     2, ast->varName_);
    // initVar->addIncoming(startVal, preHeaderBB);
    // 初始变量可能会跟循环块外面的变量名一致 所以要改变局部变量表namedValues
    // 并且在跳出循环时恢复回原来的变量
    llvm::AllocaInst *oldVal = namedValues[ast->varName_];
    namedValues[ast->varName_] = alloca;
    builder->CreateBr(forCount);
    builder->SetInsertPoint(forCount);
    // 解析循环结束条件 放在namedValues存好alloca后面 否则解析时找不到相关变量
    llvm::Value *endCond = ast->end_->ToLLvmValue(this);
    if(!endCond)
        return nullptr;
    endCond = builder->CreateFCmpONE(endCond, 
        llvm::ConstantFP::get(*theContext, llvm::APFloat(0.0)),
        "loopCond");
    // 创建一个无条件跳转分支进入loop块
    builder->CreateCondBr(endCond, forBody, afterLoopBB);
    

    // 生成循环体ir
    builder->SetInsertPoint(forBody);
    if(!ast->body_->ToLLvmValue(this))
        return nullptr;
    // 步进
    llvm::Value *stepVal = nullptr;
    if(ast->step_){
       stepVal = ast->step_->ToLLvmValue(this);
       if(!stepVal)
        return nullptr; 
    }else{
        stepVal = llvm::ConstantFP::get(*theContext, llvm::APFloat(0.0));
    }

    // 计算新值并存回去
    // auto curVal = builder->CreateLoad(alloca->getAllocatedType(), alloca, ast->varName_.c_str());
    builder->CreateStore(stepVal, alloca);
    // auto nextVal = builder->CreateFAdd(curVal, stepVal, "nextVar");
    // builder->CreateStore(nextVal, alloca);
    
    // 结束条件与0不相等时
    // endCond = builder->CreateFCmpONE(endCond, 
    //     llvm::ConstantFP::get(*theContext, llvm::APFloat(0.0)),
    //     "loopCond");
    builder->CreateBr(forCount);
    // 如果循环结束 将插入点设为afterLoopBB
    builder->SetInsertPoint(afterLoopBB);

    // 如果外层存在与initVar名称相同的变量 将其恢复到局部变量表中
    if(oldVal)
        namedValues[ast->varName_] = oldVal;
    else 
        namedValues.erase(ast->varName_);
    // 返回0.0
    return llvm::Constant::getNullValue(llvm::Type::getDoubleTy(*theContext));
}

auto CodeGenVisitor::CodeGen(BlockExprAST *ast) -> llvm::Value* {
    // BlockExpr以最后一句表达式作为返回
    llvm::Value *ret = nullptr;
    for(auto&expr : ast->body_) {
        if(ret = expr->ToLLvmValue(this); !ret){
            return nullptr;
        }
    }
    return ret==nullptr? 
    llvm::Constant::getNullValue(llvm::Type::getDoubleTy(*theContext)) : ret;
}


auto CodeGenVisitor::CodeGen(CallExprAST *ast) -> llvm::Value* {
    llvm::Function *calleeFunc = getFunction(ast->callee_);
    if(!calleeFunc)
        return LOG_ERROR_V("Unknow function reference");
    if(calleeFunc->arg_size() != ast->args_.size())
        return LOG_ERROR_V("Incorrect # arguments passed");
    std::vector<llvm::Value*> args_val;
    for(size_t i{0}; i!=ast->args_.size(); ++i){
        args_val.push_back(ast->args_[i]->ToLLvmValue(this));
        // 确保新加进args_val的llvm value*不为nullptr
        if(!args_val.back())
            return nullptr;
    }
    return builder->CreateCall(calleeFunc, args_val, "calltmp");
}

auto CodeGenVisitor::CodeGen(PrototypeAST *ast) -> llvm::Function* {
    std::vector<llvm::Type*>doubles {ast->args_name_.size(), 
        llvm::Type::getDoubleTy(*theContext)};
    // function类型 包括返回值、所有参数的类型(数组)、参数是否可变
    llvm::FunctionType *ft = llvm::FunctionType::get(
        llvm::Type::getDoubleTy(*theContext),
        doubles,
        false
    );
    // 创建extern函数声明到module中 名称为ast->name_
    llvm::Function *func = llvm::Function::Create(
        ft,
        llvm::Function::ExternalLinkage,
        ast->name_,
        theModule.get()
    );
    size_t i{0};
    // 给参数命名
    for(auto&arg : func->args()){
        arg.setName(ast->args_name_[i++]);
    }
    return func;
}

auto CodeGenVisitor::CodeGen(FunctionAST *ast) -> llvm::Function* {
    auto &proto = *ast->proto_;
    // 注册到全局函数表
    functionProtos[std::string{proto.GetFuncName()}] = std::move(ast->proto_);
    // 从找到extern声明 
    auto theFunc = getFunction(proto.GetFuncName());
    // 创建失败则返回nullptr
    if(!theFunc)
        return nullptr;
    if(proto.isBinaryOp())
        binOpPrecedence[proto.GetOperator()] = proto.GetBinaryPrecedence();
    /* 
        函数定义的参数名称要与声明时一致 
        如果先前用extern声明一个函数如extern foo(a) 
        之后再def foo(b)会出现unknow variable name错误 必须要def foo(a)
        这非常不智能 需要对函数名称进行重命名
    */
    assert(theFunc->arg_size() == proto.args_name_.size() 
        && "extern func's argument size should be same as the def func's");
    // 发现参数不一致时进行参数重命名
    if(theFunc->arg_size()!=0 &&
     !theFunc->getArg(0)->getName().equals(proto.args_name_[0])){
        for(size_t i{0}; i<theFunc->arg_size();++i){
            theFunc->getArg(i)->setName(proto.args_name_[i]);
        }
    }
    // 在此 我们断言该func还没有实现(函数体为空)
    if(!theFunc->empty())
        return (llvm::Function*)LOG_ERROR_V("function cannot be redefined");
    // BasicBlock是一个重要的概念 这里创建了一个block名为entry 将其插入到theFunc中
    auto *bb = llvm::BasicBlock::Create(*theContext, 
        "entry", theFunc);
    // builder设置插入指令的地方 为创建的basic block的末尾
    builder->SetInsertPoint(bb);
    // 将函数参数记录在表中
    namedValues.clear();
    // 为函数参数创建alloca局部变量到栈上
    for(auto&arg : theFunc->args()) {
        auto alloca = CreateEntryBlockAlloca(theFunc, arg.getName().str());
        builder->CreateStore(&arg, alloca);
        namedValues[arg.getName().str()] = alloca;
    }
    auto res = CreateEntryBlockAlloca(theFunc, "$ret");
    namedValues[res->getName().str()] = res;
    // 给函数体创建指令 并获得返回的Value 如果不出错 则会在entry block中创建指令
    if(llvm::Value *retVal = ast->body_->ToLLvmValue(this)){
        // retVal为函数体中的顶层表达式的ast的llvm Value
        // 创建llvm ret指令 表示函数的完成
        builder->CreateStore(retVal, res);
        auto ret_val = builder->CreateLoad(res->getAllocatedType(), res, "$ret");
        builder->CreateRet(ret_val);
        // 利用verifyFunction对生成的代码进行各种一致性检查 它可以捕获许多错误
        llvm::verifyFunction(*theFunc);
        // 使用function pass manager内的pass优化函数体
        // theFPM->run(*theFunc);

        return theFunc;
    }
    // 处理错误情况 到这里说明函数定义出错 
    // 此时需要使用eraseFromParent将函数从module符号表中抹除
    // 如果不从符号表中抹除 llvm不会允许将来再次出现相同的函数
    // 即 如果你第一次函数写错了 没有抹除它 则第二次再写一遍相同的函数是不被允许的
    theFunc->eraseFromParent();
    if(proto.isBinaryOp())
        binOpPrecedence.erase(proto.GetOperator());
    return nullptr;
}