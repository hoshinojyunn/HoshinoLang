#pragma once
#include "ast/basic_ast.h"
#include <llvm/IR/Value.h>
#include <memory>


inline auto log_err(const char *str) -> std::unique_ptr<kalei::ExprAST>{ 
    fprintf(stderr, "Error: %s\n", str); 
    fflush(stderr);
    return nullptr; 
}

inline auto log_err_p(const char *str) -> std::unique_ptr<kalei::PrototypeAST>{
    log_err(str);
    return nullptr;
}

inline auto log_err_v(const char *str) -> llvm::Value* {
    log_err(str);
    return nullptr;
}



#define LOG_ERROR(msg) log_err(msg)
#define LOG_ERROR_P(msg) log_err_p(msg)
#define LOG_ERROR_V(msg) log_err_v(msg)


#define LOG_INFO(...)               \
    ::fprintf(stdout, __VA_ARGS__); \
    fprintf(stdout, "\n");          \
    ::fflush(stdout)


#define IS_ASCII(c) ((c & ~0x7f) == 0)
