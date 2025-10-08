// Zynq PL Fabric Clocks settings
// (c) Koheron

#ifndef __SERVER_CONTEXT_ZYNQ_FCLK__
#define __SERVER_CONTEXT_ZYNQ_FCLK__

#include <string>
#include <cstdint>
#include <filesystem>

enum class FclkIntent {
    CheckOnly,          // validate overlay/driver state; no writes
    ForceUpdate         // always write (even if already correct)
};

class ZynqFclk {
    using Path = std::filesystem::path;
  public:
    // When devcfg is used clock rate is always updated.
    //
    // When using devicetree overlay the clock rate is updated when the overlay is loaded.
    // Therefore there is no need to set it in most cases and we only check it.
    // If we want to update it, set update_rate flag to true when calling ctx.fclk.set()
    void set(const std::string& fclk_name,
             uint32_t fclk_rate,
             FclkIntent intent = FclkIntent::CheckOnly);

    void write(const std::string& fclk_name, uint32_t fclk_rate) {
         return set(fclk_name, fclk_rate, FclkIntent::ForceUpdate);
    }

  private:
    const Path devcfg = "/sys/devices/soc0/amba/f8007000.devcfg";
    const Path amba_clocking = "/sys/devices/soc0/fpga-region/fpga-region:clocking";

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
                                uint32_t fclk_rate, FclkIntent intent);
    int amba_clocking_set_rate(const Path& clkdir, char clkid, uint32_t fclk_rate);
    long amba_clocking_get_rate(const Path& clkdir);
}; // class ZynqFclk

#endif // __SERVER_CONTEXT_ZYNQ_FCLK__