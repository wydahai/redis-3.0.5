/* SDSLib, A C dynamic strings library
 *
 * Copyright (c) 2006-2010, Salvatore Sanfilippo <antirez at gmail dot com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Redis nor the names of its contributors may be used
 *     to endorse or promote products derived from this software without
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __SDS_H
#define __SDS_H

/**
 * 用于控制SDS内存预分配策略，减少用于扩容而引起的内存重分配次数：
 * 1.若扩容后的空间size=sdshdr.len+addlen小于此值时，直接按照两倍进行扩容：
 *      2 * size；
 * 2.若扩容后的空间size=sdshdr.len+addlen大于等于此值时，多扩容此值这么大的空间：
 *      size + SDS_MAX_PREALLOC
 */
#define SDS_MAX_PREALLOC (1024*1024)

#include <sys/types.h>
#include <stdarg.h>

// SDS类型定义
typedef char *sds;

/**
 * SDS数据结构定义
 *
 * Redis实现了自己的字符串表示
 * Redis中字符串的定义(SDS: simple dynamic string)
 * 在Redis中C字符串只会作为字符串字面值用在一些无须对字符串值进行修改对地方
 * SDS遵循C字符串以空字符('\0')结尾的惯例，好处：可以直接重用一部分C字符串函
 * 数库里的函数，以'\0'结尾对于使用者来说是透明的。
 *
 * sdshdr使用了变长结构体（GNU C支持，标准C不支持），好处：
 *  1.可以节省内存空间，如下面的数据成员buf不占用空间
 *  2.一次分配即可，若buf定义成char *，则需要首先分配结构体，然后再分配
 *    char *，需要两次分配；而变长结构体一次分配即可：
 *    struct sdshdr *p = (struct sdshdr *)malloc(sizeof(sdshdr) + n)，其中n
 *    是buf的长度，同时对应的释放次数也减少
 *
 * SDS相较于C字符串的区别（优势）：
 * 1.获取字符串长度的时间复杂度是O(1)，可以直接通过sdshdr.len获取
 * 2.API安全，不会造成缓冲区溢出(buffer overflow)
 * 3.修改字符串长度N次最多需要执行N次内存重分配
 * 4.可以保存文本或这二进制数据
 * 5.可以使用一部分<string.h>库中的函数
 */
struct sdshdr {
    /**
     * 记录buf数组中已使用的字节数量，等于SDS所保存字符串的长度，此长度不包
     * 含结束符：'\0'
     */
    unsigned int len;
    /**
     * 记录buf数组中未使用字节的数量，通过此值，Redis实现了空间预分配和惰性
     * 空间释放 - 提高字符串扩容（增加或缩减）的性能
     */
    unsigned int free;
    /**
     * 字节数组，用于保存字符串，世界分配的大小：len + free + 1，其中：1是C
     * 字符串中的结束符。
     * 注意：此处使用了变长数组-sdshdr变成了一个变长结构体，成员buf本身不会
     * 占用任何的空间
     */
    char buf[];
};

/**
 * sds的类型本质上是一个char *，但内部是通过struct sdshdr来维护的，二者是如何
 * 进行关联的？二者的关系？
 * SDS对外的接口，参数和返回值都是sds(char *)，实际上返回的是sdshdr.buf，但是
 * SDS的实际分配的内存是：
 *      0x00...00    -- len
 *      0x00...04    -- free
 *      0x00...08    -- buf    <---sds（实际返回的是buf的地址）
 *      0x00.....
 * 实际内存举例：
 *      --> 地址由低至高 -->
 *      ---------------------------------
 *      |len|free|char[0]....char[n]....|
 *      ---------------------------------
 *               |
 *              buf(sds)
 */

// 常数时间获取SDS的长度
static inline size_t sdslen(const sds s) {
    // 因为sds是一个char *，实际上指向的是sdshdr.buf，要想获取sdshdr的地址，需
    // 要减去sdshdr的大小
    struct sdshdr *sh = (void*)(s-(sizeof(struct sdshdr)));
    return sh->len;
}

// 获取SDS的可用空间大小
static inline size_t sdsavail(const sds s) {
    struct sdshdr *sh = (void*)(s-(sizeof(struct sdshdr)));
    return sh->free;
}

sds sdsnewlen(const void *init, size_t initlen);
sds sdsnew(const char *init);
sds sdsempty(void);
size_t sdslen(const sds s);
sds sdsdup(const sds s);
void sdsfree(sds s);
size_t sdsavail(const sds s);
sds sdsgrowzero(sds s, size_t len);
sds sdscatlen(sds s, const void *t, size_t len);
sds sdscat(sds s, const char *t);
sds sdscatsds(sds s, const sds t);
sds sdscpylen(sds s, const char *t, size_t len);
sds sdscpy(sds s, const char *t);

sds sdscatvprintf(sds s, const char *fmt, va_list ap);
#ifdef __GNUC__
sds sdscatprintf(sds s, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));
#else
sds sdscatprintf(sds s, const char *fmt, ...);
#endif

sds sdscatfmt(sds s, char const *fmt, ...);
sds sdstrim(sds s, const char *cset);
void sdsrange(sds s, int start, int end);
void sdsupdatelen(sds s);
void sdsclear(sds s);
int sdscmp(const sds s1, const sds s2);
sds *sdssplitlen(const char *s, int len, const char *sep, int seplen, int *count);
void sdsfreesplitres(sds *tokens, int count);
void sdstolower(sds s);
void sdstoupper(sds s);
sds sdsfromlonglong(long long value);
sds sdscatrepr(sds s, const char *p, size_t len);
sds *sdssplitargs(const char *line, int *argc);
sds sdsmapchars(sds s, const char *from, const char *to, size_t setlen);
sds sdsjoin(char **argv, int argc, char *sep);

/* Low level functions exposed to the user API */
sds sdsMakeRoomFor(sds s, size_t addlen);
void sdsIncrLen(sds s, int incr);
sds sdsRemoveFreeSpace(sds s);
size_t sdsAllocSize(sds s);

#endif
