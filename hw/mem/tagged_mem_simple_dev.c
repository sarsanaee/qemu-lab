#include "qemu/osdep.h"
#include "hw/mem/tagged_mem_simple_dev.h"
#include "qapi/error.h"
#include "qemu/module.h"
#include "hw/qdev-properties.h"
#include "hw/qdev-properties-system.h"
#include "qapi/error.h"
#include "qapi/qapi-commands-tagged-mem.h"
#include "qom/object.h"
#include "qom/qom-qobject.h"

static int check_property_equals_test(Object *obj, const char *prop_name,
                                      void *opaque)
{
    struct TagSearchContext *ctx = opaque;
    ObjectProperty *prop = object_property_find(obj, prop_name);
    if (!prop)
        return -1;

    Error *err = NULL;
    char *value = object_property_get_str(obj, prop_name, &err);
    if (err) {
        error_report_err(err);
        return -1;
    }

    if (strcmp(value, ctx->tag_value) == 0) {
        ctx->result = MEMORY_BACKEND(obj);
        g_free(value);
        return 0;
    }

    g_free(value);
    return -1;
}

int visit_tagged(Object *obj, void *opaque)
{
    int res;

    res = check_property_equals_test(obj, "tag", opaque);
    if (res < 0) {
        object_child_foreach(obj, visit_tagged, opaque);
    }

    return 0;
}

HostMemoryBackend *memory_backend_tagged_find_by_tag(const char *tag,
                                                     Error **errp)
{
    struct TagSearchContext ctx = {
        .tag_value = tag,
        .result = NULL,
    };

    Object *root = object_get_objects_root();
    object_child_foreach(root, visit_tagged, &ctx);

    if (!ctx.result) {
        printf("didn't find any results!\n");
        return NULL;
    }

    return ctx.result;
}


void qmp_hello_from_tagged_mem(const char *tag, Error **errp)
{
    Object *root = object_get_objects_root();
	object_child_foreach(root, visit_tagged, NULL);
}

static void tagged_mem_set_tag(Object *obj, const char *value, Error **errp)
{
    MemoryBackendTagged *tm = MEMORY_BACKEND_TAGGED(obj);
    g_free(tm->tag);
    tm->tag = g_strdup(value);
}

static char *tagged_mem_get_tag(Object *obj, Error **errp)
{
    MemoryBackendTagged *tm = MEMORY_BACKEND_TAGGED(obj);
    return g_strdup(tm->tag);
}

static bool ram_backend_memory_alloc(HostMemoryBackend *backend, Error **errp)
{
    g_autofree char *name = NULL;
    uint32_t ram_flags;

    if (!backend->size) {
        error_setg(errp, "can't create backend with size 0");
        return false;
    }

    name = host_memory_backend_get_name(backend);
    ram_flags = backend->share ? RAM_SHARED : RAM_PRIVATE;
    ram_flags |= backend->reserve ? 0 : RAM_NORESERVE;
    ram_flags |= backend->guest_memfd ? RAM_GUEST_MEMFD : 0;
    return memory_region_init_ram_flags_nomigrate(
        &backend->mr, OBJECT(backend), name, backend->size, ram_flags, errp);
}

static void memory_backend_tagged_class_init(ObjectClass *oc, const void *data)
{
    HostMemoryBackendClass *bc = MEMORY_BACKEND_CLASS(oc);
    bc->alloc = ram_backend_memory_alloc;
    object_class_property_add_str(oc, "tag", tagged_mem_get_tag,
                                  tagged_mem_set_tag);
}

static const TypeInfo memory_backend_tagged_info = {
    .name = TYPE_MEMORY_BACKEND_TAGGED,
    .parent = TYPE_MEMORY_BACKEND,
    .instance_size = sizeof(MemoryBackendTagged),
    .class_init = memory_backend_tagged_class_init,
};

static void memory_backend_tagged_register_types(void)
{
    type_register_static(&memory_backend_tagged_info);
}

type_init(memory_backend_tagged_register_types);
