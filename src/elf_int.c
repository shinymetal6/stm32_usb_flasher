#include <stdio.h>
#include <stdlib.h>
#include <libusb-1.0/libusb.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <time.h>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>
#include "elf_int.h"

unsigned char   file_data[ELF_DATA_MAX_SIZE], out_data[OUT_DATA_MAX_SIZE];
Elf32_Ehdr      *elf_header;
Elf32_Shdr      *elf_sections[32];

int check_if_valid(char *name)
{
    if ( strcmp(name,".isr_vector" ) == 0 )
        return 0;
    if ( strcmp(name,".text" ) == 0 )
        return 0;
    if ( strcmp(name,".rodata" ) == 0 )
        return 0;
    if ( strcmp(name,".ARM.extab" ) == 0 )
        return 0;
    if ( strcmp(name,".ARM" ) == 0 )
        return 0;
    if ( strcmp(name,".preinit_array" ) == 0 )
        return 0;
    if ( strcmp(name,".init_array" ) == 0 )
        return 0;
    if ( strcmp(name,".fini_array" ) == 0 )
        return 0;
    if ( strcmp(name,".data" ) == 0 )
        return 0;
    return 1;
}

/* Returns -1 if file not found or file write error  , -2 if file is not ELF */
int load_elf(char *in_file, char *out_file , unsigned int verbose)
{
FILE *fp;
int len,i;
unsigned char section_name[256];
unsigned int initial_shoffset , magic , out_size;
unsigned char *entaddr;
unsigned char data_section[512];
unsigned char *data_section_ptr = &data_section[0];
unsigned int data_section_size;

    fp = fopen(in_file,"rb");
    if ( fp )
    {
        len = fread(file_data,1,ELF_DATA_MAX_SIZE,fp);
        fclose(fp);
    }
    else
    {
        printf("%s : Could not open file %s for reading\n", __FUNCTION__, in_file);
        return -1;
    }

    elf_header = (Elf32_Ehdr *)file_data;
    magic=elf_header->e_ident[0]<<24 | elf_header->e_ident[1] <<16 | elf_header->e_ident[2] << 8 | elf_header->e_ident[3];
    if ( magic != ELF_MAGIC )
    {
        return -2;
    }
    if ( verbose != 0 )
    {
        printf("Read file %s , len %d\n",in_file,len);
        printf("e_ident             0x%02x %c%c%c\n", elf_header->e_ident[0],elf_header->e_ident[1],elf_header->e_ident[2],elf_header->e_ident[3]);
        printf("e_type              0x%04x\n", elf_header->e_type );
        printf("e_machine           0x%04x\n", elf_header->e_machine );
        printf("e_version           0x%08x\n", elf_header->e_version );
        printf("e_entry             0x%08x\n", elf_header->e_entry );
        printf("e_phoff             0x%08x\n", elf_header->e_phoff );
        printf("e_shoff             0x%08x ( %d )\n", elf_header->e_shoff , elf_header->e_shoff );
        printf("e_flags             0x%08x\n", elf_header->e_flags );
        printf("e_ehsize            0x%04x ( %d bytes )\n", elf_header->e_ehsize, elf_header->e_ehsize );
        printf("e_phentsize         0x%04x\n", elf_header->e_phentsize );
        printf("e_phnum             0x%04x\n", elf_header->e_phnum );
        printf("e_shentsize         0x%04x\n", elf_header->e_shentsize );
        printf("e_shnum             0x%04x ( %d )\n", elf_header->e_shnum, elf_header->e_shnum );
        printf("e_shstrndx          0x%04x ( %d )\n", elf_header->e_shstrndx, elf_header->e_shstrndx );
        printf("Section header table starts @ 0x%08x , %d entries each %d bytes long\n",elf_header->e_shoff,elf_header->e_shnum,elf_header->e_shentsize);
    }

    for(i=0;i<elf_header->e_shnum;i++)
        elf_sections[i] = (Elf32_Shdr *)(file_data+elf_header->e_shoff+(i*elf_header->e_shentsize));
    out_size = 0;
    initial_shoffset = 0xffffffff;
    for(i=0;i<elf_header->e_shnum;i++)
    {
        entaddr = file_data+elf_sections[elf_header->e_shstrndx]->sh_addr+elf_sections[elf_header->e_shstrndx]->sh_offset+elf_sections[i]->sh_name;// pointer to name
        memcpy(section_name,(unsigned char*)entaddr,elf_sections[elf_header->e_shstrndx]->sh_size);
        if ( check_if_valid((char *)section_name) == 0 )
        {
            if ( verbose != 0 )
            {
                printf("****** Section %d : %s ***********\n",i,section_name);
                printf("sh_name         0x%08x\n", elf_sections[i]->sh_name );
                printf("sh_type         0x%08x\n", elf_sections[i]->sh_type );
                printf("sh_flags        0x%08x\n", elf_sections[i]->sh_flags );
                printf("sh_addr         0x%08x\n", elf_sections[i]->sh_addr );
                printf("sh_offset       0x%08x\n", elf_sections[i]->sh_offset );
                printf("sh_size         0x%08x\n", elf_sections[i]->sh_size );
                printf("sh_link         0x%08x\n", elf_sections[i]->sh_link );
                printf("sh_info         0x%08x\n", elf_sections[i]->sh_info );
                printf("sh_addralign    0x%08x\n", elf_sections[i]->sh_addralign );
                printf("sh_entsize      0x%08x\n\n", elf_sections[i]->sh_entsize );
            }
            if ( initial_shoffset == 0xffffffff)
                initial_shoffset = elf_sections[i]->sh_offset;
            if ( strcmp((char *)section_name,".data" ) == 0 )
            {
                data_section_size = elf_sections[i]->sh_size;
                memcpy(data_section_ptr, file_data+elf_sections[i]->sh_offset,data_section_size);
            }
            out_size += elf_sections[i]->sh_size;
        }
    }
    memcpy(out_data, file_data+initial_shoffset,out_size);
    memcpy(out_data+out_size-data_section_size, data_section_ptr,data_section_size);
    fp = fopen(out_file,"wb");
    if ( !fp )
    {
        printf("%s : File %s can't be opened\n",__FUNCTION__,out_file);
        return -1;
    }
    fwrite(out_data,1,out_size,fp);
    fclose(fp);
    return 0;
}

