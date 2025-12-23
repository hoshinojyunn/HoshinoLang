#include "lexer/token.h"
#include "ast/basic_ast.h"
#include "tools/basic_tool.h"
#include <cctype>
#include <charconv>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include "context.h"

using namespace hoshino;

// static announce
static Token GetTok();
static std::unique_ptr<ExprAST> ParseExpression();
static std::unique_ptr<ExprAST>ParseNumberExpr();
static std::unique_ptr<ExprAST>ParseStrExpr();
static std::unique_ptr<ExprAST>ParseVarExpr();
static std::unique_ptr<ExprAST> ParseParenExpr();
static std::unique_ptr<ExprAST> ParsePrimary();
static std::unique_ptr<ExprAST>ParseIdentifierExpr();
static std::unique_ptr<ExprAST> ParseBinOpRHS(int, std::unique_ptr<ExprAST>);
static std::unique_ptr<ExprAST> ParseUnary();
static std::unique_ptr<ExprAST> ParseBlockExpr();
static std::unique_ptr<ExprAST> ParseIfExpr();
static std::unique_ptr<ExprAST> ParseForExpr();
static std::unique_ptr<PrototypeAST> ParsePrototype();
static std::unique_ptr<ExprAST> ParseBinOpRHS(int exprPrece, 
                std::unique_ptr<ExprAST>lhs);
static int GetTokPrecedence();

static int lastChar = ' ';

static int globalFuncCounting = 0;

static int GetChar(){
    return sourceInput->get();
}
static Token GetTok(){
    while(std::isspace(lastChar)){
        lastChar = GetChar();
        // if(lastChar == 125){
        //     std::cout << "there!!!" << '\n';
        // }
    }
    // token以字母开头
    if(std::isalpha(lastChar)){
        identifierStr = lastChar;
        while(std::isalnum(lastChar = GetChar()) || lastChar == '_'){
            identifierStr += lastChar;
        } // token解析完成 一直解析到当前字符不是数字或字母为止

        if(identifierStr == "def")
            return Token{TokenNum::TOK_DEF};
        if(identifierStr == "extern")
            return Token{TokenNum::TOK_EXTERN};
        if(identifierStr == "if")
            return Token{TokenNum::TOK_IF};
        if(identifierStr == "then")
            return Token{TokenNum::TOK_THEN};
        if(identifierStr == "else")
            return Token{TokenNum::TOK_ELSE};
        if(identifierStr == "for")
            return Token{TokenNum::TOK_FOR};
        if(identifierStr == "binary")
            return Token{TokenNum::TOK_BINARY};
        if(identifierStr == "unary")
            return Token{TokenNum::TOK_UNARY};
        if(identifierStr == "var")
            return Token{TokenNum::TOK_VAR};
        return Token{TokenNum::TOK_IDENTIFIER};
    }
    // token以数字开头
    if(std::isdigit(lastChar) || lastChar == '.'){
        identifierStr = lastChar;
        while(std::isdigit(lastChar = GetChar()) || lastChar == '.'){
            identifierStr += lastChar;
        }
        std::from_chars(
        identifierStr.data(), 
        identifierStr.data()+identifierStr.size(), numVal);
        return Token{TokenNum::TOK_NUMBER};
    }
    if(lastChar == '"'){
        lastChar = GetChar(); // eat "
        identifierStr = lastChar;
        while ((lastChar = GetChar()) != '"') {
            identifierStr += lastChar;
        }
        lastChar = GetChar(); // eat "
        return Token{TokenNum::TOK_STR};
    }
    // 注释
    if(lastChar == '#'){
        lastChar = GetChar();
        // 一直解析到该行注释的末尾
        while(lastChar!= EOF && lastChar != '\n' && lastChar != '\r'){
            lastChar = GetChar();
        }
        // 略过注释 继续解析 
        if(lastChar != EOF)
            return GetTok();
    }
    if(lastChar == ';'){
        lastChar = GetChar();
        if(lastChar != EOF)
            return Token{TokenNum::TOK_EXPR_END};
    }
    if(lastChar == EOF){
        return Token{TokenNum::TOK_EOF};
    }
    
    // 到这里 就说明解析到的token是非法的 返回未定义字符
    int thisChar = lastChar;
    lastChar = GetChar();
    return Token{thisChar};
}

int GetNextToken(){
    return curTok = GetTok();
}

static std::string ReadNTok(size_t n){
    if(curTok == TOK_EOF)
        return {};
    int tempChar = lastChar;
    std::string tempIdentifier = identifierStr;
    int tempNum = numVal;
    std::string res{};
    auto curPos = sourceInput->tellg();
    // std::cout << "current pos is: " << curPos << '\n'; 
    // 当前curTok指向op的第一个字符
    res += (char)curTok;
    int len = n;
    while(--len) {
        if(char c = (char)GetTok();c!=EOF){
            res += c;
        }else{
            // 当文件流到末尾时 eof标志位会置1 此时seekg无法生效 需要清空标志位
            sourceInput->clear();
            break;
        }
    }
    // 回到读出n个byte之前的位置
    sourceInput->seekg(curPos);
    // std::cout << "seekg to current pos, now: " << sourceInput->tellg() << '\n';
    identifierStr = tempIdentifier;
    numVal = tempNum;
    lastChar = tempChar;
    return res;
}

static std::string VerifyBinaryOp(){
    // 双目运算符长度最大为3 (<<= 左移赋值运算符)
    std::string op2 = ReadNTok(3);
    if(auto it = validBinOp.find(op2); it!=validBinOp.end()){
        return op2;
    }
    std::string op1 = op2.substr(0, 2);
    if(auto it = validBinOp.find(op1); it!=validBinOp.end()){
        return op1;
    }
    std::string op0 = op2.substr(0, 1);
    if(auto it = validBinOp.find(op0); it!=validBinOp.end()){
        return op0;
    }
    return {};
}

static std::string GetBinaryOp(){
    std::string op = VerifyBinaryOp();
    if(!op.empty()){
        int len = op.size();
        while(len--)
            GetNextToken();
    }
    return op;
}


static std::unique_ptr<ExprAST>ParseNumberExpr(){
    auto result = std::make_unique<NumberExprAST>(numVal);
    GetNextToken();
    return std::move(result);
}

static std::unique_ptr<ExprAST>ParseStrExpr(){
    auto result = std::make_unique<StrExprAST>(identifierStr);
    GetNextToken();
    return std::move(result);
}

// 括号
static std::unique_ptr<ExprAST> ParseParenExpr(){
    GetNextToken(); // eat (
    auto expr = ParseExpression();
    if(!expr)
        return nullptr;
    if(curTok != ')')
        return LOG_ERROR("expected ')'");
    GetNextToken(); // eat )

    return expr;
}

/*
    检查标识符是独立变量引用还是函数调用表达式
    独立变量标识符: identifier后没有(
    函数调用表达式: identifier后跟着(
*/
static std::unique_ptr<ExprAST>ParseIdentifierExpr(){
    std::string idName = identifierStr;
    GetNextToken(); // eat identifier
    if(curTok != '(')
        return std::make_unique<VariableExprAST>(idName);
    GetNextToken(); // eat (
    //  到这里可以确定是函数调用表达式
    std::vector<std::unique_ptr<ExprAST>>args;
    while(curTok != ')'){
        if(auto arg = ParseExpression(); arg != nullptr)
            args.push_back(std::move(arg));
        else
            return nullptr;
        if(curTok == ')')
            break;
        if(curTok != ',')
            return LOG_ERROR("expected ')' or ',' in argment list");
        GetNextToken();
    }
    GetNextToken(); // eat )
    return std::make_unique<CallExprAST>(idName, std::move(args));
}

// identifier number paren if
static std::unique_ptr<ExprAST> ParsePrimary(){
    switch (curTok) {
        case TOK_IDENTIFIER:
            return ParseIdentifierExpr();
        case TOK_NUMBER:
            return ParseNumberExpr();
        case TOK_STR:
            return ParseStrExpr();
        case TOK_IF:
            return ParseIfExpr();
        case TOK_FOR:
            return ParseForExpr();
        case TOK_VAR:
            return ParseVarExpr();
        case '(':
            return ParseParenExpr();
        case '{':
            return ParseBlockExpr();
        default:
            return LOG_ERROR("unknow token when expecting an expression");
    }
}

static std::unique_ptr<ExprAST> ParseVarExpr(){
    GetNextToken(); // eat var
    if(curTok != TOK_IDENTIFIER)
        return LOG_ERROR("expected identifier after var");
    std::string varName = identifierStr;
    GetNextToken(); // eat identifier
    std::unique_ptr<ExprAST>initVal;
    if(curTok == '='){
        GetNextToken(); // eat =
        initVal = ParseExpression();
        if(!initVal)
            return nullptr;
    }
    return std::make_unique<VarExprAST>(varName, std::move(initVal));
}


static int GetTokPrecedence(){
    // if(!IS_ASCII(curTok))
    //     return -1;
    std::string op = VerifyBinaryOp();
    if(op.empty())
        return -1;
    if(auto it = binOpPrecedence.find(op);
    it != binOpPrecedence.end()){
        return it->second;
    }
    return -1;
}

static std::unique_ptr<ExprAST> ParseExpression(){
    auto lhs = ParseUnary();
    if(!lhs)
        return nullptr;
    return  ParseBinOpRHS(0, std::move(lhs));
}

/*
    运算符优先级分析
    （其实可以用中缀转后缀表达式分析的）
*/
static std::unique_ptr<ExprAST> ParseBinOpRHS(int exprPrece, 
                std::unique_ptr<ExprAST>lhs){
    // 当前curTok应指向一个二元运算符
    while(true){
        int tokPrece = GetTokPrecedence();
        // 如果当前运算符的优先级比先前表达式的优先级小 就可以直接返回了
        /* 
            比如 a+(b*c)+d parser解析到b*c时 exprPrece为上一个运算的优先级 
            即+的优先级再+1 再次往下解析时发现是+运算 优先级小于先前的+运算 
            此时可以直接返回b*c 返回结果再与前面的a运算 最后再与d运算
        */
        if(tokPrece < exprPrece)
            return lhs;
        // int binOp = curTok;
        std::string binOp = GetBinaryOp(); // eat binOp
        auto rhs = ParseUnary();
        if(!rhs)
            return nullptr;
        int nextPrece = GetTokPrecedence();
        // 如果rhs下一个二元运算符优先级比当前运算符优先级高
        // 说明rhs应该先与后面的表达式进行计算
        if(tokPrece < nextPrece){
            rhs = ParseBinOpRHS(tokPrece+1, std::move(rhs));
            if(!rhs)
                return nullptr;
        }
        // 如果rhs下一个二元运算符优先级比当前运算符优先级低或相等
        // 那么说明lhs与rhs就可以先进行运算了 即(lhs op rhs)
        // 合并lhs rhs
        lhs = std::make_unique<BinaryExprAST>(binOp, std::move(lhs), std::move(rhs));
    }
}

/*
    解析单目运算表达式或普通操作数
*/
static std::unique_ptr<ExprAST> ParseUnary(){
    /*
        若curTok不是operator 则它是一个操作数
        若curTok为Token中定义的枚举值 则其不是一个ascii码(Token中的枚举值都是负数)
        又或者单目运算符后面是一个括号表达式
    */
    if(!IS_ASCII(curTok) || curTok == '(' || curTok == ',' || curTok == '{')
        return ParsePrimary();
    //
    int opCode = curTok;
    GetNextToken(); // eat op
    // 单目运算符后的操作数 递归使用ParseUnary 
    // 因为可能会有多次调用单目运算符 比如 !!x
    if(auto operand = ParseUnary())
        return std::make_unique<UnaryExprAST>(opCode, std::move(operand));
    return nullptr;
}

std::unique_ptr<ExprAST> ParseIfExpr(){
    GetNextToken(); // eat if
    auto condition = ParseExpression(); // parse condition
    if(!condition)
        return nullptr;
    // 当前curTok应该指向then
    // if(curTok != TOK_THEN)
    //     return LOG_ERROR("expected then after condition");
    // GetNextToken(); // eat then
    // if(curTok != '{')
    //     return LOG_ERROR("expected '{' after 'then'");
    // auto then = ParseBlockExpr();
    auto then = ParseExpression(); // then之后的表达式
    if(!then)
        return nullptr;
    if(curTok == TOK_EXPR_END)
        GetNextToken(); // eat ;
    std::unique_ptr<ExprAST>Else;
    if(curTok == TOK_ELSE) {
        GetNextToken(); // eat else
        Else = ParseExpression();
        if(!Else)
            return nullptr;
        if(curTok == TOK_EXPR_END)
            GetNextToken(); // eat ;
    }
    return std::make_unique<IfExprAST>(std::move(condition), std::move(then), std::move(Else));

}

static std::unique_ptr<ExprAST> ParseForExpr(){
    GetNextToken(); // eat for
    if(curTok != TOK_IDENTIFIER)
        return LOG_ERROR("expected identifier after for");
    auto idName = identifierStr;
    GetNextToken(); // eat initial variable
    if(curTok != '=')
        return LOG_ERROR("expected '=' after identifier in for expr");
    GetNextToken(); // eat =
    // 初始值
    auto start = ParseExpression();
    if(!start)
        return nullptr;
    if(curTok != TOK_EXPR_END)
        return LOG_ERROR("expected ';' after for start value");
    GetNextToken(); // eat ;
    auto end = ParseExpression();
    if(!end)
        return nullptr;
    std::unique_ptr<ExprAST>step;
    if(curTok != TOK_EXPR_END)
        return LOG_ERROR("expected ';' after for end value");
    // step value is optional
    GetNextToken(); // eat ;
    if(curTok != '{'){
        step = ParseExpression();
        if(!step)
            return nullptr;
    }
    if(curTok != '{')
        return LOG_ERROR("expected '{' after for's step");
    // GetNextToken(); // eat then
    auto body = ParseExpression();
    if(!body)
        return nullptr;
    if(curTok == TOK_EXPR_END)
        GetNextToken(); //eat ;
    return std::make_unique<ForExprAST>(idName, 
        std::move(start), std::move(end), std::move(step), std::move(body));

}


// 解析函数原型
static std::unique_ptr<PrototypeAST> ParsePrototype(){
    
    std::string fnName;
    /*
        if 0: identifier
           1: unary
           2: binary
    */
    unsigned kindOfProto = 0; 
    unsigned binaryPrece = 30;
    switch (curTok) {
    case TOK_IDENTIFIER:    
        fnName = identifierStr;
        kindOfProto = 0;
        GetNextToken(); // eat fnName
        break;
    case TOK_UNARY:
        GetNextToken(); // eat unary
        if(curTok != '@')
            return LOG_ERROR_P("expected '@' after unary");
        fnName = "unary@";
        kindOfProto = 1;
        GetNextToken(); // eat @
        fnName += (char)curTok;
        GetNextToken(); // eat op
        break;
    case TOK_BINARY: {
        GetNextToken(); // eat binary
        if(curTok != '@')
            return LOG_ERROR_P("expected '@' after binary");
        fnName = "binary@";
        // operatorStr += (char)curTok; // operator 
        kindOfProto = 2;
        GetNextToken(); // eat @
        // TODO: 解析到优先级数字为止 未来支持多字符双目运算符
        std::string binOp = GetBinaryOp();
        fnName += binOp;
        if(curTok == TOK_NUMBER){
            if(numVal < 1 || numVal > 100)
                return LOG_ERROR_P("Invalid precedence, it must be 1..100");
            binaryPrece = (unsigned)numVal;
            GetNextToken(); // eat precedence
        }
        break;
    }
    default:
        return LOG_ERROR_P("expected function name in prototype");
    }
    if(curTok != '(')
        return LOG_ERROR_P("Expected '(' in prototype");
    std::vector<std::string>args;
    // eat (
    while(GetNextToken() == TOK_IDENTIFIER){ 
        args.push_back(identifierStr);
    }
    if(curTok != ')')
        return LOG_ERROR_P("Expected ')' in prototype");
    GetNextToken(); // eat )
    if(kindOfProto && args.size()!=kindOfProto)
        return LOG_ERROR_P("Invalid number of operands for operator");
    return std::make_unique<PrototypeAST>(
        fnName, std::move(args),
         kindOfProto!=0, binaryPrece);
}

std::unique_ptr<ExprAST> ParseBlockExpr(){
    if(curTok != '{')
        return LOG_ERROR("expected '{' while parse block expression");
    GetNextToken(); // eat {
    std::vector<std::unique_ptr<ExprAST>>body;
    while(curTok != '}'){
        auto expr = ParseExpression();
        // 一个表达式可能会以}结尾 比如if for
        bool isIfExpr = false;
        bool isForExpr = false;
        if(expr.get()){
            auto&tmpExpr =  *expr.get();
            // typeid的参数应该是一个纯粹的类型表达式 
            // 因此最好不要使用求值表达式 如：*expr等
            isIfExpr = (typeid(tmpExpr) == typeid(IfExprAST));
            isForExpr = (typeid(tmpExpr) == typeid(ForExprAST));
        }
        // auto temp1 = dynamic_cast<IfExprAST*>(expr.get());
        // auto temp2 = dynamic_cast<ForExprAST*>(expr.get());
        // 如果解析出的表达式不是If或For表达式
        if(!isIfExpr && !isForExpr){
            if(curTok != TOK_EXPR_END)
                return LOG_ERROR("exptected ';' after an expression when parsing block");
            GetNextToken(); // eat ; or }
            // 一个表达式结尾是;或者是}
        }
        body.push_back(std::move(expr));
        
    }
    GetNextToken(); // eat }
    return std::make_unique<BlockExprAST>(std::move(body));
}

std::unique_ptr<FunctionAST>ParseDefinition(){
    GetNextToken(); // eat def
    auto proto = ParsePrototype();
    if(!proto)
        return nullptr;
    if(auto body = ParseExpression())
        return std::make_unique<FunctionAST>(std::move(proto), std::move(body));
    else{
        LOG_ERROR("expected function body when parsing definition");
        return nullptr;
    }
}

std::unique_ptr<PrototypeAST> ParseExtern(){
    GetNextToken(); // eat extern
    return ParsePrototype();
}

/*
    将顶层表达式转化为匿名函数
*/
std::unique_ptr<FunctionAST> ParseTopLevelExpr(std::string&anonFuncName){
    if(auto expr = ParseExpression(); expr!=nullptr){
        anonFuncName = anonymous_expr_name + std::string("_") + std::to_string(globalFuncCounting++);
        // make an empty proto
        auto proto = std::make_unique<PrototypeAST>(
            anonFuncName, std::vector<std::string>{});
        return std::make_unique<FunctionAST>(std::move(proto), std::move(expr));
    }
    return nullptr;
}



// a+b+(c+d)*e*f+g


