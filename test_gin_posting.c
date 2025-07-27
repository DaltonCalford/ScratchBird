#include <stdio.h>
#include <stddef.h>

typedef unsigned char UCHAR;
typedef unsigned short USHORT;
typedef unsigned int ULONG;

struct pag {
    UCHAR pag_type;
    UCHAR pag_flags;
    USHORT pag_reserved;
    ULONG pag_generation;
    ULONG pag_scn;
    ULONG pag_pageno;
};

struct __attribute__((packed)) gin_posting_page {
    struct pag gin_header;
    ULONG gin_sibling;
    ULONG gin_left_sibling;
    USHORT gin_relation;
    UCHAR gin_id;
    UCHAR gin_compression_type;
    ULONG gin_posting_count;
    ULONG gin_total_records;
    USHORT gin_free_space;
    USHORT gin_largest_posting;
    UCHAR gin_postings[1];
};

int main() {
    printf("gin_posting_page size: %zu\n", sizeof(struct gin_posting_page));
    printf("gin_postings offset: %zu\n", offsetof(struct gin_posting_page, gin_postings));
    return 0;
}
