/*
 * QEMU CXL Host Setup
 *
 * Copyright (c) 2022 Huawei
 *
 * This work is licensed under the terms of the GNU GPL, version 2. See the
 * COPYING file in the top-level directory.
 */

#include "hw/cxl/cxl.h"
#include "hw/boards.h"

#ifndef CXL_HOST_H
#define CXL_HOST_H

// not sure but for now!
struct cxl_direct_pt_state {
    CXLType3Dev *ct3d;
    hwaddr decoder_base;
    hwaddr decoder_size;
    hwaddr dpa_base;
    unsigned int hdm_decoder_idx;
};

void cxl_machine_init(Object *obj, CXLState *state);
void cxl_fmws_link_targets(Error **errp);
void cxl_hook_up_pxb_registers(PCIBus *bus, CXLState *state, Error **errp);
hwaddr cxl_fmws_set_memmap(hwaddr base, hwaddr max_addr);
void cxl_fmws_update_mmio(void);
GSList *cxl_fmws_get_all_sorted(void);
// not sure
int cxl_fmws_direct_passthrough(Object *obj, void *opaque);

extern const MemoryRegionOps cfmws_ops;

#endif
