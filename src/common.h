/* common.h - Last modified: 29-Mar-2023 (kobayasy)
 *
 * Copyright (C) 2018-2023 by Yuichi Kobayashi <kobayasy@kobayasy.com>
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef _INCLUDE_common_h
#define _INCLUDE_common_h

#include <limits.h>
#include <signal.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#define ISERR(_status) ((_status) < 0)
#define ONERR(_status, _error) \
    do { \
        if (ISERR(_status)) { \
            status = (_error); \
            goto error; \
        } \
    } while (0)
#define ISSTOP(_stop) ((_stop) != NULL && *(_stop))
#define ONSTOP(_stop, _error) \
    do { \
        if (ISSTOP(_stop)) { \
            status = (_error); \
            goto error; \
        } \
    } while (0)

#define WRITE(_data, _fd, _write, _status) \
    do { \
        uint8_t _buffer[1+sizeof(intmax_t)]; \
        size_t _size; \
    \
        _size = 0; \
        if ((_data) < 0) { \
            *_buffer = 0x80|0x20; \
            while ((_data) < -1) { \
                _buffer[++_size] = (_data) & 0xff; \
                (_data) >>= 8; \
            } \
        } \
        else { \
            *_buffer = 0x80|0x00; \
            while ((_data) > 0) { \
                _buffer[++_size] = (_data) & 0xff; \
                (_data) >>= 8; \
            } \
        } \
        (_status) = _size; \
        *_buffer |= _size++; \
        if ((_write)((_fd), _buffer, _size) != _size) { \
            (_status) = INT_MIN; \
            break; \
        } \
    } while (0)
#define READ(_data, _fd, _read, _status) \
    do { \
        uint8_t _buffer[1+sizeof(intmax_t)]; \
        size_t _size; \
    \
        if ((_read)((_fd), _buffer, 1) != 1) { \
            (_status) = INT_MIN; \
            break; \
        } \
        if ((*_buffer & 0xc0) != 0x80) { \
            (_data) = *_buffer; \
            (_status) = -1; \
            break; \
        } \
        (_data) = *_buffer & 0x20 ? -1 : 0; \
        _size = *_buffer & 0x1f; \
        (_status) = _size; \
        if (_size > 0) { \
            if ((_read)((_fd), _buffer + 1, _size) != _size) { \
                (_status) = INT_MIN; \
                break; \
            } \
            if (_size > sizeof(_data)) { \
                (_status) = -2; \
                break; \
            } \
            do { \
                (_data) <<= 8; \
                (_data) |= _buffer[_size--]; \
            } while (_size > 0); \
        } \
    } while (0)
extern ssize_t write_size(int fd, const void *buf, size_t count);
extern ssize_t read_size(int fd, void *buf, size_t count);

#define WRITE_ONERR(_data, _fd, _write, _error) \
    do { \
        int _status; \
    \
        WRITE((_data), (_fd), (_write), _status); \
        if (ISERR(_status)) { \
            status = (_error); \
            goto error; \
        } \
    } while (0)
#define READ_ONERR(_data, _fd, _read, _error) \
    do { \
        int _status; \
    \
        READ((_data), (_fd), (_read), _status); \
        if (ISERR(_status)) { \
            status = (_error); \
            goto error; \
        } \
    } while (0)

#define LIST_NEW(_list) \
    do { \
        *(_list)->name = 0; \
        (_list)->next = (_list), (_list)->prev = (_list); \
    } while (0)
#define LIST_DELETE(_p) \
    do { \
        (_p)->prev->next = (_p)->next, (_p)->next->prev = (_p)->prev; \
    } while (0)
#define LIST_INSERT_NEXT(_p, _list) \
    do { \
        (_p)->prev = (_list), (_p)->next = (_list)->next; \
        (_list)->next->prev = (_p), (_list)->next = (_p); \
    } while (0)
#define LIST_INSERT_PREV(_p, _list) \
    do { \
        (_p)->next = (_list), (_p)->prev = (_list)->prev; \
        (_list)->prev->next = (_p), (_list)->prev = (_p); \
    } while (0)
#define LIST_SEEK_NEXT(_p, _name, _seek) \
    do { \
        while (strcmp_next((_p)->next->name, (_name)) <= 0) \
            (_p) = (_p)->next; \
        while (((_seek) = strcmp((_p)->name, (_name))) > 0) \
            (_p) = (_p)->prev; \
    } while (0)
#define LIST_SEEK_PREV(_p, _name, _seek) \
    do { \
        while (strcmp((_p)->prev->name, (_name)) >= 0) \
            (_p) = (_p)->prev; \
        while (((_seek) = strcmp_next((_p)->name, (_name))) < 0) \
            (_p) = (_p)->next; \
    } while (0)
extern int strcmp_next(const char *s1, const char *s2);

typedef enum {
    SETS_1AND2,
    SETS_1NOT2,
    SETS_2NOT1
} SETS;
#define each_next(_LIST) \
int each_next_##_LIST(_LIST *list, \
                      int (*func)(_LIST *p, void *data), \
                                            void *data, \
                      volatile sig_atomic_t *stop ) { \
    int status = INT_MIN; \
    _LIST *p = list; \
    _LIST *next; \
\
    if (*p->name) \
        goto error; \
    p = list->next; \
    while (*p->name) { \
        ONSTOP(stop, -1); \
        next = p->next; \
        status = func(p, data); \
        if (ISERR(status)) \
            goto error; \
        p = next; \
    } \
    status = 0; \
error: \
    return status; \
}
#define each_prev(_LIST) \
int each_prev_##_LIST(_LIST *list, \
                      int (*func)(_LIST *p, void *data), \
                                            void *data, \
                      volatile sig_atomic_t *stop ) { \
    int status = INT_MIN; \
    _LIST *p = list; \
    _LIST *prev; \
\
    if (*p->name) \
        goto error; \
    p = list->prev; \
    while (*p->name) { \
        ONSTOP(stop, -1); \
        prev = p->prev; \
        status = func(p, data); \
        if (ISERR(status)) \
            goto error; \
        p = prev; \
    } \
    status = 0; \
error: \
    return status; \
}
#define sets_next(_LIST) \
int sets_next_##_LIST(_LIST *list1, _LIST *list2, \
                      int (*func)(SETS sets, _LIST *p1, _LIST *p2, void *data), \
                                                                   void *data, \
                      volatile sig_atomic_t *stop ) { \
    int status = INT_MIN; \
    _LIST *p1 = list1; \
    _LIST *p2 = list2; \
    _LIST *next1, *next2; \
    int n; \
\
    if (*p1->name || *p2->name) \
        goto error; \
    p1 = p1->next, p2 = p2->next; \
    while (*p1->name || *p2->name) { \
        ONSTOP(stop, -1); \
        n = strcmp_next(p1->name, p2->name); \
        if (n < 0) { \
            next1 = p1->next; \
            status = func(SETS_1NOT2, p1, p2, data); \
            if (ISERR(status)) \
                goto error; \
            p1 = next1; \
        } \
        else if (n > 0) { \
            next2 = p2->next; \
            status = func(SETS_2NOT1, p1, p2, data); \
            if (ISERR(status)) \
                goto error; \
            p2 = next2; \
        } \
        else { \
            next1 = p1->next, next2 = p2->next; \
            status = func(SETS_1AND2, p1, p2, data); \
            if (ISERR(status)) \
                goto error; \
            p1 = next1, p2 = next2; \
        } \
    } \
    status = 0; \
error: \
    return status; \
}
#define sets_prev(_LIST) \
int sets_prev_##_LIST(_LIST *list1, _LIST *list2, \
                      int (*func)(SETS sets, _LIST *p1, _LIST *p2, void *data), \
                                                                   void *data, \
                      volatile sig_atomic_t *stop ) { \
    int status = INT_MIN; \
    _LIST *p1 = list1; \
    _LIST *p2 = list2; \
    _LIST *prev1, *prev2; \
    int n; \
\
    if (*p1->name || *p2->name) \
        goto error; \
    p1 = p1->prev, p2 = p2->prev; \
    while (*p1->name || *p2->name) { \
        ONSTOP(stop, -1); \
        n = strcmp(p1->name, p2->name); \
        if (n < 0) { \
            prev1 = p1->prev; \
            status = func(SETS_1NOT2, p1, p2, data); \
            if (ISERR(status)) \
                goto error; \
            p1 = prev1; \
        } \
        else if (n > 0) { \
            prev2 = p2->prev; \
            status = func(SETS_2NOT1, p1, p2, data); \
            if (ISERR(status)) \
                goto error; \
            p2 = prev2; \
        } \
        else { \
            prev1 = p1->prev, prev2 = p2->prev; \
            status = func(SETS_1AND2, p1, p2, data); \
            if (ISERR(status)) \
                goto error; \
            p1 = prev1, p2 = prev2; \
        } \
    } \
    status = 0; \
error: \
    return status; \
}

#endif  /* #ifndef _INCLUDE_common_h */
