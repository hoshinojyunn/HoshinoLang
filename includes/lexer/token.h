#pragma once

#include "ast/basic_ast.h"
#include <memory>
#include <ostream>
#include <string>
#include <unordered_map>
#include <unordered_set>
class Token;
enum TokenNum{
    TOK_EOF = -1,
    TOK_DEF = -2,
    TOK_EXTERN = -3,
    TOK_IDENTIFIER = -4,
    TOK_NUMBER = -5,
    TOK_IF = -6,
    TOK_THEN = -7,
    TOK_ELSE = -8,
    TOK_FOR = -9,
    TOK_BINARY = -10,
    TOK_UNARY = -11,
    TOK_VAR = -12,
    TOK_EXPR_END = -13,
};

class Token{
    int tok;
public:
    Token() = default;
    // Token(const Token&) = default;
    // Token(Token&&) = default;
    Token(int num){
        this->tok = num;
    }
    int GetTokNum(){
        return tok;
    }
    operator int(){
        return tok;
    }

    friend std::ostream& operator<< (std::ostream&out, Token&tok){
        return out << tok.GetTokNum();
    }
};


inline std::string identifierStr;
inline double numVal;
inline Token curTok;
inline std::unordered_map<std::string, int>binOpPrecedence;
inline std::unordered_set<std::string>validBinOp;

inline void InitBinOpPrecedence(){
    binOpPrecedence["="] = 2;
    binOpPrecedence["<"] = 10;
    binOpPrecedence["+"] = 20;
    binOpPrecedence["-"] = 20;
    binOpPrecedence["*"] = 40;
}

inline void InitValidBinOpSet(){
    {
        validBinOp.insert("=");
        validBinOp.insert("+");
        validBinOp.insert("-");
        validBinOp.insert("*");
        validBinOp.insert("/");
        validBinOp.insert("%");
        validBinOp.insert(">");
        validBinOp.insert("<");
        validBinOp.insert("^");
        validBinOp.insert("&");
        validBinOp.insert("|");
        validBinOp.insert(",");
    }
    {
        validBinOp.insert("==");
        validBinOp.insert("!=");
        validBinOp.insert("+=");
        validBinOp.insert("-=");
        validBinOp.insert("*=");
        validBinOp.insert("/=");
        validBinOp.insert("%=");
        validBinOp.insert(">=");
        validBinOp.insert("<=");
        validBinOp.insert("^=");
        validBinOp.insert("&=");
        validBinOp.insert("|=");
        validBinOp.insert("&&");
        validBinOp.insert("||");
        validBinOp.insert("<<");
        validBinOp.insert(">>");
    }
    {
        validBinOp.insert("<<=");
        validBinOp.insert(">>=");
    }
}

constexpr const char* anonymous_expr_name = "__anon_expr";

// extern 
extern int GetNextToken();
extern std::unique_ptr<kalei::FunctionAST> ParseTopLevelExpr();
extern std::unique_ptr<kalei::FunctionAST>ParseDefinition();
extern std::unique_ptr<kalei::PrototypeAST> ParseExtern();



