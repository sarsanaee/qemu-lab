/*
 * CPU core abstract device
 *
 * Copyright (C) 2016 Bharata B Rao <bharata@linux.vnet.ibm.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */
#ifndef HW_CPU_CORE_H
#define HW_CPU_CORE_H

#include "hw/qdev-core.h"
#include "qom/object.h"

#define TYPE_CPU_CORE "cpu-core"

OBJECT_DECLARE_SIMPLE_TYPE(CPUCore, CPU_CORE)

struct CPUCore {
    /*< private >*/
    DeviceState parent_obj;

    /*< public >*/
    int core_id;
    int nr_threads;
};

typedef enum CPUCacheType {
    DATA,
    INSTRUCTION,
    UNIFIED,
} PPTTCPUCacheType;

typedef struct PPTTCPUCaches {
    PPTTCPUCacheType type;
    uint32_t pptt_id;
    uint32_t sets;
    uint32_t size;
    uint32_t level;
    uint16_t linesize;
    uint8_t attributes; /* write policy: 0x0 write back, 0x1 write through */
    uint8_t associativity;
} PPTTCPUCaches;

int partial_cache_description(const MachineState *ms, PPTTCPUCaches *caches,
                              int num_caches);

bool cache_described_at(const MachineState *ms, CpuTopologyLevel level);

bool find_the_lowest_level_cache_defined_at_level(const MachineState *ms,
                                                  int *level_found,
                                                  CpuTopologyLevel topo_level);

/* Note: topology field names need to be kept in sync with
 * 'CpuInstanceProperties' */

#define CPU_CORE_PROP_CORE_ID "core-id"

#endif
