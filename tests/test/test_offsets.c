#include <stdio.h>
#include <stddef.h>

typedef unsigned char UCHAR;
typedef unsigned short USHORT;
typedef unsigned int ULONG;
typedef signed int SLONG;

struct pag {
    UCHAR pag_type;
    UCHAR pag_flags;
    USHORT pag_reserved;
    ULONG pag_generation;
    ULONG pag_scn;
    ULONG pag_pageno;
};

struct __attribute__((packed)) gin_token_page {
    struct pag gin_header;
    ULONG gin_sibling;
    ULONG gin_left_sibling;
    USHORT gin_relation;
    USHORT gin_level;
    UCHAR gin_id;
    UCHAR gin_flags;
    USHORT gin_token_count;
    USHORT gin_free_space;
    ULONG gin_prefix_total;
    USHORT gin_max_token_len;
    UCHAR gin_nodes[1];
};

int main() {
    printf("gin_token_page size: %zu\n", sizeof(struct gin_token_page));
    printf("gin_prefix_total offset: %zu\n", offsetof(struct gin_token_page, gin_prefix_total));
    printf("gin_max_token_len offset: %zu\n", offsetof(struct gin_token_page, gin_max_token_len));
    printf("gin_nodes offset: %zu\n", offsetof(struct gin_token_page, gin_nodes));
    return 0;
}
