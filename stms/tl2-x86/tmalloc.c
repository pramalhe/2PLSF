/* =============================================================================
 *
 * tmalloc.c
 *
 * Memory allocator with extra metadata for TM.
 *
 * =============================================================================
 *
 * Copyright (C) Stanford University, 2006.  All Rights Reserved.
 * Author: Chi Cao Minh
 *
 * =============================================================================
 */


#include <stdlib.h>
#include "tmalloc.h"


/*
 * Convenience macros for accessing metadata/data.
 *
 * "block" = tmalloc_info_t + "data"
 */
#define BLK2DATA(blk)   ((void*)((char*)(blk) + sizeof(tmalloc_info_t)))
#define DATA2BLK(data)  ((void*)((char*)(data) - sizeof(tmalloc_info_t)))
#define INFO2BLK(info)  ((void*)(info))
#define BLK2INFO(blk)   ((tmalloc_info_t*)(blk))
#define DATA2INFO(data) ((tmalloc_info_t*)DATA2BLK(data))
#define INFO2DATA(info) (BLK2DATA(INFO2BLK(info)))

typedef struct tmalloc_info {
    size_t size;
    char pad[sizeof(long) - sizeof(size_t)];
} tmalloc_info_t;


/* =============================================================================
 * tmalloc_reserve
 * =============================================================================
 */
void*
tmalloc_reserve (size_t size)
{
    void* blockPtr = malloc(sizeof(tmalloc_info_t) + size);

    if (!blockPtr) {
        return NULL;
    }

    tmalloc_info_t* infoPtr = BLK2INFO(blockPtr);
    infoPtr->size = size;

    void* dataPtr = BLK2DATA(blockPtr);

    return dataPtr;
}


/* =============================================================================
 * tmalloc_reserveAgain
 * =============================================================================
 */
void*
tmalloc_reserveAgain (void* ptr, size_t size)
{
    void* blockPtr = DATA2BLK(ptr);

    blockPtr = realloc(blockPtr, (sizeof(tmalloc_info_t) + size));

    if (!blockPtr) {
        return NULL;
    }

    tmalloc_info_t* infoPtr = BLK2INFO(blockPtr);
    infoPtr->size = size;

    void* dataPtr = BLK2DATA(blockPtr);

    return dataPtr;
}


/* =============================================================================
 * tmalloc_release
 * =============================================================================
 */
void
tmalloc_release (void* dataPtr)
{
    void* blockPtr = DATA2BLK(dataPtr);
    free(blockPtr);
}


/* =============================================================================
 * tmalloc_alloc
 * -- Returns NULL if failed
 * =============================================================================
 */
tmalloc_t*
tmalloc_alloc (long initCapacity)
{
    tmalloc_t* tmallocPtr;
    long capacity = ((initCapacity > 1) ? initCapacity : 1);

    tmallocPtr = (tmalloc_t*)malloc(sizeof(tmalloc_t));

    if (tmallocPtr != NULL) {
        tmallocPtr->size = 0;
        tmallocPtr->capacity = capacity;
        tmallocPtr->elements = (void**)malloc(capacity * sizeof(void*));
        if (tmallocPtr->elements == NULL) {
            return NULL;
        }
    }

    return tmallocPtr;
}


/* =============================================================================
 * tmalloc_free
 * =============================================================================
 */
void
tmalloc_free (tmalloc_t* tmallocPtr)
{
    free(tmallocPtr->elements);
    free(tmallocPtr);
}


/* =============================================================================
 * tmalloc_append
 *
 * Returns 0 if fail, else 1
 * =============================================================================
 */
long
tmalloc_append (tmalloc_t* tmallocPtr, void* dataPtr)
{
    if (tmallocPtr->size == tmallocPtr->capacity) {

        /* Allocate more space */
        long newCapacity = tmallocPtr->capacity * 2;
        void** newElements = (void**)malloc(newCapacity * sizeof(void*));
        if (newElements == NULL) {
            return 0;
        }

        /* Copy to new space */
        tmallocPtr->capacity = newCapacity;
        long i;
        for (i = 0; i < tmallocPtr->size; i++) {
            newElements[i] = tmallocPtr->elements[i];
        }
        free(tmallocPtr->elements);
        tmallocPtr->elements = newElements;
    }

    tmallocPtr->elements[tmallocPtr->size++] = dataPtr;

    return 1;
}


/* =============================================================================
 * tmalloc_clear
 * =============================================================================
 */
void
tmalloc_clear (tmalloc_t* tmallocPtr)
{
    tmallocPtr->size = 0;
}


/* =============================================================================
 * tmalloc_releaseAllForward
 *
 * If visit() is NULL, it will not be performed
 * =============================================================================
 */
void
tmalloc_releaseAllForward (tmalloc_t* tmallocPtr, void (*visit)(void*, size_t))
{
    long size = tmallocPtr->size;
    void** elements = tmallocPtr->elements;

    if (visit) {
        long i;
        for (i = 0; i < size; i++) {
            void* dataPtr = elements[i];
            tmalloc_info_t* infoPtr = DATA2INFO(dataPtr);
            size_t dataSize = infoPtr->size;
            visit(dataPtr, dataSize);
            void* blockPtr = INFO2BLK(infoPtr);
            free(blockPtr);
        }
    } else {
        long i;
        for (i = 0; i < size; i++) {
            void* dataPtr = elements[i];
            void* blockPtr = DATA2BLK(dataPtr);
            free(blockPtr);
        }
    }

    tmalloc_clear(tmallocPtr);
}


/* =============================================================================
 * tmalloc_releaseAllReverse
 *
 * If visit() is NULL, it will not be performed
 * =============================================================================
 */
void
tmalloc_releaseAllReverse (tmalloc_t* tmallocPtr, void (*visit)(void*, size_t))
{
    long size = tmallocPtr->size;
    void** elements = tmallocPtr->elements;

    if (visit) {
        long i;
        for (i = (size-1); i >= 0; i--) {
            void* dataPtr = elements[i];
            tmalloc_info_t* infoPtr = DATA2INFO(dataPtr);
            size_t dataSize = infoPtr->size;
            visit(dataPtr, dataSize);
            void* blockPtr = INFO2BLK(infoPtr);
            free(blockPtr);
        }
    } else {
        long i;
        for (i = (size-1); i >= 0; i--) {
            void* dataPtr = elements[i];
            void* blockPtr = DATA2BLK(dataPtr);
            free(blockPtr);
        }
    }

    tmalloc_clear(tmallocPtr);
}


/* =============================================================================
 * TEST_TMALLOC
 * =============================================================================
 */
#ifdef TEST_TMALLOC

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void
visit (void* ptr, size_t size)
{
    printf("ptr 0x%lx -> %lu (%s)\n",
           (unsigned long)ptr, (unsigned long)size, (char*)ptr);
}

int
main ()
{
    char* str1 = "abcdefg";
    char* str2 = "abc_efg";
    long  len = strlen(str1);

    puts("Starting...");

    char* aStr = (char*)tmalloc_reserve(len + 1);
    assert(aStr);
    aStr[len] = '\0';
    strcpy(aStr, str1);
    assert(strcmp(str1, aStr) == 0);

    char* bStr = (char*)tmalloc_reserve(len + 1);
    assert(bStr);
    bStr[len] = '\0';
    strcpy(bStr, aStr);
    assert(strcmp(str1, aStr) == 0);
    strcpy(bStr, str2);
    assert(strcmp(str1, aStr) == 0);
    assert(strcmp(str2, bStr) == 0);
    assert(strcmp(bStr, aStr) != 0);

    char* cStr = (char*)tmalloc_reserve(len + 2);
    assert(cStr);
    cStr[len+1] = '\0';
    strcpy(cStr, aStr);
    assert(strcmp(str1, aStr) == 0);
    strcpy(cStr, str2);
    assert(strcmp(str1, aStr) == 0);
    assert(strcmp(str2, cStr) == 0);
    assert(strcmp(cStr, aStr) != 0);

    tmalloc_t* tmallocPtr = tmalloc_alloc(1);
    assert(tmallocPtr);

    tmalloc_append(tmallocPtr, (void*)aStr);
    tmalloc_append(tmallocPtr, (void*)bStr);
    tmalloc_append(tmallocPtr, (void*)cStr);
    assert(strcmp(tmallocPtr->elements[0], str1) == 0);
    assert(strcmp(tmallocPtr->elements[1], str2) == 0);
    assert(strcmp(tmallocPtr->elements[2], str2) == 0);
    tmalloc_releaseAllForward(tmallocPtr, &visit);

    aStr = (char*)tmalloc_reserve(len + 1);
    assert(aStr);
    aStr[len] = '\0';
    strcpy(aStr, str1);

    bStr = (char*)tmalloc_reserve(len + 1);
    assert(bStr);
    bStr[len] = '\0';
    strcpy(bStr, str2);

    cStr = (char*)tmalloc_reserve(len + 2);
    assert(cStr);
    cStr[len+1] = '\0';
    strcpy(cStr, str2);

    tmalloc_append(tmallocPtr, (void*)aStr);
    tmalloc_append(tmallocPtr, (void*)bStr);
    tmalloc_append(tmallocPtr, (void*)cStr);
    assert(strcmp(tmallocPtr->elements[0], str1) == 0);
    assert(strcmp(tmallocPtr->elements[1], str2) == 0);
    assert(strcmp(tmallocPtr->elements[2], str2) == 0);
    tmalloc_releaseAllReverse(tmallocPtr, &visit);

    void* ptr = tmalloc_reserve(len);
    tmalloc_release(ptr);

    tmalloc_free(tmallocPtr);

    puts("All tests passed.");

    return 0;
}


#endif /* TEST_TMALLOC */


/* =============================================================================
 *
 * End of tmalloc.c
 *
 * =============================================================================
 */
