///// DAC driver
/////
///// (c) Koheron

//#ifndef __DRIVERS_CORE_DAC_HPP__
//#define __DRIVERS_CORE_DAC_HPP__

//#include "dev_mem.hpp"
//#include "wr_register.hpp"

//#include <signal/kvector.hpp>
// 
//#define MAP_SIZE 4096
//#define SAMPLING_RATE 125E6

//// Addresses
//#define DAC_ADDR          0x44000000

////> \description DAC driver
//class Dac
//{
//  public:
//    Dac(Klib::DevMem& dev_mem_);
//    ~Dac();

//    //> \description Open the device
//    //> \io_type WRITE
//    //> \param waveform_size_ Number of points to acquire
//    //> \status ERROR_IF_NEG
//    //> \on_error Cannot open DAC device
//    //> \flag AT_INIT
//    int Open(uint32_t waveform_size_);
//    
//    void Close();

//    //> \description Set DAC to constant values
//    //> \io_type WRITE
//    //> \param dac_1_float Constant value of DAC_1 (-1 <= dac_1_float < 1)
//    //> \param dac_2_float Constant value of DAC_2 (-1 <= dac_2_float < 1)
//    void set_dac_constant(float dac_1_float, float dac_2_float);
//    
//    //> \description Set DAC
//    //> \io_type WRITE_ARRAY param=>buffer param=>len
//    void set_dac(const uint32_t *buffer, size_t len);

//    enum Status {
//        CLOSED,
//        OPENED,
//        FAILED
//    };

//    //> \is_failed
//    bool IsFailed() const {return status == FAILED;}

//  private:
//    Klib::DevMem& dev_mem;
//    uint32_t waveform_size;
//    int status;
//    
//    // Memory maps IDs:
//    Klib::MemMapID dac_map;
//    
//}; // class Dac

//#endif // __DRIVERS_CORE_DAC_HPP__
