#ifndef EXEC64_EXEC_UTILITY_H
#define EXEC64_EXEC_UTILITY_H

#include <stdint.h>

#include <stddef.h> // size_t
#include <exec/exec.h>   // Need struct Node and struct Interface

#include <stddef.h> // size_t

// Kernel-owned utility primitives that also back the libc wrappers.
// We rename the internal implementations to k_* to avoid symbol clashes
// with the external libc entry points.
#ifndef COMPILING_LIB
#define strlen k_strlen
#define strcmp k_strcmp
#define strcpy k_strcpy
#define strncpy k_strncpy
#define strcat k_strcat
#define strncat k_strncat
#define strchr k_strchr
#define strrchr k_strrchr
#define memcpy k_memcpy
#define memmove k_memmove
#define memset k_memset
#define isspace k_isspace
#define tolower k_tolower
#define toupper k_toupper
#endif

size_t strlen(const char *s);
char *strcpy(char *dest, const char *src);
char *strncpy(char *dest, const char *src, size_t n);
char *strcat(char *dest, const char *src);
char *strncat(char *dest, const char *src, size_t n);
char *strchr(const char *s, int c);
char *strrchr(const char *s, int c);
int isspace(int c);
int tolower(int c);
int toupper(int c);
void *memcpy(void *dest, const void *src, size_t n);
void *memmove(void *dest, const void *src, size_t n);
void *memset(void *s, int c, size_t n);
int strcmp(const char *s1, const char *s2);
int stricmp(const char *s1, const char *s2);
int strnicmp(const char *s1, const char *s2, size_t n);
int stricmp_n(const char *s1, const char *s2, int n);
uint32_t ToLower(uint32_t c);
uint32_t ToUpper(uint32_t c);
void *SetMem(void *destination, int fillChar, size_t length);
void ClearMem(void *destination, size_t size);
void MoveMem(const void *source, void *destination, size_t size);
void long_to_string(uint32_t n, char *s);
void long_to_string64(uint64_t n, char *s);
void hex_to_string(uint64_t n, char *s);
uint64_t string_to_hex(const char *s);
void sys_yield(void);

// --- TAG ITEMS ---
struct TagItem {
    uint32_t  ti_Tag;
    uintptr_t ti_Data;
};

#define TAG_DONE   0
#define TAG_END    0 // Alias
#define TAG_IGNORE 1
#define TAG_MORE   2
#define TAG_SKIP   3

#define TAG_USER   0x80000000

enum TagFilterLogic {
    TAGFILTER_AND = 0,
    TAGFILTER_NOT = 1
};

enum TagMapType {
    MAP_REMOVE_NOT_FOUND = 0,
    MAP_KEEP_NOT_FOUND   = 1
};

struct NamedObject {
    void *no_Object;
};

enum AllocNamedObjectTags {
    ANO_NameSpace = 4000,
    ANO_UserSpace = 4001,
    ANO_Priority  = 4002,
    ANO_Flags     = 4003
};

enum ANOFlagBits {
    NSB_NODUPS = 0,
    NSB_CASE   = 1
};

enum ANOFlags {
    NSF_NODUPS = (1u << NSB_NODUPS),
    NSF_CASE   = (1u << NSB_CASE)
};

struct TagItem *AllocateTagItems(uint32_t numTags);
void ApplyTagChanges(struct TagItem *list, const struct TagItem *changeList);
void FilterTagChanges(struct TagItem *changeList, struct TagItem *originalList, uint32_t apply);
struct TagItem *CloneTagItems(const struct TagItem *original);
void FreeTagItems(struct TagItem *tagList);
void MapTags(struct TagItem *tagList, const struct TagItem *mapList, uint32_t mapType);
uint32_t PackBoolTags(uint32_t initialFlags, const struct TagItem *tagList, const struct TagItem *boolMap);
void RefreshTagItemClones(struct TagItem *clone, const struct TagItem *original);
uint32_t FilterTagItems(struct TagItem *tagList, const uint32_t *filterArray, uint32_t logic);
int TagInArray(uint32_t tagValue, const uint32_t *tagArray);

// --- HOOKS ---
struct Hook {
    struct Node h_MinNode;
    uint32_t  (*h_Entry)(struct Hook *hook, void *object, void *message);
    uint32_t  (*h_SubEntry)(struct Hook *hook, void *object, void *message);
    void       *h_Data;
};

uint32_t CallHookPkt(struct Hook *hook, void *object, void *message);
uint32_t CallHook(struct Hook *hook, void *object, ...);
struct Node *FindNameNC(struct List *start, const char *name);
uint32_t GetUniqueID(void);
struct NamedObject *AllocNamedObjectA(const char *name, const struct TagItem *tags);
void FreeNamedObject(struct NamedObject *object);
char *NamedObjectName(struct NamedObject *object);

// --- UTILITY LIBRARY INTERFACE ---
// Include Library definitions if not present (exec.h usually handles it but we need the struct)
struct UtilityInterface {
    struct Interface i;
    
    // Tags
    struct TagItem* (*AllocateTagItems)(uint32_t numTags);
    void            (*ApplyTagChanges)(struct TagItem *list, const struct TagItem *changeList);
    void            (*FilterTagChanges)(struct TagItem *changeList, struct TagItem *originalList, uint32_t apply);
    struct TagItem* (*CloneTagItems)(const struct TagItem *original);
    void            (*FreeTagItems)(struct TagItem *tagList);
    void            (*MapTags)(struct TagItem *tagList, const struct TagItem *mapList, uint32_t mapType);
    uint32_t        (*PackBoolTags)(uint32_t initialFlags, const struct TagItem *tagList, const struct TagItem *boolMap);
    void            (*RefreshTagItemClones)(struct TagItem *clone, const struct TagItem *original);
    uint32_t        (*FilterTagItems)(struct TagItem *tagList, const uint32_t *filterArray, uint32_t logic);
    struct TagItem* (*NextTagItem)(struct TagItem **tagListPtr);
    struct TagItem* (*FindTagItem)(uint32_t tagValue, const struct TagItem *tagList);
    uintptr_t       (*GetTagData)(uint32_t tagValue, uintptr_t defaultVal, const struct TagItem *tagList);
    
    // Hooks
    uint32_t        (*CallHookPkt)(struct Hook *hook, void *object, void *message);
    uint32_t        (*CallHook)(struct Hook *hook, void *object, ...);
    struct Node*    (*FindNameNC)(struct List *start, const char *name);
    uint32_t        (*GetUniqueID)(void);
    struct NamedObject* (*AllocNamedObjectA)(const char *name, const struct TagItem *tags);
    void            (*FreeNamedObject)(struct NamedObject *object);
    char*           (*NamedObjectName)(struct NamedObject *object);
    
    // String
    int             (*Stricmp)(const char *s1, const char *s2);
    int             (*Strnicmp)(const char *s1, const char *s2, size_t n);
    uint32_t        (*ToLower)(uint32_t c);
    uint32_t        (*ToUpper)(uint32_t c);
    void*           (*SetMem)(void *destination, int fillChar, size_t length);
    void            (*ClearMem)(void *destination, size_t size);
    void            (*MoveMem)(const void *source, void *destination, size_t size);
    int             (*TagInArray)(uint32_t tagValue, const uint32_t *tagArray);
};

#endif
