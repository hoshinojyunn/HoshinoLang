#pragma once

#include <cassert>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Value.h>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>


namespace kalei {
class NumberExprAST;
class VariableExprAST;
class VarExprAST;
class BinaryExprAST;
class UnaryExprAST;
class BlockExprAST;
class CallExprAST;
class IfExprAST;
class ForExprAST;
class PrototypeAST;
class FunctionAST;


class Visitor{
public:
    virtual llvm::Value* CodeGen(NumberExprAST*) = 0;
    virtual llvm::Value* CodeGen(VariableExprAST*) = 0;
    virtual llvm::Value* CodeGen(VarExprAST*) = 0;
    virtual llvm::Value* CodeGen(BinaryExprAST*) = 0;
    virtual llvm::Value* CodeGen(UnaryExprAST*) = 0;
    virtual llvm::Value* CodeGen(BlockExprAST*) = 0;
    virtual llvm::Value* CodeGen(CallExprAST*) = 0;
    virtual llvm::Value* CodeGen(IfExprAST*) = 0;
    virtual llvm::Value* CodeGen(ForExprAST*) = 0;
    virtual llvm::Function* CodeGen(PrototypeAST*) = 0;
    virtual llvm::Function* CodeGen(FunctionAST*) = 0;
    
    virtual ~Visitor() {};
};

class CodeGenVisitor : public Visitor {
public:
    auto CodeGen(NumberExprAST*ast) -> llvm::Value* override;
    auto CodeGen(VariableExprAST*ast) -> llvm::Value* override;
    auto CodeGen(VarExprAST*) -> llvm::Value* override;
    auto CodeGen(BinaryExprAST *ast) -> llvm::Value* override;
    auto CodeGen(UnaryExprAST *ast) -> llvm::Value* override;
    auto CodeGen(BlockExprAST *ast) -> llvm::Value* override;
    auto CodeGen(CallExprAST *ast) -> llvm::Value* override;
    auto CodeGen(IfExprAST*ast) -> llvm::Value* override;
    auto CodeGen(ForExprAST*ast) -> llvm::Value* override;
    auto CodeGen(PrototypeAST *ast) -> llvm::Function* override;
    auto CodeGen(FunctionAST *ast) -> llvm::Function* override;
    ~CodeGenVisitor() = default;
};

class ExprAST{
public:
    virtual ~ExprAST() = default;
    virtual llvm::Value* ToLLvmValue(Visitor*) = 0;
};

class NumberExprAST : public ExprAST {
    friend class CodeGenVisitor;
    double val_;
public:
    NumberExprAST(double val) : val_(val) {}
    llvm::Value* ToLLvmValue(Visitor*v) override {
        return v->CodeGen(this);
    }
};

class VariableExprAST : public ExprAST {
    friend class CodeGenVisitor;
    std::string name_;
public:
    VariableExprAST(const std::string&name) : name_(name){}
    llvm::Value* ToLLvmValue(Visitor*v) override {
        return v->CodeGen(this);
    }
};

class VarExprAST : public ExprAST {
    friend class CodeGenVisitor;
    std::pair<std::string, std::unique_ptr<ExprAST>>varNames_;
public:
    VarExprAST(const std::string&varName, std::unique_ptr<ExprAST>initVal) 
        : varNames_(varName, std::move(initVal)){}; 
    llvm::Value* ToLLvmValue(Visitor*v) override {
        return v->CodeGen(this);
    }
};

// 双目运算符
class BinaryExprAST : public ExprAST{
    friend class CodeGenVisitor;
    std::string op_;
    std::unique_ptr<ExprAST>lhs_, rhs_;
public:
    BinaryExprAST(const std::string&op, 
    std::unique_ptr<ExprAST>lhs, 
    std::unique_ptr<ExprAST>rhs) : op_(op), lhs_(std::move(lhs)), rhs_(std::move(rhs)){}
    llvm::Value* ToLLvmValue(Visitor*v) override {
        return v->CodeGen(this);
    }
};
// 包含被调用函数名称以及参数的ast
class CallExprAST : public ExprAST {
    friend class CodeGenVisitor;
    std::string callee_;
    std::vector<std::unique_ptr<ExprAST>>args_;
public:
    CallExprAST(const std::string&callee, 
    std::vector<std::unique_ptr<ExprAST>>&&args) : callee_(callee), args_(std::move(args)){}
    llvm::Value* ToLLvmValue(Visitor*v) override {
        return v->CodeGen(this);
    }
};

class UnaryExprAST : public ExprAST {
    friend class CodeGenVisitor;
    char op_;
    std::unique_ptr<ExprAST>operand_;
public:
    UnaryExprAST(char op, std::unique_ptr<ExprAST>operand):op_(op), operand_(std::move(operand)){}
    llvm::Value* ToLLvmValue(Visitor*v) override {
        return v->CodeGen(this);
    }
};

class IfExprAST : public ExprAST {
    friend class CodeGenVisitor;
    std::unique_ptr<ExprAST>condition_, then_, else_;
public:
    IfExprAST(std::unique_ptr<ExprAST>cond, 
        std::unique_ptr<ExprAST>then, 
        std::unique_ptr<ExprAST>Else) : condition_(std::move(cond)), 
            then_(std::move(then)), else_(std::move(Else)){}
    llvm::Value* ToLLvmValue(Visitor*v) override{
        return v->CodeGen(this);
    }
};

class ForExprAST : public ExprAST {
    friend class CodeGenVisitor;
    std::string varName_;
    std::unique_ptr<ExprAST>start_, end_, step_, body_;
public:
    ForExprAST(const std::string&varName, 
    std::unique_ptr<ExprAST>start, std::unique_ptr<ExprAST>end, 
    std::unique_ptr<ExprAST>step, std::unique_ptr<ExprAST>body) : 
    varName_(varName), start_(std::move(start)), end_(std::move(end)), 
    step_(std::move(step)), body_(std::move(body)) {}
    llvm::Value* ToLLvmValue(Visitor*v) override{
        return v->CodeGen(this);
    }
};


// 函数原型 包含函数名称以及参数名称
class PrototypeAST {
    friend class CodeGenVisitor;
    std::string name_;
    std::vector<std::string>args_name_;
    bool isOperator_;
    unsigned precedence_;
public:
    PrototypeAST(const std::string&name, 
    std::vector<std::string>&&args_name,
    bool isOperator = false, 
    unsigned precedence = 0) : name_(name), 
                               args_name_(std::move(args_name)),
                               isOperator_(isOperator),
                               precedence_(precedence){};
    llvm::Function* ToLLvmValue(Visitor*v)  {
        return v->CodeGen(this);
    }
    std::string_view GetFuncName() const {
        return name_;
    }
    bool isUnaryOp() const { return isOperator_ && args_name_.size() == 1; }
    bool isBinaryOp() const { return isOperator_ && args_name_.size() == 2; }
    std::string GetOperator() const {
        assert((isUnaryOp() || isBinaryOp()) && "prototype should be a unary or binary op");
        size_t index = name_.find_first_of('@');
        if(index >= name_.size()-1)
            return nullptr;
        return name_.substr(index + 1);
    }
    unsigned GetBinaryPrecedence() const {
        return precedence_;
    }
};

// 函数ast 包含一个函数原型以及函数体
class FunctionAST {
    friend class CodeGenVisitor;
    std::unique_ptr<PrototypeAST>proto_;
    std::unique_ptr<ExprAST>body_;
public:
    FunctionAST(std::unique_ptr<PrototypeAST>proto, 
    std::unique_ptr<ExprAST>body) : proto_(std::move(proto)), body_(std::move(body)){}
    llvm::Function* ToLLvmValue(Visitor*v)  {
        return v->CodeGen(this);
    }
};

class BlockExprAST : public ExprAST {
    friend class CodeGenVisitor;
    std::vector<std::unique_ptr<ExprAST>>body_;
public:
    BlockExprAST(std::vector<std::unique_ptr<ExprAST>>&&body) : body_(std::move(body)) {}
    llvm::Value* ToLLvmValue(Visitor*v)  {
        return v->CodeGen(this);
    }
};


}



