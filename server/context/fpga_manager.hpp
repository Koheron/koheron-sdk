// Zynq bistream loader
// (c) Koheron

#ifndef __SERVER_CONTEXT_FPGA_MANAGER__
#define __SERVER_CONTEXT_FPGA_MANAGER__

#include <unistd.h>

#include <array>
#include <string>
#include <vector>

#include <context_base.hpp>

class FpgaManager {
  public:
    FpgaManager(ContextBase& ctx_)
    : ctx(ctx_)
    {}

    int load_bitstream(const char* name) {
        FILE *xdevcfg = fopen("/dev/xdevcfg", "w");

        if (xdevcfg != nullptr) {
            std::vector<char> bitstream_data{};
            
            if (read_bitstream_data(name, bitstream_data) < 0) {
                fclose(xdevcfg);
                return -1;
            }

            if (fwrite(bitstream_data.data(), bitstream_data.size(), 1, xdevcfg) != 1) {
                ctx.log<PANIC>("FpgaManager: Failed to write bitstream to xdevcfg\n");
                fclose(xdevcfg);
                return -1;
            }

            fclose(xdevcfg);
            return check_bitstream_loaded();
        } else {
            // TODO Ubuntu 18.04 call fpga_manager driver
            // cf. rsarwar87 install_instrument.sh
            ctx.log<PANIC>("FpgaManager: Cannot open xdevcfg\n");
            return -1;
        }
    }

  private:
    ContextBase& ctx;

    const std::string live_instrument_dirname = "/tmp/live-instrument/";
    const std::string xdev = "/sys/bus/platform/drivers/xdevcfg/f8007000.devcfg/prog_done";
    // const std::string fman = "/sys/class/fpga_manager/fpga0/firmware";
    // const std::string ffull = "/configfs/device-tree/overlays/full/";

    int read_bitstream_data(const char* name, std::vector<char>& bitstream_data) {
        // https://stackoverflow.com/questions/22059189/read-a-file-as-byte-array

        const auto bistream_filename = live_instrument_dirname + name + ".bit";
        ctx.log<INFO>("FpgaManager: Loading bitstream %s...\n", bistream_filename.c_str());
        FILE *fbitstream = fopen(bistream_filename.c_str(), "rb");

        if (fbitstream == nullptr) {
            ctx.log<PANIC>("FpgaManager: Cannot open bitstream file\n");
            return -1;
        }

        fseek(fbitstream, 0, SEEK_END);
        bitstream_data.resize(ftell(fbitstream));
        rewind(fbitstream);

        if (fread(bitstream_data.data(), bitstream_data.size(), 1, fbitstream) != 1) {
            ctx.log<PANIC>("FpgaManager: Cannot read bitstream data\n");
            fclose(fbitstream);
            return -1;
        }
        
        fclose(fbitstream);
        return 0;
    }

    int check_bitstream_loaded() {
        FILE *fprog_done = fopen("/sys/bus/platform/drivers/xdevcfg/f8007000.devcfg/prog_done", "r");

        if (fprog_done == nullptr) {
            ctx.log<PANIC>("FpgaManager: Failed to open prog_done\n");
            return -1;
        }

        std::array<char, 1> buff{};

        if (read(fileno(fprog_done), buff.data(), 1) == 1) {
            if (buff[0] == '1') {
                ctx.log<INFO>("FpgaManager:Bitstream successfully loaded\n");
                fclose(fprog_done);
                return 0;
            } else {
                ctx.log<PANIC>("FpgaManager: Failed to load bitstream\n");
                fclose(fprog_done);
                return -1;
            }
        } else {
            ctx.log<PANIC>("FpgaManager: Failed to read prog_done\n");
            fclose(fprog_done);
            return -1;
        }
    }
}; // FpgaManager

#endif // __SERVER_CONTEXT_FPGA_MANAGER__
