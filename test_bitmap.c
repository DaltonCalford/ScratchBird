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

struct bitmap_meta_page {
    struct pag bmp_header;
    ULONG bmp_root_value_page;
    ULONG bmp_total_values;
    ULONG bmp_total_records;
    ULONG bmp_cardinality;
    USHORT bmp_compression_type;
    USHORT bmp_chunk_size;
    ULONG bmp_max_cardinality;
    double bmp_cardinality_ratio;
    UCHAR bmp_flags;
    UCHAR bmp_data_type;
    USHORT bmp_version;
    ULONG bmp_last_maintenance;
    UCHAR bmp_reserved[16];
};

int main() {
    printf("bitmap_meta_page size: %zu\n", sizeof(struct bitmap_meta_page));
    return 0;
}
