#pragma once

#include "value.h"


namespace JSON20_NAMESPACE_NAME {

enum class format_t {
    //standard JSON
    text,
    //TLV binary version
    binary
};

/* ***************** TLV *************************************
 *
 * Tag = <ttttnnnn>
 *    - 4bit type
 *    - 4bit a unsigned integer number with various meaning
 *
 * The number can be extended up to 8-bytes unsigned long
 *
 * for number 0..7, the number is directly stored.
 *    Example: tttt0010: number = 3
 *             tttt0000: number = 0
 *             tttt0111: number = 7
 *
 * for number above 7, the n contains count of additional bytes -7.
 *    Example: tttt1000: 1 byte follows which contains a number (8-7=1)
 *             tttt1010: 3 bytes follows                        (10-7=3)
 *             tttt1111: 8 bytes follows                        (15-7=8)
 *
 *    Example of number:
 *             tttt1111 E8 6A 23 4A 28 CQ EB A1: number = A1EBC1284A236AE8
 *             tttt1000 41: number is 0x41
 *             tttt1001 AA 55: number is 55AA
 *             tttt1000 03: number is 0x03, however it should be written as tttt0011
 *
 *
 *
 * 0x0  signle tags (each tag is signle, defines specified action)
 *          0x0 - sync - NOP, skip byte, binary mark (detects binary format)
 *          0x1 - undefined
 *          0x2 - null value.
 *          0x3 - false value
 *          0x4 - true value
 * 0xB - follows 4 bytes containing float number in little endian order (only defined, not used)
 * 0xF - follows 8 bytes containing double number in little endian order
 * 0x10 - string: number=length, follows characters up to length
 * 0x20 - binary string: number=length, follows bytes up to length
 * 0x30 - number string: number=length, follows characters up to length (number as string)
 * 0x40 - positive number: number = value (+value)
 * 0x50 - negative number: number = value (-value)
 * 0x60 - array: number = count - follows items
 * 0x70 - object: number = count - follows pairs key-value (key is encoded as 0x10)
 * 0x80-0xFF - reserved for future use
 */

enum class bin_element_t:unsigned char {
    sync = 0x0,             //just NOP, ignored, no element is emited
    undefined = 0x1,        //emit undefined
    null = 0x2,             //emit null
    bool_false = 0x3,       //emit boolean false
    bool_true = 0x4,        //emit boolean true
    placeholder = 0xA,       //emit placeholder, follows 2 bytes position
    num_double =0xF,        //8 bytes of double follows little endian
    string = 0x10,          //0x1<size...> string
    bin_string = 0x20,      //0x2<size...> string number
    num_string = 0x30,      //0x3<size...> binary string
    pos_number = 0x40,      //0x4<number...> positive number
    neg_number = 0x50,      //0x5<number...> negative number
    array = 0x60,           //0x6<count...> (items)
    object = 0x70           //0x6<count...> (key, value)
};




}
