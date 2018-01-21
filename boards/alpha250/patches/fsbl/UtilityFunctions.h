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
 * \brief Convert decimal to binary coded decimal.
 *
 * @param decimal	Decimal input
 * @return			Corresponding binary coded decimal value
 */
static inline uint8_t ConvertDecimalToBinaryCodedDecimal(int decimal)
{
    return ((decimal / 10) << 4) + (decimal % 10);
}


/**
 * \brief Convert binary coded decimal to decimal.
 *
 * @param bcd		Binary coded decimal
 * @return			Corresponding decimal value
 */
static int ConvertBinaryCodedDecimalToDecimal(uint8_t bcd)
{
    return ((bcd >> 4) * 10) + bcd % 16;
}


/**
 * Simple sleep function.
 *
 * @param value Cycles to sleep for.
 */
static inline void Sleep(int value)
{
    volatile int i = 0;
    while (value--)
    {
        i++;
    }
}


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


/**
 * \brief Function which extracts a range of bits from a 8-bit unsigned integer.
 *
 * The operation is performed with a series of 2 bitwise logical operations.
 * Assuming bits are numbered from LSB = 0 to MSB = 7, the first operation
 * shifts the input right by LSB bits, putting the desired bit range LSB in
 * the 0th (least significant) bit of the expression. The second operation
 * masks off any bits in positions above those designated by the desired
 * bit range MSB.
 *
 * \param input 		The input 8-bit unsigned integer
 * \param bitRangeMSB 	The MSB position (0..7) of the bit range we wish to extract
 * \param bitRangeLSB 	The LSB position (0..7) of the bit range we wish to extract
 * \return 				An unsigned integer representing the value of the bits bitRangeMSB.. bitRangeLSB
 */
static inline uint8_t ExtractBitRange(uint8_t input, uint8_t bitRangeMSB, uint8_t bitRangeLSB)
{
    if ((bitRangeMSB <= 7) && (bitRangeLSB <= 7) && (bitRangeLSB <= bitRangeMSB))
    {
        return (input >> bitRangeLSB) & ~(~0 << (bitRangeMSB - bitRangeLSB + 1));
    }
    else
    {
        return 0;
    }
}


/// Rounds up result of unsigned integer division, without possibility of overflow.
static inline uint32_t DivideRoundUp(uint32_t numerator, uint32_t denominator)
{
    return 1 + ((numerator - 1) / denominator);
}
