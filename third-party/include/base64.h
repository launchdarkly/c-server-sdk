#pragma once

unsigned char *LDi_base64_encode(const unsigned char *src, size_t len,
    size_t *out_len);

unsigned char * LDi_base64_decode(const unsigned char *src, size_t len,
    size_t *out_len);
