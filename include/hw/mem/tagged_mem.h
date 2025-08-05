#ifndef HW_TAGGED_MEM_H
#define HW_TAGGED_MEM_H

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

struct TagSearchContext {
    const char *tag_value;
    HostMemoryBackend *result;
};

HostMemoryBackend *memory_backend_tagged_find_by_tag(const char *tag,
                                                     Error **errp);

int visit_tagged(Object *obj, void *opaque);

#endif // HW_TAGGED_MEM_H
