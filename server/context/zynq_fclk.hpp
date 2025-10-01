// Zynq PL Fabric Clocks settings
// (c) Koheron

#ifndef __SERVER_CONTEXT_ZYNQ_FCLK__
#define __SERVER_CONTEXT_ZYNQ_FCLK__

#include <string>
#include <cstdint>

class ZynqFclk {
  public:
    // When devcfg is used clock rate is always updated.
    //
    // When using devicetree overlay the clock rate is updated when the overlay is loaded.
    // Therefore there is no need to set it in most cases.
    // If we want to update it, set update_rate to true when calling ctx.fclk.set()
    void set(const std::string& fclk_name,
             uint32_t fclk_rate,
             [[maybe_unused]] bool update_rate=false);

  private:
    const std::string devcfg = "/sys/devices/soc0/amba/f8007000.devcfg";
    const std::string amba_clocking = "/sys/devices/soc0/fpga-region/fpga-region:clocking";

    // ------------------------------------------------------------------------
    // Use devcfg
    // ------------------------------------------------------------------------

    void set_fclk_devcfg(const std::string& fclk_name, uint32_t fclk_rate);
    int fclk_export(const std::string& fclk_name, const std::string& fclk_dir_name);
    int fclk_enable(const std::string& fclk_name, const std::string& fclk_dir_name);
    int fclk_set_rate(const std::string& fclk_name,
                      const std::string& fclk_dir_name,
                      uint32_t fclk_rate);

    // ------------------------------------------------------------------------
    // Use amba_clocking
    // ------------------------------------------------------------------------

    void set_fclk_amba_clocking(const std::string& clkdir, char clkid,
                                uint32_t fclk_rate, bool update_rate);
    int amba_clocking_set_rate(const std::string& clkdir, char clkid, uint32_t fclk_rate);
    long amba_clocking_get_rate(const std::string& clkdir);
}; // class ZynqFclk

#endif // __SERVER_CONTEXT_ZYNQ_FCLK__