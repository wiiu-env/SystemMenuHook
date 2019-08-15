#ifndef ELF_LOADING_H
#define	ELF_LOADING_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int32_t LoadFileToMem(const char *relativefilepath, char **fileOut, uint32_t * sizeOut);
uint32_t load_loader_elf(unsigned char* baseAddress, const char* relativePath);

#ifdef __cplusplus
}
#endif

#endif	/* ELF_LOADING_H */

