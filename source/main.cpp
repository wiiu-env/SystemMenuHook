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
        // When the kernel exploit is set up successfully, we signal the rpx redirecting to move on.
        int mcpFd = IOS_Open("/dev/mcp", (IOSOpenMode)0);
        if(mcpFd >= 0) {
            int out = 0;
            const char * path = "done";
            IOS_Ioctl(mcpFd, 100, (void*)path, strlen(path), &out, sizeof(out));
            IOS_Close(mcpFd);
        }
    }
    WHBLogUdpDeinit();
    OSRestartGame(argc,argv);

    return 0;
}
