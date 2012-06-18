#ifndef _HOSTFS_H_
#define _HOSTFS_H_

DeviceState *hostfs_create(target_phys_addr_t  base,
                           MemoryRegion       *mr);


#endif /* ! _HOSTFS_H_ */
