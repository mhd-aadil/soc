
#ifndef BUS_H
#define BUS_H

#include <cstdint>
#include <string>
#include <vector>

class IBusSlave {
public:
    virtual ~IBusSlave() = default;

    virtual void bus_access(uint32_t addr,
                            uint32_t& data,
                            bool is_write) = 0;
};

class Bus {
public:
    struct Region {
        std::string name;
        uint32_t base;
        uint32_t size;
        IBusSlave* slave;
    };

    void register_slave(const std::string& name,
                        uint32_t base,
                        uint32_t size,
                        IBusSlave* slave) {
        regions.push_back({name, base, size, slave});
    }

    bool transact(uint32_t addr,
                  uint32_t& data,
                  bool is_write) {
        for (const auto& region : regions) {
            if (addr >= region.base &&
                uint64_t(addr) < uint64_t(region.base) + region.size) {
                region.slave->bus_access(
                    addr - region.base, data, is_write);
                return true;
            }
        }

        if (!is_write)
            data = 0;

        return false;
    }

    bool write(uint32_t addr, uint32_t data) {
        return transact(addr, data, true);
    }

    bool read(uint32_t addr, uint32_t& data) {
        return transact(addr, data, false);
    }

private:
    std::vector<Region> regions;
};

#endif
