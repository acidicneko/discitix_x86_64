#include <mm/bitmap.h>

bool find_bit(bitmap_t* bitmap, uint64_t index){
    if (index > bitmap->size * 8) return false;
    uint64_t byte_index = index / 8;
    uint8_t bit_index = index % 8;
    uint8_t bit_indexer = 0b10000000 >> bit_index;
    if((bitmap->buffer[byte_index] & bit_indexer) > 0) return true;
    return false;
}

bool set_bit(bitmap_t* bitmap, uint64_t index, bool value){
    if (index > bitmap->size * 8) return false;
    uint64_t byte_index = index / 8;
    uint8_t bit_index = index % 8;
    uint8_t bit_indexer = 0b10000000 >> bit_index;
    bitmap->buffer[byte_index] &= ~bit_indexer;
    if(value) bitmap->buffer[byte_index] |= bit_indexer;
    return true;
}