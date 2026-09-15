#include <stdio.h>
#include <string>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

// [header_count] [blocks_start_ptr] [header - token:ptr][token:ptr][token:ptr] | [blockdata:ptr][block:ptr][block:ptr]
// Token is unique byte array with size token_max_size

namespace vladbase {
const int token_max_size = 64;
const int block_data_size = 128;

struct header_t {
	char token[token_max_size] = "";
	int64_t full_data_size = 0;
	int64_t ptr = 0;
	int64_t last_block_ptr = 0;
};

struct prefix_t {
	int64_t headers_count = 32;
	int64_t blocks_start;
};

struct block_t {
	char data[block_data_size] = "";
    int64_t ptr = -1;
};

struct database {
	FILE* f;
	char* buffer = NULL;
	prefix_t prefix;
	block_t block;
	header_t head;

    public:
	database(const char* database_name_local);
	~database();
	int add_record(const char* token, const char* data, int data_size);
	int write_to_record_end(const char* token, const char* data, int data_size);
	int remove_record(const char* token);
	int read_record(const char* token, char* output, int64_t data_size);
	int read_record_with_offset(const char* token, char* output, int64_t offset, int64_t data_size);
	void print_data_base();
	bool is_record_exitst(const char* token);
    int64_t get_record_full_size(const char* token);

    private:
    int64_t get_file_length();
    int64_t create_or_get_free_block_ptr(prefix_t& prefix, int header_size);
    int64_t get_header_offset(const char* token, int64_t ptr_in_head = -1);
    int64_t get_free_header();
};
} // end of namespace
