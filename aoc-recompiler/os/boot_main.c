/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "hos.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
int main(int argc,char **argv) {
    if(argc<2 || argc>3){fprintf(stderr,"Usage: aoc-os-diagnostic image.bin [entry-address]\n");return 1;}
    uint64_t address=HOS_IMAGE_BASE;
    if(argc==3){char *end;errno=0;address=strtoull(argv[2],&end,0);if(errno || !*argv[2] || *end){fprintf(stderr,"Invalid entry\n");return 1;}}
    Hos *os=hos_create();
    if(!os){fprintf(stderr,"Cannot reserve the guest address window\n");return 1;}
    if(!hos_load_image(os,argv[1])){fprintf(stderr,"Cannot load linked image\n");hos_destroy(os);return 1;}
    A64Fn entry=a64_lookup(address);
    if(!entry){fprintf(stderr,"Entry is not in the generated function table\n");hos_destroy(os);return 1;}
    Ctx context;hos_context(os,&context);
    fprintf(stderr,"[HOS] single-thread bootstrap diagnostic at %#llx; no renderer\n",(unsigned long long)address);fflush(stderr);
    int reason=hos_run(os,&context,entry);
    fprintf(stderr,"[HOS] stop=%d svc=%#x pc=%#llx heap=%#llx: %s\n",reason,hos_last_svc(os),
        (unsigned long long)context.pc,(unsigned long long)hos_heap_size(os),hos_stop_message(os));
    puts("Game boot/playability has not been established by this diagnostic.");
    hos_destroy(os);
    return reason==HOS_STOP_RETURNED || reason==HOS_STOP_PROCESS_EXIT || reason==HOS_STOP_THREAD_EXIT ? 0:2;
}
