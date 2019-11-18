#include <stdio.h>
#include <string.h>

#include <coreinit/ios.h>
#include <coreinit/time.h>

#include <coreinit/systeminfo.h>
#include <coreinit/foreground.h>

#include <proc_ui/procui.h>
#include <coreinit/thread.h>

#include <whb/proc.h>
#include <whb/log.h>
#include <whb/log_udp.h>
#include <sysapp/launch.h>
#include <coreinit/exit.h>

#include "gx2sploit.h"

#ifdef __cplusplus
extern "C" {
#endif

// Similar to SYSRelaunchTitle but with less checks
void OSRestartGame(int,char**);
void SYSLaunchMenu();

#ifdef __cplusplus
}
#endif


bool CheckRunning(){

    switch(ProcUIProcessMessages(true))
    {
    case PROCUI_STATUS_EXITING:
    { 
		return false;
    }
    case PROCUI_STATUS_RELEASE_FOREGROUND:
    {
		ProcUIDrawDoneRelease();        
        break;
    }
    case PROCUI_STATUS_IN_FOREGROUND:
    {
        break;
    }
    case PROCUI_STATUS_IN_BACKGROUND:
    default:
        break;
    }
    return true;
}


int main(int argc, char **argv) {
    WHBLogUdpInit();
    bool returnToMenu = false;
    if(CheckKernelExploit()) {
        // When the kernel exploit is set up successfully, we signal the ios to move on.
        int mcpFd = IOS_Open("/dev/mcp", (IOSOpenMode)0);
        if(mcpFd >= 0) {
            int in = 0xFD;//IPC_CUSTOM_MEN_RPX_HOOK_COMPLETED;
            int out = 0;
           
            IOS_Ioctl(mcpFd, 100, &in, sizeof(in), &out, sizeof(out));
            IOS_Close(mcpFd);
            returnToMenu = true;
        }
    }
     
   
    
    WHBLogUdpDeinit();
    if(returnToMenu){     
        ProcUIInit(OSSavesDone_ReadyToRelease);    
        SYSLaunchMenu();
        while(CheckRunning()){
            // wait.
            OSSleepTicks(OSMillisecondsToTicks(100));
        }
        ProcUIShutdown();
    
    }else{
        OSRestartGame(argc, argv);
    }

 

    return 0;
}
