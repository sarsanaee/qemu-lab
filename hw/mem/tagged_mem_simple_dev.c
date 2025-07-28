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

static int check_property_equals_test(Object *obj, const char *prop_name) {
    ObjectProperty *prop = object_property_find(obj, prop_name);
    if (!prop) {
        printf("Property '%s' not found in object type '%s'\n",
               prop_name, object_get_typename(obj));
        return -1;
    }

    Error *err = NULL;
    char *value = object_property_get_str(obj, prop_name, &err);
    if (err) {
        error_report_err(err);
        return -1;
    }

    if (strcmp(value, "test") == 0) {
        printf("Property '%s' equals \"test\"\n", prop_name);
    } else {
        printf("Property '%s' is \"%s\" (not \"test\")\n", prop_name, value);
    }

    g_free(value);
    return 0;
}

int my_child_iterator(Object *obj, void *opaque);
int my_child_iterator(Object *obj, void *opaque) {
	// Do something with the child
    int res;
	printf("Child: (%s)\n", object_get_typename(obj));

	res = check_property_equals_test(obj, "tag");

    if (res < 0) {
	    object_child_foreach(obj, my_child_iterator, opaque);
    }

	return res;
}

void qmp_hello_from_tagged_mem(const char *tag, Error **errp)
{
    Object *root = object_get_objects_root();

	object_child_foreach(root, my_child_iterator, NULL);
}

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
