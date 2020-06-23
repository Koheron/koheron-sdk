
#pragma once

#include "StandardIncludes.h"

//-------------------------------------------------------------------------------------------------
// Definitions, typedefs and constants
//-------------------------------------------------------------------------------------------------

#define MODULE_INFO_ADDRESS_PRODUCT_NUMBER 0x04
#define MODULE_INFO_ADDRESS_MAC_ADDRESS 0x10
#define MODULE_INFO_ADDRESS_SERIAL_NUMBER 0x00

/// Mask defining the position of the product family code in the 32-bit product number.
#define PRODUCT_FAMILY_CODE_MASK 0xFFFF0000
#define PRODUCT_FAMILY_CODE_BIT_OFFSET 16

/// Mask defining the position of the product subtype in the 32-bit product number.
#define PRODUCT_SUBTYPE_MASK 0x0000FF00
#define PRODUCT_SUBTYPE_BIT_OFFSET 8

/// Mask defining the position of the product revision number in the 32-bit product number.
#define REVISION_NUMBER_MASK 0x000000FF

/// Struct for constituent product number elements.
typedef struct ProductNumberInfo_t
{
    uint16_t productFamilyCode;
    uint8_t productSubtype;
    uint8_t revisionNumber;
} ProductNumberInfo_t;

/**
 * \brief Parse a raw 32-bit product number to its constituent parts.
 *
 * @param productNumber 	32-bit product number
 * @return					Struct containing decoded values
 */
static inline ProductNumberInfo_t ParseProductNumber(uint32_t productNumber)
{
    ProductNumberInfo_t productNumberInfo;

    productNumberInfo.productFamilyCode = (productNumber & PRODUCT_FAMILY_CODE_MASK) >> PRODUCT_FAMILY_CODE_BIT_OFFSET;
    productNumberInfo.productSubtype = (productNumber & PRODUCT_SUBTYPE_MASK) >> PRODUCT_SUBTYPE_BIT_OFFSET;
    productNumberInfo.revisionNumber = productNumber & REVISION_NUMBER_MASK;

    return productNumberInfo;
}
