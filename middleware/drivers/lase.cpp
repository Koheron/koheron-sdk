/// (c) Koheron

#include "lase.hpp"

Lase::Lase(Klib::DevMem& dev_mem_)
: dev_mem(dev_mem_),
  xadc(dev_mem_),
  gpio(dev_mem_)
{
    dac_wfm_size = 0;
    status = CLOSED;
}

Lase::~Lase()
{
    Close();
}

# define MAP_SIZE 4096

int Lase::Open(uint32_t dac_wfm_size_)
{  
    // Reopening
    if(status == OPENED && dac_wfm_size_ != dac_wfm_size) {
        Close();
    }

    if(status == CLOSED) {
        dac_wfm_size = dac_wfm_size_;
    
        // Initializes memory maps
        config_map = dev_mem.AddMemoryMap(CONFIG_ADDR, MAP_SIZE);
        
        if(static_cast<int>(config_map) < 0) {
            status = FAILED;
            return -1;
        }
        
        status_map = dev_mem.AddMemoryMap(STATUS_ADDR, MAP_SIZE);
        
        if(static_cast<int>(status_map) < 0) {
            status = FAILED;
            return -1;
        }
        
        dac_map = dev_mem.AddMemoryMap(DAC_ADDR, dac_wfm_size*MAP_SIZE/1024);
        
        if(static_cast<int>(dac_map) < 0) {
            status = FAILED;
            return -1;
        }
        
        // Open core drivers
        xadc.Open();
        gpio.Open();
        
        status = OPENED;
        reset();
    }
    
    return 0;
}

void Lase::Close()
{
    if(status == OPENED) {
        reset();
        
        dev_mem.RmMemoryMap(config_map);
        dev_mem.RmMemoryMap(status_map);
        dev_mem.RmMemoryMap(dac_map);
        
        xadc.Close();
        gpio.Close();
    
        status = CLOSED;
    }
}

void Lase::reset()
{
    assert(status == OPENED);

    // XADC
    xadc.set_channel(LASER_POWER_CHANNEL, LASER_CURRENT_CHANNEL);
    xadc.enable_averaging();
    xadc.set_averaging(256);
    
    // GPIO
    gpio.set_as_output(7, 2);
    
    // Config
    Klib::ClearBit(dev_mem.GetBaseAddr(config_map) + ADDR_OFF, 0);
    Klib::ClearBit(dev_mem.GetBaseAddr(config_map) + ADDR_OFF, 1);
    Klib::SetBit(dev_mem.GetBaseAddr(config_map) + ADDR_OFF, 0);
    
    stop_laser();
    set_laser_current(0.0);
}

uint32_t Lase::get_laser_current()
{
    return xadc.read(LASER_CURRENT_CHANNEL);
}

uint32_t Lase::get_laser_power()
{
    return xadc.read(LASER_POWER_CHANNEL);
}

std::tuple<uint32_t, uint32_t> Lase::get_monitoring()
{
    return std::make_tuple(get_laser_current(), get_laser_power());
}

void Lase::start_laser()
{
    gpio.clear_bit(7, 2); // Laser enable on pin DIO7_P
}

void Lase::stop_laser()
{
    gpio.set_bit(7, 2); // Laser enable on pin DIO7_P
}

void Lase::set_laser_current(float current)
{
    float current_;
    current > MAX_LASER_CURRENT ? current_ = MAX_LASER_CURRENT : current_ = current;    
    uint32_t voltage = (uint32_t) ((1024/250) * current_);
    Klib::WriteReg32(dev_mem.GetBaseAddr(config_map) + PWM3_OFF, voltage);
}

void Lase::set_dac_buffer(const uint32_t *data, uint32_t len)
{
    for (uint32_t i=0; i<len; i++)
        Klib::WriteReg32(dev_mem.GetBaseAddr(dac_map)+sizeof(uint32_t)*i, data[i]);
}

uint32_t Lase::get_bitstream_id()
{
    return Klib::ReadReg32(dev_mem.GetBaseAddr(config_map) + BITSTREAM_ID_OFF);
}

void Lase::set_led(uint32_t value)
{
    Klib::WriteReg32(dev_mem.GetBaseAddr(config_map) + LEDS_OFF, value);
}

void Lase::reset_acquisition()
{
    Klib::ClearBit(dev_mem.GetBaseAddr(config_map) + ADDR_OFF, 1);
    Klib::SetBit(dev_mem.GetBaseAddr(config_map) + ADDR_OFF, 1);
}


