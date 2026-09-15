#include "vladbase.h"

vladbase::database::database(const char* database_name_local) {
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
	
	int buffer_last_wrote_byte = 0;
	if (get_file_length() == 0) {
		prefix.blocks_start = (prefix.headers_count * sizeof(header_t)) + sizeof(prefix_t);
		memcpy(buffer, &prefix, sizeof(prefix));
		buffer_last_wrote_byte += sizeof(prefix);
		head.ptr = 0;
		for (int i = 0; i < prefix.headers_count; ++i) {
			memcpy(buffer+buffer_last_wrote_byte, &head, sizeof(head));
			buffer_last_wrote_byte += sizeof(head);
		}
		fwrite(buffer, 1, buffer_last_wrote_byte, f);
	}
}
vladbase::database::~database() {
	if (buffer != NULL) {
		free(buffer);
	}
}
long int vladbase::database::get_file_length() {
	if (fseek(f, 0, SEEK_END) != 0) {
		printf("fseek error\n");
	}
	long int file_size = ftell(f);
	if (fseek(f, 0, SEEK_SET) != 0) {
		printf("Reset seek err\n");
	}
	return file_size;
}
int64_t vladbase::database::create_or_get_free_block_ptr(prefix_t& prefix, int header_size) { // make block no more free
	for (int64_t pointer = prefix.blocks_start; ; pointer += sizeof(block)) {
		if (pointer == get_file_length()) {
			fseek(f, pointer, SEEK_SET);
			block.ptr = 0;
			fwrite((char*)&block, 1, sizeof(block), f);
			return pointer;
		}
		fseek(f, pointer, SEEK_SET);
		fread((char*)&block, 1, sizeof(block), f);
		if (block.ptr == -1) {
			fseek(f, pointer, SEEK_SET);
			block;	  
			block.ptr = 0;
			fwrite((char*)&block, 1, sizeof(block), f);
			return pointer;
		}
	}
	return -1;
}
int64_t vladbase::database::get_header_offset(const char* token, int64_t ptr_in_head) { // get head ptr by token or ptr inside
	fseek(f, 0, SEEK_SET);
	fread((char*)&prefix, 1, sizeof(prefix), f);
	
	int64_t read_headers_once_count = 32;
	int remain_headers = prefix.headers_count;
	for (int i = 0; i<prefix.headers_count; i += read_headers_once_count) {
		int64_t offset = sizeof(prefix_t) + (sizeof(head) * i);
		int how_much_headers_read = remain_headers > read_headers_once_count ? read_headers_once_count : remain_headers;
		fseek(f, offset, SEEK_SET);
		fread(buffer, 1, sizeof(head) * how_much_headers_read, f);
		for (int x = 0; x<how_much_headers_read; ++x) {
			memcpy(&head, buffer+(sizeof(head)*x), sizeof(head));
			if (token != NULL) {
				if (strncmp(head.token, token, token_max_size) == 0 && head.ptr != 0) {
					return offset+x*sizeof(head);
				}
			} else if (ptr_in_head != -1) {
				if (head.ptr == ptr_in_head) {
					return offset+x*sizeof(head);
				}
			}
		}
		remain_headers -= how_much_headers_read;
	}
	return -1;
}
int64_t vladbase::database::get_free_header() {
	fseek(f, 0, SEEK_SET);
	fread((char*)&prefix, 1, sizeof(prefix), f);

	int64_t read_headers_once_count = 32;
	int remain_headers = prefix.headers_count;
	for (int i = 0; i<prefix.headers_count; i += read_headers_once_count) {
		int64_t offset = sizeof(prefix_t) + (sizeof(head) * i);
		int how_much_headers_read = remain_headers > read_headers_once_count ? read_headers_once_count : remain_headers;
		fseek(f, offset, SEEK_SET);
		fread(buffer, 1, sizeof(head) * how_much_headers_read, f);
		for (int x = 0; x<how_much_headers_read; ++x) {
			//printf("This header num is %d\n", i + x);
			memcpy(&head, buffer+(sizeof(head)*x), sizeof(head));
			if (head.ptr == 0) {
				return offset+x*sizeof(head);
			}
		}
		remain_headers -= how_much_headers_read;
	}
	return -1;
}
int vladbase::database::add_record(const char* token, const char* data, int data_size) {
	_start:
	if (data_size <= 0) {
		return -2;
	}
	fseek(f, 0, SEEK_SET);
	fread((char*)&prefix, 1, sizeof(prefix), f);
	//printf("Headers count is %d\n", prefix.headers_count);
	int64_t thisHeaderPtr = get_free_header();
	if (thisHeaderPtr != -1) {
		fseek(f, thisHeaderPtr, SEEK_SET);
		memcpy(head.token, token, token_max_size);

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
			fwrite((char*)&block, 1, sizeof(block), f);
			alredy_wrote_in_block += will_write_bytes;
			data_size -= will_write_bytes;
			//printf("Writing part of data in block 0x%d next block 0x%d\n", pointer, block.ptr);
		} while (data_size > 0);

		head.ptr = first_block_ptr;
		fseek(f, thisHeaderPtr, SEEK_SET);
		fwrite((char*)&head, 1, sizeof(head), f);
		return 0;
		//printf("Found free header - [%d]: token \"%s\", ptr: %d\n", thisHeaderPtr/sizeof(head), head.token, head.ptr);
	}
	else {
		// to move we need clear first block and move its data to end, then change prewious block ptr to new
		//printf("Moving first block\n");
		// get first block
		fseek(f, 0, SEEK_SET);
		fread((char*)&prefix, 1, sizeof(prefix), f);
		fseek(f, prefix.blocks_start, SEEK_SET);
		fread((char*)&block, 1, sizeof(block), f);

		// move to end
		int64_t file_size = get_file_length();
		fseek(f, file_size, SEEK_SET);
		fwrite((char*)&block, 1, sizeof(block), f);
		//printf("Moved block %d to %d\n", prefix.blocks_start, file_size);

		// change ptr's
		int head_point_to_removed_block = get_header_offset(NULL, prefix.blocks_start);
		if (head_point_to_removed_block != -1) { // its first data block
			fseek(f, head_point_to_removed_block, SEEK_SET);
			fread((char*)&head, 1, sizeof(head), f);
			head.ptr = file_size;
			fseek(f, head_point_to_removed_block, SEEK_SET);
			fwrite((char*)&head, 1, sizeof(head), f);
		} else { // its not first data block, finding in blocks space
			for (int64_t block_may_use_first_block = prefix.blocks_start + sizeof(block); block_may_use_first_block < file_size; block_may_use_first_block += sizeof(block)) {
				fseek(f, block_may_use_first_block, SEEK_SET);
				fread((char*)&block, 1, sizeof(block), f);
				if (block.ptr == prefix.blocks_start) {
					block.ptr = file_size;
					fseek(f, block_may_use_first_block, SEEK_SET);
					fwrite((char*)&block, 1, sizeof(block), f);
					break;
				}
			}
		}

		// clear space of removed block
		fseek(f, prefix.blocks_start, SEEK_SET);
		memset(&block, 0, sizeof(block_t));
		fwrite((char*)&block, 1, sizeof(block), f);

		int64_t headers_end = prefix.headers_count * sizeof(head) + sizeof(prefix);


		fseek(f, headers_end, SEEK_SET);
		int new_headers = 0;
		while (headers_end + sizeof(head) < (prefix.blocks_start + sizeof(block))) {
			head.ptr = 0;
			fwrite((char*)&head, 1, sizeof(head), f);
			headers_end += sizeof(head);
			new_headers += 1;
		}

		fseek(f, 0, SEEK_SET);
		prefix.headers_count += new_headers;
		prefix.blocks_start += sizeof(block);
		//printf("Now blocks start is %d\n", prefix.blocks_start);
		fwrite((char*)&prefix, 1, sizeof(prefix), f);

		goto _start;
	}
	return 0;
}
int vladbase::database::remove_record(const char* token) {
	int64_t token_header = get_header_offset(token);
	if (token_header == -1) {
		return -1;
	}
	int64_t next = head.ptr;
	head.ptr = 0;
	fseek(f, token_header, SEEK_SET);
	fwrite((char*)&head, 1, sizeof(head), f);

	while (next != 0 && next != -1) {
		//printf("Removing 0x%d\n", next);
		fseek(f, next, SEEK_SET);
		fread(&block, 1, sizeof(block_t), f);

		int64_t next_block = block.ptr;
		block.ptr = -1;

		fseek(f, next, SEEK_SET);
		fwrite(&block, 1, sizeof(block_t), f);

		next = next_block;
		//printf("Going to %d\n", next);
	}
	return 0;
}
int64_t vladbase::database::get_record_full_size(const char* token) {
	int64_t ptr = get_header_offset(token);
	if (ptr == -1) {
		return -1;
	}
	fseek(f, ptr, SEEK_SET);
	fread((char*)&head, 1, sizeof(head), f);
	return head.full_data_size;
}
int vladbase::database::read_record(const char* token, char* output, int64_t data_size) {
	int64_t token_header = get_header_offset(token);
	if (token_header == -1) {
		return -1;
	}
	fseek(f, token_header, SEEK_SET);
	fread((char*)&head, 1, sizeof(head), f);
	int64_t next = head.ptr;
	int64_t written_to_output = 0;
	while (next >= 0) {
		fseek(f, next, SEEK_SET);
		fread((char*)&block, 1, sizeof(block), f);
		next = block.ptr;
		int will_be_written = (data_size) > block_data_size ? block_data_size : data_size;
		memcpy(output+written_to_output, block.data, will_be_written);
		written_to_output += will_be_written;
		data_size -= will_be_written;
		if (next == 0 || data_size == 0) {
			break;
		}
	}
	return 0;
}
int vladbase::database::read_record_with_offset(const char* token, char* output, int64_t offset, int64_t data_size) {
	int64_t token_header = get_header_offset(token);
	if (token_header == -1) {
		return -1;
	}
	fseek(f, token_header, SEEK_SET);
	fread((char*)&head, 1, sizeof(head), f);
	int64_t next = head.ptr;
	int64_t written_to_output = 0;
	int64_t rod_from_record = 0;
	int offset_in_one_block = -1;
	while (next >= 0) {
		fseek(f, next, SEEK_SET);
		fread((char*)&block, 1, sizeof(block), f);
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
			memcpy(output+written_to_output, block.data+offset_in_one_block, will_be_written);
			written_to_output += will_be_written;
			data_size -= will_be_written;
		}
		if (next == 0 || data_size == 0) {
			break;
		}
	}
	if (written_to_output < data_size) {
		return -4;
	}
	return 0;
}
int vladbase::database::write_to_record_end(const char* token, const char* data, int data_size) {
	int64_t token_header = get_header_offset(token);
	if (token_header == -1) {
		return -1;
	}
	fseek(f, token_header, SEEK_SET);
	fread((char*)&head, 1, sizeof(head), f);

	int alredy_wrote_in_block = 0;
	int64_t last_next_block_holder = -1; // block that was next in last iteration
	int offset = head.full_data_size % block_data_size;
	if (offset == 0) {
		offset = block_data_size;
	}
	while (data_size > 0) {
		int64_t pointer = last_next_block_holder == -1 ? head.last_block_ptr : last_next_block_holder;
		int free_space_in_block = block_data_size - offset;
		if (data_size > free_space_in_block) {
			last_next_block_holder = create_or_get_free_block_ptr(prefix, sizeof(head));
		}
		if (offset > 0) {
			fseek(f, pointer, SEEK_SET);
			fread((char*)&block, 1, sizeof(block), f);
		}
		int will_write_bytes = (data_size > free_space_in_block) ? free_space_in_block : data_size;
		memcpy(block.data+offset, data+alredy_wrote_in_block, will_write_bytes);
		block.ptr = last_next_block_holder == pointer || last_next_block_holder == -1 ? 0 : last_next_block_holder;
		fseek(f, pointer, SEEK_SET);
		fwrite((char*)&block, 1, sizeof(block), f);
		alredy_wrote_in_block += will_write_bytes;
		data_size -= will_write_bytes;
		offset = 0;
	}
	head.full_data_size += alredy_wrote_in_block;
	head.last_block_ptr = last_next_block_holder == -1 ? head.last_block_ptr : last_next_block_holder;
	fseek(f, token_header, SEEK_SET);
	fwrite((char*)&head, 1, sizeof(head), f);
	return 0;
}
void vladbase::database::print_data_base() {
	fseek(f, 0, SEEK_SET);
	fread((char*)&prefix, 1, sizeof(prefix), f);
	printf("Printing FULL DATABASE [%d headers]\n", prefix.headers_count);
	for (int i = 0; i<prefix.headers_count; ++i) {
		fseek(f, sizeof(prefix) + sizeof(head) * i, SEEK_SET);
		fread((char*)&head, 1, sizeof(head), f);
		int64_t next = head.ptr;
		if (next == 0) {
			continue;
		}
		printf("Record #%d (size %d) \"[%s]\" | ", i, head.full_data_size, head.token);
		int data_remain = head.full_data_size;
		printf("{");
		while (data_remain > 0) {
			fseek(f, next, SEEK_SET);
			fread((char*)&block, 1, sizeof(block), f);
			int will_be_read = data_remain > block_data_size ? block_data_size : data_remain;
			next = block.ptr;
			for (int z = 0; z<will_be_read; ++z) {
				printf("%c", block.data[z]);
			}
			data_remain -= will_be_read;
		}
		printf("}\n");
	}
}
bool vladbase::database::is_record_exitst(const char* token) {
	if (get_header_offset(token) == -1) {
		return false;
	} else {
		return true;
	}
}