#ifndef _SHA1_H
#define _SHA1_H

#include <stdint.h>

#define SHA1_BLOCK_SIZE 20

void SHA1(const uint8_t* data, uint32_t len, uint8_t digest[SHA1_BLOCK_SIZE]);

#endif
