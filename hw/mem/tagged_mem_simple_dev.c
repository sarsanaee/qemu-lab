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

// void qmp_hello_from_tagged_mem(const char *name, Error **errp)
// {
//         fprintf(stderr, ">>> Hello %s from tagged memory backend module! \n", name);
// }

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

int visit_tagged(Object *obj, void *opaque) {
    int res;

	res = check_property_equals_test(obj, "tag", opaque);
    if (res < 0) {
	    object_child_foreach(obj, visit_tagged, opaque); // this is not the best way
                                                         // because it searches all children which is wrong. I want to bail out immediately if one of the childs give 0
    }

	return res;
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
        return NULL;
    }

    return ctx.result;
}


void qmp_hello_from_tagged_mem(const char *tag, Error **errp)
{
    Object *root = object_get_objects_root();

	object_child_foreach(root, visit_tagged, NULL);
}

// static void visit_tagged(Object *obj, const char *name, Object *child, void *opaque) {
//     TagSearchContext *ctx = opaque;
// 
//     if (object_dynamic_cast(child, TYPE_YOUR_TAGGED_BACKEND)) {
//         Error *err = NULL;
//         char *val = object_property_get_str(child, "tag", &err);
//         if (!err && val && strcmp(val, ctx->tag_value) == 0) {
//             ctx->result = MEMORY_BACKEND(child);
//             g_free(val);
//             return;
//         }
//         g_free(val);
//         if (err) error_free(err);
//     }
// 
//     object_child_foreach(child, visit_tagged, opaque);
// }



// void qmp_hello_from_tagged_mem(const char *tag, Error **errp)
// {
//     Object *root = object_get_objects_root();
//     Object *found = NULL;
// 
//     void search_cb(Object *obj, void *opaque)
//     {
//         const char *tag = opaque;
//         Error *local_err = NULL;
// 
//         if (!object_property_find(obj, "tag"))
//             return;
// 
//         char *val = object_property_get_str(obj, "tag", &local_err);
//         if (local_err) {
//             error_free(local_err);
//             return;
//         }
// 
//         if (strcmp(val, tag) == 0) {
//             fprintf(stderr, ">>> Hello from object '%s' with tag='%s'\n",
//                     object_get_canonical_path_component(obj), tag);
//             g_free(val);
//             found = obj;
//         } else {
//             g_free(val);
//         }
//     }
// 
//     object_child_foreach(root, search_cb, (void *)tag);
// 
//     if (!found) {
//         error_setg(errp, "No object found with tag='%s'", tag);
//     }
// }

//static void memory_backend_tagged_realize(DeviceState *dev, Error **errp)
//{
//     MemoryBackendTagged *mbt = MEMORY_BACKEND_TAGGED(dev);
//     HostMemoryBackend *backend = MEMORY_BACKEND(dev);
//
//     if (backend->size == 0) {
//         error_setg(errp, "memory-backend-tagged: size must be > 0");
//         return;
//     }
//
//     memory_region_init_ram(&backend->mr, OBJECT(dev),
//                            "memory-backend-tagged.ram", backend->size, errp);
//     if (*errp) {
//         return;
//     }
//
//     backend->mr_inited = true;
//}
//
// static const Property TaggedMemoryProps[] = {
//     DEFINE_PROP_STRING("tag", MemoryBackendTagged, tag),
// };

// static void tagged_mem_class_init(ObjectClass *klass, const void *data)
// {
//     DeviceClass *dc = DEVICE_CLASS(klass);
//     dc->realize = memory_backend_tagged_realize;
//     dc->desc = "Tagged Memory Device with RAM Region";
//     device_class_set_props(dc, TaggedMemoryProps);
//     // dc->props_ = tagged_mem_properties;
// }

// void qmp_hello_from_tagged_mem(const char *name, Error **errp)
// {
//     fprintf(stderr, ">>> Hello %s from tagged memory backend module! 🎉\n", name);
// }

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

static bool
ram_backend_memory_alloc(HostMemoryBackend *backend, Error **errp)
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
    return memory_region_init_ram_flags_nomigrate(&backend->mr, OBJECT(backend),
                                                  name, backend->size,
                                                  ram_flags, errp);
}

static void memory_backend_tagged_class_init(ObjectClass *oc, const void *data)
{
    // DeviceClass *dc = DEVICE_CLASS(oc);
    // dc->realize = memory_backend_tagged_realize;
    // dc->desc = "Minimal tagged memory backend";
    // device_class_set_props(dc, TaggedMemoryProps);
    HostMemoryBackendClass *bc = MEMORY_BACKEND_CLASS(oc);
    bc->alloc = ram_backend_memory_alloc;
    printf("HERE too??\n");
    object_class_property_add_str(oc, "tag",
        tagged_mem_get_tag,
        tagged_mem_set_tag);
}

static const TypeInfo memory_backend_tagged_info = {
    .name          = TYPE_MEMORY_BACKEND_TAGGED,
    .parent        = TYPE_MEMORY_BACKEND,
    .instance_size = sizeof(MemoryBackendTagged),
    .class_init    = memory_backend_tagged_class_init,
};

static void memory_backend_tagged_register_types(void)
{
    printf("HERE?\n");
    type_register_static(&memory_backend_tagged_info);
}

type_init(memory_backend_tagged_register_types);
