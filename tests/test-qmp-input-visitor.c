/*
 * QMP Input Visitor unit-tests.
 *
 * Copyright (C) 2011, 2015 Red Hat Inc.
 *
 * Authors:
 *  Luiz Capitulino <lcapitulino@redhat.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#include <glib.h>
#include <stdarg.h>

#include "qemu-common.h"
#include "qapi/qmp-input-visitor.h"
#include "test-qapi-types.h"
#include "test-qapi-visit.h"
#include "qapi/qmp/types.h"

typedef struct TestInputVisitorData {
    QObject *obj;
    QmpInputVisitor *qiv;
} TestInputVisitorData;

static void visitor_input_teardown(TestInputVisitorData *data,
                                   const void *unused)
{
    qobject_decref(data->obj);
    data->obj = NULL;

    if (data->qiv) {
        qmp_input_visitor_cleanup(data->qiv);
        data->qiv = NULL;
    }
}

/* The various test_init functions are provided instead of a test setup
   function so that the JSON string used by the tests are kept in the test
   functions (and not in main()). */
static Visitor *visitor_input_test_init_internal(TestInputVisitorData *data,
                                                 const char *json_string,
                                                 va_list *ap)
{
    Visitor *v;

    visitor_input_teardown(data, NULL);

    data->obj = qobject_from_jsonv(json_string, ap);
    g_assert(data->obj);

    data->qiv = qmp_input_visitor_new(data->obj);
    g_assert(data->qiv);

    v = qmp_input_get_visitor(data->qiv);
    g_assert(v);

    return v;
}

static GCC_FMT_ATTR(2, 3)
Visitor *visitor_input_test_init(TestInputVisitorData *data,
                                 const char *json_string, ...)
{
    Visitor *v;
    va_list ap;

    va_start(ap, json_string);
    v = visitor_input_test_init_internal(data, json_string, &ap);
    va_end(ap);
    return v;
}

/* similar to visitor_input_test_init(), but does not expect a string
 * literal/format json_string argument and so can be used for
 * programatically generated strings (and we can't pass in programatically
 * generated strings via %s format parameters since qobject_from_jsonv()
 * will wrap those in double-quotes and treat the entire object as a
 * string)
 */
static Visitor *visitor_input_test_init_raw(TestInputVisitorData *data,
                                            const char *json_string)
{
    return visitor_input_test_init_internal(data, json_string, NULL);
}

static void test_visitor_in_int(TestInputVisitorData *data,
                                const void *unused)
{
    int64_t res = 0, value = -42;
    Visitor *v;

    v = visitor_input_test_init(data, "%" PRId64, value);

    visit_type_int(v, &res, NULL, &error_abort);
    g_assert_cmpint(res, ==, value);
}

static void test_visitor_in_int_overflow(TestInputVisitorData *data,
                                         const void *unused)
{
    int64_t res = 0;
    Error *err = NULL;
    Visitor *v;

    /* this will overflow a Qint/int64, so should be deserialized into
     * a QFloat/double field instead, leading to an error if we pass it
     * to visit_type_int. confirm this.
     */
    v = visitor_input_test_init(data, "%f", DBL_MAX);

    visit_type_int(v, &res, NULL, &err);
    error_free_or_abort(&err);
}

static void test_visitor_in_bool(TestInputVisitorData *data,
                                 const void *unused)
{
    bool res = false;
    Visitor *v;

    v = visitor_input_test_init(data, "true");

    visit_type_bool(v, &res, NULL, &error_abort);
    g_assert_cmpint(res, ==, true);
}

static void test_visitor_in_number(TestInputVisitorData *data,
                                   const void *unused)
{
    double res = 0, value = 3.14;
    Visitor *v;

    v = visitor_input_test_init(data, "%f", value);

    visit_type_number(v, &res, NULL, &error_abort);
    g_assert_cmpfloat(res, ==, value);
}

static void test_visitor_in_string(TestInputVisitorData *data,
                                   const void *unused)
{
    char *res = NULL, *value = (char *) "Q E M U";
    Visitor *v;

    v = visitor_input_test_init(data, "%s", value);

    visit_type_str(v, &res, NULL, &error_abort);
    g_assert_cmpstr(res, ==, value);

    g_free(res);
}

static void test_visitor_in_enum(TestInputVisitorData *data,
                                 const void *unused)
{
    Visitor *v;
    EnumOne i;

    for (i = 0; EnumOne_lookup[i]; i++) {
        EnumOne res = -1;

        v = visitor_input_test_init(data, "%s", EnumOne_lookup[i]);

        visit_type_EnumOne(v, &res, NULL, &error_abort);
        g_assert_cmpint(i, ==, res);
    }
}


static void test_visitor_in_struct(TestInputVisitorData *data,
                                   const void *unused)
{
    TestStruct *p = NULL;
    Visitor *v;

    v = visitor_input_test_init(data, "{ 'integer': -42, 'boolean': true, 'string': 'foo' }");

    visit_type_TestStruct(v, &p, NULL, &error_abort);
    g_assert_cmpint(p->integer, ==, -42);
    g_assert(p->boolean == true);
    g_assert_cmpstr(p->string, ==, "foo");

    g_free(p->string);
    g_free(p);
}

static void test_visitor_in_struct_nested(TestInputVisitorData *data,
                                          const void *unused)
{
    UserDefTwo *udp = NULL;
    Visitor *v;

    v = visitor_input_test_init(data, "{ 'string0': 'string0', "
                                "'dict1': { 'string1': 'string1', "
                                "'dict2': { 'userdef': { 'integer': 42, "
                                "'string': 'string' }, 'string': 'string2'}}}");

    visit_type_UserDefTwo(v, &udp, NULL, &error_abort);

    g_assert_cmpstr(udp->string0, ==, "string0");
    g_assert_cmpstr(udp->dict1->string1, ==, "string1");
    g_assert_cmpint(udp->dict1->dict2->userdef->integer, ==, 42);
    g_assert_cmpstr(udp->dict1->dict2->userdef->string, ==, "string");
    g_assert_cmpstr(udp->dict1->dict2->string, ==, "string2");
    g_assert(udp->dict1->has_dict3 == false);

    qapi_free_UserDefTwo(udp);
}

static void test_visitor_in_list(TestInputVisitorData *data,
                                 const void *unused)
{
    UserDefOneList *item, *head = NULL;
    Visitor *v;
    int i;

    v = visitor_input_test_init(data, "[ { 'string': 'string0', 'integer': 42 }, { 'string': 'string1', 'integer': 43 }, { 'string': 'string2', 'integer': 44 } ]");

    visit_type_UserDefOneList(v, &head, NULL, &error_abort);
    g_assert(head != NULL);

    for (i = 0, item = head; item; item = item->next, i++) {
        char string[12];

        snprintf(string, sizeof(string), "string%d", i);
        g_assert_cmpstr(item->value->string, ==, string);
        g_assert_cmpint(item->value->integer, ==, 42 + i);
    }

    qapi_free_UserDefOneList(head);
    head = NULL;

    /* An empty list is valid */
    v = visitor_input_test_init(data, "[]");
    visit_type_UserDefOneList(v, &head, NULL, &error_abort);
    g_assert(!head);
}

static void test_visitor_in_any(TestInputVisitorData *data,
                                const void *unused)
{
    QObject *res = NULL;
    Visitor *v;
    QInt *qint;
    QBool *qbool;
    QString *qstring;
    QDict *qdict;
    QObject *qobj;

    v = visitor_input_test_init(data, "-42");
    visit_type_any(v, &res, NULL, &error_abort);
    qint = qobject_to_qint(res);
    g_assert(qint);
    g_assert_cmpint(qint_get_int(qint), ==, -42);
    qobject_decref(res);

    v = visitor_input_test_init(data, "{ 'integer': -42, 'boolean': true, 'string': 'foo' }");
    visit_type_any(v, &res, NULL, &error_abort);
    qdict = qobject_to_qdict(res);
    g_assert(qdict && qdict_size(qdict) == 3);
    qobj = qdict_get(qdict, "integer");
    g_assert(qobj);
    qint = qobject_to_qint(qobj);
    g_assert(qint);
    g_assert_cmpint(qint_get_int(qint), ==, -42);
    qobj = qdict_get(qdict, "boolean");
    g_assert(qobj);
    qbool = qobject_to_qbool(qobj);
    g_assert(qbool);
    g_assert(qbool_get_bool(qbool) == true);
    qobj = qdict_get(qdict, "string");
    g_assert(qobj);
    qstring = qobject_to_qstring(qobj);
    g_assert(qstring);
    g_assert_cmpstr(qstring_get_str(qstring), ==, "foo");
    qobject_decref(res);
}

static void test_visitor_in_union_flat(TestInputVisitorData *data,
                                       const void *unused)
{
    Visitor *v;
    UserDefFlatUnion *tmp;
    UserDefUnionBase *base;

    v = visitor_input_test_init(data,
                                "{ 'enum1': 'value1', "
                                "'integer': 41, "
                                "'string': 'str', "
                                "'boolean': true }");

    visit_type_UserDefFlatUnion(v, &tmp, NULL, &error_abort);
    g_assert_cmpint(tmp->enum1, ==, ENUM_ONE_VALUE1);
    g_assert_cmpstr(tmp->string, ==, "str");
    g_assert_cmpint(tmp->integer, ==, 41);
    g_assert_cmpint(tmp->u.value1->boolean, ==, true);

    base = qapi_UserDefFlatUnion_base(tmp);
    g_assert(&base->enum1 == &tmp->enum1);

    qapi_free_UserDefFlatUnion(tmp);
}

static void test_visitor_in_alternate(TestInputVisitorData *data,
                                      const void *unused)
{
    Visitor *v;
    Error *err = NULL;
    UserDefAlternate *tmp;

    v = visitor_input_test_init(data, "42");
    visit_type_UserDefAlternate(v, &tmp, NULL, &error_abort);
    g_assert_cmpint(tmp->type, ==, QTYPE_QINT);
    g_assert_cmpint(tmp->u.i, ==, 42);
    qapi_free_UserDefAlternate(tmp);

    v = visitor_input_test_init(data, "'string'");
    visit_type_UserDefAlternate(v, &tmp, NULL, &error_abort);
    g_assert_cmpint(tmp->type, ==, QTYPE_QSTRING);
    g_assert_cmpstr(tmp->u.s, ==, "string");
    qapi_free_UserDefAlternate(tmp);

    v = visitor_input_test_init(data, "false");
    visit_type_UserDefAlternate(v, &tmp, NULL, &err);
    error_free_or_abort(&err);
    qapi_free_UserDefAlternate(tmp);
}

static void test_visitor_in_alternate_number(TestInputVisitorData *data,
                                             const void *unused)
{
    Visitor *v;
    Error *err = NULL;
    AltStrBool *asb;
    AltStrNum *asn;
    AltNumStr *ans;
    AltStrInt *asi;
    AltIntNum *ain;
    AltNumInt *ani;

    /* Parsing an int */

    v = visitor_input_test_init(data, "42");
    visit_type_AltStrBool(v, &asb, NULL, &err);
    error_free_or_abort(&err);
    qapi_free_AltStrBool(asb);

    v = visitor_input_test_init(data, "42");
    visit_type_AltStrNum(v, &asn, NULL, &error_abort);
    g_assert_cmpint(asn->type, ==, QTYPE_QFLOAT);
    g_assert_cmpfloat(asn->u.n, ==, 42);
    qapi_free_AltStrNum(asn);

    v = visitor_input_test_init(data, "42");
    visit_type_AltNumStr(v, &ans, NULL, &error_abort);
    g_assert_cmpint(ans->type, ==, QTYPE_QFLOAT);
    g_assert_cmpfloat(ans->u.n, ==, 42);
    qapi_free_AltNumStr(ans);

    v = visitor_input_test_init(data, "42");
    visit_type_AltStrInt(v, &asi, NULL, &error_abort);
    g_assert_cmpint(asi->type, ==, QTYPE_QINT);
    g_assert_cmpint(asi->u.i, ==, 42);
    qapi_free_AltStrInt(asi);

    v = visitor_input_test_init(data, "42");
    visit_type_AltIntNum(v, &ain, NULL, &error_abort);
    g_assert_cmpint(ain->type, ==, QTYPE_QINT);
    g_assert_cmpint(ain->u.i, ==, 42);
    qapi_free_AltIntNum(ain);

    v = visitor_input_test_init(data, "42");
    visit_type_AltNumInt(v, &ani, NULL, &error_abort);
    g_assert_cmpint(ani->type, ==, QTYPE_QINT);
    g_assert_cmpint(ani->u.i, ==, 42);
    qapi_free_AltNumInt(ani);

    /* Parsing a double */

    v = visitor_input_test_init(data, "42.5");
    visit_type_AltStrBool(v, &asb, NULL, &err);
    error_free_or_abort(&err);
    qapi_free_AltStrBool(asb);

    v = visitor_input_test_init(data, "42.5");
    visit_type_AltStrNum(v, &asn, NULL, &error_abort);
    g_assert_cmpint(asn->type, ==, QTYPE_QFLOAT);
    g_assert_cmpfloat(asn->u.n, ==, 42.5);
    qapi_free_AltStrNum(asn);

    v = visitor_input_test_init(data, "42.5");
    visit_type_AltNumStr(v, &ans, NULL, &error_abort);
    g_assert_cmpint(ans->type, ==, QTYPE_QFLOAT);
    g_assert_cmpfloat(ans->u.n, ==, 42.5);
    qapi_free_AltNumStr(ans);

    v = visitor_input_test_init(data, "42.5");
    visit_type_AltStrInt(v, &asi, NULL, &err);
    error_free_or_abort(&err);
    qapi_free_AltStrInt(asi);

    v = visitor_input_test_init(data, "42.5");
    visit_type_AltIntNum(v, &ain, NULL, &error_abort);
    g_assert_cmpint(ain->type, ==, QTYPE_QFLOAT);
    g_assert_cmpfloat(ain->u.n, ==, 42.5);
    qapi_free_AltIntNum(ain);

    v = visitor_input_test_init(data, "42.5");
    visit_type_AltNumInt(v, &ani, NULL, &error_abort);
    g_assert_cmpint(ani->type, ==, QTYPE_QFLOAT);
    g_assert_cmpfloat(ani->u.n, ==, 42.5);
    qapi_free_AltNumInt(ani);
}

static void test_native_list_integer_helper(TestInputVisitorData *data,
                                            const void *unused,
                                            UserDefNativeListUnionKind kind)
{
    UserDefNativeListUnion *cvalue = NULL;
    Visitor *v;
    GString *gstr_list = g_string_new("");
    GString *gstr_union = g_string_new("");
    int i;

    for (i = 0; i < 32; i++) {
        g_string_append_printf(gstr_list, "%d", i);
        if (i != 31) {
            g_string_append(gstr_list, ", ");
        }
    }
    g_string_append_printf(gstr_union,  "{ 'type': '%s', 'data': [ %s ] }",
                           UserDefNativeListUnionKind_lookup[kind],
                           gstr_list->str);
    v = visitor_input_test_init_raw(data,  gstr_union->str);

    visit_type_UserDefNativeListUnion(v, &cvalue, NULL, &error_abort);
    g_assert(cvalue != NULL);
    g_assert_cmpint(cvalue->type, ==, kind);

    switch (kind) {
    case USER_DEF_NATIVE_LIST_UNION_KIND_INTEGER: {
        intList *elem = NULL;
        for (i = 0, elem = cvalue->u.integer; elem; elem = elem->next, i++) {
            g_assert_cmpint(elem->value, ==, i);
        }
        break;
    }
    case USER_DEF_NATIVE_LIST_UNION_KIND_S8: {
        int8List *elem = NULL;
        for (i = 0, elem = cvalue->u.s8; elem; elem = elem->next, i++) {
            g_assert_cmpint(elem->value, ==, i);
        }
        break;
    }
    case USER_DEF_NATIVE_LIST_UNION_KIND_S16: {
        int16List *elem = NULL;
        for (i = 0, elem = cvalue->u.s16; elem; elem = elem->next, i++) {
            g_assert_cmpint(elem->value, ==, i);
        }
        break;
    }
    case USER_DEF_NATIVE_LIST_UNION_KIND_S32: {
        int32List *elem = NULL;
        for (i = 0, elem = cvalue->u.s32; elem; elem = elem->next, i++) {
            g_assert_cmpint(elem->value, ==, i);
        }
        break;
    }
    case USER_DEF_NATIVE_LIST_UNION_KIND_S64: {
        int64List *elem = NULL;
        for (i = 0, elem = cvalue->u.s64; elem; elem = elem->next, i++) {
            g_assert_cmpint(elem->value, ==, i);
        }
        break;
    }
    case USER_DEF_NATIVE_LIST_UNION_KIND_U8: {
        uint8List *elem = NULL;
        for (i = 0, elem = cvalue->u.u8; elem; elem = elem->next, i++) {
            g_assert_cmpint(elem->value, ==, i);
        }
        break;
    }
    case USER_DEF_NATIVE_LIST_UNION_KIND_U16: {
        uint16List *elem = NULL;
        for (i = 0, elem = cvalue->u.u16; elem; elem = elem->next, i++) {
            g_assert_cmpint(elem->value, ==, i);
        }
        break;
    }
    case USER_DEF_NATIVE_LIST_UNION_KIND_U32: {
        uint32List *elem = NULL;
        for (i = 0, elem = cvalue->u.u32; elem; elem = elem->next, i++) {
            g_assert_cmpint(elem->value, ==, i);
        }
        break;
    }
    case USER_DEF_NATIVE_LIST_UNION_KIND_U64: {
        uint64List *elem = NULL;
        for (i = 0, elem = cvalue->u.u64; elem; elem = elem->next, i++) {
            g_assert_cmpint(elem->value, ==, i);
        }
        break;
    }
    default:
        g_assert_not_reached();
    }

    g_string_free(gstr_union, true);
    g_string_free(gstr_list, true);
    qapi_free_UserDefNativeListUnion(cvalue);
}

static void test_visitor_in_native_list_int(TestInputVisitorData *data,
                                            const void *unused)
{
    test_native_list_integer_helper(data, unused,
                                    USER_DEF_NATIVE_LIST_UNION_KIND_INTEGER);
}

static void test_visitor_in_native_list_int8(TestInputVisitorData *data,
                                             const void *unused)
{
    test_native_list_integer_helper(data, unused,
                                    USER_DEF_NATIVE_LIST_UNION_KIND_S8);
}

static void test_visitor_in_native_list_int16(TestInputVisitorData *data,
                                              const void *unused)
{
    test_native_list_integer_helper(data, unused,
                                    USER_DEF_NATIVE_LIST_UNION_KIND_S16);
}

static void test_visitor_in_native_list_int32(TestInputVisitorData *data,
                                              const void *unused)
{
    test_native_list_integer_helper(data, unused,
                                    USER_DEF_NATIVE_LIST_UNION_KIND_S32);
}

static void test_visitor_in_native_list_int64(TestInputVisitorData *data,
                                              const void *unused)
{
    test_native_list_integer_helper(data, unused,
                                    USER_DEF_NATIVE_LIST_UNION_KIND_S64);
}

static void test_visitor_in_native_list_uint8(TestInputVisitorData *data,
                                             const void *unused)
{
    test_native_list_integer_helper(data, unused,
                                    USER_DEF_NATIVE_LIST_UNION_KIND_U8);
}

static void test_visitor_in_native_list_uint16(TestInputVisitorData *data,
                                               const void *unused)
{
    test_native_list_integer_helper(data, unused,
                                    USER_DEF_NATIVE_LIST_UNION_KIND_U16);
}

static void test_visitor_in_native_list_uint32(TestInputVisitorData *data,
                                               const void *unused)
{
    test_native_list_integer_helper(data, unused,
                                    USER_DEF_NATIVE_LIST_UNION_KIND_U32);
}

static void test_visitor_in_native_list_uint64(TestInputVisitorData *data,
                                               const void *unused)
{
    test_native_list_integer_helper(data, unused,
                                    USER_DEF_NATIVE_LIST_UNION_KIND_U64);
}

static void test_visitor_in_native_list_bool(TestInputVisitorData *data,
                                            const void *unused)
{
    UserDefNativeListUnion *cvalue = NULL;
    boolList *elem = NULL;
    Visitor *v;
    GString *gstr_list = g_string_new("");
    GString *gstr_union = g_string_new("");
    int i;

    for (i = 0; i < 32; i++) {
        g_string_append_printf(gstr_list, "%s",
                               (i % 3 == 0) ? "true" : "false");
        if (i != 31) {
            g_string_append(gstr_list, ", ");
        }
    }
    g_string_append_printf(gstr_union,  "{ 'type': 'boolean', 'data': [ %s ] }",
                           gstr_list->str);
    v = visitor_input_test_init_raw(data,  gstr_union->str);

    visit_type_UserDefNativeListUnion(v, &cvalue, NULL, &error_abort);
    g_assert(cvalue != NULL);
    g_assert_cmpint(cvalue->type, ==, USER_DEF_NATIVE_LIST_UNION_KIND_BOOLEAN);

    for (i = 0, elem = cvalue->u.boolean; elem; elem = elem->next, i++) {
        g_assert_cmpint(elem->value, ==, (i % 3 == 0) ? 1 : 0);
    }

    g_string_free(gstr_union, true);
    g_string_free(gstr_list, true);
    qapi_free_UserDefNativeListUnion(cvalue);
}

static void test_visitor_in_native_list_string(TestInputVisitorData *data,
                                               const void *unused)
{
    UserDefNativeListUnion *cvalue = NULL;
    strList *elem = NULL;
    Visitor *v;
    GString *gstr_list = g_string_new("");
    GString *gstr_union = g_string_new("");
    int i;

    for (i = 0; i < 32; i++) {
        g_string_append_printf(gstr_list, "'%d'", i);
        if (i != 31) {
            g_string_append(gstr_list, ", ");
        }
    }
    g_string_append_printf(gstr_union,  "{ 'type': 'string', 'data': [ %s ] }",
                           gstr_list->str);
    v = visitor_input_test_init_raw(data,  gstr_union->str);

    visit_type_UserDefNativeListUnion(v, &cvalue, NULL, &error_abort);
    g_assert(cvalue != NULL);
    g_assert_cmpint(cvalue->type, ==, USER_DEF_NATIVE_LIST_UNION_KIND_STRING);

    for (i = 0, elem = cvalue->u.string; elem; elem = elem->next, i++) {
        gchar str[8];
        sprintf(str, "%d", i);
        g_assert_cmpstr(elem->value, ==, str);
    }

    g_string_free(gstr_union, true);
    g_string_free(gstr_list, true);
    qapi_free_UserDefNativeListUnion(cvalue);
}

#define DOUBLE_STR_MAX 16

static void test_visitor_in_native_list_number(TestInputVisitorData *data,
                                               const void *unused)
{
    UserDefNativeListUnion *cvalue = NULL;
    numberList *elem = NULL;
    Visitor *v;
    GString *gstr_list = g_string_new("");
    GString *gstr_union = g_string_new("");
    int i;

    for (i = 0; i < 32; i++) {
        g_string_append_printf(gstr_list, "%f", (double)i / 3);
        if (i != 31) {
            g_string_append(gstr_list, ", ");
        }
    }
    g_string_append_printf(gstr_union,  "{ 'type': 'number', 'data': [ %s ] }",
                           gstr_list->str);
    v = visitor_input_test_init_raw(data,  gstr_union->str);

    visit_type_UserDefNativeListUnion(v, &cvalue, NULL, &error_abort);
    g_assert(cvalue != NULL);
    g_assert_cmpint(cvalue->type, ==, USER_DEF_NATIVE_LIST_UNION_KIND_NUMBER);

    for (i = 0, elem = cvalue->u.number; elem; elem = elem->next, i++) {
        GString *double_expected = g_string_new("");
        GString *double_actual = g_string_new("");

        g_string_printf(double_expected, "%.6f", (double)i / 3);
        g_string_printf(double_actual, "%.6f", elem->value);
        g_assert_cmpstr(double_expected->str, ==, double_actual->str);

        g_string_free(double_expected, true);
        g_string_free(double_actual, true);
    }

    g_string_free(gstr_union, true);
    g_string_free(gstr_list, true);
    qapi_free_UserDefNativeListUnion(cvalue);
}

static void input_visitor_test_add(const char *testpath,
                                   TestInputVisitorData *data,
                                   void (*test_func)(TestInputVisitorData *data, const void *user_data))
{
    g_test_add(testpath, TestInputVisitorData, data, NULL, test_func,
               visitor_input_teardown);
}

static void test_visitor_in_errors(TestInputVisitorData *data,
                                   const void *unused)
{
    TestStruct *p = NULL;
    Error *err = NULL;
    Visitor *v;
    strList *q = NULL;

    v = visitor_input_test_init(data, "{ 'integer': false, 'boolean': 'foo', "
                                "'string': -42 }");

    visit_type_TestStruct(v, &p, NULL, &err);
    error_free_or_abort(&err);
    /* FIXME - a failed parse should not leave a partially-allocated p
     * for us to clean up; this could cause callers to leak memory. */
    g_assert(p->string == NULL);

    g_free(p->string);
    g_free(p);

    v = visitor_input_test_init(data, "[ '1', '2', false, '3' ]");
    visit_type_strList(v, &q, NULL, &err);
    error_free_or_abort(&err);
    assert(q);
    qapi_free_strList(q);
}

static void test_visitor_in_wrong_type(TestInputVisitorData *data,
                                       const void *unused)
{
    TestStruct *p = NULL;
    Visitor *v;
    strList *q = NULL;
    int64_t i;
    Error *err = NULL;

    /* Make sure arrays and structs cannot be confused */

    v = visitor_input_test_init(data, "[]");
    visit_type_TestStruct(v, &p, NULL, &err);
    error_free_or_abort(&err);
    g_assert(!p);

    v = visitor_input_test_init(data, "{}");
    visit_type_strList(v, &q, NULL, &err);
    error_free_or_abort(&err);
    assert(!q);

    /* Make sure primitives and struct cannot be confused */

    v = visitor_input_test_init(data, "1");
    visit_type_TestStruct(v, &p, NULL, &err);
    error_free_or_abort(&err);
    g_assert(!p);

    v = visitor_input_test_init(data, "{}");
    visit_type_int(v, &i, NULL, &err);
    error_free_or_abort(&err);

    /* Make sure primitives and arrays cannot be confused */

    v = visitor_input_test_init(data, "1");
    visit_type_strList(v, &q, NULL, &err);
    error_free_or_abort(&err);
    assert(!q);

    v = visitor_input_test_init(data, "[]");
    visit_type_int(v, &i, NULL, &err);
    error_free_or_abort(&err);
}

int main(int argc, char **argv)
{
    TestInputVisitorData in_visitor_data;

    g_test_init(&argc, &argv, NULL);

    input_visitor_test_add("/visitor/input/int",
                           &in_visitor_data, test_visitor_in_int);
    input_visitor_test_add("/visitor/input/int_overflow",
                           &in_visitor_data, test_visitor_in_int_overflow);
    input_visitor_test_add("/visitor/input/bool",
                           &in_visitor_data, test_visitor_in_bool);
    input_visitor_test_add("/visitor/input/number",
                           &in_visitor_data, test_visitor_in_number);
    input_visitor_test_add("/visitor/input/string",
                           &in_visitor_data, test_visitor_in_string);
    input_visitor_test_add("/visitor/input/enum",
                           &in_visitor_data, test_visitor_in_enum);
    input_visitor_test_add("/visitor/input/struct",
                           &in_visitor_data, test_visitor_in_struct);
    input_visitor_test_add("/visitor/input/struct-nested",
                           &in_visitor_data, test_visitor_in_struct_nested);
    input_visitor_test_add("/visitor/input/list",
                           &in_visitor_data, test_visitor_in_list);
    input_visitor_test_add("/visitor/input/any",
                           &in_visitor_data, test_visitor_in_any);
    input_visitor_test_add("/visitor/input/union-flat",
                           &in_visitor_data, test_visitor_in_union_flat);
    input_visitor_test_add("/visitor/input/alternate",
                           &in_visitor_data, test_visitor_in_alternate);
    input_visitor_test_add("/visitor/input/errors",
                           &in_visitor_data, test_visitor_in_errors);
    input_visitor_test_add("/visitor/input/wrong-type",
                           &in_visitor_data, test_visitor_in_wrong_type);
    input_visitor_test_add("/visitor/input/alternate-number",
                           &in_visitor_data, test_visitor_in_alternate_number);
    input_visitor_test_add("/visitor/input/native_list/int",
                           &in_visitor_data,
                           test_visitor_in_native_list_int);
    input_visitor_test_add("/visitor/input/native_list/int8",
                           &in_visitor_data,
                           test_visitor_in_native_list_int8);
    input_visitor_test_add("/visitor/input/native_list/int16",
                           &in_visitor_data,
                           test_visitor_in_native_list_int16);
    input_visitor_test_add("/visitor/input/native_list/int32",
                           &in_visitor_data,
                           test_visitor_in_native_list_int32);
    input_visitor_test_add("/visitor/input/native_list/int64",
                           &in_visitor_data,
                           test_visitor_in_native_list_int64);
    input_visitor_test_add("/visitor/input/native_list/uint8",
                           &in_visitor_data,
                           test_visitor_in_native_list_uint8);
    input_visitor_test_add("/visitor/input/native_list/uint16",
                           &in_visitor_data,
                           test_visitor_in_native_list_uint16);
    input_visitor_test_add("/visitor/input/native_list/uint32",
                           &in_visitor_data,
                           test_visitor_in_native_list_uint32);
    input_visitor_test_add("/visitor/input/native_list/uint64",
                           &in_visitor_data,
                           test_visitor_in_native_list_uint64);
    input_visitor_test_add("/visitor/input/native_list/bool",
                           &in_visitor_data, test_visitor_in_native_list_bool);
    input_visitor_test_add("/visitor/input/native_list/str",
                           &in_visitor_data,
                           test_visitor_in_native_list_string);
    input_visitor_test_add("/visitor/input/native_list/number",
                           &in_visitor_data,
                           test_visitor_in_native_list_number);

    g_test_run();

    return 0;
}
