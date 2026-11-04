#include<systemc>
#include <cassert>
#include <cstdint>
#include <iostream>

#include "ram.h"

int sc_main(int argc,char* argv[]){
    Bus bus;
    Ram ram("ram",bus);
    uint32_t valuein=0x12345678;
    uint32_t valueout=0;
    //Test 1
    assert(bus.write(0x30000000, valuein));
    assert(bus.read(0x30000000,valueout));

    if(valuein==valueout)
    std::cout<<"value equal test1 passed"<<'\n';
    else
    std::cout<<"value not equal test1 failed"<<'\n';

    //Test 2 
    //multiple writes and read
    assert(bus.write(0x30000004, 0xa));
    assert(bus.write(0x30000008, 0xb));
    assert(bus.write(0x3000000c, 0xc));
    assert(bus.write(0x30000010, 0xd));
    uint32_t value;
    assert(bus.read(0x3000000c,value));
    std::cout<<std::hex<<value<<'\n';
    //Test 3
    //out of range access
    assert(!bus.read(0x30005000, value));
    std::cout<<value<<'\n';
    std::cout<<"Test 3 passed: Out of range read failed as expected"<<'\n';
    //Test 4
    //out of range access
    assert(!bus.write(0x30005000, 0xdeadbeef));
    std::cout<<"Test 4 passed: Out of range write failed as expected"<<'\n';
    
    return 0;

}