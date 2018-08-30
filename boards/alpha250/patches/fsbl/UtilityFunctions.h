/** \file UtilityFunctions.h
 * \brief Header file for utility functions.
 * \author Garry Jeromson
 * \date 18.06.15
 */

#pragma once


//-------------------------------------------------------------------------------------------------
// Includes
//-------------------------------------------------------------------------------------------------

#include "StandardIncludes.h"


//-------------------------------------------------------------------------------------------------
// Function declarations
//-------------------------------------------------------------------------------------------------

/**
 * \brief Convert a little-endian byte array to a 32-bit unsigned int.
 *
 * @param pByteArray	Pointer to the byte array
 * @return				32-bit unsigned int value
 */
static inline uint32_t ByteArrayToUnsignedInt32(uint8_t* pByteArray)
{
    uint32_t value = (*pByteArray << 24);
    value += (*(pByteArray + 1) << 16);
    value += (*(pByteArray + 2) << 8);
    value += *(pByteArray + 3);
    return value;
}


/**
 * \brief Convert a little-endian byte array to a 64-bit unsigned int.
 *
 * @param pByteArray	Pointer to the byte array
 * @return				64-bit unsigned int value
 */
static inline uint64_t ByteArrayToUnsignedInt64(uint8_t* pByteArray)
{
    uint64_t value;
    value = (uint64_t) * (pByteArray + 0) << 40;
    value += (uint64_t) * (pByteArray + 1) << 32;
    value += (uint64_t) * (pByteArray + 2) << 24;
    value += (uint64_t) * (pByteArray + 3) << 16;
    value += (uint64_t) * (pByteArray + 4) << 8;
    value += (uint64_t) * (pByteArray + 5);
    return value;
}


/**
 * \brief Get the upper byte from a 16-bit unsigned int
 *
 * @param var	16-bit unsigned int
 * @return		The upper byte
 */
static inline uint8_t GetUpperByte(uint16_t var)
{
    return (var >> 8) & 0xFF;
}


/**
 * \brief Get the lower byte from a 16-byte number
 *
 * @param var	16-bit unsigned int
 * @return		The lower byte
 */
static inline uint8_t GetLowerByte(uint16_t var)
{
    return (var & 0xFF);
}


/**
 * \brief change byte order of a 16-bit unsigned int.
 *
 * @param InByte		16-bit unsigned int value
 * @return				16-bit unsigned int value
 */
static inline uint16_t ChangeByteOrder(uint16_t InByte)
{
    uint16_t value = InByte >> 8;
    value += (InByte & 0xFF) << 8;
    return value;
}


/**
* \brief Returns the higher of the two numerical arguments.
*/
#ifndef max
#define max(a, b) (((a) > (b)) ? (a) : (b))
#endif


/**
* \brief Returns the lower of the two numerical arguments.
*/
#ifndef min
#define min(a, b) (((a) < (b)) ? (a) : (b))
#endif