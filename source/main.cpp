#include <stdio.h>
#include <string.h>

#include <coreinit/ios.h>

#include <whb/proc.h>
#include <whb/log.h>
#include <whb/log_udp.h>

#include "gx2sploit.h"

#ifdef __cplusplus
extern "C" {
#endif

// Similar to SYSRelaunchTitle but with less checks
void OSRestartGame(int,char**);

#ifdef __cplusplus
}
#endif

int main(int argc, char **argv) {
    WHBLogUdpInit();
    if(CheckKernelExploit()) {
        // When the kernel exploit is set up successfully, we signal the ios to move on.
        int mcpFd = IOS_Open("/dev/mcp", (IOSOpenMode)0);
        if(mcpFd >= 0) {
            int in = 0xFD;//IPC_CUSTOM_MEN_RPX_HOOK_COMPLETED;
            int out = 0;
           
            IOS_Ioctl(mcpFd, 100, &in, sizeof(in), &out, sizeof(out));
            IOS_Close(mcpFd);
        }
    }
    WHBLogUdpDeinit();
    OSRestartGame(argc,argv);

    return 0;
}
