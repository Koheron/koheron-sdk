// Include the production shared driver, bypassing the instrument DMA test stub.
#include "../../../../server/drivers/dma-s2mm.hpp"
#include <cassert>
#include <iostream>
#include <type_traits>
#include <utility>

static_assert(std::is_same_v<decltype(std::declval<DmaS2MM>().wait_for_transfer(0.f)), void>);
static_assert(std::is_same_v<decltype(std::declval<DmaS2MM>().wait_for_transfer(scicpp::units::time<double>{})), void>);
static_assert(std::is_same_v<decltype(std::declval<DmaS2MM>().wait_for_transfer_checked(0.f)), bool>);

int main() {
    DmaS2MM driver;
    auto& status = hw::get_memory<mem::dma>().registers[0x34 / 4];
    status = 0x2; // idle, no errors
    driver.wait_for_transfer(0.f);
    driver.wait_for_transfer(scicpp::units::time<double>{0.});
    assert(driver.wait_for_transfer_checked(0.f));
    status = 0; // busy until timeout
    assert(!driver.wait_for_transfer_checked(0.f));
    status = 0x10; // DMA error while busy
    assert(!driver.wait_for_transfer_checked(0.f));
    status = 0x12; // DMA error even though idle
    assert(!driver.wait_for_transfer_checked(0.f));
    std::cout << "Shared DMA API compatibility and checked-status tests passed\n";
}
