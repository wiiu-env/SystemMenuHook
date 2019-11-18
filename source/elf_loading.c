#include <stdio.h>
#include <string.h>

#include <coreinit/debug.h>
#include <coreinit/cache.h>
#include <coreinit/memdefaultheap.h>
#include <whb/sdcard.h>
#include <whb/file.h>
#include <whb/log.h>

#include "elf_abi.h"
#include "elf_loading.h"

int32_t LoadFileToMem(const char *relativefilepath, char **fileOut, uint32_t * sizeOut) {
    char path[256];
    int result = 0;
    if (!WHBMountSdCard()) {
        WHBLogPrintf("Failed to mount SD Card...");
        result = -1;
        goto exit;
    }

    char * sdRootPath = WHBGetSdCardMountPath();
    sprintf(path, "%s/%s", sdRootPath,relativefilepath);

    WHBLogPrintf("Loading file %s.",path);

    *fileOut = WHBReadWholeFile(path, sizeOut);
    if (!(*fileOut)) {
        result = -2;
        WHBLogPrintf("WHBReadWholeFile(%s) returned NULL", path);
        goto exit;
    }

exit:
    WHBUnmountSdCard();
    return result;
}

uint32_t load_loader_elf(unsigned char* baseAddress, const char* relativePath) {
    char * elf_data = NULL;
    uint32_t fileSize = 0;
    if(LoadFileToMem(relativePath, &elf_data, &fileSize) != 0) {
        OSFatal("Failed to load hook_payload.elf from the SD Card.");
    }

    Elf32_Ehdr *ehdr;
    Elf32_Phdr *phdrs;
    uint8_t *image;
    int32_t i;

    ehdr = (Elf32_Ehdr *) elf_data;

    if(ehdr->e_phoff == 0 || ehdr->e_phnum == 0) {
        return 0;
    }

    if(ehdr->e_phentsize != sizeof(Elf32_Phdr)) {
        return 0;
    }

    phdrs = (Elf32_Phdr*)(elf_data + ehdr->e_phoff);

    for(i = 0; i < ehdr->e_phnum; i++) {
        if(phdrs[i].p_type != PT_LOAD) {
            continue;
        }

        if(phdrs[i].p_filesz > phdrs[i].p_memsz) {
            continue;
        }

        if(!phdrs[i].p_filesz) {
            continue;
        }

        uint32_t p_paddr = phdrs[i].p_paddr + (uint32_t)baseAddress;
        image = (uint8_t *) (elf_data + phdrs[i].p_offset);

        memcpy ((void *) p_paddr, image, phdrs[i].p_filesz);
        DCFlushRange((void*)p_paddr, phdrs[i].p_filesz);

        if(phdrs[i].p_flags & PF_X) {
            ICInvalidateRange ((void *) p_paddr, phdrs[i].p_memsz);
        }
    }

    //! clear BSS
    Elf32_Shdr *shdr = (Elf32_Shdr *) (elf_data + ehdr->e_shoff);
    for(i = 0; i < ehdr->e_shnum; i++) {
        const char *section_name = ((const char*)elf_data) + shdr[ehdr->e_shstrndx].sh_offset + shdr[i].sh_name;
        if(section_name[0] == '.' && section_name[1] == 'b' && section_name[2] == 's' && section_name[3] == 's') {
            memset((void*)(shdr[i].sh_addr + baseAddress), 0, shdr[i].sh_size);
            DCFlushRange((void*)(shdr[i].sh_addr + baseAddress), shdr[i].sh_size);
        } else if(section_name[0] == '.' && section_name[1] == 's' && section_name[2] == 'b' && section_name[3] == 's' && section_name[4] == 's') {
            memset((void*)(shdr[i].sh_addr + baseAddress), 0, shdr[i].sh_size);
            DCFlushRange((void*)(shdr[i].sh_addr + baseAddress), shdr[i].sh_size);
        }
    }

    MEMFreeToDefaultHeap((void*)elf_data);

    return ehdr->e_entry;
}
