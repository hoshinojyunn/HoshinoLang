#pragma once
#include "ast/basic_ast.h"
#include <condition_variable>
#include <cstdio>
#include <llvm-14/llvm/ADT/StringRef.h>
#include <llvm/IR/Value.h>
#include <memory>
#include <mutex>


inline auto log_err(const char *str) -> std::unique_ptr<hoshino::ExprAST>{ 
    fprintf(stderr, "Error: %s\n", str); 
    fflush(stderr);
    return nullptr; 
}

inline auto log_err_p(const char *str) -> std::unique_ptr<hoshino::PrototypeAST>{
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

static std::mutex outputMutex;


#define LOG_INFO(...)               \
[](){                               \
    std::lock_guard<std::mutex>(outputMutext); \
    ::fprintf(stdout, __VA_ARGS__); \
    fprintf(stdout, "\n");          \
    ::fflush(stdout)                \
}()


#define IS_ASCII(c) ((c & ~0x7f) == 0)

struct DebugOStream {

    friend DebugOStream& operator<< (DebugOStream&os, llvm::StringRef s) {
        os.Lock();
        ::fprintf(stdout, "%s", s.data());
        ::fflush(stdout);
        os.UnLock();
        return os;
    }
    void Lock(){
        mtx_.lock();
    }
    void UnLock(){
        mtx_.unlock();
    }
private:
    std::mutex mtx_;
    std::condition_variable cv_;
};