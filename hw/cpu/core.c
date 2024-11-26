/*
 * CPU core abstract device
 *
 * Copyright (C) 2016 Bharata B Rao <bharata@linux.vnet.ibm.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#include "qemu/osdep.h"

#include "hw/boards.h"
#include "hw/cpu/core.h"
#include "qapi/error.h"
#include "qapi/visitor.h"

static void core_prop_get_core_id(Object *obj, Visitor *v, const char *name,
                                  void *opaque, Error **errp)
{
    CPUCore *core = CPU_CORE(obj);
    int64_t value = core->core_id;

    visit_type_int(v, name, &value, errp);
}

static void core_prop_set_core_id(Object *obj, Visitor *v, const char *name,
                                  void *opaque, Error **errp)
{
    CPUCore *core = CPU_CORE(obj);
    int64_t value;

    if (!visit_type_int(v, name, &value, errp)) {
        return;
    }

    if (value < 0) {
        error_setg(errp, "Invalid core id %"PRId64, value);
        return;
    }

    core->core_id = value;
}

static void core_prop_get_nr_threads(Object *obj, Visitor *v, const char *name,
                                     void *opaque, Error **errp)
{
    CPUCore *core = CPU_CORE(obj);
    int64_t value = core->nr_threads;

    visit_type_int(v, name, &value, errp);
}

static void core_prop_set_nr_threads(Object *obj, Visitor *v, const char *name,
                                     void *opaque, Error **errp)
{
    CPUCore *core = CPU_CORE(obj);
    int64_t value;

    if (!visit_type_int(v, name, &value, errp)) {
        return;
    }

    core->nr_threads = value;
}

static void cpu_core_instance_init(Object *obj)
{
    CPUCore *core = CPU_CORE(obj);

    /*
     * Only '-device something-cpu-core,help' can get us there before
     * the machine has been created. We don't care to set nr_threads
     * in this case since it isn't used afterwards.
     */
    if (current_machine) {
        core->nr_threads = current_machine->smp.threads;
    }
}

static void cpu_core_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    set_bit(DEVICE_CATEGORY_CPU, dc->categories);
    object_class_property_add(oc, "core-id", "int", core_prop_get_core_id,
                              core_prop_set_core_id, NULL, NULL);
    object_class_property_add(oc, "nr-threads", "int", core_prop_get_nr_threads,
                              core_prop_set_nr_threads, NULL, NULL);
}

static const TypeInfo cpu_core_type_info = {
    .name = TYPE_CPU_CORE,
    .parent = TYPE_DEVICE,
    .abstract = true,
    .class_init = cpu_core_class_init,
    .instance_size = sizeof(CPUCore),
    .instance_init = cpu_core_instance_init,
};

static void cpu_core_register_types(void)
{
    type_register_static(&cpu_core_type_info);
}

bool cache_described_at(const MachineState *ms, CpuTopologyLevel level)
{
    if (machine_get_cache_topo_level(ms, CACHE_LEVEL_AND_TYPE_L3) == level ||
        machine_get_cache_topo_level(ms, CACHE_LEVEL_AND_TYPE_L2) == level ||
        machine_get_cache_topo_level(ms, CACHE_LEVEL_AND_TYPE_L1I) == level ||
        machine_get_cache_topo_level(ms, CACHE_LEVEL_AND_TYPE_L1D) == level) {
        return true;
    }
    return false;
}

int partial_cache_description(const MachineState *ms, PPTTCPUCaches *caches,
                              int num_caches)
{
    int level, c;

    for (level = 1; level < num_caches; level++) {
        for (c = 0; c < num_caches; c++) {
            if (caches[c].level != level) {
                continue;
            }

            switch (level) {
            case 1:
                /*
                 * L1 cache is assumed to have both L1I and L1D available.
                 * Technically both need to be checked.
                 */
                if (machine_get_cache_topo_level(ms, 
                                                 CACHE_LEVEL_AND_TYPE_L1I) ==
                    CPU_TOPOLOGY_LEVEL_DEFAULT) {
                    return level;
                }
                break;
            case 2:
                if (machine_get_cache_topo_level(ms, CACHE_LEVEL_AND_TYPE_L2) ==
                    CPU_TOPOLOGY_LEVEL_DEFAULT) {
                    return level;
                }
                break;
            case 3:
                if (machine_get_cache_topo_level(ms, CACHE_LEVEL_AND_TYPE_L3) ==
                    CPU_TOPOLOGY_LEVEL_DEFAULT) {
                    return level;
                }
                break;
            }
        }
    }

    return 0;
}

/*
 * This function assumes l3 and l2 have unified cache and l1 is split l1d
 * and l1i, and further prepares the lowest cache level for a topology
 * level.  The info will be fed to build_caches to create caches at the
 * right level.
 */
bool find_the_lowest_level_cache_defined_at_level(const MachineState *ms,
                                                  int *level_found,
                                                  CpuTopologyLevel topo_level) {

    CpuTopologyLevel level;

    level = machine_get_cache_topo_level(ms, CACHE_LEVEL_AND_TYPE_L1I);
    if (level == topo_level) {
        *level_found = 1;
        return true;
    }

    level = machine_get_cache_topo_level(ms, CACHE_LEVEL_AND_TYPE_L1D);
    if (level == topo_level) {
        *level_found = 1;
        return true;
    }

    level = machine_get_cache_topo_level(ms, CACHE_LEVEL_AND_TYPE_L2);
    if (level == topo_level) {
        *level_found = 2;
        return true;
    }

    level = machine_get_cache_topo_level(ms, CACHE_LEVEL_AND_TYPE_L3);
    if (level == topo_level) {
        *level_found = 3;
        return true;
    }

    return false;
}

type_init(cpu_core_register_types)
