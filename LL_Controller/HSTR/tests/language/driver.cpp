#include <HSTR/HSTR.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
namespace fs = std::filesystem;
int main(int argc, char** argv) {
  int code=0;
  if(HSTR::HandleCompilerInvocation(argc,argv,code)) return code;
  try {
    if(argc!=3) return 2;
    HSTR::Runtime r({argv[1],"cc"});
    r.AddApplication("Test"); r.SelectApplication("Test");
    r.AddProgram("Main"); r.AddTask("Scan");
    fs::path source=argv[2], app=fs::path(argv[1])/"Test";
    fs::copy_file(source, app/"Main.st",fs::copy_options::overwrite_existing);
    for(auto pair : {std::make_pair("Types.typ",0),std::make_pair("Helpers.fun",1)}) {
      if(fs::exists(source.parent_path()/pair.first)) {
        if(pair.second) r.AddFunction("Helpers"); else r.AddTypeFile("Types");
        fs::copy_file(source.parent_path()/pair.first,app/(pair.second ? "Helpers.st.fun" : pair.first),fs::copy_options::overwrite_existing);
      }
    }
    std::ofstream(app/"Scan.task") << R"({"name":"Scan","type":"Cyclic","intervalUs":1000,"priority":0,"watchdogUs":0,"cpuAffinity":-1,"enabled":true,"programs":["Main"]})";
    r.CompileProgram(); r.ExecuteProgram();
    if(r.Cycle(0)!=HSTR::ApplicationStatus::Running) throw std::runtime_error(r.GetLastError());
    if(!r.ReadVariable<bool>("MAIN_INSTANCE.OK")) {
      for(const auto& v : r.GetVariableList()) {
        if(v.type == "ULINT") std::cerr << v.name << "=" << r.ReadVariable<uint64_t>(v.name) << '\n';
      }
      throw std::runtime_error("ST assertion failed");
    }
    for (const auto& v : r.GetVariableList()) {
      if(v.name.find("MAIN_INSTANCE.RECORDVALUE") != std::string::npos || v.name.find("MAIN_INSTANCE.ARRAYVALUE") != std::string::npos) {
        auto result=v.name.find("MAIN_INSTANCE.RECORDVALUE") != std::string::npos ? "MAIN_INSTANCE.RECORDSIZE" : "MAIN_INSTANCE.ARRAYSIZE";
        if(r.ReadVariable<uint64_t>(result)!=v.size) throw std::runtime_error("SIZEOF/ABI mismatch");
      }
    }
    r.StopProgram(); std::cout << "PASS " << source.filename() << '\n';
    return 0;
  } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
