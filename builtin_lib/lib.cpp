#include "lib.h"
#include <cstdio>
#include <llvm/ExecutionEngine/Orc/ThreadSafeModule.h>
#include <llvm/IR/Module.h>


extern "C" DLLEXPORT double putchard(double x){
    fputc((char)x, stderr);
    return 0;
}

extern "C" DLLEXPORT double tab(){
    return putchard(9);
}

extern "C" DLLEXPORT double endl() {
    return putchard(10);
}

extern "C" DLLEXPORT double printNum(double x){
    char outputs[10] = {};
    int num = (int)x;
    sprintf(outputs, "%d", num);
    fputs(outputs, stderr);
    return 0;
}
