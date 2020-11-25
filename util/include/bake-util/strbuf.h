
/** @file
 * @section buffer Strbuf API
 * @brief Utility API for efficient appending to strings.
 */

#ifndef UT_STRBUF_H_
#define UT_STRBUF_H_

#ifdef __cplusplus
extern "C" {
#endif

#define UT_STRBUF_INIT (ut_strbuf){0}
#define UT_STRBUF_ELEMENT_SIZE (511)

/* A buffer builds up a list of elements which individually can be up to N bytes
 * large. While appending, data is added to these elements. More elements are
 * added on the fly when needed. When an application calls ut_strbuf_get, all
 * elements are combined in one string and the element administration is freed.
 *
 * This approach prevents reallocs of large blocks of memory, and therefore
 * copying large blocks of memory when appending to a large buffer. A buffer
 * preallocates some memory for the element overhead so that for small strings
 * there is hardly any overhead, while for large strings the overhead is offset
 * by the reduced time spent on copying memory.
 */

typedef struct ut_strbuf_element {
    bool buffer_embedded;
    uint32_t pos;
    char *buf;
    struct ut_strbuf_element *next;
} ut_strbuf_element;

typedef struct ut_strbuf_element_embedded {
    ut_strbuf_element super;
    char buf[UT_STRBUF_ELEMENT_SIZE + 1];
} ut_strbuf_element_embedded;

typedef struct ut_strbuf_element_str {
    ut_strbuf_element super;
    char *alloc_str;
} ut_strbuf_element_str;

typedef struct ut_strbuf {
    /* When set by an application, append will write to this buffer */
    char *buf;

    /* The maximum number of characters that may be printed */
    uint32_t max;

    /* Size of elements minus current element */
    uint32_t size;

    /* The number of elements in use */
    uint32_t elementCount;

    /* Always allocate at least one element */
    ut_strbuf_element_embedded firstElement;

    /* The current element being appended to */
    ut_strbuf_element *current;
} ut_strbuf;

/* Append format string to a buffer.
 * Returns false when max is reached, true when there is still space */
UT_API
bool ut_strbuf_append(
    ut_strbuf *buffer,
    const char *fmt,
    ...);

/* Append format string with argument list to a buffer.
 * Returns false when max is reached, true when there is still space */
UT_API
bool ut_strbuf_vappend(
    ut_strbuf *buffer,
    const char *fmt,
    va_list args);

/* Append string to buffer.
 * Returns false when max is reached, true when there is still space */
UT_API
bool ut_strbuf_appendstr(
    ut_strbuf *buffer,
    const char *str);

/* Append source buffer to destination buffer.
 * Returns false when max is reached, true when there is still space */
UT_API
bool ut_strbuf_mergebuff(
    ut_strbuf *dst_buffer,
    ut_strbuf *src_buffer);

/* Append string to buffer, transfer ownership to buffer.
 * Returns false when max is reached, true when there is still space */
UT_API
bool ut_strbuf_appendstr_zerocpy(
    ut_strbuf *buffer,
    char *str);

/* Append string to buffer, do not free/modify string.
 * Returns false when max is reached, true when there is still space */
UT_API
bool ut_strbuf_appendstr_zerocpy_const(
    ut_strbuf *buffer,
    const char *str);

/* Append n characters to buffer.
 * Returns false when max is reached, true when there is still space */
UT_API
bool ut_strbuf_appendstrn(
    ut_strbuf *buffer,
    const char *str,
    int64_t n);

/* Return result string (also resets buffer) */
UT_API
char *ut_strbuf_get(
    ut_strbuf *buffer);

/* Reset buffer without returning a string */
UT_API
void ut_strbuf_reset(
    ut_strbuf *buffer);


#ifdef __cplusplus
}
#endif

#endif
