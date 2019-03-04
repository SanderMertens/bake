/* Copyright (c) 2010-2019 Sander Mertens
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
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "bake.h"

int16_t bake_json_set_boolean(
    bool *ptr,
    const char *member,
    JSON_Value *v)
{
    if (json_value_get_type(v) != JSONBoolean) {
        ut_throw("expected boolean for member '%s'", member);
        return -1;
    }

    *ptr = json_value_get_boolean(v);

    return 0;
}

int16_t bake_json_set_string(
    char **ptr,
    const char *member,
    JSON_Value *v)
{
    if (json_value_get_type(v) != JSONString &&
        json_value_get_type(v) != JSONNull)
    {
        ut_throw("expected string for member '%s'", member);
        return -1;
    }
    if (*ptr) {
        free(*ptr);
    }
    const char *str = json_value_get_string(v);
    if (str) {
        *ptr = ut_strdup(str);
    } else {
        *ptr = NULL;
    }
    return 0;
}

int16_t bake_json_set_array(
    ut_ll *ptr,
    const char *member,
    JSON_Value *v)
{
    if (json_value_get_type(v) != JSONArray) {
        ut_throw("expected array for member '%s'", member);
        return -1;
    }

    if (!*ptr) {
        *ptr = ut_ll_new();
    }

    JSON_Array *array = json_value_get_array(v);
    uint32_t i, count = json_array_get_count(array);

    for (i = 0; i < count; i ++) {
        JSON_Value *value = json_array_get_value(array, i);
        if (json_value_get_type(value) != JSONString) {
            ut_throw("array in member '%s' should only contain strings", member);
            return -1;
        }

        const char *json_el = json_array_get_string(array, i);
        ut_ll_append(*ptr, strdup(json_el));
    }

    return 0;
}

JSON_Object* bake_json_find_or_create_object(
    JSON_Object *jsonObject,
    const char *member)
{
    JSON_Object *result = NULL;
    if (json_object_has_value(jsonObject, member)) {
        result = json_object_get_object(jsonObject, member);
        if (!result) {
            /* If JSON has a value member, but wasn't resolved by the previous
             * call, it is not of an object type */
            ut_throw("invalid JSON: expected '%s' to be a JSON object", member);
            goto error;
        }
    } else {
        /* This is not an error, a project file can leave all project
         * settings to defaults. Add a value member. */
        json_object_set_value(
            jsonObject, member, json_value_init_object());
        result = json_object_get_object(jsonObject, member);
    }

    return result;
error:
    return NULL;
}
