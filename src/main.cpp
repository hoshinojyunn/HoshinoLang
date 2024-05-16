#include "context.h"
#include <cassert>


int main(int argc, char* argv[]){
    // for(int i=0;i<argc;++i){
    //     std::cout << argv[i] << '\n';
    // }
    assert((argc > 1) && "should specify a source file name");
    std::string sourceFile = argv[1];
    SettingContext(sourceFile);
    MainLoop();
    ContextClose();
}