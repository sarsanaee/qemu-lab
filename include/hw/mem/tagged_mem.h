/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Tagged Memory backend
 *
 * Copyright (c) 2025 Alireza Sanaee <alireza.sanaee@huawei.com>
 */
#ifndef HW_TAGGED_MEM_H
#define HW_TAGGED_MEM_H

#include "system/memory.h"
#include "system/hostmem.h"

#define TYPE_MEMORY_BACKEND_TAGGED "memory-backend-tagged"
OBJECT_DECLARE_SIMPLE_TYPE(MemoryBackendTagged, MEMORY_BACKEND_TAGGED)

typedef struct MemoryBackendTagged {
    HostMemoryBackend parent_obj;

    char *tag;
} MemoryBackendTagged;

struct TagSearchContext {
    const char *tag_value;
    HostMemoryBackend *result;
};

HostMemoryBackend *memory_backend_tagged_find_by_tag(const char *tag,
                                                     Error **errp);

#endif
