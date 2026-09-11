#include <stdio.h>
#include <string>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>

// [header_count] [blocks_start_ptr] [header - token:ptr][token:ptr][token:ptr] | [blockdata:ptr][block:ptr][block:ptr]

namespace vladbase {

const int block_data_size = 128;

struct header_t {
    char token[64] = "";
    int64_t full_data_size = 0;
    int64_t ptr = 0;
    int64_t last_block_ptr = 0;
};

struct prefix_t {
    int64_t headers_count = 32;
    int64_t blocks_start = (headers_count * sizeof(header_t)) + sizeof(prefix_t);
};

struct block_t {
    char data[block_data_size] = "";
    int64_t ptr = -1;
};

struct database {
    std::string database_name;
    FILE* f;
    char* buffer = NULL;
    int block_size = sizeof(block);
    prefix_t prefix;
    block_t block;
    header_t head;
    database(const char* database_name_local) {
        f = fopen(database_name_local, "a"); // create file
        if (f == NULL) {
            printf("Cant create/reopen file\n");
        }
        fclose(f);
        f = fopen(database_name_local, "r+b");
        if (f == NULL) {
            printf("Cant open database %s\n", database_name_local);
            return;
        }
        buffer = (char*)malloc(1024*1024);
        if (buffer == NULL) {
            printf("Cant alloc memory!\n");
            return;
        }
        database_name = database_name_local;
    }
    ~database() {
        if (buffer != NULL) {
            free(buffer);
        }
    }
    /*int dbseek(int, ) {

    }
    int dbread(char*, int, int) {
        
    }
    int dbwrite(char*, int, int) {
        return 0;
    }*/
    long int get_file_length() {
        if (fseek(f, 0, SEEK_END) != 0) {
            printf("fseek error\n");
        }
        long int file_size = ftell(f);
        if (fseek(f, 0, SEEK_SET) != 0) {
            printf("Reset seek err\n");
        }
        return file_size;
    }
    int64_t create_or_get_free_block_ptr(prefix_t& prefix, int header_size) { // make block no more free
        for (int64_t pointer = prefix.blocks_start; ; pointer += block_size) {
            if (pointer == get_file_length()) {
                fseek(f, pointer, SEEK_SET);
                block.ptr = 0;
                fwrite((char*)&block, 1, block_size, f);
                return pointer;
            }
            fseek(f, pointer, SEEK_SET);
            fread((char*)&block, 1, block_size, f);
            if (block.ptr == -1) {
                fseek(f, pointer, SEEK_SET);
                block;      
                block.ptr = 0;
                fwrite((char*)&block, 1, block_size, f);
                return pointer;
            }
        }
        return -1;
    }
    int64_t get_header_offset(const char* token) {
        fseek(f, 0, SEEK_SET);
        fread((char*)&prefix, 1, sizeof(prefix), f);
        
        int64_t read_headers_once_count = 32;
        for (int i = 0; i<prefix.headers_count; i += read_headers_once_count) {
            int64_t offset = sizeof(prefix_t) + (sizeof(head) * i);
            int how_much_headers_read = (prefix.headers_count - read_headers_once_count) <= 0 ? read_headers_once_count : prefix.headers_count;
            fseek(f, offset, SEEK_SET);
            fread(buffer, 1, sizeof(head) * how_much_headers_read, f);
            for (int x = 0; x<how_much_headers_read; ++x) {
                memcpy(&head, buffer+(sizeof(head)*x), sizeof(head));
                if (strcmp(head.token, token) == 0 && head.ptr != 0) {
                    return offset+x*sizeof(head);
                }
            }
            prefix.headers_count -= how_much_headers_read; // dont save it!
        }
        return -1;
    }
    int64_t get_free_header() {
        fseek(f, 0, SEEK_SET);
        fread((char*)&prefix, 1, sizeof(prefix), f);
        
        int64_t read_headers_once_count = 32;
        for (int i = 0; i<prefix.headers_count; i += read_headers_once_count) {
            int64_t offset = sizeof(prefix_t) + (sizeof(head) * i);
            int how_much_headers_read = (prefix.headers_count - read_headers_once_count) <= 0 ? read_headers_once_count : prefix.headers_count;
            fseek(f, offset, SEEK_SET);
            fread(buffer, 1, sizeof(head) * how_much_headers_read, f);
            for (int x = 0; x<how_much_headers_read; ++x) {
                memcpy(&head, buffer+(sizeof(head)*x), sizeof(head));
                if (head.ptr == 0) {
                    return offset+x*sizeof(head);
                }
            }
            prefix.headers_count -= how_much_headers_read; // dont save it!
        }
        return -1;
    }
    void addRecord(const char* token, const char* data, int data_size) {
        int buffer_last_wrote_byte = 0;
        fseek(f, 0, SEEK_SET);
        fread((char*)&prefix, 1, sizeof(prefix), f);

        if (get_file_length() == 0) {
            memcpy(buffer, &prefix, sizeof(prefix));
            buffer_last_wrote_byte += sizeof(prefix);
            head.ptr = 0;
            for (int i = 0; i < prefix.headers_count; ++i) {
                memcpy(buffer+buffer_last_wrote_byte, &head, sizeof(head));
                buffer_last_wrote_byte += sizeof(head);
            }
            fwrite(buffer, 1, buffer_last_wrote_byte, f);
        }

        fseek(f, 0, SEEK_SET);
        fread(buffer, 1, 64, f);
        memcpy((char*)&prefix.headers_count, buffer, sizeof(prefix.headers_count));
        printf("Headers count is %d\n", prefix.headers_count);
        int64_t thisHeaderPtr = get_free_header();
        if (thisHeaderPtr != -1) {
            fseek(f, thisHeaderPtr, SEEK_SET);
            strcpy(head.token, token);

            int alredy_wrote_in_block = 0;
            int64_t first_block_ptr = -1;
            int64_t last_next_block_holder = -1; // block that was next in last iteration
            head.full_data_size = data_size;
            do {
                int64_t pointer = last_next_block_holder == -1 ? create_or_get_free_block_ptr(prefix, sizeof(head)) : last_next_block_holder;
                first_block_ptr = first_block_ptr == -1 ? pointer : first_block_ptr;
                int will_write_bytes = (data_size > block_data_size) ? block_data_size : data_size;
                block.ptr = 0;
                head.last_block_ptr = pointer;
                if (data_size > block_data_size) {
                    last_next_block_holder = create_or_get_free_block_ptr(prefix, sizeof(head));
                    block.ptr = last_next_block_holder;
                }
                memcpy(block.data, data+alredy_wrote_in_block, will_write_bytes);
                fseek(f, pointer, SEEK_SET);
                fwrite((char*)&block, 1, block_size, f);
                alredy_wrote_in_block += will_write_bytes;
                data_size -= will_write_bytes;
                printf("Writing part of data in block 0x%d next block 0x%d\n", pointer, block.ptr);
            } while (data_size > 0);

            head.ptr = first_block_ptr;
            fseek(f, thisHeaderPtr, SEEK_SET);
            fwrite((char*)&head, 1, sizeof(head), f);
            printf("Found free header - [%d]: token \"%s\", ptr: %d\n", thisHeaderPtr/sizeof(head), head.token, head.ptr);
        }
        else {
            fseek(f, prefix.blocks_start, SEEK_SET);
            fread((char*)&block, 1, block_size, f);

            fseek(f, 0, SEEK_END);
            fwrite((char*)&block, 1, block_size, f);

            int64_t headers_end = prefix.headers_count * sizeof(head) + sizeof(prefix);

            fseek(f, headers_end, SEEK_SET);
            int new_blocks = 0;
            while (headers_end + sizeof(head) < (prefix.blocks_start + block_size)) {
                head.ptr = 0;
                fwrite((char*)&head, 1, sizeof(head), f);
                headers_end += sizeof(head);
                new_blocks += 1;
            }

            fseek(f, 0, SEEK_SET);
            prefix.headers_count += new_blocks;
            prefix.blocks_start += block_size;
            fwrite((char*)&prefix, 1, sizeof(prefix), f);

            addRecord(token,data,data_size);
        }
    }
    void removeRecord(const char* token) {
        int64_t token_header = get_header_offset(token);
        if (token_header == -1) {
            return;
        }
        int64_t next = head.ptr;
        head.ptr = 0;
        fseek(f, token_header, SEEK_SET);
        fwrite((char*)&head, 1, sizeof(head), f);

        while (next != 0 && next != -1) {
            printf("Removing 0x%d\n", next);
            fseek(f, next, SEEK_SET);
            fread(&block, 1, sizeof(block_t), f);

            int64_t next_block = block.ptr;
            block.ptr = -1;

            fseek(f, next, SEEK_SET);
            fwrite(&block, 1, sizeof(block_t), f);

            next = next_block;
            printf("Going to %d\n", next);
        }
    }
    int64_t get_full_size(const char* token) {
        int64_t ptr = get_header_offset(token);
        fseek(f, ptr, SEEK_SET);
        fread((char*)&head, 1, sizeof(head), f);
        return head.full_data_size;
    }
    void readRecord(const char* token, char* output, int64_t data_size) {
        int64_t token_header = get_header_offset(token);
        if (token_header == -1) {
            printf("Cant find this token\n");
            return;
        }
        fseek(f, token_header, SEEK_SET);
        fread((char*)&head, 1, sizeof(head), f);
        int64_t next = head.ptr;
        int64_t written_to_file = 0;
        while (next >= 0) {
            fseek(f, next, SEEK_SET);
            fread((char*)&block, 1, block_size, f);
            next = block.ptr;
            int will_be_written = (data_size) > block_data_size ? block_data_size : data_size;
            memcpy(output+written_to_file, block.data, will_be_written);
            written_to_file += will_be_written;
            data_size -= will_be_written;
            if (next == 0 || data_size == 0) {
                break;
            }
        }
    }
    void read_record_with_offset(const char* token, char* output, int64_t offset, int64_t data_size) {
        int64_t token_header = get_header_offset(token);
        if (token_header == -1) {
            printf("Cant find this token\n");
            return;
        }
        fseek(f, token_header, SEEK_SET);
        fread((char*)&head, 1, sizeof(head), f);
        int64_t next = head.ptr;
        int64_t written_to_file = 0;
        int64_t rod_from_record = 0;
        int offset_in_one_block = -1;
        while (next >= 0) {
            fseek(f, next, SEEK_SET);
            fread((char*)&block, 1, block_size, f);
            rod_from_record += block_data_size;
            next = block.ptr;
            if (offset - rod_from_record <= block_data_size) {
                offset_in_one_block = offset_in_one_block == -1 ? offset%block_data_size : 0;
                int will_be_written;
                if (data_size > block_data_size) {
                    will_be_written = block_data_size-offset_in_one_block;
                } else {
                    if (data_size + offset_in_one_block > block_data_size) {
                        will_be_written = data_size - offset_in_one_block;
                    } else {
                        will_be_written = data_size;
                    }
                }
                //printf("Reading %d/%d from 0x%d\n", will_be_written, data_size, offset_in_one_block);
                memcpy(output+written_to_file, block.data+offset_in_one_block, will_be_written);
                written_to_file += will_be_written;
                data_size -= will_be_written;
            }
            if (next == 0 || data_size == 0) {
                break;
            }
        }
    }
    void write_to_end(const char* token, const char* data, int data_size) {
        int64_t token_header = get_header_offset(token);
        if (token_header == -1) {
            printf("Cant find this token\n");
            return;
        }
        fseek(f, token_header, SEEK_SET);
        fread((char*)&head, 1, sizeof(head), f);

        int alredy_wrote_in_block = 0;
        int64_t last_next_block_holder = -1; // block that was next in last iteration
        int offset = head.full_data_size % block_data_size;
        do {
            int64_t pointer = last_next_block_holder == -1 ? head.last_block_ptr : last_next_block_holder;
            int free_space_in_block = block_data_size - offset;
            if (free_space_in_block == 0) {
                pointer = create_or_get_free_block_ptr(prefix, sizeof(head));
                free_space_in_block = block_data_size;
                last_next_block_holder = pointer;
            }
            int will_write_bytes = (data_size > free_space_in_block) ? free_space_in_block : data_size;
            block.ptr = 0;
            head.last_block_ptr = pointer;
            if (data_size > free_space_in_block) {
                last_next_block_holder = create_or_get_free_block_ptr(prefix, sizeof(head));
                block.ptr = last_next_block_holder;
            }
            memcpy(block.data+offset, data+alredy_wrote_in_block, will_write_bytes);
            fseek(f, pointer, SEEK_SET);
            fwrite((char*)&block, 1, block_size, f);
            alredy_wrote_in_block += will_write_bytes;
            data_size -= will_write_bytes;
            offset = 0;
        } while (data_size > 0);
        head.full_data_size += alredy_wrote_in_block;
        head.last_block_ptr = last_next_block_holder == -1 ? head.ptr : last_next_block_holder;
        fseek(f, token_header, SEEK_SET);
        fwrite((char*)&head, 1, sizeof(head), f);
    }
};
} // end of namespace