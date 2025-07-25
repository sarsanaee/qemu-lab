#ifndef HW_MISC_TAGGED_MEM_H
#define HW_MISC_TAGGED_MEM_H

#include "hw/qdev-core.h"
#include "system/memory.h"

#define TYPE_TAGGED_MEM "tagged-mem"
OBJECT_DECLARE_SIMPLE_TYPE(TaggedMemState, TAGGED_MEM)

typedef struct TaggedMemState {
    DeviceState parent_obj;
    MemoryRegion mem;
    uint64_t size;
} TaggedMemState;

#endif // HW_MISC_TAGGED_MEM_H

