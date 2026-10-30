/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "hos.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#ifdef HOS_HAS_STARTUP
#include "hos_startup.h"
#endif
int main(int argc,char **argv) {
    if(argc<2 || argc>3){fprintf(stderr,"Usage: aoc-os-diagnostic image.bin [entry-address]\n");return 1;}
    uint64_t address=HOS_IMAGE_BASE;
    if(argc==3){char *end;errno=0;address=strtoull(argv[2],&end,0);if(errno || !*argv[2] || *end){fprintf(stderr,"Invalid entry\n");return 1;}}
    Hos *os=hos_create();
    if(!os){fprintf(stderr,"Cannot reserve the guest address window\n");return 1;}
    if(!hos_load_image(os,argv[1])){fprintf(stderr,"Cannot load linked image\n");hos_destroy(os);return 1;}
    Ctx context;hos_context(os,&context);
    int reason=HOS_STOP_RETURNED;
#ifdef HOS_HAS_STARTUP
    unsigned count=argc==3?1:HOS_STARTUP_COUNT;
#else
    unsigned count=1;
    if(argc!=3){fprintf(stderr,"No startup manifest: provide an explicit entry address\n");hos_destroy(os);return 1;}
#endif
    for(unsigned stage=0;stage<count;stage++) {
        const char *name="explicit entry";
#ifdef HOS_HAS_STARTUP
        if(argc!=3){
            address=hos_startup[stage].address;name=hos_startup[stage].name;
            context.x[0]=hos_startup[stage].thread_argument?HOS_MAIN_HANDLE:0;
            context.x[1]=0;
        }
#endif
        A64Fn entry=a64_lookup(address);
        if(!entry){fprintf(stderr,"Entry %s at %#llx is not lifted\n",name,(unsigned long long)address);hos_destroy(os);return 1;}
        context.x[30]=0;context.x[31]=HOS_STACK_BASE+HOS_STACK_SIZE;context.pc=address;
        fprintf(stderr,"[HOS] experimental startup: %s at %#llx; no renderer\n",name,(unsigned long long)address);fflush(stderr);
        reason=hos_run(os,&context,entry);
        fprintf(stderr,"[HOS] stop=%d svc=%#x pc=%#llx heap=%#llx: %s\n",reason,hos_last_svc(os),
            (unsigned long long)context.pc,(unsigned long long)hos_heap_size(os),hos_stop_message(os));
        if(reason!=HOS_STOP_RETURNED)break;
    }
    puts("Game boot/playability has not been established by this diagnostic.");
    hos_destroy(os);
    return reason==HOS_STOP_RETURNED || reason==HOS_STOP_PROCESS_EXIT || reason==HOS_STOP_THREAD_EXIT ? 0:2;
}
