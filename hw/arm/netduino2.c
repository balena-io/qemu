/*
 * Netduino 2 Machine Model
 *
 * Copyright (c) 2014 Alistair Francis <alistair@alistair23.me>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "hw/boards.h"
#include "qemu/error-report.h"
#include "hw/arm/stm32f205_soc.h"

static void netduino2_init(MachineState *machine)
{
    DeviceState *dev;
    Error *err = NULL;

    dev = qdev_create(NULL, TYPE_STM32F205_SOC);
    if (machine->kernel_filename) {
        qdev_prop_set_string(dev, "kernel-filename", machine->kernel_filename);
    }
    qdev_prop_set_string(dev, "cpu-model", "cortex-m3");
    object_property_set_bool(OBJECT(dev), true, "realized", &err);
    if (err != NULL) {
        error_report_err(err);
        exit(1);
    }
}

static void netduino2_machine_init(MachineClass *mc)
{
    mc->desc = "Netduino 2 Machine";
    mc->init = netduino2_init;
}

DEFINE_MACHINE("netduino2", netduino2_machine_init)
