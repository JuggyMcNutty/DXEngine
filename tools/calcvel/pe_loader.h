#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

uint32_t pe_map(const char* path);
uint32_t pe_export(uint32_t base, const char* name);
void pe_bind(uint32_t base, uint32_t coreBase);
int pe_strip_seh(uint32_t start, uint32_t end);

#ifdef __cplusplus
}
#endif
