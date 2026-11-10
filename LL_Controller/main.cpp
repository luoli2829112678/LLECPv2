#include "LL_ControllerRuntime.h"
#include "HSTR/HSTR.hpp"
#include <csignal>
#include <chrono>
#include <iostream>
#include <thread>
namespace { volatile std::sig_atomic_t quit=0; void stop(int){quit=1;} }
int main(int argc,char** argv) {
    int compilerResult=0;
    if(HSTR::HandleCompilerInvocation(argc,argv,compilerResult))return compilerResult;
    if(argc>1&&std::string(argv[1])=="--help") {
        std::cout<<"Usage: LL_Controller [runtime-directory]\n";return 0;
    }
    std::signal(SIGINT,stop);std::signal(SIGTERM,stop);std::signal(SIGPIPE,SIG_IGN);
    try {
        if(!LL_InitializeController(argc>1?argv[1]:"runtime"))return 1;
        std::cout<<"LL_Controller ready; register a master before starting it.\n";
        while(!quit)std::this_thread::sleep_for(std::chrono::milliseconds(100));
        LL_ShutdownController();return 0;
    } catch(const std::exception& error){std::cerr<<error.what()<<'\n';LL_ShutdownController();return 1;}
}
