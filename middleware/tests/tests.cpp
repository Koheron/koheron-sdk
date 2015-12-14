/// @file tests.cpp
/// (c) Koheron 2014-2015 

#include "tests.hpp"

#include <cmath>
#include <random>
#include <thread>

Tests::Tests(Klib::DevMem& dvm_unused_)
: data(0)
{
    waveform_size = 0;
    mean = 0;
    std_dev = 0;
    status = CLOSED;
}
 
Tests::~Tests()
{
    Close();
}

int Tests::Open(uint32_t waveform_size_)
{
    // Reopening
    if(status == OPENED && waveform_size_ != waveform_size) {
        Close();
    }

    if(status == CLOSED) {
        waveform_size = waveform_size_;        
        data = Klib::KVector<float>(waveform_size, 0);       
        status = OPENED;
    }
    
    return 0;
}

void Tests::Close()
{
    if(status == OPENED) {
        status = CLOSED;
    }
}

Klib::KVector<float>& Tests::read()
{
    std::default_random_engine generator(std::random_device{}());
    std::normal_distribution<float> distribution(mean, std_dev);

    for(unsigned int i=0; i<data.size(); i++) {
        data[i] = distribution(generator);
    }
    return data;
}

void Tests::set_mean(float mean_)
{
    mean = mean_;
}

void Tests::set_std_dev(float std_dev_)
{
    std_dev = std_dev_;
}


