/// @file tuple_tests.hpp
///
/// @brief Tests for Eigen librairy
///
/// @author Thomas Vanderbruggen <thomas@koheron.com>
/// @date 06/10/2015
///
/// (c) Koheron 2014-2015

#ifndef __EIGEN_TESTS_HPP__
#define __EIGEN_TESTS_HPP__

#include <Eigen/Dense>

#include "../drivers/core/dev_mem.hpp" // Unused but needed for now

//> \description Tests for tuple tranfers
class EigenTests
{
  public:
    EigenTests(Klib::DevMem& dvm_unused_) {}
    
    //> \io_type WRITE
    void small_vector()
    {
        Eigen::Vector2f v(1, 2);
        
        for(int i=0; i<v.size(); i++)
            printf("%i --> %f", i, v(i));
                
    }
    
    //> \io_type WRITE
    void dynamic_vector(unsigned int len)
    {
        Eigen::VectorXf v(len);
        
        for(int i=0; i<v.size(); i++)
            v(i) = i*i;
        
        for(int i=0; i<v.size(); i++)
            printf("%i --> %f", i, v(i));
                
    }
};

#endif // __TUPLE_TESTS_HPP__
