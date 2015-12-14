/// @file kvector.hpp
///
/// @brief Inlined vectorial computations
///
/// @author Thomas Vanderbruggen <thomas@koheron.com>
/// @date 10/04/2015
///
/// (c) Koheron 2014-2015

#ifndef __SIGNAL_KVECTOR_HPP__
#define __SIGNAL_KVECTOR_HPP__

/// Use a std::vector as a container if true
#define USE_STD_VECTOR 1

#include <cmath>
#include <stdio.h>
#include <cassert>
#include <iostream>
#include <vector>

namespace Klib {

template<typename T> class KVector;
//template<> class KVector<float>;
//template<> class KVector<double>;
//template<> class KVector<long double>;

template<typename T> void display(const KVector<T>& vect_);

/// @brief Template class for vectorial computations
///
/// @T Scalar type, must be compatible with numeric calculations
template<typename T>
class KVector
{
  public:
    // ---------------------------------------
    // Contructors/ destructor
    // ---------------------------------------
    
    /// @brief Allocate a vector
    KVector(size_t size_)
    {
        _alloc(size_);
    }
  
    /// @brief Allocate a vector and fill it
    ///
    /// @fill_val Value to fill the vector with
    KVector(size_t size_, T fill_val)
    {
        _alloc(size_);
        
        for(size_t i=0; i<size_; i++) {
            data[i] = fill_val;
        }
    }
  
    #if 0
    /// XXX This is done to avoid a copy.
    ///
    /// Since this could be used to manipulate directly
    /// data in a BRAM, WriteReg32 should be used for 
    /// manipulations in all the functions of this file.
    ///
    /// Therefore, I don't use that and a copy in RAM is 
    /// performed for every KVector initialization.
    
    /// @brief Load an already allocated vector
    ///
    /// Assumes that the allocated memory for @data_ptr
    /// is equal to size_ * sizeof(T)
    KVector(T *data_ptr, size_t size_)
    {
        data = data_ptr;
    }
    #endif
    
    /// @brief Build a vector from an existing buffer
    ///
    /// A copy of the initial buffer is performed into RAM.
    /// This allows safe manipulation of data from BRAMs.
    KVector(T *data_ptr, size_t size_)
    {
        _alloc(size_);
        
        // Here we don't use memcpy since we
        // may  want to copy data from a BRAM
        for(size_t i=0; i<size_; i++) {
            data[i] = data_ptr[i];
        }
    }
    
    /// @brief Copy constructor
    KVector(const KVector<T>& kvector_)
    {
        _alloc(kvector_.size());
    
        for(size_t i=0; i<kvector_.size(); i++) {
            data[i] = kvector_.data[i];
        }
    }
    
    /// @brief Equivalent to linspace(begin,end) in Matlab
    KVector(T begin_, T end_, size_t size_)
    { 
        assert(size_ != 0);
        _alloc(size_);
	
        T step = std::fabs(end_-begin_) / size_;

        if(begin_< end_) {
            data[0] = begin_;
			
            for(size_t i=1; i<size_; i++) {
                data[i] = data[i-1] + step;
            }	
        } else {
            data[0] = end_;
			
            for(size_t i=1; i<size_; i++) {
                data[i] = data[i-1] - step;
            }	
        }
    }
	
    ~KVector()
    {}
	
    inline void resize(size_t size_)
    {
        data.resize(size_);
    }

    // ---------------------------------------
    // Accessors / Setters
    // ---------------------------------------
	
    /// @brief Get the data pointer
    inline const T* get_ptr() const
    {
        return data.data();
    }
    
    inline size_t size() const 
    {
        return data.size();
    }
	
    /// Set accessor
    inline T& operator[](size_t i)
    {
        assert(i < size());
        return data[i];
    }
    
    /// Get accessor
    inline const T& operator[](size_t i) const 
    {
        assert(i < size());
        return data[i];
    }
    
    inline KVector<T>& operator=(KVector<T> vect)
    {        
        this->data = vect.data;        
        return *this;
    }
	
    // ---------------------------------------
    // Arithmetics
    // ---------------------------------------
	
    /// Add a scalar to all vector components
    inline void operator+=(const T& scal_)
    {
        for(size_t i=0; i<size(); i++)
            data[i] += scal_;	
    }
	
    /// Substract a scalar to all vector components
    inline void operator-=(const T& scal_)
    {
        for(size_t i=0; i<size(); i++)
            data[i] -= scal_;	
    }
	
    /// Multiply a scalar to all vector components
    inline void operator*=(const T& scal_)
    {
        for(size_t i=0; i<size(); i++)
            data[i] *= scal_;	
    }
	
    /// Divide all vector components by a scalar
    inline void operator/=(const T& scal_)
    {
        for(size_t i=0; i<size(); i++)
            data[i] /= scal_;	
    }
	
    /// Add a vector of the same size
    inline void operator+=(const KVector<T>& vect_)
    {
        assert(vect_.size() == size());
    
        for(size_t i=0; i<size(); i++)
            data[i] += vect_[i];
    }
	
    /// Substract a vector of the same size
    inline void operator-=(const KVector<T>& vect_)
    {
        assert(vect_.size() == size());
    
        for(size_t i=0; i<size(); i++)
            data[i] -= vect_[i];
    }
	
	/// Multiply each component with the ones of a vector of the same size
    inline void operator*=(const KVector<T>& vect_)
    {
        assert(vect_.size() == size());
    
        for(size_t i=0; i<size(); i++)
            data[i] *= vect_[i];	
    }
	
    /// Divide each component with the ones of a vector of the same size
    inline void operator/=(const KVector<T>& vect_)
    {
        assert(vect_.size() == size());
    
        for(size_t i=0; i<size(); i++)
            data[i] /= vect_[i];
    }
	
    /// Put each component to a given power
    inline void operator^=(const T& pow_)
    {
        for(size_t i=0; i<size(); i++)
            data[i] = std::pow(data[i], pow_);
    }

    // ---------------------------------------
    // Other unary functions
    // ---------------------------------------
	
    /// Sum all the components of a vector
    inline T sum() const
    {
        T res = 0;
		
        for(size_t i=0; i<size(); i++)
            res += data[i];
			
        return res;
    }
	
    /// Return the mean value of the vector
    inline T mean() const 
    {
        return sum() / size();
    }
	
    /// Return the maximum value of the vector
    inline T max() const
    {
        T res = data[0];

        for(size_t i=1; i<size(); i++) {
            if(data[i] > res) {
                res = data[i];
            }
        }

        return res;
    }
	
    /// Return the minimum value of the vector
    inline T min() const
    {
        T res = data[0];

        for(size_t i=1; i<size(); i++) {
            if(data[i] < res) {
                res = data[i];
            }
        }

        return res;
    }
    
    /// Return the 1-norm
    inline T norm1() const
    {
        T res = std::fabs(data[0]);
        
        for(size_t i=1; i<size(); i++) {
            res += std::fabs(data[i]);
        }
        
        return res;
    }
    
    /// Return the Euclidian norm (2-norm)
    inline T norm2() const
    {
        T res = data[0]*data[0];
        
        for(size_t i=1; i<size(); i++) {
            res += data[i]*data[i];
        }
        
        return std::sqrt(res);
    }
    
    /// Return the p-norm
    inline T normp(uint32_t p) const
    {
        T res = std::pow(std::fabs(data[0]), static_cast<T>(p));
        
        for(size_t i=1; i<size(); i++) {
            res += std::pow(std::fabs(data[i]), static_cast<T>(p));
        }
        
        return std::pow(res, 1/static_cast<T>(p));
    }
    
    T var() const
    {
        if(size() == 0) {
            return static_cast<T>(0);
        }

        T mean = mean();
        T res = 0;
        T delta;
        
        for(size_t i=0; i<size(); i++) {
            delta = data[i] - mean;
            res += delta*delta;
        }
        
        return res / size();
    }
    
    inline T stdev() const
    {
        return std::sqrt(var());
    }
    
    // TODO (TV, 11/04/2015) Add:
    // - decimations

  private:
    std::vector<T> data;
    
    /// @brief Allocate the data buffer
    void _alloc(size_t size)
    {
        data = std::vector<T>(size);
    }
}; // class KVector

// Arithmetics

/// Add a scalar to each components of a vector
template<typename T>
inline KVector<T>
operator+(const KVector<T>& vect_, const T& scal_)
{
    KVector<T> res = vect_;
    res += scal_;
    return res; 
}

/// Add a scalar to each components of a vector
template<typename T>
inline KVector<T>
operator+(const T& scal_, const KVector<T>& vect_)
{
    KVector<T> res = vect_;
    res += scal_;
    return res; 
}

/// Substract a scalar to each components of a vector
template<typename T>
inline KVector<T>
operator-(const KVector<T>& vect_, const T& scal_)
{
    KVector<T> res = vect_;
    res -= scal_;
    return res; 
}

/// Substract a scalar to each components of a vector
template<typename T>
inline KVector<T>
operator-(const T& scal_, const KVector<T>& vect_)
{
    KVector<T> res = vect_;
    res -= scal_;
    return res; 
}

/// Multiply by a scalar each components of a vector
template<typename T>
inline KVector<T>
operator*(const KVector<T>& vect_, const T& scal_)
{
    KVector<T> res = vect_;
    res *= scal_;
    return res; 
}

/// Multiply by a scalar each components of a vector
template<typename T>
inline KVector<T>
operator*(const T& scal_, const KVector<T>& vect_)
{
    KVector<T> res = vect_;
    res *= scal_;
    return res; 
}

/// Divide by a scalar each components of a vector
template<typename T>
inline KVector<T>
operator/(const KVector<T>& vect_, const T& scal_)
{
    KVector<T> res = vect_;
    res /= scal_;
    return res; 
}

/// Add two vectors of the same size
template<typename T>
inline KVector<T>
operator+(const KVector<T>& vect1_, const KVector<T>& vect2_)
{
    assert(vect1_.size()==vect2_.size());
    
    KVector<T> res = vect1_;
    res += vect2_;
    return res; 
}

/// Substract two vectors of the same size
template<typename T>
inline KVector<T>
operator-(const KVector<T>& vect1_, const KVector<T>& vect2_)
{
    assert(vect1_.size()==vect2_.size());
    
    KVector<T> res = vect1_;
    res -= vect2_;
    return res; 
}

/// Multiply two vectors of the same size
template<typename T>
inline KVector<T>
operator*(const KVector<T>& vect1_, const KVector<T>& vect2_)
{
    assert(vect1_.size()==vect2_.size());

    KVector<T> res = vect1_;
    res *= vect2_;
    return res; 
}

/// Divide two vectors of the same size
template<typename T>
inline KVector<T>
operator/(const KVector<T>& vect1_, const KVector<T>& vect2_)
{
    assert(vect1_.size()==vect2_.size());
    
    KVector<T> res = vect1_;
    res /= vect2_;
    return res;
}

/// Put every component of a vector to a given power
template<typename T>
inline KVector<T>
operator^(const KVector<T>& vect_, const T& pow_)
{
    KVector<T> res = vect_;
    res ^= pow_;
    return res; 
}

/// Sum the elements of a vector
template<typename T>
inline T sum(const KVector<T>& vect_)
{
    return vect_.sum();
}

/// Return the maximum value of the vector
template<typename T>
inline T max(const KVector<T>& vect_)
{
    return vect_.max();
}

/// Return the minimum value of the vector
template<typename T>
inline T min(const KVector<T>& vect_)
{
    return vect_.min();
}

/// Return the 1-norm of the vector
template<typename T>
inline T norm1(const KVector<T>& vect_)
{
    return vect_.norm1();
}

/// Return the Euclidian norm (2-norm) of the vector
template<typename T>
inline T norm2(const KVector<T>& vect_)
{
    return vect_.norm2();
}

/// Return the p-norm of the vector
template<typename T>
inline T normp(const KVector<T>& vect_, uint32_t p)
{
    return vect_.normp(p);
}

// Statistical functions

/// Return the average of a vector
template<typename T>
inline T mean(const KVector<T>& vect_)
{
    return vect_.mean();
}

/// Return the variance of a vector
template<typename T>
inline T var(const KVector<T>& vect_)
{   
    return vect_.var();
}

/// Return the standard deviation of a vector
template<typename T>
inline T stdev(const KVector<T>& vect_)
{
    return vect_.stdev();
}

/// Return the absolute value of each component of a vector
template<typename T>
inline KVector<T> abs(const KVector<T>& x_)
{
    KVector<T> res(x_.size());

    for(size_t i=0; i<x_.size(); i++)
        res[i] = std::fabs(x_[i]);
	
    return res;
}

/// Return the absolute value of each component of a vector
template<typename T>
inline KVector<T> fabs(const KVector<T>& x_)
{
    return abs(x_);
}

/// Calculate the square root of all the components of a vector
template<typename T>
inline KVector<T> sqrt(const KVector<T>& x_)
{
    KVector<T> res(x_.size());
    
    for(size_t i=0; i<x_.size(); i++)
        res[i] = std::sqrt(x_[i]);

    return res;
}

/// Return the ceil of all the components of a vector 
template<typename T>
inline KVector<T> ceil(const KVector<T>& x_)
{
    KVector<T> res(x_.size());
	
    for(size_t i=0; i<x_.size(); i++)
        res[i] = std::ceil(x_[i]);
		
    return res;
}

/// Return the floor of all the components of a vector 
template<typename T>
inline KVector<T> floor(const KVector<T>& x_)
{
    KVector<T> res(x_.size());
	
    for(size_t i=0; i<x_.size(); i++)
        res[i] = std::floor(x_[i]);
		
    return res;
}

/// Calculate the exponential of all the components of a vector
template<typename T>
inline KVector<T> exp(const KVector<T>& x_)
{
    KVector<T> res(x_.size());
	
    for(size_t i=0; i<x_.size(); i++)
        res[i] = std::exp(x_[i]);
		
    return res;
}

/// Calculate the exp(x)-1 of all the components of a vector
template<typename T>
inline KVector<T> expm1(const KVector<T>& x_)
{
    KVector<T> res(x_.size());
	
    for(size_t i=0; i<x_.size(); i++)
        res[i] = std::expm1(x_[i]);
		
    return res;
}

/// Calculate the Gauss function exp(-x^2)
template<typename T>
inline KVector<T> gauss(const KVector<T>& x_)
{
    KVector<T> res(x_.size());
	
    for(size_t i=0; i<x_.size(); i++)
        res[i] = std::exp(-x_[i]*x_[i]);
		
    return res;
}

/// Calculate the Lorentzian 1/(1+x^2)
template<typename T>
inline KVector<T> lorentz(const KVector<T>& x_)
{
    KVector<T> res(x_.size());
	
    for(size_t i=0; i<x_.size(); i++)
        res[i] = 1/(1+x_[i]*x_[i]);
		
    return res;
}

/// Calculate the logarithm in base e of all the components of a vector
template<typename T>
inline KVector<T> log(const KVector<T>& x_)
{
    KVector<T> res(x_.size());
	
    for(size_t i=0; i<x_.size(); i++)
        res[i] = std::log(x_[i]);
		
    return res;
}

/// Calculate the logarithm in base 10 of all the components of a vector
template<typename T>
inline KVector<T> log10(const KVector<T>& x_)
{
    KVector<T> res(x_.size());
	
    for(size_t i=0; i<x_.size(); i++)
        res[i] = std::log10(x_[i]);
		
    return res;
}

/// Calculate the sinus of all the components of a vector
template<typename T>
inline KVector<T> sin(const KVector<T>& x_)
{
    KVector<T> res(x_.size());
	
    for(size_t i=0; i<x_.size(); i++)
        res[i] = std::sin(x_[i]);
		
    return res;
}

/// Calculate the cosine of all the components of a vector
template<typename T>
inline KVector<T> cos(const KVector<T>& x_)
{
    KVector<T> res(x_.size());
	
    for(size_t i=0; i<x_.size(); i++)
        res[i] = std::cos(x_[i]);
		
    return res;
}

/// Calculate the cardinal sinus sin(x)/x
template<typename T>
inline KVector<T> sinc(const KVector<T>& x_)
{
    KVector<T> res(x_.size());
	
    for(size_t i=0; i<x_.size(); i++)
        res[i] = std::sin(x_[i])/x_[i];
		
    return res;
}

/// Calculate the tangent of all the components of a vector
template<typename T>
inline KVector<T> tan(const KVector<T>& x_)
{
    KVector<T> res(x_.size());
	
    for(size_t i=0; i<x_.size(); i++)
        res[i] = std::tan(x_[i]);
		
    return res;
}

/// Calculate the arccosine of all the components of a vector
template<typename T>
inline KVector<T> acos(const KVector<T>& x_)
{
    KVector<T> res(x_.size());
	
    for(size_t i=0; i<x_.size(); i++)
        res[i] = std::acos(x_[i]);
		
    return res;
}

/// Calculate the arccosine of all the components of a vector
template<typename T>
inline KVector<T> arccos(const KVector<T>& x_)
{
    return acos(x_);
}

/// Calculate the arcsinus of all the components of a vector
template<typename T>
inline KVector<T> asin(const KVector<T>& x_)
{
    KVector<T> res(x_.size());
	
    for(size_t i=0; i<x_.size(); i++)
        res[i] = std::asin(x_[i]);
		
    return res;
}

/// Calculate the arcsinus of all the components of a vector
template<typename T>
inline KVector<T> arcsin(const KVector<T>& x_)
{
    return asin(x_);
}

/// Calculate the arctangent of all the components of a vector
template<typename T>
inline KVector<T> atan(const KVector<T>& x_)
{
    KVector<T> res(x_.size());
	
    for(size_t i=0; i<x_.size(); i++)
        res[i] = std::atan(x_[i]);
		
    return res;
}

/// Calculate the arctangent of all the components of a vector
template<typename T>
inline KVector<T> arctan(const KVector<T>& x_)
{
    return atan(x_);
}

#if 0

/// Calculate atan(x/y), x and y must have the same size
inline scal_vector 
atan(const scal_vector& x_, const scal_vector& y_)
{
	scal_vector res(x_.size());
	
	for(size_t i=0;i<res.size();i++)
	    // XXX Should use atan2 ?
		res(i) = atan(x_(i)/y_(i));
		
	return res;
}

#endif

/// Calculate the hyperbolic sinus of all the components of a vector
template<typename T>
inline KVector<T> sinh(const KVector<T>& x_)
{
    KVector<T> res(x_.size());
	
    for(size_t i=0; i<x_.size(); i++)
        res[i] = std::sinh(x_[i]);
		
    return res;
}

/// Calculate the hyperbolic cosine of all the components of a vector
template<typename T>
inline KVector<T> cosh(const KVector<T>& x_)
{
	KVector<T> res(x_.size());
	
	for(size_t i=0; i<x_.size(); i++)
		res[i] = std::cosh(x_[i]);
		
	return res;
}

/// Calculate the hyperbolic tangent of all the components of a vector
template<typename T>
inline KVector<T> tanh(const KVector<T>& x_)
{
    KVector<T> res(x_.size());
	
    for(size_t i=0; i<x_.size(); i++)
        res[i] = std::tanh(x_[i]);
		
    return res;
}

/// Calculate the arccosh of all the components of a vector
template<typename T>
inline KVector<T> acosh(const KVector<T>& x_)
{
    KVector<T> res(x_.size());
	
    for(size_t i=0; i<x_.size(); i++)
        res[i] = std::acosh(x_[i]);
		
    return res;
}

/// Calculate the arccosh of all the components of a vector
template<typename T>
inline KVector<T> arccosh(const KVector<T>& x_)
{
    return acosh(x_);
}

/// Calculate the arcsinh of all the components of a vector
template<typename T>
inline KVector<T> asinh(const KVector<T>& x_)
{
    KVector<T> res(x_.size());
	
    for(size_t i=0; i<x_.size(); i++)
        res[i] = std::asinh(x_[i]);
		
    return res;
}

/// Calculate the arcsinh of all the components of a vector
template<typename T>
inline KVector<T> arcsinh(const KVector<T>& x_)
{
    return asinh(x_);
}

/// Calculate the arctanh of all the components of a vector
template<typename T>
inline KVector<T> atanh(const KVector<T>& x_)
{
    KVector<T> res(x_.size());
	
    for(size_t i=0; i<x_.size(); i++)
        res[i] = std::atanh(x_[i]);
		
    return res;
}

/// Calculate the arctanh of all the components of a vector
template<typename T>
inline KVector<T> arctanh(const KVector<T>& x_)
{
    return atanh(x_);
}

/// Print a vector on a given output stream
template<typename T>
inline void display(const KVector<T>& vect_)
{
    for(size_t i=0; i<vect_.size(); i++) {
        printf("%u -> %f\n", i, vect_[i]);
    }
}

} // Klib

#endif // __SIGNAL_KVECTOR_HPP__
