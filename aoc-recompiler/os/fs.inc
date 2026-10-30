/* SPDX-License-Identifier: GPL-2.0-or-later
 * Serialized, read-only RomFS IStorage. No save-data writes or empty fake mounts.
 * ABI consulted: libnx services/fs.c and romfs_dev.h; original implementation.
 */
static int fs_seek(FILE *file,uint64_t offset) {
    if(offset>INT64_MAX)return 0;
#if defined(_WIN32)
    return _fseeki64(file,(__int64)offset,SEEK_SET)==0;
#else
    return fseeko(file,(off_t)offset,SEEK_SET)==0;
#endif
}
int hos_mount_romfs(Hos *os,const char *path) {
    if(!os || !path || os->romfs)return 0;
    FILE *file=fopen(path,"rb");if(!file)return 0;
#if defined(_WIN32)
    int end_ok=_fseeki64(file,0,SEEK_END)==0;int64_t total=end_ok?_ftelli64(file):-1;
#else
    int end_ok=fseeko(file,0,SEEK_END)==0;int64_t total=end_ok?(int64_t)ftello(file):-1;
#endif
    uint64_t header[10];
    if(total<80 || !fs_seek(file,0) || fread(header,1,80,file)!=80 || header[0]!=80){fclose(file);return 0;}
    /* This host/guest profile is little endian. Validate every table span. */
    for(unsigned i=1;i<9;i+=2) {
        if(header[i]<80 || header[i]>(uint64_t)total || header[i+1]>(uint64_t)total-header[i]){fclose(file);return 0;}
        for(unsigned j=1;j<i;j+=2)if(header[i]<header[j]+header[j+1] && header[j]<header[i]+header[i+1]){fclose(file);return 0;}
    }
    if(!header[2] || !header[6] || (header[2]|header[6])&3 || header[4]<24 || header[9]<80 || header[9]>(uint64_t)total){fclose(file);return 0;}
    os->romfs=file;os->romfs_size=(uint64_t)total;return 1;
}
static int fs_request(Ctx *c,Session *session,int kind,int domain,uint32_t command,uint32_t token,
                      const uint8_t *data,uint32_t size,int pid,uint64_t buffer,uint64_t length,uint32_t buffers) {
    Hos *os=owner(c);Session *state=session->domain_owner?session->domain_owner:session;
    uint8_t *out=os->memory+c->tpidrro_el0;
    if(kind==26) { /* IFileSystemProxy */
        if(command==1) {
            if(!pid || size<8 || buffers){c->x[0]=0x1c01;return 1;}
            state->initialized=1;ipc_response(out,token,0,0,0,0,domain,0);c->x[0]=0;return 1;
        }
        if(command==1005) { /* GetGlobalAccessLogMode: disabled in this read-only profile. */
            ipc_response(out,token,state->initialized?0:0xfa01,4,0,0,domain,0);c->x[0]=0;return 1;
        }
        if(command==200) { /* OpenDataStorageByCurrentProcess */
            if(!state->initialized){ipc_response(out,token,0xfa01,0,0,0,domain,0);c->x[0]=0;return 1;}
            if(!os->romfs){stop(c,HOS_STOP_UNIMPLEMENTED,"FSP RomFS storage is not mounted; set HOS_ROMFS_PATH to a raw RomFS image");return 1;}
            return applet_child(c,session,27,domain,token);
        }
    }
    if(kind==27) { /* IStorage */
        if(!os->romfs){stop(c,HOS_STOP_TRAP,"FSP storage lost its immutable backing image");return 1;}
        if(command==4){ipc_response_u64(out,token,os->romfs_size,domain);c->x[0]=0;return 1;}
        if(command==0) {
            if(size<16 || buffers!=1){c->x[0]=0x1c01;return 1;}
            uint64_t offset,count;memcpy(&offset,data,8);memcpy(&count,data+8,8);
            if(offset>INT64_MAX || offset>os->romfs_size || count>os->romfs_size-offset) {
                ipc_response(out,token,(3005u<<9)|2u,0,0,0,domain,0);c->x[0]=0;return 1;
            }
            if(count>length){c->x[0]=R_SIZE;return 1;}
            if(!hos_access(os,buffer,count,2)){c->x[0]=R_MEMORY;return 1;}
            if(os->trace)fprintf(stderr,"[HOS] RomFS read offset=%#llx bytes=%#llx\n",(unsigned long long)offset,(unsigned long long)count);
            uint32_t result=0;
            if(!fs_seek(os->romfs,offset) || fread(os->memory+buffer,1,(size_t)count,os->romfs)!=(size_t)count)result=(4703u<<9)|2u;
            ipc_response(out,token,result,0,0,0,domain,0);c->x[0]=0;return 1;
        }
    }
    return 0;
}
