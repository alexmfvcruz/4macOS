#pragma once
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Allocated premultiplied BGRA8; caller frees with fourstory_account_image_free. */
uint8_t* fourstory_account_image(const uint8_t* data,size_t size,size_t* width,size_t* height);
void fourstory_account_image_free(uint8_t* data);
#ifdef __cplusplus
}
#endif
