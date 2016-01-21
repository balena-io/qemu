/*
 * Core Definitions for QAPI Visitor Classes
 *
 * Copyright IBM, Corp. 2011
 *
 * Authors:
 *  Anthony Liguori   <aliguori@us.ibm.com>
 *
 * This work is licensed under the terms of the GNU LGPL, version 2.1 or later.
 * See the COPYING.LIB file in the top-level directory.
 *
 */
#ifndef QAPI_VISITOR_CORE_H
#define QAPI_VISITOR_CORE_H

#include "qemu/typedefs.h"
#include "qapi/qmp/qobject.h"
#include "qapi/error.h"
#include <stdlib.h>

typedef struct GenericList
{
    union {
        void *value;
        uint64_t padding;
    };
    struct GenericList *next;
} GenericList;

void visit_start_struct(Visitor *v, void **obj, const char *kind,
                        const char *name, size_t size, Error **errp);
void visit_end_struct(Visitor *v, Error **errp);
void visit_start_implicit_struct(Visitor *v, void **obj, size_t size,
                                 Error **errp);
void visit_end_implicit_struct(Visitor *v, Error **errp);
void visit_start_list(Visitor *v, const char *name, Error **errp);
GenericList *visit_next_list(Visitor *v, GenericList **list, Error **errp);
void visit_end_list(Visitor *v, Error **errp);

/**
 * Check if an optional member @name of an object needs visiting.
 * For input visitors, set *@present according to whether the
 * corresponding visit_type_*() needs calling; for other visitors,
 * leave *@present unchanged.  Return *@present for convenience.
 */
bool visit_optional(Visitor *v, bool *present, const char *name);

/**
 * Determine the qtype of the item @name in the current object visit.
 * For input visitors, set *@type to the correct qtype of a qapi
 * alternate type; for other visitors, leave *@type unchanged.
 * If @promote_int, treat integers as QTYPE_FLOAT.
 */
void visit_get_next_type(Visitor *v, QType *type, bool promote_int,
                         const char *name, Error **errp);
void visit_type_enum(Visitor *v, int *obj, const char * const strings[],
                     const char *kind, const char *name, Error **errp);
void visit_type_int(Visitor *v, int64_t *obj, const char *name, Error **errp);
void visit_type_uint8(Visitor *v, uint8_t *obj, const char *name, Error **errp);
void visit_type_uint16(Visitor *v, uint16_t *obj, const char *name, Error **errp);
void visit_type_uint32(Visitor *v, uint32_t *obj, const char *name, Error **errp);
void visit_type_uint64(Visitor *v, uint64_t *obj, const char *name, Error **errp);
void visit_type_int8(Visitor *v, int8_t *obj, const char *name, Error **errp);
void visit_type_int16(Visitor *v, int16_t *obj, const char *name, Error **errp);
void visit_type_int32(Visitor *v, int32_t *obj, const char *name, Error **errp);
void visit_type_int64(Visitor *v, int64_t *obj, const char *name, Error **errp);
void visit_type_size(Visitor *v, uint64_t *obj, const char *name, Error **errp);
void visit_type_bool(Visitor *v, bool *obj, const char *name, Error **errp);
void visit_type_str(Visitor *v, char **obj, const char *name, Error **errp);
void visit_type_number(Visitor *v, double *obj, const char *name, Error **errp);
void visit_type_any(Visitor *v, QObject **obj, const char *name, Error **errp);
bool visit_start_union(Visitor *v, bool data_present, Error **errp);
void visit_end_union(Visitor *v, bool data_present, Error **errp);

#endif
