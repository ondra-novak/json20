#pragma once

#include "value.h"

#include <utility>
namespace json20 {

enum class format_t {
    //standard JSON
    text,
    //TLV binary version
    binary
};

/* ***************** TLV *************************************
 *
 * Tag = <tttttlll>
 *    - 5bit type
 *    - 3bit bytes of length or value - 1 /depend on type
 *
 *
 * unsigned numbers are big endian
 * double is bit_cast to unsigned number in big endian
 *
 * 0x0  sync- empty byte, skipped
 * 0x08 undefined       00001000
 * 0x10 string          00010lll <length> <text>  (l - bytes of length -1 )
 * 0x18 binary string   00011lll <length> <text>  (l - bytes of length -1 )
 * 0x20 positive number 00100lll <value>  (l - bytes of value)
 * 0x28 negative number 00101lll <value>  (l - bytes of value)
 * 0x30 array           00110lll <count of items> (l - bytes of count - 1)
 * 0x38 object          00111lll <count of pairs> (l - bytes of count - 1)
 * 0x40 double          01000000 <8 bytes of double>
 * 0x48 num.string      01001lll <length> <text>  (l - bytes of length -1 )
 * 0x50 boolean         0101000b b - boolean value
 * 0x58 null            01011000
 *
 * 0x38 0x07 0x10 0x06 0x64 0x65 0x6c 0x65 0x74 0x65 0x08
 *  |     |    |    |    |                            +----- null
 *  |     |    |    |    +------ "delete"
 *  |     |    |    +-------6 bytes
 *  |     |    +------- string, 1 byte for length
 *  |     +--------7 pairs
 *  +----- object, 1 byte for length
 *
 */

enum class bin_element_t:char {
    sync = 0x0,       /**< sync */
    undefined = 0x08, /**< undefined */
    string = 0x10,    /**< string */
    bin_string = 0x18,/**< bin_string */
    pos_number = 0x20,/**< pos_number */
    neg_number = 0x28,/**< neg_number */
    array = 0x30,     /**< array */
    object = 0x38,    /**< object */
    num_double = 0x40,/**< num_double */
    num_string = 0x48,/**< num_string */
    boolean = 0x50,   /**< boolean */
    null = 0x58,      /**< null */
};

constexpr char encode_tag(bin_element_t el, unsigned int size = 0) {
    return static_cast<char>(el) | static_cast<char>(size);
}

constexpr std::pair<bin_element_t, unsigned int> decode_tag(char tag) {
    return {static_cast<bin_element_t>(tag & ~0x7), tag & 0x7};
}



}
