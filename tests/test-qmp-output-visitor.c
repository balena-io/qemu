/*
 * QMP Output Visitor unit-tests.
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

#include "qemu-common.h"
#include "qapi/qmp-output-visitor.h"
#include "test-qapi-types.h"
#include "test-qapi-visit.h"
#include "qapi/qmp/types.h"

typedef struct TestOutputVisitorData {
    QmpOutputVisitor *qov;
    Visitor *ov;
} TestOutputVisitorData;

static void visitor_output_setup(TestOutputVisitorData *data,
                                 const void *unused)
{
    data->qov = qmp_output_visitor_new();
    g_assert(data->qov != NULL);

    data->ov = qmp_output_get_visitor(data->qov);
    g_assert(data->ov != NULL);
}

static void visitor_output_teardown(TestOutputVisitorData *data,
                                    const void *unused)
{
    qmp_output_visitor_cleanup(data->qov);
    data->qov = NULL;
    data->ov = NULL;
}

static void test_visitor_out_int(TestOutputVisitorData *data,
                                 const void *unused)
{
    int64_t value = -42;
    QObject *obj;

    visit_type_int(data->ov, &value, NULL, &error_abort);

    obj = qmp_output_get_qobject(data->qov);
    g_assert(obj != NULL);
    g_assert(qobject_type(obj) == QTYPE_QINT);
    g_assert_cmpint(qint_get_int(qobject_to_qint(obj)), ==, value);

    qobject_decref(obj);
}

static void test_visitor_out_bool(TestOutputVisitorData *data,
                                  const void *unused)
{
    bool value = true;
    QObject *obj;

    visit_type_bool(data->ov, &value, NULL, &error_abort);

    obj = qmp_output_get_qobject(data->qov);
    g_assert(obj != NULL);
    g_assert(qobject_type(obj) == QTYPE_QBOOL);
    g_assert(qbool_get_bool(qobject_to_qbool(obj)) == value);

    qobject_decref(obj);
}

static void test_visitor_out_number(TestOutputVisitorData *data,
                                    const void *unused)
{
    double value = 3.14;
    QObject *obj;

    visit_type_number(data->ov, &value, NULL, &error_abort);

    obj = qmp_output_get_qobject(data->qov);
    g_assert(obj != NULL);
    g_assert(qobject_type(obj) == QTYPE_QFLOAT);
    g_assert(qfloat_get_double(qobject_to_qfloat(obj)) == value);

    qobject_decref(obj);
}

static void test_visitor_out_string(TestOutputVisitorData *data,
                                    const void *unused)
{
    char *string = (char *) "Q E M U";
    QObject *obj;

    visit_type_str(data->ov, &string, NULL, &error_abort);

    obj = qmp_output_get_qobject(data->qov);
    g_assert(obj != NULL);
    g_assert(qobject_type(obj) == QTYPE_QSTRING);
    g_assert_cmpstr(qstring_get_str(qobject_to_qstring(obj)), ==, string);

    qobject_decref(obj);
}

static void test_visitor_out_no_string(TestOutputVisitorData *data,
                                       const void *unused)
{
    char *string = NULL;
    QObject *obj;

    /* A null string should return "" */
    visit_type_str(data->ov, &string, NULL, &error_abort);

    obj = qmp_output_get_qobject(data->qov);
    g_assert(obj != NULL);
    g_assert(qobject_type(obj) == QTYPE_QSTRING);
    g_assert_cmpstr(qstring_get_str(qobject_to_qstring(obj)), ==, "");

    qobject_decref(obj);
}

static void test_visitor_out_enum(TestOutputVisitorData *data,
                                  const void *unused)
{
    QObject *obj;
    EnumOne i;

    for (i = 0; i < ENUM_ONE__MAX; i++) {
        visit_type_EnumOne(data->ov, &i, "unused", &error_abort);

        obj = qmp_output_get_qobject(data->qov);
        g_assert(obj != NULL);
        g_assert(qobject_type(obj) == QTYPE_QSTRING);
        g_assert_cmpstr(qstring_get_str(qobject_to_qstring(obj)), ==,
                        EnumOne_lookup[i]);
        qobject_decref(obj);
    }
}

static void test_visitor_out_enum_errors(TestOutputVisitorData *data,
                                         const void *unused)
{
    EnumOne i, bad_values[] = { ENUM_ONE__MAX, -1 };
    Error *err;

    for (i = 0; i < ARRAY_SIZE(bad_values) ; i++) {
        err = NULL;
        visit_type_EnumOne(data->ov, &bad_values[i], "unused", &err);
        g_assert(err);
        error_free(err);
    }
}


static void test_visitor_out_struct(TestOutputVisitorData *data,
                                    const void *unused)
{
    TestStruct test_struct = { .integer = 42,
                               .boolean = false,
                               .string = (char *) "foo"};
    TestStruct *p = &test_struct;
    QObject *obj;
    QDict *qdict;

    visit_type_TestStruct(data->ov, &p, NULL, &error_abort);

    obj = qmp_output_get_qobject(data->qov);
    g_assert(obj != NULL);
    g_assert(qobject_type(obj) == QTYPE_QDICT);

    qdict = qobject_to_qdict(obj);
    g_assert_cmpint(qdict_size(qdict), ==, 3);
    g_assert_cmpint(qdict_get_int(qdict, "integer"), ==, 42);
    g_assert_cmpint(qdict_get_bool(qdict, "boolean"), ==, false);
    g_assert_cmpstr(qdict_get_str(qdict, "string"), ==, "foo");

    QDECREF(qdict);
}

static void test_visitor_out_struct_nested(TestOutputVisitorData *data,
                                           const void *unused)
{
    int64_t value = 42;
    UserDefTwo *ud2;
    QObject *obj;
    QDict *qdict, *dict1, *dict2, *dict3, *userdef;
    const char *string = "user def string";
    const char *strings[] = { "forty two", "forty three", "forty four",
                              "forty five" };

    ud2 = g_malloc0(sizeof(*ud2));
    ud2->string0 = g_strdup(strings[0]);

    ud2->dict1 = g_malloc0(sizeof(*ud2->dict1));
    ud2->dict1->string1 = g_strdup(strings[1]);

    ud2->dict1->dict2 = g_malloc0(sizeof(*ud2->dict1->dict2));
    ud2->dict1->dict2->userdef = g_new0(UserDefOne, 1);
    ud2->dict1->dict2->userdef->string = g_strdup(string);
    ud2->dict1->dict2->userdef->integer = value;
    ud2->dict1->dict2->string = g_strdup(strings[2]);

    ud2->dict1->dict3 = g_malloc0(sizeof(*ud2->dict1->dict3));
    ud2->dict1->has_dict3 = true;
    ud2->dict1->dict3->userdef = g_new0(UserDefOne, 1);
    ud2->dict1->dict3->userdef->string = g_strdup(string);
    ud2->dict1->dict3->userdef->integer = value;
    ud2->dict1->dict3->string = g_strdup(strings[3]);

    visit_type_UserDefTwo(data->ov, &ud2, "unused", &error_abort);

    obj = qmp_output_get_qobject(data->qov);
    g_assert(obj != NULL);
    g_assert(qobject_type(obj) == QTYPE_QDICT);

    qdict = qobject_to_qdict(obj);
    g_assert_cmpint(qdict_size(qdict), ==, 2);
    g_assert_cmpstr(qdict_get_str(qdict, "string0"), ==, strings[0]);

    dict1 = qdict_get_qdict(qdict, "dict1");
    g_assert_cmpint(qdict_size(dict1), ==, 3);
    g_assert_cmpstr(qdict_get_str(dict1, "string1"), ==, strings[1]);

    dict2 = qdict_get_qdict(dict1, "dict2");
    g_assert_cmpint(qdict_size(dict2), ==, 2);
    g_assert_cmpstr(qdict_get_str(dict2, "string"), ==, strings[2]);
    userdef = qdict_get_qdict(dict2, "userdef");
    g_assert_cmpint(qdict_size(userdef), ==, 2);
    g_assert_cmpint(qdict_get_int(userdef, "integer"), ==, value);
    g_assert_cmpstr(qdict_get_str(userdef, "string"), ==, string);

    dict3 = qdict_get_qdict(dict1, "dict3");
    g_assert_cmpint(qdict_size(dict3), ==, 2);
    g_assert_cmpstr(qdict_get_str(dict3, "string"), ==, strings[3]);
    userdef = qdict_get_qdict(dict3, "userdef");
    g_assert_cmpint(qdict_size(userdef), ==, 2);
    g_assert_cmpint(qdict_get_int(userdef, "integer"), ==, value);
    g_assert_cmpstr(qdict_get_str(userdef, "string"), ==, string);

    QDECREF(qdict);
    qapi_free_UserDefTwo(ud2);
}

static void test_visitor_out_struct_errors(TestOutputVisitorData *data,
                                           const void *unused)
{
    EnumOne bad_values[] = { ENUM_ONE__MAX, -1 };
    UserDefOne u = {0};
    UserDefOne *pu = &u;
    Error *err;
    int i;

    for (i = 0; i < ARRAY_SIZE(bad_values) ; i++) {
        err = NULL;
        u.has_enum1 = true;
        u.enum1 = bad_values[i];
        visit_type_UserDefOne(data->ov, &pu, "unused", &err);
        g_assert(err);
        error_free(err);
    }
}


static void test_visitor_out_list(TestOutputVisitorData *data,
                                  const void *unused)
{
    const char *value_str = "list value";
    TestStructList *p, *head = NULL;
    const int max_items = 10;
    bool value_bool = true;
    int value_int = 10;
    QListEntry *entry;
    QObject *obj;
    QList *qlist;
    int i;

    /* Build the list in reverse order... */
    for (i = 0; i < max_items; i++) {
        p = g_malloc0(sizeof(*p));
        p->value = g_malloc0(sizeof(*p->value));
        p->value->integer = value_int + (max_items - i - 1);
        p->value->boolean = value_bool;
        p->value->string = g_strdup(value_str);

        p->next = head;
        head = p;
    }

    visit_type_TestStructList(data->ov, &head, NULL, &error_abort);

    obj = qmp_output_get_qobject(data->qov);
    g_assert(obj != NULL);
    g_assert(qobject_type(obj) == QTYPE_QLIST);

    qlist = qobject_to_qlist(obj);
    g_assert(!qlist_empty(qlist));

    /* ...and ensure that the visitor sees it in order */
    i = 0;
    QLIST_FOREACH_ENTRY(qlist, entry) {
        QDict *qdict;

        g_assert(qobject_type(entry->value) == QTYPE_QDICT);
        qdict = qobject_to_qdict(entry->value);
        g_assert_cmpint(qdict_size(qdict), ==, 3);
        g_assert_cmpint(qdict_get_int(qdict, "integer"), ==, value_int + i);
        g_assert_cmpint(qdict_get_bool(qdict, "boolean"), ==, value_bool);
        g_assert_cmpstr(qdict_get_str(qdict, "string"), ==, value_str);
        i++;
    }
    g_assert_cmpint(i, ==, max_items);

    QDECREF(qlist);
    qapi_free_TestStructList(head);
}

static void test_visitor_out_list_qapi_free(TestOutputVisitorData *data,
                                            const void *unused)
{
    UserDefTwoList *p, *head = NULL;
    const char string[] = "foo bar";
    int i, max_count = 1024;

    for (i = 0; i < max_count; i++) {
        p = g_malloc0(sizeof(*p));
        p->value = g_malloc0(sizeof(*p->value));

        p->value->string0 = g_strdup(string);
        p->value->dict1 = g_new0(UserDefTwoDict, 1);
        p->value->dict1->string1 = g_strdup(string);
        p->value->dict1->dict2 = g_new0(UserDefTwoDictDict, 1);
        p->value->dict1->dict2->userdef = g_new0(UserDefOne, 1);
        p->value->dict1->dict2->userdef->string = g_strdup(string);
        p->value->dict1->dict2->userdef->integer = 42;
        p->value->dict1->dict2->string = g_strdup(string);
        p->value->dict1->has_dict3 = false;

        p->next = head;
        head = p;
    }

    qapi_free_UserDefTwoList(head);
}

static void test_visitor_out_any(TestOutputVisitorData *data,
                                 const void *unused)
{
    QObject *qobj;
    QInt *qint;
    QBool *qbool;
    QString *qstring;
    QDict *qdict;
    QObject *obj;

    qobj = QOBJECT(qint_from_int(-42));
    visit_type_any(data->ov, &qobj, NULL, &error_abort);
    obj = qmp_output_get_qobject(data->qov);
    g_assert(obj != NULL);
    g_assert(qobject_type(obj) == QTYPE_QINT);
    g_assert_cmpint(qint_get_int(qobject_to_qint(obj)), ==, -42);
    qobject_decref(obj);
    qobject_decref(qobj);

    qdict = qdict_new();
    qdict_put(qdict, "integer", qint_from_int(-42));
    qdict_put(qdict, "boolean", qbool_from_bool(true));
    qdict_put(qdict, "string", qstring_from_str("foo"));
    qobj = QOBJECT(qdict);
    visit_type_any(data->ov, &qobj, NULL, &error_abort);
    qobject_decref(qobj);
    obj = qmp_output_get_qobject(data->qov);
    g_assert(obj != NULL);
    qdict = qobject_to_qdict(obj);
    g_assert(qdict);
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
    qobject_decref(obj);
}

static void test_visitor_out_union_flat(TestOutputVisitorData *data,
                                        const void *unused)
{
    QObject *arg;
    QDict *qdict;

    UserDefFlatUnion *tmp = g_malloc0(sizeof(UserDefFlatUnion));
    tmp->enum1 = ENUM_ONE_VALUE1;
    tmp->string = g_strdup("str");
    tmp->u.value1 = g_malloc0(sizeof(UserDefA));
    tmp->integer = 41;
    tmp->u.value1->boolean = true;

    visit_type_UserDefFlatUnion(data->ov, &tmp, NULL, &error_abort);
    arg = qmp_output_get_qobject(data->qov);

    g_assert(qobject_type(arg) == QTYPE_QDICT);
    qdict = qobject_to_qdict(arg);

    g_assert_cmpstr(qdict_get_str(qdict, "enum1"), ==, "value1");
    g_assert_cmpstr(qdict_get_str(qdict, "string"), ==, "str");
    g_assert_cmpint(qdict_get_int(qdict, "integer"), ==, 41);
    g_assert_cmpint(qdict_get_bool(qdict, "boolean"), ==, true);

    qapi_free_UserDefFlatUnion(tmp);
    QDECREF(qdict);
}

static void test_visitor_out_alternate(TestOutputVisitorData *data,
                                       const void *unused)
{
    QObject *arg;
    UserDefAlternate *tmp;

    tmp = g_new0(UserDefAlternate, 1);
    tmp->type = QTYPE_QINT;
    tmp->u.i = 42;

    visit_type_UserDefAlternate(data->ov, &tmp, NULL, &error_abort);
    arg = qmp_output_get_qobject(data->qov);

    g_assert(qobject_type(arg) == QTYPE_QINT);
    g_assert_cmpint(qint_get_int(qobject_to_qint(arg)), ==, 42);

    qapi_free_UserDefAlternate(tmp);
    qobject_decref(arg);

    tmp = g_new0(UserDefAlternate, 1);
    tmp->type = QTYPE_QSTRING;
    tmp->u.s = g_strdup("hello");

    visit_type_UserDefAlternate(data->ov, &tmp, NULL, &error_abort);
    arg = qmp_output_get_qobject(data->qov);

    g_assert(qobject_type(arg) == QTYPE_QSTRING);
    g_assert_cmpstr(qstring_get_str(qobject_to_qstring(arg)), ==, "hello");

    qapi_free_UserDefAlternate(tmp);
    qobject_decref(arg);
}

static void test_visitor_out_empty(TestOutputVisitorData *data,
                                   const void *unused)
{
    QObject *arg;

    arg = qmp_output_get_qobject(data->qov);
    g_assert(qobject_type(arg) == QTYPE_QNULL);
    qobject_decref(arg);
}

static void init_native_list(UserDefNativeListUnion *cvalue)
{
    int i;
    switch (cvalue->type) {
    case USER_DEF_NATIVE_LIST_UNION_KIND_INTEGER: {
        intList **list = &cvalue->u.integer;
        for (i = 0; i < 32; i++) {
            *list = g_new0(intList, 1);
            (*list)->value = i;
            (*list)->next = NULL;
            list = &(*list)->next;
        }
        break;
    }
    case USER_DEF_NATIVE_LIST_UNION_KIND_S8: {
        int8List **list = &cvalue->u.s8;
        for (i = 0; i < 32; i++) {
            *list = g_new0(int8List, 1);
            (*list)->value = i;
            (*list)->next = NULL;
            list = &(*list)->next;
        }
        break;
    }
    case USER_DEF_NATIVE_LIST_UNION_KIND_S16: {
        int16List **list = &cvalue->u.s16;
        for (i = 0; i < 32; i++) {
            *list = g_new0(int16List, 1);
            (*list)->value = i;
            (*list)->next = NULL;
            list = &(*list)->next;
        }
        break;
    }
    case USER_DEF_NATIVE_LIST_UNION_KIND_S32: {
        int32List **list = &cvalue->u.s32;
        for (i = 0; i < 32; i++) {
            *list = g_new0(int32List, 1);
            (*list)->value = i;
            (*list)->next = NULL;
            list = &(*list)->next;
        }
        break;
    }
    case USER_DEF_NATIVE_LIST_UNION_KIND_S64: {
        int64List **list = &cvalue->u.s64;
        for (i = 0; i < 32; i++) {
            *list = g_new0(int64List, 1);
            (*list)->value = i;
            (*list)->next = NULL;
            list = &(*list)->next;
        }
        break;
    }
    case USER_DEF_NATIVE_LIST_UNION_KIND_U8: {
        uint8List **list = &cvalue->u.u8;
        for (i = 0; i < 32; i++) {
            *list = g_new0(uint8List, 1);
            (*list)->value = i;
            (*list)->next = NULL;
            list = &(*list)->next;
        }
        break;
    }
    case USER_DEF_NATIVE_LIST_UNION_KIND_U16: {
        uint16List **list = &cvalue->u.u16;
        for (i = 0; i < 32; i++) {
            *list = g_new0(uint16List, 1);
            (*list)->value = i;
            (*list)->next = NULL;
            list = &(*list)->next;
        }
        break;
    }
    case USER_DEF_NATIVE_LIST_UNION_KIND_U32: {
        uint32List **list = &cvalue->u.u32;
        for (i = 0; i < 32; i++) {
            *list = g_new0(uint32List, 1);
            (*list)->value = i;
            (*list)->next = NULL;
            list = &(*list)->next;
        }
        break;
    }
    case USER_DEF_NATIVE_LIST_UNION_KIND_U64: {
        uint64List **list = &cvalue->u.u64;
        for (i = 0; i < 32; i++) {
            *list = g_new0(uint64List, 1);
            (*list)->value = i;
            (*list)->next = NULL;
            list = &(*list)->next;
        }
        break;
    }
    case USER_DEF_NATIVE_LIST_UNION_KIND_BOOLEAN: {
        boolList **list = &cvalue->u.boolean;
        for (i = 0; i < 32; i++) {
            *list = g_new0(boolList, 1);
            (*list)->value = (i % 3 == 0);
            (*list)->next = NULL;
            list = &(*list)->next;
        }
        break;
    }
    case USER_DEF_NATIVE_LIST_UNION_KIND_STRING: {
        strList **list = &cvalue->u.string;
        for (i = 0; i < 32; i++) {
            *list = g_new0(strList, 1);
            (*list)->value = g_strdup_printf("%d", i);
            (*list)->next = NULL;
            list = &(*list)->next;
        }
        break;
    }
    case USER_DEF_NATIVE_LIST_UNION_KIND_NUMBER: {
        numberList **list = &cvalue->u.number;
        for (i = 0; i < 32; i++) {
            *list = g_new0(numberList, 1);
            (*list)->value = (double)i / 3;
            (*list)->next = NULL;
            list = &(*list)->next;
        }
        break;
    }
    default:
        g_assert_not_reached();
    }
}

static void check_native_list(QObject *qobj,
                              UserDefNativeListUnionKind kind)
{
    QDict *qdict;
    QList *qlist;
    int i;

    g_assert(qobj);
    g_assert(qobject_type(qobj) == QTYPE_QDICT);
    qdict = qobject_to_qdict(qobj);
    g_assert(qdict);
    g_assert(qdict_haskey(qdict, "data"));
    qlist = qlist_copy(qobject_to_qlist(qdict_get(qdict, "data")));

    switch (kind) {
    case USER_DEF_NATIVE_LIST_UNION_KIND_S8:
    case USER_DEF_NATIVE_LIST_UNION_KIND_S16:
    case USER_DEF_NATIVE_LIST_UNION_KIND_S32:
    case USER_DEF_NATIVE_LIST_UNION_KIND_S64:
    case USER_DEF_NATIVE_LIST_UNION_KIND_U8:
    case USER_DEF_NATIVE_LIST_UNION_KIND_U16:
    case USER_DEF_NATIVE_LIST_UNION_KIND_U32:
    case USER_DEF_NATIVE_LIST_UNION_KIND_U64:
        /* all integer elements in JSON arrays get stored into QInts when
         * we convert to QObjects, so we can check them all in the same
         * fashion, so simply fall through here
         */
    case USER_DEF_NATIVE_LIST_UNION_KIND_INTEGER:
        for (i = 0; i < 32; i++) {
            QObject *tmp;
            QInt *qvalue;
            tmp = qlist_peek(qlist);
            g_assert(tmp);
            qvalue = qobject_to_qint(tmp);
            g_assert_cmpint(qint_get_int(qvalue), ==, i);
            qobject_decref(qlist_pop(qlist));
        }
        break;
    case USER_DEF_NATIVE_LIST_UNION_KIND_BOOLEAN:
        for (i = 0; i < 32; i++) {
            QObject *tmp;
            QBool *qvalue;
            tmp = qlist_peek(qlist);
            g_assert(tmp);
            qvalue = qobject_to_qbool(tmp);
            g_assert_cmpint(qbool_get_bool(qvalue), ==, i % 3 == 0);
            qobject_decref(qlist_pop(qlist));
        }
        break;
    case USER_DEF_NATIVE_LIST_UNION_KIND_STRING:
        for (i = 0; i < 32; i++) {
            QObject *tmp;
            QString *qvalue;
            gchar str[8];
            tmp = qlist_peek(qlist);
            g_assert(tmp);
            qvalue = qobject_to_qstring(tmp);
            sprintf(str, "%d", i);
            g_assert_cmpstr(qstring_get_str(qvalue), ==, str);
            qobject_decref(qlist_pop(qlist));
        }
        break;
    case USER_DEF_NATIVE_LIST_UNION_KIND_NUMBER:
        for (i = 0; i < 32; i++) {
            QObject *tmp;
            QFloat *qvalue;
            GString *double_expected = g_string_new("");
            GString *double_actual = g_string_new("");

            tmp = qlist_peek(qlist);
            g_assert(tmp);
            qvalue = qobject_to_qfloat(tmp);
            g_string_printf(double_expected, "%.6f", (double)i / 3);
            g_string_printf(double_actual, "%.6f", qfloat_get_double(qvalue));
            g_assert_cmpstr(double_actual->str, ==, double_expected->str);

            qobject_decref(qlist_pop(qlist));
            g_string_free(double_expected, true);
            g_string_free(double_actual, true);
        }
        break;
    default:
        g_assert_not_reached();
    }
    QDECREF(qlist);
}

static void test_native_list(TestOutputVisitorData *data,
                             const void *unused,
                             UserDefNativeListUnionKind kind)
{
    UserDefNativeListUnion *cvalue = g_new0(UserDefNativeListUnion, 1);
    QObject *obj;

    cvalue->type = kind;
    init_native_list(cvalue);

    visit_type_UserDefNativeListUnion(data->ov, &cvalue, NULL, &error_abort);

    obj = qmp_output_get_qobject(data->qov);
    check_native_list(obj, cvalue->type);
    qapi_free_UserDefNativeListUnion(cvalue);
    qobject_decref(obj);
}

static void test_visitor_out_native_list_int(TestOutputVisitorData *data,
                                             const void *unused)
{
    test_native_list(data, unused, USER_DEF_NATIVE_LIST_UNION_KIND_INTEGER);
}

static void test_visitor_out_native_list_int8(TestOutputVisitorData *data,
                                              const void *unused)
{
    test_native_list(data, unused, USER_DEF_NATIVE_LIST_UNION_KIND_S8);
}

static void test_visitor_out_native_list_int16(TestOutputVisitorData *data,
                                               const void *unused)
{
    test_native_list(data, unused, USER_DEF_NATIVE_LIST_UNION_KIND_S16);
}

static void test_visitor_out_native_list_int32(TestOutputVisitorData *data,
                                               const void *unused)
{
    test_native_list(data, unused, USER_DEF_NATIVE_LIST_UNION_KIND_S32);
}

static void test_visitor_out_native_list_int64(TestOutputVisitorData *data,
                                               const void *unused)
{
    test_native_list(data, unused, USER_DEF_NATIVE_LIST_UNION_KIND_S64);
}

static void test_visitor_out_native_list_uint8(TestOutputVisitorData *data,
                                               const void *unused)
{
    test_native_list(data, unused, USER_DEF_NATIVE_LIST_UNION_KIND_U8);
}

static void test_visitor_out_native_list_uint16(TestOutputVisitorData *data,
                                                const void *unused)
{
    test_native_list(data, unused, USER_DEF_NATIVE_LIST_UNION_KIND_U16);
}

static void test_visitor_out_native_list_uint32(TestOutputVisitorData *data,
                                                const void *unused)
{
    test_native_list(data, unused, USER_DEF_NATIVE_LIST_UNION_KIND_U32);
}

static void test_visitor_out_native_list_uint64(TestOutputVisitorData *data,
                                                const void *unused)
{
    test_native_list(data, unused, USER_DEF_NATIVE_LIST_UNION_KIND_U64);
}

static void test_visitor_out_native_list_bool(TestOutputVisitorData *data,
                                              const void *unused)
{
    test_native_list(data, unused, USER_DEF_NATIVE_LIST_UNION_KIND_BOOLEAN);
}

static void test_visitor_out_native_list_str(TestOutputVisitorData *data,
                                              const void *unused)
{
    test_native_list(data, unused, USER_DEF_NATIVE_LIST_UNION_KIND_STRING);
}

static void test_visitor_out_native_list_number(TestOutputVisitorData *data,
                                                const void *unused)
{
    test_native_list(data, unused, USER_DEF_NATIVE_LIST_UNION_KIND_NUMBER);
}

static void output_visitor_test_add(const char *testpath,
                                    TestOutputVisitorData *data,
                                    void (*test_func)(TestOutputVisitorData *data, const void *user_data))
{
    g_test_add(testpath, TestOutputVisitorData, data, visitor_output_setup,
               test_func, visitor_output_teardown);
}

int main(int argc, char **argv)
{
    TestOutputVisitorData out_visitor_data;

    g_test_init(&argc, &argv, NULL);

    output_visitor_test_add("/visitor/output/int",
                            &out_visitor_data, test_visitor_out_int);
    output_visitor_test_add("/visitor/output/bool",
                            &out_visitor_data, test_visitor_out_bool);
    output_visitor_test_add("/visitor/output/number",
                            &out_visitor_data, test_visitor_out_number);
    output_visitor_test_add("/visitor/output/string",
                            &out_visitor_data, test_visitor_out_string);
    output_visitor_test_add("/visitor/output/no-string",
                            &out_visitor_data, test_visitor_out_no_string);
    output_visitor_test_add("/visitor/output/enum",
                            &out_visitor_data, test_visitor_out_enum);
    output_visitor_test_add("/visitor/output/enum-errors",
                            &out_visitor_data, test_visitor_out_enum_errors);
    output_visitor_test_add("/visitor/output/struct",
                            &out_visitor_data, test_visitor_out_struct);
    output_visitor_test_add("/visitor/output/struct-nested",
                            &out_visitor_data, test_visitor_out_struct_nested);
    output_visitor_test_add("/visitor/output/struct-errors",
                            &out_visitor_data, test_visitor_out_struct_errors);
    output_visitor_test_add("/visitor/output/list",
                            &out_visitor_data, test_visitor_out_list);
    output_visitor_test_add("/visitor/output/any",
                            &out_visitor_data, test_visitor_out_any);
    output_visitor_test_add("/visitor/output/list-qapi-free",
                            &out_visitor_data, test_visitor_out_list_qapi_free);
    output_visitor_test_add("/visitor/output/union-flat",
                            &out_visitor_data, test_visitor_out_union_flat);
    output_visitor_test_add("/visitor/output/alternate",
                            &out_visitor_data, test_visitor_out_alternate);
    output_visitor_test_add("/visitor/output/empty",
                            &out_visitor_data, test_visitor_out_empty);
    output_visitor_test_add("/visitor/output/native_list/int",
                            &out_visitor_data,
                            test_visitor_out_native_list_int);
    output_visitor_test_add("/visitor/output/native_list/int8",
                            &out_visitor_data,
                            test_visitor_out_native_list_int8);
    output_visitor_test_add("/visitor/output/native_list/int16",
                            &out_visitor_data,
                            test_visitor_out_native_list_int16);
    output_visitor_test_add("/visitor/output/native_list/int32",
                            &out_visitor_data,
                            test_visitor_out_native_list_int32);
    output_visitor_test_add("/visitor/output/native_list/int64",
                            &out_visitor_data,
                            test_visitor_out_native_list_int64);
    output_visitor_test_add("/visitor/output/native_list/uint8",
                            &out_visitor_data,
                            test_visitor_out_native_list_uint8);
    output_visitor_test_add("/visitor/output/native_list/uint16",
                            &out_visitor_data,
                            test_visitor_out_native_list_uint16);
    output_visitor_test_add("/visitor/output/native_list/uint32",
                            &out_visitor_data,
                            test_visitor_out_native_list_uint32);
    output_visitor_test_add("/visitor/output/native_list/uint64",
                            &out_visitor_data,
                            test_visitor_out_native_list_uint64);
    output_visitor_test_add("/visitor/output/native_list/bool",
                            &out_visitor_data,
                            test_visitor_out_native_list_bool);
    output_visitor_test_add("/visitor/output/native_list/string",
                            &out_visitor_data,
                            test_visitor_out_native_list_str);
    output_visitor_test_add("/visitor/output/native_list/number",
                            &out_visitor_data,
                            test_visitor_out_native_list_number);

    g_test_run();

    return 0;
}
