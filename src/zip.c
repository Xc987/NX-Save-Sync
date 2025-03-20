#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <switch.h>
#include "main.h"

#define ZIP_LOCAL_FILE_HEADER_SIG 0x04034b50
#define ZIP_VERSION 20

typedef struct {
    uint32_t signature;
    uint16_t version;
    uint16_t bit_flag;
    uint16_t compression;
    uint16_t mod_time;
    uint16_t mod_date;
    uint32_t crc32;
    uint32_t compressed_size;
    uint32_t uncompressed_size;
    uint16_t filename_len;
    uint16_t extra_len;
} __attribute__((packed)) zip_local_file_header;

void writeHeader(FILE *zip, const char *filename, uint32_t size) {
    zip_local_file_header header = {
        .signature = ZIP_LOCAL_FILE_HEADER_SIG,
        .version = ZIP_VERSION,
        .bit_flag = 0,
        .compression = 0,
        .mod_time = 0,
        .mod_date = 0,
        .crc32 = 0,
        .compressed_size = size,
        .uncompressed_size = size,
        .filename_len = strlen(filename),
        .extra_len = 0
    };
    fwrite(&header, sizeof(header), 1, zip);
    fwrite(filename, strlen(filename), 1, zip);
}

void zipFile(FILE *zip, const char *filepath, const char *filename) {
    FILE *file = fopen(filepath, "rb");
    if (!file) return;
    
    fseek(file, 0, SEEK_END);
    uint32_t size = ftell(file);
    rewind(file);
    
    writeHeader(zip, filename, size);
    
    char *buffer = malloc(size);
    fread(buffer, 1, size, file);
    fwrite(buffer, 1, size, zip);
    
    fclose(file);
    free(buffer);
}

void zipDir(FILE *zip, const char *dirpath, const char *basepath) {
    DIR *dir = opendir(dirpath);
    if (!dir) return;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        
        char fullpath[512];
        char relativepath[512];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", dirpath, entry->d_name);
        snprintf(relativepath, sizeof(relativepath), "%s/%s", basepath, entry->d_name);
        
        if (entry->d_type == DT_DIR) {
            zipDir(zip, fullpath, relativepath);
        } else {
            zipFile(zip, fullpath, relativepath);
        }
    }
    closedir(dir);
}