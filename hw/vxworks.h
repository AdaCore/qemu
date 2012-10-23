#ifndef _VXWORKS_H_
#define _VXWORKS_H_

#include "qemu-common.h"
#include "loader.h"
#include "elf.h"

int vx653_loader(const char *filename,
                 uint64_t    *elf_entry_out,
                 uint64_t    *elf_lowaddr_out);


#endif /* ! _VXWORKS_H_ */
