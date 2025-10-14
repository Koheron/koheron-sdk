// Zynq PL Fabric Clocks settings
// (c) Koheron

#ifndef __SERVER_CONTEXT_ZYNQ_FCLK__
#define __SERVER_CONTEXT_ZYNQ_FCLK__

#include <string>
#include <cstdint>
#include <filesystem>

namespace hw {

class ZynqFclk {
    using Path = std::filesystem::path;
  public:
    void set(const std::string& fclk_name, uint32_t fclk_rate) {
        set_impl(fclk_name, fclk_rate, true);
    }

    void set_if_devcfg(const std::string& fclk_name, uint32_t fclk_rate) {
        set_impl(fclk_name, fclk_rate, false);
    }

  private:
    const Path devcfg = "/sys/devices/soc0/amba/f8007000.devcfg";
    const Path amba_clocking = "/sys/devices/soc0/fpga-region/fpga-region:clocking";

    void set_impl(const std::string& fclk_name, uint32_t fclk_rate, bool update_rate);

    // ------------------------------------------------------------------------
    // Use devcfg
    // ------------------------------------------------------------------------

    void set_fclk_devcfg(const std::string& fclk_name, uint32_t fclk_rate);
    int fclk_export(const std::string& fclk_name, const Path& fclk_dir_name);
    int fclk_enable(const std::string& fclk_name, const Path& fclk_dir_name);
    int fclk_set_rate(const std::string& fclk_name,
                      const Path& fclk_dir_name,
                      uint32_t fclk_rate);

    // ------------------------------------------------------------------------
    // Use amba_clocking
    // ------------------------------------------------------------------------

    void set_fclk_amba_clocking(const Path& clkdir, char clkid,
                                uint32_t fclk_rate, bool update_rate);
    int amba_clocking_set_rate(const Path& clkdir, char clkid, uint32_t fclk_rate);
    long amba_clocking_get_rate(const Path& clkdir);
}; // class ZynqFclk

} // namespace hw

#endif // __SERVER_CONTEXT_ZYNQ_FCLK__
