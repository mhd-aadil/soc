
#include <systemc>
#include <cassert>
#include <cstdint>
#include <iostream>

#include "ram.h"

int sc_main(int argc, char* argv[]) {
    Bus bus;
    Ram ram("ram", bus);

    uint32_t value = 0;

    // Test 1: Basic write and read.
    assert(bus.write(0x30000000, 0x12345678));
    assert(bus.read(0x30000000, value));
    assert(value == 0x12345678);

    std::cout << "Test 1 passed: basic read/write\n";

    // Test 2: Independent address.
    assert(bus.write(0x30000010, 0xAABBCCDD));
    assert(bus.read(0x30000010, value));
    assert(value == 0xAABBCCDD);

    // Confirm the first word was not changed.
    assert(bus.read(0x30000000, value));
    assert(value == 0x12345678);

    std::cout << "Test 2 passed: independent locations\n";

    // Test 3: Last valid 32-bit word.
    assert(bus.write(0x30000FFC, 0xDEADBEEF));
    assert(bus.read(0x30000FFC, value));
    assert(value == 0xDEADBEEF);

    std::cout << "Test 3 passed: last valid word\n";

    // Test 4: Out-of-range offset.
    // Call RAM directly because the bus does not map this offset.
    value = 0xFFFFFFFF;
    ram.bus_access(4096, value, false);
    assert(value == 0);

    // An out-of-range write must not damage valid data.
    value = 0xCAFEBABE;
    ram.bus_access(4096, value, true);

    assert(bus.read(0x30000000, value));
    assert(value == 0x12345678);

    std::cout << "Test 4 passed: out-of-range safety\n";

    // Test 5: Unmapped system address.
    value = 0xFFFFFFFF;
    assert(!bus.read(0x40000000, value));
    assert(value == 0);
    //debug_read_byte() 
    assert(bus.write(0x30000000, 0x12345678));
    assert(ram.debug_read_byte(0) == 0x78);
    std::cout << "Endianness test passed!\n";

    std::cout << "All RAM tests passed!\n";
    return 0;
}
