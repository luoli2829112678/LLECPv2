#pragma once
#include "LL_Master.h"
#include <sstream>
#include <stdexcept>

namespace LL {
// Fixed-assignment devices may reject every write to 1C12/1C13, even when unchanged.
inline int EnsurePdoAssignment(IMaster& master, std::uint16_t slave, std::uint16_t object,
                                const std::vector<std::uint32_t>& wanted,
                                bool initialize = false, bool readbackSupported = true) {
    auto fail=[&](const char* stage) {
        std::ostringstream message;
        message<<"Master "<<master.Config().id<<", slave "<<slave<<", PDO assignment 0x"
               <<std::hex<<object<<": "<<stage;
        const auto detail=master.LastError();
        if(!detail.empty())message<<"; "<<detail;
        throw std::runtime_error(message.str());
    };
    if(wanted.size()>255)fail("too many PDOs");
    for(auto index:wanted)if(index==0||index>65535)fail("invalid PDO index");
    auto matches=[&]() {
        // Some devices return an expedited 16/32-bit container for subindex zero.
        std::uint8_t value[4]{};int size=4;
        if(master.ReadSDO(slave,object,0,value,size,false,700000)<=0||size<1||size>4)return false;
        unsigned count=0;for(int i=0;i<size;++i)count|=static_cast<unsigned>(value[i])<<(8*i);
        if(count!=wanted.size())return false;
        for(std::size_t i=0;i<wanted.size();++i){
            std::uint8_t bytes[4]{};size=4;
            if(master.ReadSDO(slave,object,static_cast<std::uint8_t>(i+1),bytes,size,false,700000)<=0||size<2||size>4)return false;
            unsigned index=0;for(int byte=0;byte<size;++byte)index|=static_cast<unsigned>(bytes[byte])<<(8*byte);
            if(index!=wanted[i])return false;
        }
        return true;
    };
    const bool unchanged=readbackSupported&&matches();
    if(unchanged&&!initialize)return 0;
    auto write=[&](std::uint8_t sub,std::uint32_t value,int width) {
        const std::uint8_t bytes[]={static_cast<std::uint8_t>(value),static_cast<std::uint8_t>(value>>8),
                                   static_cast<std::uint8_t>(value>>16),static_cast<std::uint8_t>(value>>24)};
        return master.WriteSDO(slave,object,sub,bytes,width,false,700000)>0;
    };
    int countWidth=1;
    if(!write(0,0,countWidth)) {
        countWidth=4; // Compatibility with the reference controller's writeSDO_INT.
        if(!write(0,0,countWidth)) {
            if(unchanged)return 0; // Proven unchanged, read-only assignment.
            fail("cannot clear assignment using UINT8 or INT32");
        }
    }
    for(std::size_t i=0;i<wanted.size();++i){
        const auto sub=static_cast<std::uint8_t>(i+1);
        const int width=countWidth==4?4:2;
        if(!write(sub,wanted[i],width)&&!(width!=4&&write(sub,wanted[i],4)))fail("cannot write PDO index using UINT16 or INT32");
    }
    if(!write(0,static_cast<std::uint32_t>(wanted.size()),countWidth))fail("cannot enable assignment");
    if(readbackSupported&&!matches())fail("assignment readback differs from the project");
    return countWidth;
}
}
