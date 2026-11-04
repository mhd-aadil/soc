#ifndef RAM_H
#define RAM_H

#include <systemc>
#include <array>
#include <cstdint>

#include "bus.h"

class Ram : public sc_core::sc_module, public IBusSlave
{
public:
static constexpr uint32_t RAM_BASE=0X30000000;
static constexpr uint32_t RAM_SIZE=4096;

Ram(sc_core::sc_module_name name, Bus& bus);

void bus_access(uint32_t addr,
                    uint32_t& data,
                    bool is_write) override;

uint8_t debug_read_byte(uint32_t addr) const;

private:
    std::array<uint8_t, RAM_SIZE> memory{};
};

#endif