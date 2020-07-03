// Zynq bistream loader
// (c) Koheron

#ifndef __SERVER_CONTEXT_FPGA_MANAGER__
#define __SERVER_CONTEXT_FPGA_MANAGER__

#include <unistd.h>

#include <array>
#include <string>
#include <vector>
#include <sys/mount.h>
#include <experimental/filesystem>
#include <context_base.hpp>
#include "memory.hpp"

class FpgaManager {
  public:
    FpgaManager(ContextBase& ctx_)
    : ctx(ctx_)
    {
        // Initial values used for xdevcfg
        isXDevCfg = false;
        useOverlay = false;
        fhandle_path = xdev;
        chandle_path = xdev_done;
        bit_extension = ".bit";
        // use xdev it it exists
        if (exists_fs(xdev)) {
            ctx.log<INFO>("Detected xdevcfg ... %s\n", "True" );
            isXDevCfg = false;
            return;
        }
        // check if fpga manager is present, without flag option, it wont work. 
        if (exists_fs(fmanager_flags))
        {
            ctx.log<INFO>("Detected fmanager ... %s\n", "True");

            // update params used to identify where to flash
            bit_extension = ".bit.bin";
            chandle_path = fmanager_done;
            fhandle_path = fmanager_firmware;
            namespace fs = std::experimental::filesystem;

            // check if fpga manager is present 
            bool firmware_exists = exists_fs(fmanager_firmware);

            // create /lib/firmware - without it, device loading wont work
            if (!exists_fs(lib_firmware_dirname)) 
                fs::create_directories(lib_firmware_dirname);

            // remove old binaries
            if (exists_fs(lib_firmware_dirname + "pl.dtbo")) 
                fs::remove(lib_firmware_dirname + "pl.dtbo");
            if (exists_fs(lib_firmware_dirname + instrument_name + bit_extension)) 
                fs::remove(lib_firmware_dirname + instrument_name + bit_extension);

            // copy new binaries
            fs::copy_file(live_instrument_dirname + "pl.dtbo", lib_firmware_dirname + "/pl.dtbo", fs::copy_options::overwrite_existing);
            fs::copy_file(live_instrument_dirname + instrument_name + bit_extension, lib_firmware_dirname + instrument_name + bit_extension, fs::copy_options::overwrite_existing);

            // create /configfs if needed
            if (!exists_fs("/configfs")) 
                fs::create_directories("/configfs");

            // mount configfs if needed
            if (!exists_fs("/configfs/device-tree/")) 
                mount("none", "/configfs", "configfs", 0, nullptr);
            // check if mount was successfull
            if (exists_fs("/configfs/device-tree/")) 
            {
                // remove any previous overlay
                if (exists_fs(overlay_path)) 
                {
                    fs::remove_all(overlay_path);
                }
                if (exists_fs(overlay_path))
                    ctx.log<PANIC>("Failed to remove previous overlay ...\n");

                fs::create_directories(overlay_path);

                if (exists_fs(overlay_path)) {
                    chandle_path = overlay_done;
                    useOverlay = true;
                    fhandle_path = overlay;
                    return;
                }
            }
            else
                ctx.log<PANIC>("Failed to mount configfs or device-tree overlay not included in kernel image...\n");
            // Exit if file descriptor for writing bit.bin exists
            if (firmware_exists) return;
        }

        ctx.log<PANIC>("Failed to identify bitstream loading mechanism...\n");
        exit(EXIT_FAILURE);
        
    }

    int load_bitstream(const char* name) {
        ctx.log<INFO>("Start loading bitstream...");
        FILE *xdevcfg = fopen(fhandle_path.c_str(), "w");

        if (isXDevCfg) {
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
            // set flag to indice that we are flashing entire firmware, i.e. not partial reconfig
            FILE *xflag = fopen(fmanager_flags.c_str(), "w");
            ctx.log<INFO>("echo 0 > %s\n", fmanager_flags.c_str());
            if (fwrite("0", 1, 1, xflag) != 1) {
              ctx.log<PANIC>("FpgaManager: Failed to set flag on %s\n", fmanager_flags.c_str());
              fclose(xflag);
              fclose(xdevcfg);
              return -1;
            }
            fclose(xflag);

            // construct name of file
            std::string echo = name + bit_extension + "\n";
            if (useOverlay)
            {
                ctx.log<INFO>("Using Overlay...");
                echo = "pl.dtbo\n";
            } else {
                ctx.log<INFO>("Not using Overlay...");
            }
            // write image
            ctx.log<INFO>("echo %s > %s\n", echo, fhandle_path.c_str());
            if (fwrite(echo.c_str(), echo.size(), 1, xdevcfg) != 1) {
                ctx.log<PANIC>("FpgaManager: Failed to write bitstream to %s\n", fhandle_path.c_str());
                fclose(xdevcfg);
                return -1;
            }
            fclose(xdevcfg);
            return check_bitstream_loaded();
        }
        ctx.log<PANIC>("FpgaManager: Cannot open xdevcfg\n");
        return -1;
    }

  private:
    ContextBase& ctx;

    bool isXDevCfg;
    bool useOverlay;
    const std::string live_instrument_dirname = "/tmp/live-instrument/";
    const std::string lib_firmware_dirname = "/lib/firmware/";

    const std::string xdev = "/dev/xdevcfg";
    const std::string xdev_done = "/sys/bus/platform/drivers/xdevcfg/f8007000.devcfg/prog_done";

    const std::string fmanager_flags = "/sys/class/fpga_manager/fpga0/flags";
    const std::string fmanager_done = "/sys/class/fpga_manager/fpga0/state";
    const std::string fmanager_firmware = "/sys/class/fpga_manager/fpga0/firmware";

    const std::string overlay_path = "/configfs/device-tree/overlays/full";
    const std::string overlay = "/configfs/device-tree/overlays/full/path";
    const std::string overlay_done = "/configfs/device-tree/overlays/full/status";

    std::string fhandle_path;
    std::string chandle_path;
    std::string bit_extension;


    inline bool exists_fs(const std::string& p)
    {
        namespace fs = std::experimental::filesystem;
        fs::file_status s = fs::file_status{};
        if(fs::status_known(s) ? fs::exists(s) : fs::exists(p))
        {
            ctx.log<INFO>("Found: %s...\n", p.c_str());
            return true;
        }
        ctx.log<INFO>("Not Found: %s...\n", p.c_str());
        return false;
    }

    int read_bitstream_data(const char* name, std::vector<char>& bitstream_data) {
        // https://stackoverflow.com/questions/22059189/read-a-file-as-byte-array

        const auto bistream_filename = live_instrument_dirname + name + bit_extension;
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
        FILE *fprog_done = nullptr;
        std::array<char, 1> expected{};
        if (isXDevCfg) {
            fprog_done = fopen(xdev_done.c_str(), "r");
            expected[0] = '1';
        }
        else if (useOverlay) {
            fprog_done = fopen(overlay_done.c_str(), "r");
            expected[0] = 'a';
        }
        else {
            fprog_done = fopen(fmanager_done.c_str(), "r");
            expected[0] = 'o';
        }

        if (fprog_done == nullptr) {
            ctx.log<PANIC>("FpgaManager: Failed to open %s\n", chandle_path.c_str());
            return -1;
        }

        std::array<char, 1> buff{};

        if (read(fileno(fprog_done), buff.data(), 1) == 1) {
            if (buff[0] == expected[0]) {
                ctx.log<INFO>("FpgaManager:Bitstream successfully loaded\n");
                fclose(fprog_done);
                return 0;
            } else {
                ctx.log<PANIC>("FpgaManager: Failed to load bitstream\n");
                fclose(fprog_done);
                return -1;
            }
        } else {
            ctx.log<PANIC>("FpgaManager: Failed to read %s\n", chandle_path.c_str());
            fclose(fprog_done);
            return -1;
        }
    }
}; // FpgaManager

#endif // __SERVER_CONTEXT_FPGA_MANAGER__
