#ifndef HW_MISC_TAGGED_MEM_H
#define HW_MISC_TAGGED_MEM_H

#include "hw/qdev-core.h"
#include "system/memory.h"
#include "system/hostmem.h"

#define TYPE_MEMORY_BACKEND_TAGGED "memory-backend-tagged"
OBJECT_DECLARE_SIMPLE_TYPE(MemoryBackendTagged, MEMORY_BACKEND_TAGGED)

typedef struct MemoryBackendTagged {
    HostMemoryBackend parent_obj;

    /* Optional metadata for tagging or identification */
    char *tag;
} MemoryBackendTagged;

#endif // HW_MISC_TAGGED_MEM_H

