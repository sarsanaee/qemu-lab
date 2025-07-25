#include "qemu/osdep.h"
#include "hw/mem/tagged_mem_simple_dev.h"
#include "qapi/error.h"
#include "qemu/module.h"
#include "hw/qdev-properties.h"
#include "hw/qdev-properties-system.h"

#define DEFAULT_TAGGED_MEM_SIZE (1 << 20) // 1 MB

void qmp_tagged_mem_realize(const char *id, Error **errp)
{
    DeviceState *dev = qdev_find_recursive(sysbus_get_default(), id);
    if (!dev) {
        error_setg(errp, "Device '%s' not found", id);
        return;
    }

    if (!object_dynamic_cast(OBJECT(dev), TYPE_TAGGED_MEM)) {
        error_setg(errp, "Device '%s' is not a tagged-mem device", id);
        return;
    }

    TaggedMemState *s = TAGGED_MEM(dev);
    if (memory_region_size(&s->mem) > 0) {
        error_setg(errp, "Device '%s' already has memory", id);
        return;
    }

    memory_region_init_ram(&s->mem, OBJECT(dev), "tagged-mem.ram", s->size, errp);
    if (*errp) {
        return;
    }

    // Optional: map it into system memory at a fixed location (for test/demo)
    // memory_region_add_subregion(get_system_memory(), 0x40000000, &s->mem);
}

static void tagged_mem_realize(DeviceState *dev, Error **errp)
{
    TaggedMemState *s = TAGGED_MEM(dev);

    memory_region_init_ram(&s->mem, OBJECT(dev), "tagged-mem.ram", s->size, errp);
    if (*errp) {
        return;
    }

    // You can map later using memory_region_add_subregion()
}

static const Property tagged_mem_properties[] = {
    DEFINE_PROP_SIZE("size", TaggedMemState, size, DEFAULT_TAGGED_MEM_SIZE),
};

static void tagged_mem_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    dc->realize = tagged_mem_realize;
    dc->desc = "Tagged Memory Device with RAM Region";
    device_class_set_props(dc, tagged_mem_properties);
    // dc->props_ = tagged_mem_properties;
}

static const TypeInfo tagged_mem_info = {
    .name          = TYPE_TAGGED_MEM,
    .parent        = TYPE_DEVICE,
    .instance_size = sizeof(TaggedMemState),
    .class_init    = tagged_mem_class_init,
};

static void tagged_mem_register_types(void)
{
    type_register_static(&tagged_mem_info);
}

type_init(tagged_mem_register_types);

