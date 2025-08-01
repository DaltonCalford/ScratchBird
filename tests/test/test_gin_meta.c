#include <stdio.h>
#include <stddef.h>

typedef unsigned char UCHAR;
typedef unsigned short USHORT;
typedef signed short SSHORT;
typedef unsigned int ULONG;

struct pag {
    UCHAR pag_type;
    UCHAR pag_flags;
    USHORT pag_reserved;
    ULONG pag_generation;
    ULONG pag_scn;
    ULONG pag_pageno;
};

struct __attribute__((packed)) gin_meta_page {
    struct pag gin_header;
    ULONG gin_root_token_page;
    ULONG gin_total_tokens;
    ULONG gin_total_postings;
    ULONG gin_total_records;
    USHORT gin_tokenizer_type;
    USHORT gin_min_token_length;
    USHORT gin_max_token_length;
    UCHAR gin_stop_words_enabled;
    UCHAR gin_stemming_enabled;
    UCHAR gin_compression_type;
    UCHAR gin_flags;
    SSHORT gin_language_id;
    USHORT gin_version;
    ULONG gin_last_cleanup;
    UCHAR gin_reserved[12];
};

int main() {
    printf("gin_meta_page size: %zu\n", sizeof(struct gin_meta_page));
    printf("gin_last_cleanup offset: %zu\n", offsetof(struct gin_meta_page, gin_last_cleanup));
    printf("gin_reserved offset: %zu\n", offsetof(struct gin_meta_page, gin_reserved));
    return 0;
}
