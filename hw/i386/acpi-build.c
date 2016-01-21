/* Support for generating ACPI tables and passing them to Guests
 *
 * Copyright (C) 2008-2010  Kevin O'Connor <kevin@koconnor.net>
 * Copyright (C) 2006 Fabrice Bellard
 * Copyright (C) 2013 Red Hat Inc
 *
 * Author: Michael S. Tsirkin <mst@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License along
 * with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "acpi-build.h"
#include <stddef.h>
#include <glib.h>
#include "qemu-common.h"
#include "qemu/bitmap.h"
#include "qemu/osdep.h"
#include "qemu/error-report.h"
#include "hw/pci/pci.h"
#include "qom/cpu.h"
#include "hw/i386/pc.h"
#include "target-i386/cpu.h"
#include "hw/timer/hpet.h"
#include "hw/acpi/acpi-defs.h"
#include "hw/acpi/acpi.h"
#include "hw/nvram/fw_cfg.h"
#include "hw/acpi/bios-linker-loader.h"
#include "hw/loader.h"
#include "hw/isa/isa.h"
#include "hw/acpi/memory_hotplug.h"
#include "hw/mem/nvdimm.h"
#include "sysemu/tpm.h"
#include "hw/acpi/tpm.h"
#include "sysemu/tpm_backend.h"
#include "hw/timer/mc146818rtc_regs.h"

/* Supported chipsets: */
#include "hw/acpi/piix4.h"
#include "hw/acpi/pcihp.h"
#include "hw/i386/ich9.h"
#include "hw/pci/pci_bus.h"
#include "hw/pci-host/q35.h"
#include "hw/i386/intel_iommu.h"
#include "hw/timer/hpet.h"

#include "hw/acpi/aml-build.h"

#include "qapi/qmp/qint.h"
#include "qom/qom-qobject.h"

/* These are used to size the ACPI tables for -M pc-i440fx-1.7 and
 * -M pc-i440fx-2.0.  Even if the actual amount of AML generated grows
 * a little bit, there should be plenty of free space since the DSDT
 * shrunk by ~1.5k between QEMU 2.0 and QEMU 2.1.
 */
#define ACPI_BUILD_LEGACY_CPU_AML_SIZE    97
#define ACPI_BUILD_ALIGN_SIZE             0x1000

#define ACPI_BUILD_TABLE_SIZE             0x20000

/* #define DEBUG_ACPI_BUILD */
#ifdef DEBUG_ACPI_BUILD
#define ACPI_BUILD_DPRINTF(fmt, ...)        \
    do {printf("ACPI_BUILD: " fmt, ## __VA_ARGS__); } while (0)
#else
#define ACPI_BUILD_DPRINTF(fmt, ...)
#endif

typedef struct AcpiCpuInfo {
    DECLARE_BITMAP(found_cpus, ACPI_CPU_HOTPLUG_ID_LIMIT);
} AcpiCpuInfo;

typedef struct AcpiMcfgInfo {
    uint64_t mcfg_base;
    uint32_t mcfg_size;
} AcpiMcfgInfo;

typedef struct AcpiPmInfo {
    bool s3_disabled;
    bool s4_disabled;
    bool pcihp_bridge_en;
    uint8_t s4_val;
    uint16_t sci_int;
    uint8_t acpi_enable_cmd;
    uint8_t acpi_disable_cmd;
    uint32_t gpe0_blk;
    uint32_t gpe0_blk_len;
    uint32_t io_base;
    uint16_t cpu_hp_io_base;
    uint16_t cpu_hp_io_len;
    uint16_t mem_hp_io_base;
    uint16_t mem_hp_io_len;
    uint16_t pcihp_io_base;
    uint16_t pcihp_io_len;
} AcpiPmInfo;

typedef struct AcpiMiscInfo {
    bool is_piix4;
    bool has_hpet;
    TPMVersion tpm_version;
    const unsigned char *dsdt_code;
    unsigned dsdt_size;
    uint16_t pvpanic_port;
    uint16_t applesmc_io_base;
} AcpiMiscInfo;

typedef struct AcpiBuildPciBusHotplugState {
    GArray *device_table;
    GArray *notify_table;
    struct AcpiBuildPciBusHotplugState *parent;
    bool pcihp_bridge_en;
} AcpiBuildPciBusHotplugState;

static
int acpi_add_cpu_info(Object *o, void *opaque)
{
    AcpiCpuInfo *cpu = opaque;
    uint64_t apic_id;

    if (object_dynamic_cast(o, TYPE_CPU)) {
        apic_id = object_property_get_int(o, "apic-id", NULL);
        assert(apic_id < ACPI_CPU_HOTPLUG_ID_LIMIT);

        set_bit(apic_id, cpu->found_cpus);
    }

    object_child_foreach(o, acpi_add_cpu_info, opaque);
    return 0;
}

static void acpi_get_cpu_info(AcpiCpuInfo *cpu)
{
    Object *root = object_get_root();

    memset(cpu->found_cpus, 0, sizeof cpu->found_cpus);
    object_child_foreach(root, acpi_add_cpu_info, cpu);
}

static void acpi_get_pm_info(AcpiPmInfo *pm)
{
    Object *piix = piix4_pm_find();
    Object *lpc = ich9_lpc_find();
    Object *obj = NULL;
    QObject *o;

    pm->cpu_hp_io_base = 0;
    pm->pcihp_io_base = 0;
    pm->pcihp_io_len = 0;
    if (piix) {
        obj = piix;
        pm->cpu_hp_io_base = PIIX4_CPU_HOTPLUG_IO_BASE;
        pm->pcihp_io_base =
            object_property_get_int(obj, ACPI_PCIHP_IO_BASE_PROP, NULL);
        pm->pcihp_io_len =
            object_property_get_int(obj, ACPI_PCIHP_IO_LEN_PROP, NULL);
    }
    if (lpc) {
        obj = lpc;
        pm->cpu_hp_io_base = ICH9_CPU_HOTPLUG_IO_BASE;
    }
    assert(obj);

    pm->cpu_hp_io_len = ACPI_GPE_PROC_LEN;
    pm->mem_hp_io_base = ACPI_MEMORY_HOTPLUG_BASE;
    pm->mem_hp_io_len = ACPI_MEMORY_HOTPLUG_IO_LEN;

    /* Fill in optional s3/s4 related properties */
    o = object_property_get_qobject(obj, ACPI_PM_PROP_S3_DISABLED, NULL);
    if (o) {
        pm->s3_disabled = qint_get_int(qobject_to_qint(o));
    } else {
        pm->s3_disabled = false;
    }
    qobject_decref(o);
    o = object_property_get_qobject(obj, ACPI_PM_PROP_S4_DISABLED, NULL);
    if (o) {
        pm->s4_disabled = qint_get_int(qobject_to_qint(o));
    } else {
        pm->s4_disabled = false;
    }
    qobject_decref(o);
    o = object_property_get_qobject(obj, ACPI_PM_PROP_S4_VAL, NULL);
    if (o) {
        pm->s4_val = qint_get_int(qobject_to_qint(o));
    } else {
        pm->s4_val = false;
    }
    qobject_decref(o);

    /* Fill in mandatory properties */
    pm->sci_int = object_property_get_int(obj, ACPI_PM_PROP_SCI_INT, NULL);

    pm->acpi_enable_cmd = object_property_get_int(obj,
                                                  ACPI_PM_PROP_ACPI_ENABLE_CMD,
                                                  NULL);
    pm->acpi_disable_cmd = object_property_get_int(obj,
                                                  ACPI_PM_PROP_ACPI_DISABLE_CMD,
                                                  NULL);
    pm->io_base = object_property_get_int(obj, ACPI_PM_PROP_PM_IO_BASE,
                                          NULL);
    pm->gpe0_blk = object_property_get_int(obj, ACPI_PM_PROP_GPE0_BLK,
                                           NULL);
    pm->gpe0_blk_len = object_property_get_int(obj, ACPI_PM_PROP_GPE0_BLK_LEN,
                                               NULL);
    pm->pcihp_bridge_en =
        object_property_get_bool(obj, "acpi-pci-hotplug-with-bridge-support",
                                 NULL);
}

static void acpi_get_misc_info(AcpiMiscInfo *info)
{
    Object *piix = piix4_pm_find();
    Object *lpc = ich9_lpc_find();
    assert(!!piix != !!lpc);

    if (piix) {
        info->is_piix4 = true;
    }
    if (lpc) {
        info->is_piix4 = false;
    }

    info->has_hpet = hpet_find();
    info->tpm_version = tpm_get_version();
    info->pvpanic_port = pvpanic_port();
    info->applesmc_io_base = applesmc_port();
}

/*
 * Because of the PXB hosts we cannot simply query TYPE_PCI_HOST_BRIDGE.
 * On i386 arch we only have two pci hosts, so we can look only for them.
 */
static Object *acpi_get_i386_pci_host(void)
{
    PCIHostState *host;

    host = OBJECT_CHECK(PCIHostState,
                        object_resolve_path("/machine/i440fx", NULL),
                        TYPE_PCI_HOST_BRIDGE);
    if (!host) {
        host = OBJECT_CHECK(PCIHostState,
                            object_resolve_path("/machine/q35", NULL),
                            TYPE_PCI_HOST_BRIDGE);
    }

    return OBJECT(host);
}

static void acpi_get_pci_info(PcPciInfo *info)
{
    Object *pci_host;


    pci_host = acpi_get_i386_pci_host();
    g_assert(pci_host);

    info->w32.begin = object_property_get_int(pci_host,
                                              PCI_HOST_PROP_PCI_HOLE_START,
                                              NULL);
    info->w32.end = object_property_get_int(pci_host,
                                            PCI_HOST_PROP_PCI_HOLE_END,
                                            NULL);
    info->w64.begin = object_property_get_int(pci_host,
                                              PCI_HOST_PROP_PCI_HOLE64_START,
                                              NULL);
    info->w64.end = object_property_get_int(pci_host,
                                            PCI_HOST_PROP_PCI_HOLE64_END,
                                            NULL);
}

#define ACPI_PORT_SMI_CMD           0x00b2 /* TODO: this is APM_CNT_IOPORT */

static void acpi_align_size(GArray *blob, unsigned align)
{
    /* Align size to multiple of given size. This reduces the chance
     * we need to change size in the future (breaking cross version migration).
     */
    g_array_set_size(blob, ROUND_UP(acpi_data_len(blob), align));
}

/* FACS */
static void
build_facs(GArray *table_data, GArray *linker, PcGuestInfo *guest_info)
{
    AcpiFacsDescriptorRev1 *facs = acpi_data_push(table_data, sizeof *facs);
    memcpy(&facs->signature, "FACS", 4);
    facs->length = cpu_to_le32(sizeof(*facs));
}

/* Load chipset information in FADT */
static void fadt_setup(AcpiFadtDescriptorRev1 *fadt, AcpiPmInfo *pm)
{
    fadt->model = 1;
    fadt->reserved1 = 0;
    fadt->sci_int = cpu_to_le16(pm->sci_int);
    fadt->smi_cmd = cpu_to_le32(ACPI_PORT_SMI_CMD);
    fadt->acpi_enable = pm->acpi_enable_cmd;
    fadt->acpi_disable = pm->acpi_disable_cmd;
    /* EVT, CNT, TMR offset matches hw/acpi/core.c */
    fadt->pm1a_evt_blk = cpu_to_le32(pm->io_base);
    fadt->pm1a_cnt_blk = cpu_to_le32(pm->io_base + 0x04);
    fadt->pm_tmr_blk = cpu_to_le32(pm->io_base + 0x08);
    fadt->gpe0_blk = cpu_to_le32(pm->gpe0_blk);
    /* EVT, CNT, TMR length matches hw/acpi/core.c */
    fadt->pm1_evt_len = 4;
    fadt->pm1_cnt_len = 2;
    fadt->pm_tmr_len = 4;
    fadt->gpe0_blk_len = pm->gpe0_blk_len;
    fadt->plvl2_lat = cpu_to_le16(0xfff); /* C2 state not supported */
    fadt->plvl3_lat = cpu_to_le16(0xfff); /* C3 state not supported */
    fadt->flags = cpu_to_le32((1 << ACPI_FADT_F_WBINVD) |
                              (1 << ACPI_FADT_F_PROC_C1) |
                              (1 << ACPI_FADT_F_SLP_BUTTON) |
                              (1 << ACPI_FADT_F_RTC_S4));
    fadt->flags |= cpu_to_le32(1 << ACPI_FADT_F_USE_PLATFORM_CLOCK);
    /* APIC destination mode ("Flat Logical") has an upper limit of 8 CPUs
     * For more than 8 CPUs, "Clustered Logical" mode has to be used
     */
    if (max_cpus > 8) {
        fadt->flags |= cpu_to_le32(1 << ACPI_FADT_F_FORCE_APIC_CLUSTER_MODEL);
    }
    fadt->century = RTC_CENTURY;
}


/* FADT */
static void
build_fadt(GArray *table_data, GArray *linker, AcpiPmInfo *pm,
           unsigned facs, unsigned dsdt)
{
    AcpiFadtDescriptorRev1 *fadt = acpi_data_push(table_data, sizeof(*fadt));

    fadt->firmware_ctrl = cpu_to_le32(facs);
    /* FACS address to be filled by Guest linker */
    bios_linker_loader_add_pointer(linker, ACPI_BUILD_TABLE_FILE,
                                   ACPI_BUILD_TABLE_FILE,
                                   table_data, &fadt->firmware_ctrl,
                                   sizeof fadt->firmware_ctrl);

    fadt->dsdt = cpu_to_le32(dsdt);
    /* DSDT address to be filled by Guest linker */
    bios_linker_loader_add_pointer(linker, ACPI_BUILD_TABLE_FILE,
                                   ACPI_BUILD_TABLE_FILE,
                                   table_data, &fadt->dsdt,
                                   sizeof fadt->dsdt);

    fadt_setup(fadt, pm);

    build_header(linker, table_data,
                 (void *)fadt, "FACP", sizeof(*fadt), 1, NULL);
}

static void
build_madt(GArray *table_data, GArray *linker, AcpiCpuInfo *cpu,
           PcGuestInfo *guest_info)
{
    int madt_start = table_data->len;

    AcpiMultipleApicTable *madt;
    AcpiMadtIoApic *io_apic;
    AcpiMadtIntsrcovr *intsrcovr;
    AcpiMadtLocalNmi *local_nmi;
    int i;

    madt = acpi_data_push(table_data, sizeof *madt);
    madt->local_apic_address = cpu_to_le32(APIC_DEFAULT_ADDRESS);
    madt->flags = cpu_to_le32(1);

    for (i = 0; i < guest_info->apic_id_limit; i++) {
        AcpiMadtProcessorApic *apic = acpi_data_push(table_data, sizeof *apic);
        apic->type = ACPI_APIC_PROCESSOR;
        apic->length = sizeof(*apic);
        apic->processor_id = i;
        apic->local_apic_id = i;
        if (test_bit(i, cpu->found_cpus)) {
            apic->flags = cpu_to_le32(1);
        } else {
            apic->flags = cpu_to_le32(0);
        }
    }
    io_apic = acpi_data_push(table_data, sizeof *io_apic);
    io_apic->type = ACPI_APIC_IO;
    io_apic->length = sizeof(*io_apic);
#define ACPI_BUILD_IOAPIC_ID 0x0
    io_apic->io_apic_id = ACPI_BUILD_IOAPIC_ID;
    io_apic->address = cpu_to_le32(IO_APIC_DEFAULT_ADDRESS);
    io_apic->interrupt = cpu_to_le32(0);

    if (guest_info->apic_xrupt_override) {
        intsrcovr = acpi_data_push(table_data, sizeof *intsrcovr);
        intsrcovr->type   = ACPI_APIC_XRUPT_OVERRIDE;
        intsrcovr->length = sizeof(*intsrcovr);
        intsrcovr->source = 0;
        intsrcovr->gsi    = cpu_to_le32(2);
        intsrcovr->flags  = cpu_to_le16(0); /* conforms to bus specifications */
    }
    for (i = 1; i < 16; i++) {
#define ACPI_BUILD_PCI_IRQS ((1<<5) | (1<<9) | (1<<10) | (1<<11))
        if (!(ACPI_BUILD_PCI_IRQS & (1 << i))) {
            /* No need for a INT source override structure. */
            continue;
        }
        intsrcovr = acpi_data_push(table_data, sizeof *intsrcovr);
        intsrcovr->type   = ACPI_APIC_XRUPT_OVERRIDE;
        intsrcovr->length = sizeof(*intsrcovr);
        intsrcovr->source = i;
        intsrcovr->gsi    = cpu_to_le32(i);
        intsrcovr->flags  = cpu_to_le16(0xd); /* active high, level triggered */
    }

    local_nmi = acpi_data_push(table_data, sizeof *local_nmi);
    local_nmi->type         = ACPI_APIC_LOCAL_NMI;
    local_nmi->length       = sizeof(*local_nmi);
    local_nmi->processor_id = 0xff; /* all processors */
    local_nmi->flags        = cpu_to_le16(0);
    local_nmi->lint         = 1; /* ACPI_LINT1 */

    build_header(linker, table_data,
                 (void *)(table_data->data + madt_start), "APIC",
                 table_data->len - madt_start, 1, NULL);
}

/* Assign BSEL property to all buses.  In the future, this can be changed
 * to only assign to buses that support hotplug.
 */
static void *acpi_set_bsel(PCIBus *bus, void *opaque)
{
    unsigned *bsel_alloc = opaque;
    unsigned *bus_bsel;

    if (qbus_is_hotpluggable(BUS(bus))) {
        bus_bsel = g_malloc(sizeof *bus_bsel);

        *bus_bsel = (*bsel_alloc)++;
        object_property_add_uint32_ptr(OBJECT(bus), ACPI_PCIHP_PROP_BSEL,
                                       bus_bsel, NULL);
    }

    return bsel_alloc;
}

static void acpi_set_pci_info(void)
{
    PCIBus *bus = find_i440fx(); /* TODO: Q35 support */
    unsigned bsel_alloc = 0;

    if (bus) {
        /* Scan all PCI buses. Set property to enable acpi based hotplug. */
        pci_for_each_bus_depth_first(bus, acpi_set_bsel, NULL, &bsel_alloc);
    }
}

static void build_append_pcihp_notify_entry(Aml *method, int slot)
{
    Aml *if_ctx;
    int32_t devfn = PCI_DEVFN(slot, 0);

    if_ctx = aml_if(aml_and(aml_arg(0), aml_int(0x1U << slot), NULL));
    aml_append(if_ctx, aml_notify(aml_name("S%.02X", devfn), aml_arg(1)));
    aml_append(method, if_ctx);
}

static void build_append_pci_bus_devices(Aml *parent_scope, PCIBus *bus,
                                         bool pcihp_bridge_en)
{
    Aml *dev, *notify_method, *method;
    QObject *bsel;
    PCIBus *sec;
    int i;

    bsel = object_property_get_qobject(OBJECT(bus), ACPI_PCIHP_PROP_BSEL, NULL);
    if (bsel) {
        int64_t bsel_val = qint_get_int(qobject_to_qint(bsel));

        aml_append(parent_scope, aml_name_decl("BSEL", aml_int(bsel_val)));
        notify_method = aml_method("DVNT", 2, AML_NOTSERIALIZED);
    }

    for (i = 0; i < ARRAY_SIZE(bus->devices); i += PCI_FUNC_MAX) {
        DeviceClass *dc;
        PCIDeviceClass *pc;
        PCIDevice *pdev = bus->devices[i];
        int slot = PCI_SLOT(i);
        bool hotplug_enabled_dev;
        bool bridge_in_acpi;

        if (!pdev) {
            if (bsel) { /* add hotplug slots for non present devices */
                dev = aml_device("S%.02X", PCI_DEVFN(slot, 0));
                aml_append(dev, aml_name_decl("_SUN", aml_int(slot)));
                aml_append(dev, aml_name_decl("_ADR", aml_int(slot << 16)));
                method = aml_method("_EJ0", 1, AML_NOTSERIALIZED);
                aml_append(method,
                    aml_call2("PCEJ", aml_name("BSEL"), aml_name("_SUN"))
                );
                aml_append(dev, method);
                aml_append(parent_scope, dev);

                build_append_pcihp_notify_entry(notify_method, slot);
            }
            continue;
        }

        pc = PCI_DEVICE_GET_CLASS(pdev);
        dc = DEVICE_GET_CLASS(pdev);

        /* When hotplug for bridges is enabled, bridges are
         * described in ACPI separately (see build_pci_bus_end).
         * In this case they aren't themselves hot-pluggable.
         * Hotplugged bridges *are* hot-pluggable.
         */
        bridge_in_acpi = pc->is_bridge && pcihp_bridge_en &&
            !DEVICE(pdev)->hotplugged;

        hotplug_enabled_dev = bsel && dc->hotpluggable && !bridge_in_acpi;

        if (pc->class_id == PCI_CLASS_BRIDGE_ISA) {
            continue;
        }

        /* start to compose PCI slot descriptor */
        dev = aml_device("S%.02X", PCI_DEVFN(slot, 0));
        aml_append(dev, aml_name_decl("_ADR", aml_int(slot << 16)));

        if (pc->class_id == PCI_CLASS_DISPLAY_VGA) {
            /* add VGA specific AML methods */
            int s3d;

            if (object_dynamic_cast(OBJECT(pdev), "qxl-vga")) {
                s3d = 3;
            } else {
                s3d = 0;
            }

            method = aml_method("_S1D", 0, AML_NOTSERIALIZED);
            aml_append(method, aml_return(aml_int(0)));
            aml_append(dev, method);

            method = aml_method("_S2D", 0, AML_NOTSERIALIZED);
            aml_append(method, aml_return(aml_int(0)));
            aml_append(dev, method);

            method = aml_method("_S3D", 0, AML_NOTSERIALIZED);
            aml_append(method, aml_return(aml_int(s3d)));
            aml_append(dev, method);
        } else if (hotplug_enabled_dev) {
            /* add _SUN/_EJ0 to make slot hotpluggable  */
            aml_append(dev, aml_name_decl("_SUN", aml_int(slot)));

            method = aml_method("_EJ0", 1, AML_NOTSERIALIZED);
            aml_append(method,
                aml_call2("PCEJ", aml_name("BSEL"), aml_name("_SUN"))
            );
            aml_append(dev, method);

            if (bsel) {
                build_append_pcihp_notify_entry(notify_method, slot);
            }
        } else if (bridge_in_acpi) {
            /*
             * device is coldplugged bridge,
             * add child device descriptions into its scope
             */
            PCIBus *sec_bus = pci_bridge_get_sec_bus(PCI_BRIDGE(pdev));

            build_append_pci_bus_devices(dev, sec_bus, pcihp_bridge_en);
        }
        /* slot descriptor has been composed, add it into parent context */
        aml_append(parent_scope, dev);
    }

    if (bsel) {
        aml_append(parent_scope, notify_method);
    }

    /* Append PCNT method to notify about events on local and child buses.
     * Add unconditionally for root since DSDT expects it.
     */
    method = aml_method("PCNT", 0, AML_NOTSERIALIZED);

    /* If bus supports hotplug select it and notify about local events */
    if (bsel) {
        int64_t bsel_val = qint_get_int(qobject_to_qint(bsel));
        aml_append(method, aml_store(aml_int(bsel_val), aml_name("BNUM")));
        aml_append(method,
            aml_call2("DVNT", aml_name("PCIU"), aml_int(1) /* Device Check */)
        );
        aml_append(method,
            aml_call2("DVNT", aml_name("PCID"), aml_int(3)/* Eject Request */)
        );
    }

    /* Notify about child bus events in any case */
    if (pcihp_bridge_en) {
        QLIST_FOREACH(sec, &bus->child, sibling) {
            int32_t devfn = sec->parent_dev->devfn;

            aml_append(method, aml_name("^S%.02X.PCNT", devfn));
        }
    }
    aml_append(parent_scope, method);
    qobject_decref(bsel);
}

/**
 * build_prt_entry:
 * @link_name: link name for PCI route entry
 *
 * build AML package containing a PCI route entry for @link_name
 */
static Aml *build_prt_entry(const char *link_name)
{
    Aml *a_zero = aml_int(0);
    Aml *pkg = aml_package(4);
    aml_append(pkg, a_zero);
    aml_append(pkg, a_zero);
    aml_append(pkg, aml_name("%s", link_name));
    aml_append(pkg, a_zero);
    return pkg;
}

/*
 * initialize_route - Initialize the interrupt routing rule
 * through a specific LINK:
 *  if (lnk_idx == idx)
 *      route using link 'link_name'
 */
static Aml *initialize_route(Aml *route, const char *link_name,
                             Aml *lnk_idx, int idx)
{
    Aml *if_ctx = aml_if(aml_equal(lnk_idx, aml_int(idx)));
    Aml *pkg = build_prt_entry(link_name);

    aml_append(if_ctx, aml_store(pkg, route));

    return if_ctx;
}

/*
 * build_prt - Define interrupt rounting rules
 *
 * Returns an array of 128 routes, one for each device,
 * based on device location.
 * The main goal is to equaly distribute the interrupts
 * over the 4 existing ACPI links (works only for i440fx).
 * The hash function is  (slot + pin) & 3 -> "LNK[D|A|B|C]".
 *
 */
static Aml *build_prt(bool is_pci0_prt)
{
    Aml *method, *while_ctx, *pin, *res;

    method = aml_method("_PRT", 0, AML_NOTSERIALIZED);
    res = aml_local(0);
    pin = aml_local(1);
    aml_append(method, aml_store(aml_package(128), res));
    aml_append(method, aml_store(aml_int(0), pin));

    /* while (pin < 128) */
    while_ctx = aml_while(aml_lless(pin, aml_int(128)));
    {
        Aml *slot = aml_local(2);
        Aml *lnk_idx = aml_local(3);
        Aml *route = aml_local(4);

        /* slot = pin >> 2 */
        aml_append(while_ctx,
                   aml_store(aml_shiftright(pin, aml_int(2), NULL), slot));
        /* lnk_idx = (slot + pin) & 3 */
        aml_append(while_ctx,
            aml_store(aml_and(aml_add(pin, slot, NULL), aml_int(3), NULL),
                      lnk_idx));

        /* route[2] = "LNK[D|A|B|C]", selection based on pin % 3  */
        aml_append(while_ctx, initialize_route(route, "LNKD", lnk_idx, 0));
        if (is_pci0_prt) {
            Aml *if_device_1, *if_pin_4, *else_pin_4;

            /* device 1 is the power-management device, needs SCI */
            if_device_1 = aml_if(aml_equal(lnk_idx, aml_int(1)));
            {
                if_pin_4 = aml_if(aml_equal(pin, aml_int(4)));
                {
                    aml_append(if_pin_4,
                        aml_store(build_prt_entry("LNKS"), route));
                }
                aml_append(if_device_1, if_pin_4);
                else_pin_4 = aml_else();
                {
                    aml_append(else_pin_4,
                        aml_store(build_prt_entry("LNKA"), route));
                }
                aml_append(if_device_1, else_pin_4);
            }
            aml_append(while_ctx, if_device_1);
        } else {
            aml_append(while_ctx, initialize_route(route, "LNKA", lnk_idx, 1));
        }
        aml_append(while_ctx, initialize_route(route, "LNKB", lnk_idx, 2));
        aml_append(while_ctx, initialize_route(route, "LNKC", lnk_idx, 3));

        /* route[0] = 0x[slot]FFFF */
        aml_append(while_ctx,
            aml_store(aml_or(aml_shiftleft(slot, aml_int(16)), aml_int(0xFFFF),
                             NULL),
                      aml_index(route, aml_int(0))));
        /* route[1] = pin & 3 */
        aml_append(while_ctx,
            aml_store(aml_and(pin, aml_int(3), NULL),
                      aml_index(route, aml_int(1))));
        /* res[pin] = route */
        aml_append(while_ctx, aml_store(route, aml_index(res, pin)));
        /* pin++ */
        aml_append(while_ctx, aml_increment(pin));
    }
    aml_append(method, while_ctx);
    /* return res*/
    aml_append(method, aml_return(res));

    return method;
}

typedef struct CrsRangeEntry {
    uint64_t base;
    uint64_t limit;
} CrsRangeEntry;

static void crs_range_insert(GPtrArray *ranges, uint64_t base, uint64_t limit)
{
    CrsRangeEntry *entry;

    entry = g_malloc(sizeof(*entry));
    entry->base = base;
    entry->limit = limit;

    g_ptr_array_add(ranges, entry);
}

static void crs_range_free(gpointer data)
{
    CrsRangeEntry *entry = (CrsRangeEntry *)data;
    g_free(entry);
}

static gint crs_range_compare(gconstpointer a, gconstpointer b)
{
     CrsRangeEntry *entry_a = *(CrsRangeEntry **)a;
     CrsRangeEntry *entry_b = *(CrsRangeEntry **)b;

     return (int64_t)entry_a->base - (int64_t)entry_b->base;
}

/*
 * crs_replace_with_free_ranges - given the 'used' ranges within [start - end]
 * interval, computes the 'free' ranges from the same interval.
 * Example: If the input array is { [a1 - a2],[b1 - b2] }, the function
 * will return { [base - a1], [a2 - b1], [b2 - limit] }.
 */
static void crs_replace_with_free_ranges(GPtrArray *ranges,
                                         uint64_t start, uint64_t end)
{
    GPtrArray *free_ranges = g_ptr_array_new_with_free_func(crs_range_free);
    uint64_t free_base = start;
    int i;

    g_ptr_array_sort(ranges, crs_range_compare);
    for (i = 0; i < ranges->len; i++) {
        CrsRangeEntry *used = g_ptr_array_index(ranges, i);

        if (free_base < used->base) {
            crs_range_insert(free_ranges, free_base, used->base - 1);
        }

        free_base = used->limit + 1;
    }

    if (free_base < end) {
        crs_range_insert(free_ranges, free_base, end);
    }

    g_ptr_array_set_size(ranges, 0);
    for (i = 0; i < free_ranges->len; i++) {
        g_ptr_array_add(ranges, g_ptr_array_index(free_ranges, i));
    }

    g_ptr_array_free(free_ranges, false);
}

/*
 * crs_range_merge - merges adjacent ranges in the given array.
 * Array elements are deleted and replaced with the merged ranges.
 */
static void crs_range_merge(GPtrArray *range)
{
    GPtrArray *tmp =  g_ptr_array_new_with_free_func(crs_range_free);
    CrsRangeEntry *entry;
    uint64_t range_base, range_limit;
    int i;

    if (!range->len) {
        return;
    }

    g_ptr_array_sort(range, crs_range_compare);

    entry = g_ptr_array_index(range, 0);
    range_base = entry->base;
    range_limit = entry->limit;
    for (i = 1; i < range->len; i++) {
        entry = g_ptr_array_index(range, i);
        if (entry->base - 1 == range_limit) {
            range_limit = entry->limit;
        } else {
            crs_range_insert(tmp, range_base, range_limit);
            range_base = entry->base;
            range_limit = entry->limit;
        }
    }
    crs_range_insert(tmp, range_base, range_limit);

    g_ptr_array_set_size(range, 0);
    for (i = 0; i < tmp->len; i++) {
        entry = g_ptr_array_index(tmp, i);
        crs_range_insert(range, entry->base, entry->limit);
    }
    g_ptr_array_free(tmp, true);
}

static Aml *build_crs(PCIHostState *host,
                      GPtrArray *io_ranges, GPtrArray *mem_ranges)
{
    Aml *crs = aml_resource_template();
    GPtrArray *host_io_ranges = g_ptr_array_new_with_free_func(crs_range_free);
    GPtrArray *host_mem_ranges = g_ptr_array_new_with_free_func(crs_range_free);
    CrsRangeEntry *entry;
    uint8_t max_bus = pci_bus_num(host->bus);
    uint8_t type;
    int devfn;
    int i;

    for (devfn = 0; devfn < ARRAY_SIZE(host->bus->devices); devfn++) {
        uint64_t range_base, range_limit;
        PCIDevice *dev = host->bus->devices[devfn];

        if (!dev) {
            continue;
        }

        for (i = 0; i < PCI_NUM_REGIONS; i++) {
            PCIIORegion *r = &dev->io_regions[i];

            range_base = r->addr;
            range_limit = r->addr + r->size - 1;

            /*
             * Work-around for old bioses
             * that do not support multiple root buses
             */
            if (!range_base || range_base > range_limit) {
                continue;
            }

            if (r->type & PCI_BASE_ADDRESS_SPACE_IO) {
                crs_range_insert(host_io_ranges, range_base, range_limit);
            } else { /* "memory" */
                crs_range_insert(host_mem_ranges, range_base, range_limit);
            }
        }

        type = dev->config[PCI_HEADER_TYPE] & ~PCI_HEADER_TYPE_MULTI_FUNCTION;
        if (type == PCI_HEADER_TYPE_BRIDGE) {
            uint8_t subordinate = dev->config[PCI_SUBORDINATE_BUS];
            if (subordinate > max_bus) {
                max_bus = subordinate;
            }

            range_base = pci_bridge_get_base(dev, PCI_BASE_ADDRESS_SPACE_IO);
            range_limit = pci_bridge_get_limit(dev, PCI_BASE_ADDRESS_SPACE_IO);

            /*
             * Work-around for old bioses
             * that do not support multiple root buses
             */
            if (range_base && range_base <= range_limit) {
                crs_range_insert(host_io_ranges, range_base, range_limit);
            }

            range_base =
                pci_bridge_get_base(dev, PCI_BASE_ADDRESS_SPACE_MEMORY);
            range_limit =
                pci_bridge_get_limit(dev, PCI_BASE_ADDRESS_SPACE_MEMORY);

            /*
             * Work-around for old bioses
             * that do not support multiple root buses
             */
            if (range_base && range_base <= range_limit) {
                crs_range_insert(host_mem_ranges, range_base, range_limit);
            }

            range_base =
                pci_bridge_get_base(dev, PCI_BASE_ADDRESS_MEM_PREFETCH);
            range_limit =
                pci_bridge_get_limit(dev, PCI_BASE_ADDRESS_MEM_PREFETCH);

            /*
             * Work-around for old bioses
             * that do not support multiple root buses
             */
            if (range_base && range_base <= range_limit) {
                crs_range_insert(host_mem_ranges, range_base, range_limit);
            }
        }
    }

    crs_range_merge(host_io_ranges);
    for (i = 0; i < host_io_ranges->len; i++) {
        entry = g_ptr_array_index(host_io_ranges, i);
        aml_append(crs,
                   aml_word_io(AML_MIN_FIXED, AML_MAX_FIXED,
                               AML_POS_DECODE, AML_ENTIRE_RANGE,
                               0, entry->base, entry->limit, 0,
                               entry->limit - entry->base + 1));
        crs_range_insert(io_ranges, entry->base, entry->limit);
    }
    g_ptr_array_free(host_io_ranges, true);

    crs_range_merge(host_mem_ranges);
    for (i = 0; i < host_mem_ranges->len; i++) {
        entry = g_ptr_array_index(host_mem_ranges, i);
        aml_append(crs,
                   aml_dword_memory(AML_POS_DECODE, AML_MIN_FIXED,
                                    AML_MAX_FIXED, AML_NON_CACHEABLE,
                                    AML_READ_WRITE,
                                    0, entry->base, entry->limit, 0,
                                    entry->limit - entry->base + 1));
        crs_range_insert(mem_ranges, entry->base, entry->limit);
    }
    g_ptr_array_free(host_mem_ranges, true);

    aml_append(crs,
        aml_word_bus_number(AML_MIN_FIXED, AML_MAX_FIXED, AML_POS_DECODE,
                            0,
                            pci_bus_num(host->bus),
                            max_bus,
                            0,
                            max_bus - pci_bus_num(host->bus) + 1));

    return crs;
}

static void build_processor_devices(Aml *sb_scope, unsigned acpi_cpus,
                                    AcpiCpuInfo *cpu, AcpiPmInfo *pm)
{
    int i;
    Aml *dev;
    Aml *crs;
    Aml *pkg;
    Aml *field;
    Aml *ifctx;
    Aml *method;

    /* The current AML generator can cover the APIC ID range [0..255],
     * inclusive, for VCPU hotplug. */
    QEMU_BUILD_BUG_ON(ACPI_CPU_HOTPLUG_ID_LIMIT > 256);
    g_assert(acpi_cpus <= ACPI_CPU_HOTPLUG_ID_LIMIT);

    /* create PCI0.PRES device and its _CRS to reserve CPU hotplug MMIO */
    dev = aml_device("PCI0." stringify(CPU_HOTPLUG_RESOURCE_DEVICE));
    aml_append(dev, aml_name_decl("_HID", aml_eisaid("PNP0A06")));
    aml_append(dev,
        aml_name_decl("_UID", aml_string("CPU Hotplug resources"))
    );
    /* device present, functioning, decoding, not shown in UI */
    aml_append(dev, aml_name_decl("_STA", aml_int(0xB)));
    crs = aml_resource_template();
    aml_append(crs,
        aml_io(AML_DECODE16, pm->cpu_hp_io_base, pm->cpu_hp_io_base, 1,
               pm->cpu_hp_io_len)
    );
    aml_append(dev, aml_name_decl("_CRS", crs));
    aml_append(sb_scope, dev);
    /* declare CPU hotplug MMIO region and PRS field to access it */
    aml_append(sb_scope, aml_operation_region(
        "PRST", AML_SYSTEM_IO, pm->cpu_hp_io_base, pm->cpu_hp_io_len));
    field = aml_field("PRST", AML_BYTE_ACC, AML_NOLOCK, AML_PRESERVE);
    aml_append(field, aml_named_field("PRS", 256));
    aml_append(sb_scope, field);

    /* build Processor object for each processor */
    for (i = 0; i < acpi_cpus; i++) {
        dev = aml_processor(i, 0, 0, "CP%.02X", i);

        method = aml_method("_MAT", 0, AML_NOTSERIALIZED);
        aml_append(method,
            aml_return(aml_call1(CPU_MAT_METHOD, aml_int(i))));
        aml_append(dev, method);

        method = aml_method("_STA", 0, AML_NOTSERIALIZED);
        aml_append(method,
            aml_return(aml_call1(CPU_STATUS_METHOD, aml_int(i))));
        aml_append(dev, method);

        method = aml_method("_EJ0", 1, AML_NOTSERIALIZED);
        aml_append(method,
            aml_return(aml_call2(CPU_EJECT_METHOD, aml_int(i), aml_arg(0)))
        );
        aml_append(dev, method);

        aml_append(sb_scope, dev);
    }

    /* build this code:
     *   Method(NTFY, 2) {If (LEqual(Arg0, 0x00)) {Notify(CP00, Arg1)} ...}
     */
    /* Arg0 = Processor ID = APIC ID */
    method = aml_method(AML_NOTIFY_METHOD, 2, AML_NOTSERIALIZED);
    for (i = 0; i < acpi_cpus; i++) {
        ifctx = aml_if(aml_equal(aml_arg(0), aml_int(i)));
        aml_append(ifctx,
            aml_notify(aml_name("CP%.02X", i), aml_arg(1))
        );
        aml_append(method, ifctx);
    }
    aml_append(sb_scope, method);

    /* build "Name(CPON, Package() { One, One, ..., Zero, Zero, ... })"
     *
     * Note: The ability to create variable-sized packages was first
     * introduced in ACPI 2.0. ACPI 1.0 only allowed fixed-size packages
     * ith up to 255 elements. Windows guests up to win2k8 fail when
     * VarPackageOp is used.
     */
    pkg = acpi_cpus <= 255 ? aml_package(acpi_cpus) :
                             aml_varpackage(acpi_cpus);

    for (i = 0; i < acpi_cpus; i++) {
        uint8_t b = test_bit(i, cpu->found_cpus) ? 0x01 : 0x00;
        aml_append(pkg, aml_int(b));
    }
    aml_append(sb_scope, aml_name_decl(CPU_ON_BITMAP, pkg));
}

static void build_memory_devices(Aml *sb_scope, int nr_mem,
                                 uint16_t io_base, uint16_t io_len)
{
    int i;
    Aml *scope;
    Aml *crs;
    Aml *field;
    Aml *dev;
    Aml *method;
    Aml *ifctx;

    /* build memory devices */
    assert(nr_mem <= ACPI_MAX_RAM_SLOTS);
    scope = aml_scope("\\_SB.PCI0." MEMORY_HOTPLUG_DEVICE);
    aml_append(scope,
        aml_name_decl(MEMORY_SLOTS_NUMBER, aml_int(nr_mem))
    );

    crs = aml_resource_template();
    aml_append(crs,
        aml_io(AML_DECODE16, io_base, io_base, 0, io_len)
    );
    aml_append(scope, aml_name_decl("_CRS", crs));

    aml_append(scope, aml_operation_region(
        MEMORY_HOTPLUG_IO_REGION, AML_SYSTEM_IO,
        io_base, io_len)
    );

    field = aml_field(MEMORY_HOTPLUG_IO_REGION, AML_DWORD_ACC,
                      AML_NOLOCK, AML_PRESERVE);
    aml_append(field, /* read only */
        aml_named_field(MEMORY_SLOT_ADDR_LOW, 32));
    aml_append(field, /* read only */
        aml_named_field(MEMORY_SLOT_ADDR_HIGH, 32));
    aml_append(field, /* read only */
        aml_named_field(MEMORY_SLOT_SIZE_LOW, 32));
    aml_append(field, /* read only */
        aml_named_field(MEMORY_SLOT_SIZE_HIGH, 32));
    aml_append(field, /* read only */
        aml_named_field(MEMORY_SLOT_PROXIMITY, 32));
    aml_append(scope, field);

    field = aml_field(MEMORY_HOTPLUG_IO_REGION, AML_BYTE_ACC,
                      AML_NOLOCK, AML_WRITE_AS_ZEROS);
    aml_append(field, aml_reserved_field(160 /* bits, Offset(20) */));
    aml_append(field, /* 1 if enabled, read only */
        aml_named_field(MEMORY_SLOT_ENABLED, 1));
    aml_append(field,
        /*(read) 1 if has a insert event. (write) 1 to clear event */
        aml_named_field(MEMORY_SLOT_INSERT_EVENT, 1));
    aml_append(field,
        /* (read) 1 if has a remove event. (write) 1 to clear event */
        aml_named_field(MEMORY_SLOT_REMOVE_EVENT, 1));
    aml_append(field,
        /* initiates device eject, write only */
        aml_named_field(MEMORY_SLOT_EJECT, 1));
    aml_append(scope, field);

    field = aml_field(MEMORY_HOTPLUG_IO_REGION, AML_DWORD_ACC,
                      AML_NOLOCK, AML_PRESERVE);
    aml_append(field, /* DIMM selector, write only */
        aml_named_field(MEMORY_SLOT_SLECTOR, 32));
    aml_append(field, /* _OST event code, write only */
        aml_named_field(MEMORY_SLOT_OST_EVENT, 32));
    aml_append(field, /* _OST status code, write only */
        aml_named_field(MEMORY_SLOT_OST_STATUS, 32));
    aml_append(scope, field);
    aml_append(sb_scope, scope);

    for (i = 0; i < nr_mem; i++) {
        #define BASEPATH "\\_SB.PCI0." MEMORY_HOTPLUG_DEVICE "."
        const char *s;

        dev = aml_device("MP%02X", i);
        aml_append(dev, aml_name_decl("_UID", aml_string("0x%02X", i)));
        aml_append(dev, aml_name_decl("_HID", aml_eisaid("PNP0C80")));

        method = aml_method("_CRS", 0, AML_NOTSERIALIZED);
        s = BASEPATH MEMORY_SLOT_CRS_METHOD;
        aml_append(method, aml_return(aml_call1(s, aml_name("_UID"))));
        aml_append(dev, method);

        method = aml_method("_STA", 0, AML_NOTSERIALIZED);
        s = BASEPATH MEMORY_SLOT_STATUS_METHOD;
        aml_append(method, aml_return(aml_call1(s, aml_name("_UID"))));
        aml_append(dev, method);

        method = aml_method("_PXM", 0, AML_NOTSERIALIZED);
        s = BASEPATH MEMORY_SLOT_PROXIMITY_METHOD;
        aml_append(method, aml_return(aml_call1(s, aml_name("_UID"))));
        aml_append(dev, method);

        method = aml_method("_OST", 3, AML_NOTSERIALIZED);
        s = BASEPATH MEMORY_SLOT_OST_METHOD;

        aml_append(method, aml_return(aml_call4(
            s, aml_name("_UID"), aml_arg(0), aml_arg(1), aml_arg(2)
        )));
        aml_append(dev, method);

        method = aml_method("_EJ0", 1, AML_NOTSERIALIZED);
        s = BASEPATH MEMORY_SLOT_EJECT_METHOD;
        aml_append(method, aml_return(aml_call2(
                   s, aml_name("_UID"), aml_arg(0))));
        aml_append(dev, method);

        aml_append(sb_scope, dev);
    }

    /* build Method(MEMORY_SLOT_NOTIFY_METHOD, 2) {
     *     If (LEqual(Arg0, 0x00)) {Notify(MP00, Arg1)} ... }
     */
    method = aml_method(MEMORY_SLOT_NOTIFY_METHOD, 2, AML_NOTSERIALIZED);
    for (i = 0; i < nr_mem; i++) {
        ifctx = aml_if(aml_equal(aml_arg(0), aml_int(i)));
        aml_append(ifctx,
            aml_notify(aml_name("MP%.02X", i), aml_arg(1))
        );
        aml_append(method, ifctx);
    }
    aml_append(sb_scope, method);
}

static void build_hpet_aml(Aml *table)
{
    Aml *crs;
    Aml *field;
    Aml *method;
    Aml *if_ctx;
    Aml *scope = aml_scope("_SB");
    Aml *dev = aml_device("HPET");
    Aml *zero = aml_int(0);
    Aml *id = aml_local(0);
    Aml *period = aml_local(1);

    aml_append(dev, aml_name_decl("_HID", aml_eisaid("PNP0103")));
    aml_append(dev, aml_name_decl("_UID", zero));

    aml_append(dev,
        aml_operation_region("HPTM", AML_SYSTEM_MEMORY, HPET_BASE, HPET_LEN));
    field = aml_field("HPTM", AML_DWORD_ACC, AML_LOCK, AML_PRESERVE);
    aml_append(field, aml_named_field("VEND", 32));
    aml_append(field, aml_named_field("PRD", 32));
    aml_append(dev, field);

    method = aml_method("_STA", 0, AML_NOTSERIALIZED);
    aml_append(method, aml_store(aml_name("VEND"), id));
    aml_append(method, aml_store(aml_name("PRD"), period));
    aml_append(method, aml_shiftright(id, aml_int(16), id));
    if_ctx = aml_if(aml_lor(aml_equal(id, zero),
                            aml_equal(id, aml_int(0xffff))));
    {
        aml_append(if_ctx, aml_return(zero));
    }
    aml_append(method, if_ctx);

    if_ctx = aml_if(aml_lor(aml_equal(period, zero),
                            aml_lgreater(period, aml_int(100000000))));
    {
        aml_append(if_ctx, aml_return(zero));
    }
    aml_append(method, if_ctx);

    aml_append(method, aml_return(aml_int(0x0F)));
    aml_append(dev, method);

    crs = aml_resource_template();
    aml_append(crs, aml_memory32_fixed(HPET_BASE, HPET_LEN, AML_READ_ONLY));
    aml_append(dev, aml_name_decl("_CRS", crs));

    aml_append(scope, dev);
    aml_append(table, scope);
}

static Aml *build_fdc_device_aml(void)
{
    Aml *dev;
    Aml *crs;
    Aml *method;
    Aml *if_ctx;
    Aml *else_ctx;
    Aml *zero = aml_int(0);
    Aml *is_present = aml_local(0);

    dev = aml_device("FDC0");
    aml_append(dev, aml_name_decl("_HID", aml_eisaid("PNP0700")));

    method = aml_method("_STA", 0, AML_NOTSERIALIZED);
    aml_append(method, aml_store(aml_name("FDEN"), is_present));
    if_ctx = aml_if(aml_equal(is_present, zero));
    {
        aml_append(if_ctx, aml_return(aml_int(0x00)));
    }
    aml_append(method, if_ctx);
    else_ctx = aml_else();
    {
        aml_append(else_ctx, aml_return(aml_int(0x0f)));
    }
    aml_append(method, else_ctx);
    aml_append(dev, method);

    crs = aml_resource_template();
    aml_append(crs, aml_io(AML_DECODE16, 0x03F2, 0x03F2, 0x00, 0x04));
    aml_append(crs, aml_io(AML_DECODE16, 0x03F7, 0x03F7, 0x00, 0x01));
    aml_append(crs, aml_irq_no_flags(6));
    aml_append(crs,
        aml_dma(AML_COMPATIBILITY, AML_NOTBUSMASTER, AML_TRANSFER8, 2));
    aml_append(dev, aml_name_decl("_CRS", crs));

    return dev;
}

static Aml *build_rtc_device_aml(void)
{
    Aml *dev;
    Aml *crs;

    dev = aml_device("RTC");
    aml_append(dev, aml_name_decl("_HID", aml_eisaid("PNP0B00")));
    crs = aml_resource_template();
    aml_append(crs, aml_io(AML_DECODE16, 0x0070, 0x0070, 0x10, 0x02));
    aml_append(crs, aml_irq_no_flags(8));
    aml_append(crs, aml_io(AML_DECODE16, 0x0072, 0x0072, 0x02, 0x06));
    aml_append(dev, aml_name_decl("_CRS", crs));

    return dev;
}

static Aml *build_kbd_device_aml(void)
{
    Aml *dev;
    Aml *crs;
    Aml *method;

    dev = aml_device("KBD");
    aml_append(dev, aml_name_decl("_HID", aml_eisaid("PNP0303")));

    method = aml_method("_STA", 0, AML_NOTSERIALIZED);
    aml_append(method, aml_return(aml_int(0x0f)));
    aml_append(dev, method);

    crs = aml_resource_template();
    aml_append(crs, aml_io(AML_DECODE16, 0x0060, 0x0060, 0x01, 0x01));
    aml_append(crs, aml_io(AML_DECODE16, 0x0064, 0x0064, 0x01, 0x01));
    aml_append(crs, aml_irq_no_flags(1));
    aml_append(dev, aml_name_decl("_CRS", crs));

    return dev;
}

static Aml *build_mouse_device_aml(void)
{
    Aml *dev;
    Aml *crs;
    Aml *method;

    dev = aml_device("MOU");
    aml_append(dev, aml_name_decl("_HID", aml_eisaid("PNP0F13")));

    method = aml_method("_STA", 0, AML_NOTSERIALIZED);
    aml_append(method, aml_return(aml_int(0x0f)));
    aml_append(dev, method);

    crs = aml_resource_template();
    aml_append(crs, aml_irq_no_flags(12));
    aml_append(dev, aml_name_decl("_CRS", crs));

    return dev;
}

static Aml *build_lpt_device_aml(void)
{
    Aml *dev;
    Aml *crs;
    Aml *method;
    Aml *if_ctx;
    Aml *else_ctx;
    Aml *zero = aml_int(0);
    Aml *is_present = aml_local(0);

    dev = aml_device("LPT");
    aml_append(dev, aml_name_decl("_HID", aml_eisaid("PNP0400")));

    method = aml_method("_STA", 0, AML_NOTSERIALIZED);
    aml_append(method, aml_store(aml_name("LPEN"), is_present));
    if_ctx = aml_if(aml_equal(is_present, zero));
    {
        aml_append(if_ctx, aml_return(aml_int(0x00)));
    }
    aml_append(method, if_ctx);
    else_ctx = aml_else();
    {
        aml_append(else_ctx, aml_return(aml_int(0x0f)));
    }
    aml_append(method, else_ctx);
    aml_append(dev, method);

    crs = aml_resource_template();
    aml_append(crs, aml_io(AML_DECODE16, 0x0378, 0x0378, 0x08, 0x08));
    aml_append(crs, aml_irq_no_flags(7));
    aml_append(dev, aml_name_decl("_CRS", crs));

    return dev;
}

static Aml *build_com_device_aml(uint8_t uid)
{
    Aml *dev;
    Aml *crs;
    Aml *method;
    Aml *if_ctx;
    Aml *else_ctx;
    Aml *zero = aml_int(0);
    Aml *is_present = aml_local(0);
    const char *enabled_field = "CAEN";
    uint8_t irq = 4;
    uint16_t io_port = 0x03F8;

    assert(uid == 1 || uid == 2);
    if (uid == 2) {
        enabled_field = "CBEN";
        irq = 3;
        io_port = 0x02F8;
    }

    dev = aml_device("COM%d", uid);
    aml_append(dev, aml_name_decl("_HID", aml_eisaid("PNP0501")));
    aml_append(dev, aml_name_decl("_UID", aml_int(uid)));

    method = aml_method("_STA", 0, AML_NOTSERIALIZED);
    aml_append(method, aml_store(aml_name("%s", enabled_field), is_present));
    if_ctx = aml_if(aml_equal(is_present, zero));
    {
        aml_append(if_ctx, aml_return(aml_int(0x00)));
    }
    aml_append(method, if_ctx);
    else_ctx = aml_else();
    {
        aml_append(else_ctx, aml_return(aml_int(0x0f)));
    }
    aml_append(method, else_ctx);
    aml_append(dev, method);

    crs = aml_resource_template();
    aml_append(crs, aml_io(AML_DECODE16, io_port, io_port, 0x00, 0x08));
    aml_append(crs, aml_irq_no_flags(irq));
    aml_append(dev, aml_name_decl("_CRS", crs));

    return dev;
}

static void build_isa_devices_aml(Aml *table)
{
    Aml *scope = aml_scope("_SB.PCI0.ISA");

    aml_append(scope, build_rtc_device_aml());
    aml_append(scope, build_kbd_device_aml());
    aml_append(scope, build_mouse_device_aml());
    aml_append(scope, build_fdc_device_aml());
    aml_append(scope, build_lpt_device_aml());
    aml_append(scope, build_com_device_aml(1));
    aml_append(scope, build_com_device_aml(2));

    aml_append(table, scope);
}

static void build_dbg_aml(Aml *table)
{
    Aml *field;
    Aml *method;
    Aml *while_ctx;
    Aml *scope = aml_scope("\\");
    Aml *buf = aml_local(0);
    Aml *len = aml_local(1);
    Aml *idx = aml_local(2);

    aml_append(scope,
       aml_operation_region("DBG", AML_SYSTEM_IO, 0x0402, 0x01));
    field = aml_field("DBG", AML_BYTE_ACC, AML_NOLOCK, AML_PRESERVE);
    aml_append(field, aml_named_field("DBGB", 8));
    aml_append(scope, field);

    method = aml_method("DBUG", 1, AML_NOTSERIALIZED);

    aml_append(method, aml_to_hexstring(aml_arg(0), buf));
    aml_append(method, aml_to_buffer(buf, buf));
    aml_append(method, aml_subtract(aml_sizeof(buf), aml_int(1), len));
    aml_append(method, aml_store(aml_int(0), idx));

    while_ctx = aml_while(aml_lless(idx, len));
    aml_append(while_ctx,
        aml_store(aml_derefof(aml_index(buf, idx)), aml_name("DBGB")));
    aml_append(while_ctx, aml_increment(idx));
    aml_append(method, while_ctx);

    aml_append(method, aml_store(aml_int(0x0A), aml_name("DBGB")));
    aml_append(scope, method);

    aml_append(table, scope);
}

static Aml *build_link_dev(const char *name, uint8_t uid, Aml *reg)
{
    Aml *dev;
    Aml *crs;
    Aml *method;
    uint32_t irqs[] = {5, 10, 11};

    dev = aml_device("%s", name);
    aml_append(dev, aml_name_decl("_HID", aml_eisaid("PNP0C0F")));
    aml_append(dev, aml_name_decl("_UID", aml_int(uid)));

    crs = aml_resource_template();
    aml_append(crs, aml_interrupt(AML_CONSUMER, AML_LEVEL, AML_ACTIVE_HIGH,
                                  AML_SHARED, irqs, ARRAY_SIZE(irqs)));
    aml_append(dev, aml_name_decl("_PRS", crs));

    method = aml_method("_STA", 0, AML_NOTSERIALIZED);
    aml_append(method, aml_return(aml_call1("IQST", reg)));
    aml_append(dev, method);

    method = aml_method("_DIS", 0, AML_NOTSERIALIZED);
    aml_append(method, aml_or(reg, aml_int(0x80), reg));
    aml_append(dev, method);

    method = aml_method("_CRS", 0, AML_NOTSERIALIZED);
    aml_append(method, aml_return(aml_call1("IQCR", reg)));
    aml_append(dev, method);

    method = aml_method("_SRS", 1, AML_NOTSERIALIZED);
    aml_append(method, aml_create_dword_field(aml_arg(0), aml_int(5), "PRRI"));
    aml_append(method, aml_store(aml_name("PRRI"), reg));
    aml_append(dev, method);

    return dev;
 }

static Aml *build_gsi_link_dev(const char *name, uint8_t uid, uint8_t gsi)
{
    Aml *dev;
    Aml *crs;
    Aml *method;
    uint32_t irqs;

    dev = aml_device("%s", name);
    aml_append(dev, aml_name_decl("_HID", aml_eisaid("PNP0C0F")));
    aml_append(dev, aml_name_decl("_UID", aml_int(uid)));

    crs = aml_resource_template();
    irqs = gsi;
    aml_append(crs, aml_interrupt(AML_CONSUMER, AML_LEVEL, AML_ACTIVE_HIGH,
                                  AML_SHARED, &irqs, 1));
    aml_append(dev, aml_name_decl("_PRS", crs));

    aml_append(dev, aml_name_decl("_CRS", crs));

    method = aml_method("_SRS", 1, AML_NOTSERIALIZED);
    aml_append(dev, method);

    return dev;
}

/* _CRS method - get current settings */
static Aml *build_iqcr_method(bool is_piix4)
{
    Aml *if_ctx;
    uint32_t irqs;
    Aml *method = aml_method("IQCR", 1, AML_SERIALIZED);
    Aml *crs = aml_resource_template();

    irqs = 0;
    aml_append(crs, aml_interrupt(AML_CONSUMER, AML_LEVEL,
                                  AML_ACTIVE_HIGH, AML_SHARED, &irqs, 1));
    aml_append(method, aml_name_decl("PRR0", crs));

    aml_append(method,
        aml_create_dword_field(aml_name("PRR0"), aml_int(5), "PRRI"));

    if (is_piix4) {
        if_ctx = aml_if(aml_lless(aml_arg(0), aml_int(0x80)));
        aml_append(if_ctx, aml_store(aml_arg(0), aml_name("PRRI")));
        aml_append(method, if_ctx);
    } else {
        aml_append(method,
            aml_store(aml_and(aml_arg(0), aml_int(0xF), NULL),
                      aml_name("PRRI")));
    }

    aml_append(method, aml_return(aml_name("PRR0")));
    return method;
}

/* _STA method - get status */
static Aml *build_irq_status_method(void)
{
    Aml *if_ctx;
    Aml *method = aml_method("IQST", 1, AML_NOTSERIALIZED);

    if_ctx = aml_if(aml_and(aml_int(0x80), aml_arg(0), NULL));
    aml_append(if_ctx, aml_return(aml_int(0x09)));
    aml_append(method, if_ctx);
    aml_append(method, aml_return(aml_int(0x0B)));
    return method;
}

static void build_piix4_pci0_int(Aml *table)
{
    Aml *dev;
    Aml *crs;
    Aml *field;
    Aml *method;
    uint32_t irqs;
    Aml *sb_scope = aml_scope("_SB");
    Aml *pci0_scope = aml_scope("PCI0");

    aml_append(pci0_scope, build_prt(true));
    aml_append(sb_scope, pci0_scope);

    field = aml_field("PCI0.ISA.P40C", AML_BYTE_ACC, AML_NOLOCK, AML_PRESERVE);
    aml_append(field, aml_named_field("PRQ0", 8));
    aml_append(field, aml_named_field("PRQ1", 8));
    aml_append(field, aml_named_field("PRQ2", 8));
    aml_append(field, aml_named_field("PRQ3", 8));
    aml_append(sb_scope, field);

    aml_append(sb_scope, build_irq_status_method());
    aml_append(sb_scope, build_iqcr_method(true));

    aml_append(sb_scope, build_link_dev("LNKA", 0, aml_name("PRQ0")));
    aml_append(sb_scope, build_link_dev("LNKB", 1, aml_name("PRQ1")));
    aml_append(sb_scope, build_link_dev("LNKC", 2, aml_name("PRQ2")));
    aml_append(sb_scope, build_link_dev("LNKD", 3, aml_name("PRQ3")));

    dev = aml_device("LNKS");
    {
        aml_append(dev, aml_name_decl("_HID", aml_eisaid("PNP0C0F")));
        aml_append(dev, aml_name_decl("_UID", aml_int(4)));

        crs = aml_resource_template();
        irqs = 9;
        aml_append(crs, aml_interrupt(AML_CONSUMER, AML_LEVEL,
                                      AML_ACTIVE_HIGH, AML_SHARED,
                                      &irqs, 1));
        aml_append(dev, aml_name_decl("_PRS", crs));

        /* The SCI cannot be disabled and is always attached to GSI 9,
         * so these are no-ops.  We only need this link to override the
         * polarity to active high and match the content of the MADT.
         */
        method = aml_method("_STA", 0, AML_NOTSERIALIZED);
        aml_append(method, aml_return(aml_int(0x0b)));
        aml_append(dev, method);

        method = aml_method("_DIS", 0, AML_NOTSERIALIZED);
        aml_append(dev, method);

        method = aml_method("_CRS", 0, AML_NOTSERIALIZED);
        aml_append(method, aml_return(aml_name("_PRS")));
        aml_append(dev, method);

        method = aml_method("_SRS", 1, AML_NOTSERIALIZED);
        aml_append(dev, method);
    }
    aml_append(sb_scope, dev);

    aml_append(table, sb_scope);
}

static void append_q35_prt_entry(Aml *ctx, uint32_t nr, const char *name)
{
    int i;
    int head;
    Aml *pkg;
    char base = name[3] < 'E' ? 'A' : 'E';
    char *s = g_strdup(name);
    Aml *a_nr = aml_int((nr << 16) | 0xffff);

    assert(strlen(s) == 4);

    head = name[3] - base;
    for (i = 0; i < 4; i++) {
        if (head + i > 3) {
            head = i * -1;
        }
        s[3] = base + head + i;
        pkg = aml_package(4);
        aml_append(pkg, a_nr);
        aml_append(pkg, aml_int(i));
        aml_append(pkg, aml_name("%s", s));
        aml_append(pkg, aml_int(0));
        aml_append(ctx, pkg);
    }
    g_free(s);
}

static Aml *build_q35_routing_table(const char *str)
{
    int i;
    Aml *pkg;
    char *name = g_strdup_printf("%s ", str);

    pkg = aml_package(128);
    for (i = 0; i < 0x18; i++) {
            name[3] = 'E' + (i & 0x3);
            append_q35_prt_entry(pkg, i, name);
    }

    name[3] = 'E';
    append_q35_prt_entry(pkg, 0x18, name);

    /* INTA -> PIRQA for slot 25 - 31, see the default value of D<N>IR */
    for (i = 0x0019; i < 0x1e; i++) {
        name[3] = 'A';
        append_q35_prt_entry(pkg, i, name);
    }

    /* PCIe->PCI bridge. use PIRQ[E-H] */
    name[3] = 'E';
    append_q35_prt_entry(pkg, 0x1e, name);
    name[3] = 'A';
    append_q35_prt_entry(pkg, 0x1f, name);

    g_free(name);
    return pkg;
}

static void build_q35_pci0_int(Aml *table)
{
    Aml *field;
    Aml *method;
    Aml *sb_scope = aml_scope("_SB");
    Aml *pci0_scope = aml_scope("PCI0");

    /* Zero => PIC mode, One => APIC Mode */
    aml_append(table, aml_name_decl("PICF", aml_int(0)));
    method = aml_method("_PIC", 1, AML_NOTSERIALIZED);
    {
        aml_append(method, aml_store(aml_arg(0), aml_name("PICF")));
    }
    aml_append(table, method);

    aml_append(pci0_scope,
        aml_name_decl("PRTP", build_q35_routing_table("LNK")));
    aml_append(pci0_scope,
        aml_name_decl("PRTA", build_q35_routing_table("GSI")));

    method = aml_method("_PRT", 0, AML_NOTSERIALIZED);
    {
        Aml *if_ctx;
        Aml *else_ctx;

        /* PCI IRQ routing table, example from ACPI 2.0a specification,
           section 6.2.8.1 */
        /* Note: we provide the same info as the PCI routing
           table of the Bochs BIOS */
        if_ctx = aml_if(aml_equal(aml_name("PICF"), aml_int(0)));
        aml_append(if_ctx, aml_return(aml_name("PRTP")));
        aml_append(method, if_ctx);
        else_ctx = aml_else();
        aml_append(else_ctx, aml_return(aml_name("PRTA")));
        aml_append(method, else_ctx);
    }
    aml_append(pci0_scope, method);
    aml_append(sb_scope, pci0_scope);

    field = aml_field("PCI0.ISA.PIRQ", AML_BYTE_ACC, AML_NOLOCK, AML_PRESERVE);
    aml_append(field, aml_named_field("PRQA", 8));
    aml_append(field, aml_named_field("PRQB", 8));
    aml_append(field, aml_named_field("PRQC", 8));
    aml_append(field, aml_named_field("PRQD", 8));
    aml_append(field, aml_reserved_field(0x20));
    aml_append(field, aml_named_field("PRQE", 8));
    aml_append(field, aml_named_field("PRQF", 8));
    aml_append(field, aml_named_field("PRQG", 8));
    aml_append(field, aml_named_field("PRQH", 8));
    aml_append(sb_scope, field);

    aml_append(sb_scope, build_irq_status_method());
    aml_append(sb_scope, build_iqcr_method(false));

    aml_append(sb_scope, build_link_dev("LNKA", 0, aml_name("PRQA")));
    aml_append(sb_scope, build_link_dev("LNKB", 1, aml_name("PRQB")));
    aml_append(sb_scope, build_link_dev("LNKC", 2, aml_name("PRQC")));
    aml_append(sb_scope, build_link_dev("LNKD", 3, aml_name("PRQD")));
    aml_append(sb_scope, build_link_dev("LNKE", 4, aml_name("PRQE")));
    aml_append(sb_scope, build_link_dev("LNKF", 5, aml_name("PRQF")));
    aml_append(sb_scope, build_link_dev("LNKG", 6, aml_name("PRQG")));
    aml_append(sb_scope, build_link_dev("LNKH", 7, aml_name("PRQH")));

    /*
     * TODO: UID probably shouldn't be the same for GSIx devices
     * but that's how it was in original ASL so keep it for now
     */
    aml_append(sb_scope, build_gsi_link_dev("GSIA", 0, 0x10));
    aml_append(sb_scope, build_gsi_link_dev("GSIB", 0, 0x11));
    aml_append(sb_scope, build_gsi_link_dev("GSIC", 0, 0x12));
    aml_append(sb_scope, build_gsi_link_dev("GSID", 0, 0x13));
    aml_append(sb_scope, build_gsi_link_dev("GSIE", 0, 0x14));
    aml_append(sb_scope, build_gsi_link_dev("GSIF", 0, 0x15));
    aml_append(sb_scope, build_gsi_link_dev("GSIG", 0, 0x16));
    aml_append(sb_scope, build_gsi_link_dev("GSIH", 0, 0x17));

    aml_append(table, sb_scope);
}

static void build_q35_isa_bridge(Aml *table)
{
    Aml *dev;
    Aml *scope;
    Aml *field;

    scope =  aml_scope("_SB.PCI0");
    dev = aml_device("ISA");
    aml_append(dev, aml_name_decl("_ADR", aml_int(0x001F0000)));

    /* ICH9 PCI to ISA irq remapping */
    aml_append(dev, aml_operation_region("PIRQ", AML_PCI_CONFIG,
                                         0x60, 0x0C));

    aml_append(dev, aml_operation_region("LPCD", AML_PCI_CONFIG,
                                         0x80, 0x02));
    field = aml_field("LPCD", AML_ANY_ACC, AML_NOLOCK, AML_PRESERVE);
    aml_append(field, aml_named_field("COMA", 3));
    aml_append(field, aml_reserved_field(1));
    aml_append(field, aml_named_field("COMB", 3));
    aml_append(field, aml_reserved_field(1));
    aml_append(field, aml_named_field("LPTD", 2));
    aml_append(field, aml_reserved_field(2));
    aml_append(field, aml_named_field("FDCD", 2));
    aml_append(dev, field);

    aml_append(dev, aml_operation_region("LPCE", AML_PCI_CONFIG,
                                         0x82, 0x02));
    /* enable bits */
    field = aml_field("LPCE", AML_ANY_ACC, AML_NOLOCK, AML_PRESERVE);
    aml_append(field, aml_named_field("CAEN", 1));
    aml_append(field, aml_named_field("CBEN", 1));
    aml_append(field, aml_named_field("LPEN", 1));
    aml_append(field, aml_named_field("FDEN", 1));
    aml_append(dev, field);

    aml_append(scope, dev);
    aml_append(table, scope);
}

static void build_piix4_pm(Aml *table)
{
    Aml *dev;
    Aml *scope;

    scope =  aml_scope("_SB.PCI0");
    dev = aml_device("PX13");
    aml_append(dev, aml_name_decl("_ADR", aml_int(0x00010003)));

    aml_append(dev, aml_operation_region("P13C", AML_PCI_CONFIG,
                                         0x00, 0xff));
    aml_append(scope, dev);
    aml_append(table, scope);
}

static void build_piix4_isa_bridge(Aml *table)
{
    Aml *dev;
    Aml *scope;
    Aml *field;

    scope =  aml_scope("_SB.PCI0");
    dev = aml_device("ISA");
    aml_append(dev, aml_name_decl("_ADR", aml_int(0x00010000)));

    /* PIIX PCI to ISA irq remapping */
    aml_append(dev, aml_operation_region("P40C", AML_PCI_CONFIG,
                                         0x60, 0x04));
    /* enable bits */
    field = aml_field("^PX13.P13C", AML_ANY_ACC, AML_NOLOCK, AML_PRESERVE);
    /* Offset(0x5f),, 7, */
    aml_append(field, aml_reserved_field(0x2f8));
    aml_append(field, aml_reserved_field(7));
    aml_append(field, aml_named_field("LPEN", 1));
    /* Offset(0x67),, 3, */
    aml_append(field, aml_reserved_field(0x38));
    aml_append(field, aml_reserved_field(3));
    aml_append(field, aml_named_field("CAEN", 1));
    aml_append(field, aml_reserved_field(3));
    aml_append(field, aml_named_field("CBEN", 1));
    aml_append(dev, field);
    aml_append(dev, aml_name_decl("FDEN", aml_int(1)));

    aml_append(scope, dev);
    aml_append(table, scope);
}

static void build_piix4_pci_hotplug(Aml *table)
{
    Aml *scope;
    Aml *field;
    Aml *method;

    scope =  aml_scope("_SB.PCI0");

    aml_append(scope,
        aml_operation_region("PCST", AML_SYSTEM_IO, 0xae00, 0x08));
    field = aml_field("PCST", AML_DWORD_ACC, AML_NOLOCK, AML_WRITE_AS_ZEROS);
    aml_append(field, aml_named_field("PCIU", 32));
    aml_append(field, aml_named_field("PCID", 32));
    aml_append(scope, field);

    aml_append(scope,
        aml_operation_region("SEJ", AML_SYSTEM_IO, 0xae08, 0x04));
    field = aml_field("SEJ", AML_DWORD_ACC, AML_NOLOCK, AML_WRITE_AS_ZEROS);
    aml_append(field, aml_named_field("B0EJ", 32));
    aml_append(scope, field);

    aml_append(scope,
        aml_operation_region("BNMR", AML_SYSTEM_IO, 0xae10, 0x04));
    field = aml_field("BNMR", AML_DWORD_ACC, AML_NOLOCK, AML_WRITE_AS_ZEROS);
    aml_append(field, aml_named_field("BNUM", 32));
    aml_append(scope, field);

    aml_append(scope, aml_mutex("BLCK", 0));

    method = aml_method("PCEJ", 2, AML_NOTSERIALIZED);
    aml_append(method, aml_acquire(aml_name("BLCK"), 0xFFFF));
    aml_append(method, aml_store(aml_arg(0), aml_name("BNUM")));
    aml_append(method,
        aml_store(aml_shiftleft(aml_int(1), aml_arg(1)), aml_name("B0EJ")));
    aml_append(method, aml_release(aml_name("BLCK")));
    aml_append(method, aml_return(aml_int(0)));
    aml_append(scope, method);

    aml_append(table, scope);
}

static Aml *build_q35_osc_method(void)
{
    Aml *if_ctx;
    Aml *if_ctx2;
    Aml *else_ctx;
    Aml *method;
    Aml *a_cwd1 = aml_name("CDW1");
    Aml *a_ctrl = aml_name("CTRL");

    method = aml_method("_OSC", 4, AML_NOTSERIALIZED);
    aml_append(method, aml_create_dword_field(aml_arg(3), aml_int(0), "CDW1"));

    if_ctx = aml_if(aml_equal(
        aml_arg(0), aml_touuid("33DB4D5B-1FF7-401C-9657-7441C03DD766")));
    aml_append(if_ctx, aml_create_dword_field(aml_arg(3), aml_int(4), "CDW2"));
    aml_append(if_ctx, aml_create_dword_field(aml_arg(3), aml_int(8), "CDW3"));

    aml_append(if_ctx, aml_store(aml_name("CDW2"), aml_name("SUPP")));
    aml_append(if_ctx, aml_store(aml_name("CDW3"), a_ctrl));

    /*
     * Always allow native PME, AER (no dependencies)
     * Never allow SHPC (no SHPC controller in this system)
     */
    aml_append(if_ctx, aml_and(a_ctrl, aml_int(0x1D), a_ctrl));

    if_ctx2 = aml_if(aml_lnot(aml_equal(aml_arg(1), aml_int(1))));
    /* Unknown revision */
    aml_append(if_ctx2, aml_or(a_cwd1, aml_int(0x08), a_cwd1));
    aml_append(if_ctx, if_ctx2);

    if_ctx2 = aml_if(aml_lnot(aml_equal(aml_name("CDW3"), a_ctrl)));
    /* Capabilities bits were masked */
    aml_append(if_ctx2, aml_or(a_cwd1, aml_int(0x10), a_cwd1));
    aml_append(if_ctx, if_ctx2);

    /* Update DWORD3 in the buffer */
    aml_append(if_ctx, aml_store(a_ctrl, aml_name("CDW3")));
    aml_append(method, if_ctx);

    else_ctx = aml_else();
    /* Unrecognized UUID */
    aml_append(else_ctx, aml_or(a_cwd1, aml_int(4), a_cwd1));
    aml_append(method, else_ctx);

    aml_append(method, aml_return(aml_arg(3)));
    return method;
}

static void
build_ssdt(GArray *table_data, GArray *linker,
           AcpiCpuInfo *cpu, AcpiPmInfo *pm, AcpiMiscInfo *misc,
           PcPciInfo *pci, PcGuestInfo *guest_info)
{
    MachineState *machine = MACHINE(qdev_get_machine());
    uint32_t nr_mem = machine->ram_slots;
    Aml *ssdt, *sb_scope, *scope, *pkg, *dev, *method, *crs, *field;
    PCIBus *bus = NULL;
    GPtrArray *io_ranges = g_ptr_array_new_with_free_func(crs_range_free);
    GPtrArray *mem_ranges = g_ptr_array_new_with_free_func(crs_range_free);
    CrsRangeEntry *entry;
    int root_bus_limit = 0xFF;
    int i;

    ssdt = init_aml_allocator();

    /* Reserve space for header */
    acpi_data_push(ssdt->buf, sizeof(AcpiTableHeader));

    bus = PC_MACHINE(machine)->bus;
    if (bus) {
        QLIST_FOREACH(bus, &bus->child, sibling) {
            uint8_t bus_num = pci_bus_num(bus);
            uint8_t numa_node = pci_bus_numa_node(bus);

            /* look only for expander root buses */
            if (!pci_bus_is_root(bus)) {
                continue;
            }

            if (bus_num < root_bus_limit) {
                root_bus_limit = bus_num - 1;
            }

            scope = aml_scope("\\_SB");
            dev = aml_device("PC%.02X", bus_num);
            aml_append(dev, aml_name_decl("_UID", aml_int(bus_num)));
            aml_append(dev, aml_name_decl("_HID", aml_eisaid("PNP0A03")));
            aml_append(dev, aml_name_decl("_BBN", aml_int(bus_num)));

            if (numa_node != NUMA_NODE_UNASSIGNED) {
                aml_append(dev, aml_name_decl("_PXM", aml_int(numa_node)));
            }

            aml_append(dev, build_prt(false));
            crs = build_crs(PCI_HOST_BRIDGE(BUS(bus)->parent),
                            io_ranges, mem_ranges);
            aml_append(dev, aml_name_decl("_CRS", crs));
            aml_append(scope, dev);
            aml_append(ssdt, scope);
        }
    }

    scope = aml_scope("\\_SB.PCI0");
    /* build PCI0._CRS */
    crs = aml_resource_template();
    aml_append(crs,
        aml_word_bus_number(AML_MIN_FIXED, AML_MAX_FIXED, AML_POS_DECODE,
                            0x0000, 0x0, root_bus_limit,
                            0x0000, root_bus_limit + 1));
    aml_append(crs, aml_io(AML_DECODE16, 0x0CF8, 0x0CF8, 0x01, 0x08));

    aml_append(crs,
        aml_word_io(AML_MIN_FIXED, AML_MAX_FIXED,
                    AML_POS_DECODE, AML_ENTIRE_RANGE,
                    0x0000, 0x0000, 0x0CF7, 0x0000, 0x0CF8));

    crs_replace_with_free_ranges(io_ranges, 0x0D00, 0xFFFF);
    for (i = 0; i < io_ranges->len; i++) {
        entry = g_ptr_array_index(io_ranges, i);
        aml_append(crs,
            aml_word_io(AML_MIN_FIXED, AML_MAX_FIXED,
                        AML_POS_DECODE, AML_ENTIRE_RANGE,
                        0x0000, entry->base, entry->limit,
                        0x0000, entry->limit - entry->base + 1));
    }

    aml_append(crs,
        aml_dword_memory(AML_POS_DECODE, AML_MIN_FIXED, AML_MAX_FIXED,
                         AML_CACHEABLE, AML_READ_WRITE,
                         0, 0x000A0000, 0x000BFFFF, 0, 0x00020000));

    crs_replace_with_free_ranges(mem_ranges, pci->w32.begin, pci->w32.end - 1);
    for (i = 0; i < mem_ranges->len; i++) {
        entry = g_ptr_array_index(mem_ranges, i);
        aml_append(crs,
            aml_dword_memory(AML_POS_DECODE, AML_MIN_FIXED, AML_MAX_FIXED,
                             AML_NON_CACHEABLE, AML_READ_WRITE,
                             0, entry->base, entry->limit,
                             0, entry->limit - entry->base + 1));
    }

    if (pci->w64.begin) {
        aml_append(crs,
            aml_qword_memory(AML_POS_DECODE, AML_MIN_FIXED, AML_MAX_FIXED,
                             AML_CACHEABLE, AML_READ_WRITE,
                             0, pci->w64.begin, pci->w64.end - 1, 0,
                             pci->w64.end - pci->w64.begin));
    }
    aml_append(scope, aml_name_decl("_CRS", crs));

    /* reserve GPE0 block resources */
    dev = aml_device("GPE0");
    aml_append(dev, aml_name_decl("_HID", aml_string("PNP0A06")));
    aml_append(dev, aml_name_decl("_UID", aml_string("GPE0 resources")));
    /* device present, functioning, decoding, not shown in UI */
    aml_append(dev, aml_name_decl("_STA", aml_int(0xB)));
    crs = aml_resource_template();
    aml_append(crs,
        aml_io(AML_DECODE16, pm->gpe0_blk, pm->gpe0_blk, 1, pm->gpe0_blk_len)
    );
    aml_append(dev, aml_name_decl("_CRS", crs));
    aml_append(scope, dev);

    g_ptr_array_free(io_ranges, true);
    g_ptr_array_free(mem_ranges, true);

    /* reserve PCIHP resources */
    if (pm->pcihp_io_len) {
        dev = aml_device("PHPR");
        aml_append(dev, aml_name_decl("_HID", aml_string("PNP0A06")));
        aml_append(dev,
            aml_name_decl("_UID", aml_string("PCI Hotplug resources")));
        /* device present, functioning, decoding, not shown in UI */
        aml_append(dev, aml_name_decl("_STA", aml_int(0xB)));
        crs = aml_resource_template();
        aml_append(crs,
            aml_io(AML_DECODE16, pm->pcihp_io_base, pm->pcihp_io_base, 1,
                   pm->pcihp_io_len)
        );
        aml_append(dev, aml_name_decl("_CRS", crs));
        aml_append(scope, dev);
    }
    aml_append(ssdt, scope);

    /*  create S3_ / S4_ / S5_ packages if necessary */
    scope = aml_scope("\\");
    if (!pm->s3_disabled) {
        pkg = aml_package(4);
        aml_append(pkg, aml_int(1)); /* PM1a_CNT.SLP_TYP */
        aml_append(pkg, aml_int(1)); /* PM1b_CNT.SLP_TYP, FIXME: not impl. */
        aml_append(pkg, aml_int(0)); /* reserved */
        aml_append(pkg, aml_int(0)); /* reserved */
        aml_append(scope, aml_name_decl("_S3", pkg));
    }

    if (!pm->s4_disabled) {
        pkg = aml_package(4);
        aml_append(pkg, aml_int(pm->s4_val)); /* PM1a_CNT.SLP_TYP */
        /* PM1b_CNT.SLP_TYP, FIXME: not impl. */
        aml_append(pkg, aml_int(pm->s4_val));
        aml_append(pkg, aml_int(0)); /* reserved */
        aml_append(pkg, aml_int(0)); /* reserved */
        aml_append(scope, aml_name_decl("_S4", pkg));
    }

    pkg = aml_package(4);
    aml_append(pkg, aml_int(0)); /* PM1a_CNT.SLP_TYP */
    aml_append(pkg, aml_int(0)); /* PM1b_CNT.SLP_TYP not impl. */
    aml_append(pkg, aml_int(0)); /* reserved */
    aml_append(pkg, aml_int(0)); /* reserved */
    aml_append(scope, aml_name_decl("_S5", pkg));
    aml_append(ssdt, scope);

    if (misc->applesmc_io_base) {
        scope = aml_scope("\\_SB.PCI0.ISA");
        dev = aml_device("SMC");

        aml_append(dev, aml_name_decl("_HID", aml_eisaid("APP0001")));
        /* device present, functioning, decoding, not shown in UI */
        aml_append(dev, aml_name_decl("_STA", aml_int(0xB)));

        crs = aml_resource_template();
        aml_append(crs,
            aml_io(AML_DECODE16, misc->applesmc_io_base, misc->applesmc_io_base,
                   0x01, APPLESMC_MAX_DATA_LENGTH)
        );
        aml_append(crs, aml_irq_no_flags(6));
        aml_append(dev, aml_name_decl("_CRS", crs));

        aml_append(scope, dev);
        aml_append(ssdt, scope);
    }

    if (misc->pvpanic_port) {
        scope = aml_scope("\\_SB.PCI0.ISA");

        dev = aml_device("PEVT");
        aml_append(dev, aml_name_decl("_HID", aml_string("QEMU0001")));

        crs = aml_resource_template();
        aml_append(crs,
            aml_io(AML_DECODE16, misc->pvpanic_port, misc->pvpanic_port, 1, 1)
        );
        aml_append(dev, aml_name_decl("_CRS", crs));

        aml_append(dev, aml_operation_region("PEOR", AML_SYSTEM_IO,
                                              misc->pvpanic_port, 1));
        field = aml_field("PEOR", AML_BYTE_ACC, AML_NOLOCK, AML_PRESERVE);
        aml_append(field, aml_named_field("PEPT", 8));
        aml_append(dev, field);

        /* device present, functioning, decoding, shown in UI */
        aml_append(dev, aml_name_decl("_STA", aml_int(0xF)));

        method = aml_method("RDPT", 0, AML_NOTSERIALIZED);
        aml_append(method, aml_store(aml_name("PEPT"), aml_local(0)));
        aml_append(method, aml_return(aml_local(0)));
        aml_append(dev, method);

        method = aml_method("WRPT", 1, AML_NOTSERIALIZED);
        aml_append(method, aml_store(aml_arg(0), aml_name("PEPT")));
        aml_append(dev, method);

        aml_append(scope, dev);
        aml_append(ssdt, scope);
    }

    sb_scope = aml_scope("\\_SB");
    {
        build_processor_devices(sb_scope, guest_info->apic_id_limit, cpu, pm);

        build_memory_devices(sb_scope, nr_mem, pm->mem_hp_io_base,
                             pm->mem_hp_io_len);

        {
            Object *pci_host;
            PCIBus *bus = NULL;

            pci_host = acpi_get_i386_pci_host();
            if (pci_host) {
                bus = PCI_HOST_BRIDGE(pci_host)->bus;
            }

            if (bus) {
                Aml *scope = aml_scope("PCI0");
                /* Scan all PCI buses. Generate tables to support hotplug. */
                build_append_pci_bus_devices(scope, bus, pm->pcihp_bridge_en);

                if (misc->tpm_version != TPM_VERSION_UNSPEC) {
                    dev = aml_device("ISA.TPM");
                    aml_append(dev, aml_name_decl("_HID", aml_eisaid("PNP0C31")));
                    aml_append(dev, aml_name_decl("_STA", aml_int(0xF)));
                    crs = aml_resource_template();
                    aml_append(crs, aml_memory32_fixed(TPM_TIS_ADDR_BASE,
                               TPM_TIS_ADDR_SIZE, AML_READ_WRITE));
                    aml_append(crs, aml_irq_no_flags(TPM_TIS_IRQ));
                    aml_append(dev, aml_name_decl("_CRS", crs));
                    aml_append(scope, dev);
                }

                aml_append(sb_scope, scope);
            }
        }
        aml_append(ssdt, sb_scope);
    }

    /* copy AML table into ACPI tables blob and patch header there */
    g_array_append_vals(table_data, ssdt->buf->data, ssdt->buf->len);
    build_header(linker, table_data,
        (void *)(table_data->data + table_data->len - ssdt->buf->len),
        "SSDT", ssdt->buf->len, 1, NULL);
    free_aml_allocator();
}

static void
build_hpet(GArray *table_data, GArray *linker)
{
    Acpi20Hpet *hpet;

    hpet = acpi_data_push(table_data, sizeof(*hpet));
    /* Note timer_block_id value must be kept in sync with value advertised by
     * emulated hpet
     */
    hpet->timer_block_id = cpu_to_le32(0x8086a201);
    hpet->addr.address = cpu_to_le64(HPET_BASE);
    build_header(linker, table_data,
                 (void *)hpet, "HPET", sizeof(*hpet), 1, NULL);
}

static void
build_tpm_tcpa(GArray *table_data, GArray *linker, GArray *tcpalog)
{
    Acpi20Tcpa *tcpa = acpi_data_push(table_data, sizeof *tcpa);
    uint64_t log_area_start_address = acpi_data_len(tcpalog);

    tcpa->platform_class = cpu_to_le16(TPM_TCPA_ACPI_CLASS_CLIENT);
    tcpa->log_area_minimum_length = cpu_to_le32(TPM_LOG_AREA_MINIMUM_SIZE);
    tcpa->log_area_start_address = cpu_to_le64(log_area_start_address);

    bios_linker_loader_alloc(linker, ACPI_BUILD_TPMLOG_FILE, 1,
                             false /* high memory */);

    /* log area start address to be filled by Guest linker */
    bios_linker_loader_add_pointer(linker, ACPI_BUILD_TABLE_FILE,
                                   ACPI_BUILD_TPMLOG_FILE,
                                   table_data, &tcpa->log_area_start_address,
                                   sizeof(tcpa->log_area_start_address));

    build_header(linker, table_data,
                 (void *)tcpa, "TCPA", sizeof(*tcpa), 2, NULL);

    acpi_data_push(tcpalog, TPM_LOG_AREA_MINIMUM_SIZE);
}

static void
build_tpm2(GArray *table_data, GArray *linker)
{
    Acpi20TPM2 *tpm2_ptr;

    tpm2_ptr = acpi_data_push(table_data, sizeof *tpm2_ptr);

    tpm2_ptr->platform_class = cpu_to_le16(TPM2_ACPI_CLASS_CLIENT);
    tpm2_ptr->control_area_address = cpu_to_le64(0);
    tpm2_ptr->start_method = cpu_to_le32(TPM2_START_METHOD_MMIO);

    build_header(linker, table_data,
                 (void *)tpm2_ptr, "TPM2", sizeof(*tpm2_ptr), 4, NULL);
}

typedef enum {
    MEM_AFFINITY_NOFLAGS      = 0,
    MEM_AFFINITY_ENABLED      = (1 << 0),
    MEM_AFFINITY_HOTPLUGGABLE = (1 << 1),
    MEM_AFFINITY_NON_VOLATILE = (1 << 2),
} MemoryAffinityFlags;

static void
acpi_build_srat_memory(AcpiSratMemoryAffinity *numamem, uint64_t base,
                       uint64_t len, int node, MemoryAffinityFlags flags)
{
    numamem->type = ACPI_SRAT_MEMORY;
    numamem->length = sizeof(*numamem);
    memset(numamem->proximity, 0, 4);
    numamem->proximity[0] = node;
    numamem->flags = cpu_to_le32(flags);
    numamem->base_addr = cpu_to_le64(base);
    numamem->range_length = cpu_to_le64(len);
}

static void
build_srat(GArray *table_data, GArray *linker, PcGuestInfo *guest_info)
{
    AcpiSystemResourceAffinityTable *srat;
    AcpiSratProcessorAffinity *core;
    AcpiSratMemoryAffinity *numamem;

    int i;
    uint64_t curnode;
    int srat_start, numa_start, slots;
    uint64_t mem_len, mem_base, next_base;
    PCMachineState *pcms = PC_MACHINE(qdev_get_machine());
    ram_addr_t hotplugabble_address_space_size =
        object_property_get_int(OBJECT(pcms), PC_MACHINE_MEMHP_REGION_SIZE,
                                NULL);

    srat_start = table_data->len;

    srat = acpi_data_push(table_data, sizeof *srat);
    srat->reserved1 = cpu_to_le32(1);
    core = (void *)(srat + 1);

    for (i = 0; i < guest_info->apic_id_limit; ++i) {
        core = acpi_data_push(table_data, sizeof *core);
        core->type = ACPI_SRAT_PROCESSOR;
        core->length = sizeof(*core);
        core->local_apic_id = i;
        curnode = guest_info->node_cpu[i];
        core->proximity_lo = curnode;
        memset(core->proximity_hi, 0, 3);
        core->local_sapic_eid = 0;
        core->flags = cpu_to_le32(1);
    }


    /* the memory map is a bit tricky, it contains at least one hole
     * from 640k-1M and possibly another one from 3.5G-4G.
     */
    next_base = 0;
    numa_start = table_data->len;

    numamem = acpi_data_push(table_data, sizeof *numamem);
    acpi_build_srat_memory(numamem, 0, 640*1024, 0, MEM_AFFINITY_ENABLED);
    next_base = 1024 * 1024;
    for (i = 1; i < guest_info->numa_nodes + 1; ++i) {
        mem_base = next_base;
        mem_len = guest_info->node_mem[i - 1];
        if (i == 1) {
            mem_len -= 1024 * 1024;
        }
        next_base = mem_base + mem_len;

        /* Cut out the ACPI_PCI hole */
        if (mem_base <= guest_info->ram_size_below_4g &&
            next_base > guest_info->ram_size_below_4g) {
            mem_len -= next_base - guest_info->ram_size_below_4g;
            if (mem_len > 0) {
                numamem = acpi_data_push(table_data, sizeof *numamem);
                acpi_build_srat_memory(numamem, mem_base, mem_len, i - 1,
                                       MEM_AFFINITY_ENABLED);
            }
            mem_base = 1ULL << 32;
            mem_len = next_base - guest_info->ram_size_below_4g;
            next_base += (1ULL << 32) - guest_info->ram_size_below_4g;
        }
        numamem = acpi_data_push(table_data, sizeof *numamem);
        acpi_build_srat_memory(numamem, mem_base, mem_len, i - 1,
                               MEM_AFFINITY_ENABLED);
    }
    slots = (table_data->len - numa_start) / sizeof *numamem;
    for (; slots < guest_info->numa_nodes + 2; slots++) {
        numamem = acpi_data_push(table_data, sizeof *numamem);
        acpi_build_srat_memory(numamem, 0, 0, 0, MEM_AFFINITY_NOFLAGS);
    }

    /*
     * Entry is required for Windows to enable memory hotplug in OS.
     * Memory devices may override proximity set by this entry,
     * providing _PXM method if necessary.
     */
    if (hotplugabble_address_space_size) {
        numamem = acpi_data_push(table_data, sizeof *numamem);
        acpi_build_srat_memory(numamem, pcms->hotplug_memory.base,
                               hotplugabble_address_space_size, 0,
                               MEM_AFFINITY_HOTPLUGGABLE |
                               MEM_AFFINITY_ENABLED);
    }

    build_header(linker, table_data,
                 (void *)(table_data->data + srat_start),
                 "SRAT",
                 table_data->len - srat_start, 1, NULL);
}

static void
build_mcfg_q35(GArray *table_data, GArray *linker, AcpiMcfgInfo *info)
{
    AcpiTableMcfg *mcfg;
    const char *sig;
    int len = sizeof(*mcfg) + 1 * sizeof(mcfg->allocation[0]);

    mcfg = acpi_data_push(table_data, len);
    mcfg->allocation[0].address = cpu_to_le64(info->mcfg_base);
    /* Only a single allocation so no need to play with segments */
    mcfg->allocation[0].pci_segment = cpu_to_le16(0);
    mcfg->allocation[0].start_bus_number = 0;
    mcfg->allocation[0].end_bus_number = PCIE_MMCFG_BUS(info->mcfg_size - 1);

    /* MCFG is used for ECAM which can be enabled or disabled by guest.
     * To avoid table size changes (which create migration issues),
     * always create the table even if there are no allocations,
     * but set the signature to a reserved value in this case.
     * ACPI spec requires OSPMs to ignore such tables.
     */
    if (info->mcfg_base == PCIE_BASE_ADDR_UNMAPPED) {
        /* Reserved signature: ignored by OSPM */
        sig = "QEMU";
    } else {
        sig = "MCFG";
    }
    build_header(linker, table_data, (void *)mcfg, sig, len, 1, NULL);
}

static void
build_dmar_q35(GArray *table_data, GArray *linker)
{
    int dmar_start = table_data->len;

    AcpiTableDmar *dmar;
    AcpiDmarHardwareUnit *drhd;

    dmar = acpi_data_push(table_data, sizeof(*dmar));
    dmar->host_address_width = VTD_HOST_ADDRESS_WIDTH - 1;
    dmar->flags = 0;    /* No intr_remap for now */

    /* DMAR Remapping Hardware Unit Definition structure */
    drhd = acpi_data_push(table_data, sizeof(*drhd));
    drhd->type = cpu_to_le16(ACPI_DMAR_TYPE_HARDWARE_UNIT);
    drhd->length = cpu_to_le16(sizeof(*drhd));   /* No device scope now */
    drhd->flags = ACPI_DMAR_INCLUDE_PCI_ALL;
    drhd->pci_segment = cpu_to_le16(0);
    drhd->address = cpu_to_le64(Q35_HOST_BRIDGE_IOMMU_ADDR);

    build_header(linker, table_data, (void *)(table_data->data + dmar_start),
                 "DMAR", table_data->len - dmar_start, 1, NULL);
}

static void
build_dsdt(GArray *table_data, GArray *linker,
           AcpiPmInfo *pm, AcpiMiscInfo *misc)
{
    Aml *dsdt, *sb_scope, *scope, *dev, *method, *field;
    MachineState *machine = MACHINE(qdev_get_machine());
    uint32_t nr_mem = machine->ram_slots;

    dsdt = init_aml_allocator();

    /* Reserve space for header */
    acpi_data_push(dsdt->buf, sizeof(AcpiTableHeader));

    build_dbg_aml(dsdt);
    if (misc->is_piix4) {
        sb_scope = aml_scope("_SB");
        dev = aml_device("PCI0");
        aml_append(dev, aml_name_decl("_HID", aml_eisaid("PNP0A03")));
        aml_append(dev, aml_name_decl("_ADR", aml_int(0)));
        aml_append(dev, aml_name_decl("_UID", aml_int(1)));
        aml_append(sb_scope, dev);
        aml_append(dsdt, sb_scope);

        build_hpet_aml(dsdt);
        build_piix4_pm(dsdt);
        build_piix4_isa_bridge(dsdt);
        build_isa_devices_aml(dsdt);
        build_piix4_pci_hotplug(dsdt);
        build_piix4_pci0_int(dsdt);
    } else {
        sb_scope = aml_scope("_SB");
        aml_append(sb_scope,
            aml_operation_region("PCST", AML_SYSTEM_IO, 0xae00, 0x0c));
        aml_append(sb_scope,
            aml_operation_region("PCSB", AML_SYSTEM_IO, 0xae0c, 0x01));
        field = aml_field("PCSB", AML_ANY_ACC, AML_NOLOCK, AML_WRITE_AS_ZEROS);
        aml_append(field, aml_named_field("PCIB", 8));
        aml_append(sb_scope, field);
        aml_append(dsdt, sb_scope);

        sb_scope = aml_scope("_SB");
        dev = aml_device("PCI0");
        aml_append(dev, aml_name_decl("_HID", aml_eisaid("PNP0A08")));
        aml_append(dev, aml_name_decl("_CID", aml_eisaid("PNP0A03")));
        aml_append(dev, aml_name_decl("_ADR", aml_int(0)));
        aml_append(dev, aml_name_decl("_UID", aml_int(1)));
        aml_append(dev, aml_name_decl("SUPP", aml_int(0)));
        aml_append(dev, aml_name_decl("CTRL", aml_int(0)));
        aml_append(dev, build_q35_osc_method());
        aml_append(sb_scope, dev);
        aml_append(dsdt, sb_scope);

        build_hpet_aml(dsdt);
        build_q35_isa_bridge(dsdt);
        build_isa_devices_aml(dsdt);
        build_q35_pci0_int(dsdt);
    }

    build_cpu_hotplug_aml(dsdt);
    build_memory_hotplug_aml(dsdt, nr_mem, pm->mem_hp_io_base,
                             pm->mem_hp_io_len);

    scope =  aml_scope("_GPE");
    {
        aml_append(scope, aml_name_decl("_HID", aml_string("ACPI0006")));

        aml_append(scope, aml_method("_L00", 0, AML_NOTSERIALIZED));

        if (misc->is_piix4) {
            method = aml_method("_E01", 0, AML_NOTSERIALIZED);
            aml_append(method,
                aml_acquire(aml_name("\\_SB.PCI0.BLCK"), 0xFFFF));
            aml_append(method, aml_call0("\\_SB.PCI0.PCNT"));
            aml_append(method, aml_release(aml_name("\\_SB.PCI0.BLCK")));
            aml_append(scope, method);
        } else {
            aml_append(scope, aml_method("_L01", 0, AML_NOTSERIALIZED));
        }

        method = aml_method("_E02", 0, AML_NOTSERIALIZED);
        aml_append(method, aml_call0("\\_SB." CPU_SCAN_METHOD));
        aml_append(scope, method);

        method = aml_method("_E03", 0, AML_NOTSERIALIZED);
        aml_append(method, aml_call0(MEMORY_HOTPLUG_HANDLER_PATH));
        aml_append(scope, method);

        aml_append(scope, aml_method("_L04", 0, AML_NOTSERIALIZED));
        aml_append(scope, aml_method("_L05", 0, AML_NOTSERIALIZED));
        aml_append(scope, aml_method("_L06", 0, AML_NOTSERIALIZED));
        aml_append(scope, aml_method("_L07", 0, AML_NOTSERIALIZED));
        aml_append(scope, aml_method("_L08", 0, AML_NOTSERIALIZED));
        aml_append(scope, aml_method("_L09", 0, AML_NOTSERIALIZED));
        aml_append(scope, aml_method("_L0A", 0, AML_NOTSERIALIZED));
        aml_append(scope, aml_method("_L0B", 0, AML_NOTSERIALIZED));
        aml_append(scope, aml_method("_L0C", 0, AML_NOTSERIALIZED));
        aml_append(scope, aml_method("_L0D", 0, AML_NOTSERIALIZED));
        aml_append(scope, aml_method("_L0E", 0, AML_NOTSERIALIZED));
        aml_append(scope, aml_method("_L0F", 0, AML_NOTSERIALIZED));
    }
    aml_append(dsdt, scope);

    /* copy AML table into ACPI tables blob and patch header there */
    g_array_append_vals(table_data, dsdt->buf->data, dsdt->buf->len);
    build_header(linker, table_data,
        (void *)(table_data->data + table_data->len - dsdt->buf->len),
        "DSDT", dsdt->buf->len, 1, NULL);
    free_aml_allocator();
}

static GArray *
build_rsdp(GArray *rsdp_table, GArray *linker, unsigned rsdt)
{
    AcpiRsdpDescriptor *rsdp = acpi_data_push(rsdp_table, sizeof *rsdp);

    bios_linker_loader_alloc(linker, ACPI_BUILD_RSDP_FILE, 16,
                             true /* fseg memory */);

    memcpy(&rsdp->signature, "RSD PTR ", 8);
    memcpy(rsdp->oem_id, ACPI_BUILD_APPNAME6, 6);
    rsdp->rsdt_physical_address = cpu_to_le32(rsdt);
    /* Address to be filled by Guest linker */
    bios_linker_loader_add_pointer(linker, ACPI_BUILD_RSDP_FILE,
                                   ACPI_BUILD_TABLE_FILE,
                                   rsdp_table, &rsdp->rsdt_physical_address,
                                   sizeof rsdp->rsdt_physical_address);
    rsdp->checksum = 0;
    /* Checksum to be filled by Guest linker */
    bios_linker_loader_add_checksum(linker, ACPI_BUILD_RSDP_FILE,
                                    rsdp, rsdp, sizeof *rsdp, &rsdp->checksum);

    return rsdp_table;
}

typedef
struct AcpiBuildState {
    /* Copy of table in RAM (for patching). */
    MemoryRegion *table_mr;
    /* Is table patched? */
    uint8_t patched;
    PcGuestInfo *guest_info;
    void *rsdp;
    MemoryRegion *rsdp_mr;
    MemoryRegion *linker_mr;
} AcpiBuildState;

static bool acpi_get_mcfg(AcpiMcfgInfo *mcfg)
{
    Object *pci_host;
    QObject *o;

    pci_host = acpi_get_i386_pci_host();
    g_assert(pci_host);

    o = object_property_get_qobject(pci_host, PCIE_HOST_MCFG_BASE, NULL);
    if (!o) {
        return false;
    }
    mcfg->mcfg_base = qint_get_int(qobject_to_qint(o));
    qobject_decref(o);

    o = object_property_get_qobject(pci_host, PCIE_HOST_MCFG_SIZE, NULL);
    assert(o);
    mcfg->mcfg_size = qint_get_int(qobject_to_qint(o));
    qobject_decref(o);
    return true;
}

static bool acpi_has_iommu(void)
{
    bool ambiguous;
    Object *intel_iommu;

    intel_iommu = object_resolve_path_type("", TYPE_INTEL_IOMMU_DEVICE,
                                           &ambiguous);
    return intel_iommu && !ambiguous;
}

static bool acpi_has_nvdimm(void)
{
    PCMachineState *pcms = PC_MACHINE(qdev_get_machine());

    return pcms->nvdimm;
}

static
void acpi_build(PcGuestInfo *guest_info, AcpiBuildTables *tables)
{
    GArray *table_offsets;
    unsigned facs, ssdt, dsdt, rsdt;
    AcpiCpuInfo cpu;
    AcpiPmInfo pm;
    AcpiMiscInfo misc;
    AcpiMcfgInfo mcfg;
    PcPciInfo pci;
    uint8_t *u;
    size_t aml_len = 0;
    GArray *tables_blob = tables->table_data;

    acpi_get_cpu_info(&cpu);
    acpi_get_pm_info(&pm);
    acpi_get_misc_info(&misc);
    acpi_get_pci_info(&pci);

    table_offsets = g_array_new(false, true /* clear */,
                                        sizeof(uint32_t));
    ACPI_BUILD_DPRINTF("init ACPI tables\n");

    bios_linker_loader_alloc(tables->linker, ACPI_BUILD_TABLE_FILE,
                             64 /* Ensure FACS is aligned */,
                             false /* high memory */);

    /*
     * FACS is pointed to by FADT.
     * We place it first since it's the only table that has alignment
     * requirements.
     */
    facs = tables_blob->len;
    build_facs(tables_blob, tables->linker, guest_info);

    /* DSDT is pointed to by FADT */
    dsdt = tables_blob->len;
    build_dsdt(tables_blob, tables->linker, &pm, &misc);

    /* Count the size of the DSDT and SSDT, we will need it for legacy
     * sizing of ACPI tables.
     */
    aml_len += tables_blob->len - dsdt;

    /* ACPI tables pointed to by RSDT */
    acpi_add_table(table_offsets, tables_blob);
    build_fadt(tables_blob, tables->linker, &pm, facs, dsdt);

    ssdt = tables_blob->len;
    acpi_add_table(table_offsets, tables_blob);
    build_ssdt(tables_blob, tables->linker, &cpu, &pm, &misc, &pci,
               guest_info);
    aml_len += tables_blob->len - ssdt;

    acpi_add_table(table_offsets, tables_blob);
    build_madt(tables_blob, tables->linker, &cpu, guest_info);

    if (misc.has_hpet) {
        acpi_add_table(table_offsets, tables_blob);
        build_hpet(tables_blob, tables->linker);
    }
    if (misc.tpm_version != TPM_VERSION_UNSPEC) {
        acpi_add_table(table_offsets, tables_blob);
        build_tpm_tcpa(tables_blob, tables->linker, tables->tcpalog);

        if (misc.tpm_version == TPM_VERSION_2_0) {
            acpi_add_table(table_offsets, tables_blob);
            build_tpm2(tables_blob, tables->linker);
        }
    }
    if (guest_info->numa_nodes) {
        acpi_add_table(table_offsets, tables_blob);
        build_srat(tables_blob, tables->linker, guest_info);
    }
    if (acpi_get_mcfg(&mcfg)) {
        acpi_add_table(table_offsets, tables_blob);
        build_mcfg_q35(tables_blob, tables->linker, &mcfg);
    }
    if (acpi_has_iommu()) {
        acpi_add_table(table_offsets, tables_blob);
        build_dmar_q35(tables_blob, tables->linker);
    }

    if (acpi_has_nvdimm()) {
        nvdimm_build_acpi(table_offsets, tables_blob, tables->linker);
    }

    /* Add tables supplied by user (if any) */
    for (u = acpi_table_first(); u; u = acpi_table_next(u)) {
        unsigned len = acpi_table_len(u);

        acpi_add_table(table_offsets, tables_blob);
        g_array_append_vals(tables_blob, u, len);
    }

    /* RSDT is pointed to by RSDP */
    rsdt = tables_blob->len;
    build_rsdt(tables_blob, tables->linker, table_offsets);

    /* RSDP is in FSEG memory, so allocate it separately */
    build_rsdp(tables->rsdp, tables->linker, rsdt);

    /* We'll expose it all to Guest so we want to reduce
     * chance of size changes.
     *
     * We used to align the tables to 4k, but of course this would
     * too simple to be enough.  4k turned out to be too small an
     * alignment very soon, and in fact it is almost impossible to
     * keep the table size stable for all (max_cpus, max_memory_slots)
     * combinations.  So the table size is always 64k for pc-i440fx-2.1
     * and we give an error if the table grows beyond that limit.
     *
     * We still have the problem of migrating from "-M pc-i440fx-2.0".  For
     * that, we exploit the fact that QEMU 2.1 generates _smaller_ tables
     * than 2.0 and we can always pad the smaller tables with zeros.  We can
     * then use the exact size of the 2.0 tables.
     *
     * All this is for PIIX4, since QEMU 2.0 didn't support Q35 migration.
     */
    if (guest_info->legacy_acpi_table_size) {
        /* Subtracting aml_len gives the size of fixed tables.  Then add the
         * size of the PIIX4 DSDT/SSDT in QEMU 2.0.
         */
        int legacy_aml_len =
            guest_info->legacy_acpi_table_size +
            ACPI_BUILD_LEGACY_CPU_AML_SIZE * max_cpus;
        int legacy_table_size =
            ROUND_UP(tables_blob->len - aml_len + legacy_aml_len,
                     ACPI_BUILD_ALIGN_SIZE);
        if (tables_blob->len > legacy_table_size) {
            /* Should happen only with PCI bridges and -M pc-i440fx-2.0.  */
            error_report("Warning: migration may not work.");
        }
        g_array_set_size(tables_blob, legacy_table_size);
    } else {
        /* Make sure we have a buffer in case we need to resize the tables. */
        if (tables_blob->len > ACPI_BUILD_TABLE_SIZE / 2) {
            /* As of QEMU 2.1, this fires with 160 VCPUs and 255 memory slots.  */
            error_report("Warning: ACPI tables are larger than 64k.");
            error_report("Warning: migration may not work.");
            error_report("Warning: please remove CPUs, NUMA nodes, "
                         "memory slots or PCI bridges.");
        }
        acpi_align_size(tables_blob, ACPI_BUILD_TABLE_SIZE);
    }

    acpi_align_size(tables->linker, ACPI_BUILD_ALIGN_SIZE);

    /* Cleanup memory that's no longer used. */
    g_array_free(table_offsets, true);
}

static void acpi_ram_update(MemoryRegion *mr, GArray *data)
{
    uint32_t size = acpi_data_len(data);

    /* Make sure RAM size is correct - in case it got changed e.g. by migration */
    memory_region_ram_resize(mr, size, &error_abort);

    memcpy(memory_region_get_ram_ptr(mr), data->data, size);
    memory_region_set_dirty(mr, 0, size);
}

static void acpi_build_update(void *build_opaque)
{
    AcpiBuildState *build_state = build_opaque;
    AcpiBuildTables tables;

    /* No state to update or already patched? Nothing to do. */
    if (!build_state || build_state->patched) {
        return;
    }
    build_state->patched = 1;

    acpi_build_tables_init(&tables);

    acpi_build(build_state->guest_info, &tables);

    acpi_ram_update(build_state->table_mr, tables.table_data);

    if (build_state->rsdp) {
        memcpy(build_state->rsdp, tables.rsdp->data, acpi_data_len(tables.rsdp));
    } else {
        acpi_ram_update(build_state->rsdp_mr, tables.rsdp);
    }

    acpi_ram_update(build_state->linker_mr, tables.linker);
    acpi_build_tables_cleanup(&tables, true);
}

static void acpi_build_reset(void *build_opaque)
{
    AcpiBuildState *build_state = build_opaque;
    build_state->patched = 0;
}

static MemoryRegion *acpi_add_rom_blob(AcpiBuildState *build_state,
                                       GArray *blob, const char *name,
                                       uint64_t max_size)
{
    return rom_add_blob(name, blob->data, acpi_data_len(blob), max_size, -1,
                        name, acpi_build_update, build_state);
}

static const VMStateDescription vmstate_acpi_build = {
    .name = "acpi_build",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_UINT8(patched, AcpiBuildState),
        VMSTATE_END_OF_LIST()
    },
};

void acpi_setup(PcGuestInfo *guest_info)
{
    AcpiBuildTables tables;
    AcpiBuildState *build_state;

    if (!guest_info->fw_cfg) {
        ACPI_BUILD_DPRINTF("No fw cfg. Bailing out.\n");
        return;
    }

    if (!guest_info->has_acpi_build) {
        ACPI_BUILD_DPRINTF("ACPI build disabled. Bailing out.\n");
        return;
    }

    if (!acpi_enabled) {
        ACPI_BUILD_DPRINTF("ACPI disabled. Bailing out.\n");
        return;
    }

    build_state = g_malloc0(sizeof *build_state);

    build_state->guest_info = guest_info;

    acpi_set_pci_info();

    acpi_build_tables_init(&tables);
    acpi_build(build_state->guest_info, &tables);

    /* Now expose it all to Guest */
    build_state->table_mr = acpi_add_rom_blob(build_state, tables.table_data,
                                               ACPI_BUILD_TABLE_FILE,
                                               ACPI_BUILD_TABLE_MAX_SIZE);
    assert(build_state->table_mr != NULL);

    build_state->linker_mr =
        acpi_add_rom_blob(build_state, tables.linker, "etc/table-loader", 0);

    fw_cfg_add_file(guest_info->fw_cfg, ACPI_BUILD_TPMLOG_FILE,
                    tables.tcpalog->data, acpi_data_len(tables.tcpalog));

    if (!guest_info->rsdp_in_ram) {
        /*
         * Keep for compatibility with old machine types.
         * Though RSDP is small, its contents isn't immutable, so
         * we'll update it along with the rest of tables on guest access.
         */
        uint32_t rsdp_size = acpi_data_len(tables.rsdp);

        build_state->rsdp = g_memdup(tables.rsdp->data, rsdp_size);
        fw_cfg_add_file_callback(guest_info->fw_cfg, ACPI_BUILD_RSDP_FILE,
                                 acpi_build_update, build_state,
                                 build_state->rsdp, rsdp_size);
        build_state->rsdp_mr = NULL;
    } else {
        build_state->rsdp = NULL;
        build_state->rsdp_mr = acpi_add_rom_blob(build_state, tables.rsdp,
                                                  ACPI_BUILD_RSDP_FILE, 0);
    }

    qemu_register_reset(acpi_build_reset, build_state);
    acpi_build_reset(build_state);
    vmstate_register(NULL, 0, &vmstate_acpi_build, build_state);

    /* Cleanup tables but don't free the memory: we track it
     * in build_state.
     */
    acpi_build_tables_cleanup(&tables, false);
}
