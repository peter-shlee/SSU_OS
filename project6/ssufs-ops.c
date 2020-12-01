#include "ssufs-ops.h"

extern struct filehandle_t file_handle_array[MAX_OPEN_FILES];

int ssufs_allocFileHandle() {
	for(int i = 0; i < MAX_OPEN_FILES; i++) {
		if (file_handle_array[i].inode_number == -1) {
			return i;
		}
	}
	return -1;
}

int ssufs_create(char *filename){
	/* 1 */
	struct inode_t *tmp;
	int inode_number;

	if (strlen(filename) > MAX_NAME_STRLEN) {
		return -1;
	}

	if (open_namei(filename) != -1) {
		return -1;
	}

	if ((inode_number = ssufs_allocInode()) == -1) {
		return -1;
	}

	tmp = (struct inode_t *) malloc(sizeof(struct inode_t));
	ssufs_readInode(inode_number, tmp);
	tmp->status = INODE_IN_USE;
	strcpy(tmp->name, filename);
	tmp->file_size = 0;
	ssufs_writeInode(inode_number, tmp);

	return inode_number;
}

void ssufs_delete(char *filename){
	/* 2 */
	int inode_number;

	if (strlen(filename) > MAX_NAME_STRLEN) {
		return;
	}

	if ((inode_number = open_namei(filename)) == -1) {
		return;
	}

	ssufs_freeInode(inode_number);

	return;
}

int ssufs_open(char *filename){
	/* 3 */
	int inode_number;

	if ((inode_number = open_namei(filename)) == -1) {
		return -1;
	}

	for (int i = 0; i < MAX_OPEN_FILES; ++i) {
		if (file_handle_array[i].inode_number == -1) {
			file_handle_array[i].inode_number = inode_number;
			file_handle_array[i].offset = 0;
			return i;
		}
	}

	return -1;
}

void ssufs_close(int file_handle){
	file_handle_array[file_handle].inode_number = -1;
	file_handle_array[file_handle].offset = 0;
}

int ssufs_read(int file_handle, char *buf, int nbytes){
	/* 4 */
	struct inode_t *tmp;
	char *block_buf;
	int file_size, offset;
	int read_bytes;
	int start_byte, end_byte;
	int start_block_index, end_block_index;

	if (file_handle_array[file_handle].inode_number == -1) {
		return -1;
	}

	tmp = (struct inode_t *) malloc(sizeof(struct inode_t));
	ssufs_readInode(file_handle_array[file_handle].inode_number, tmp);

	offset = file_handle_array[file_handle].offset;
	file_size = tmp->file_size;
	start_byte = offset;
	end_byte = offset + nbytes - 1;
	start_block_index = offset / BLOCKSIZE;
	end_block_index = end_byte / BLOCKSIZE;

	if (offset + nbytes > file_size) {
		free(tmp);
		return -1;
	}

	for (int i = start_block_index, read_bytes = 0; i <= end_block_index; ++i) {
		int read_start_byte, read_end_byte;
		int data_block_index = tmp->direct_blocks[i];

//		if (data_block_index == -1) {
//			continue;
//		}

		block_buf = (char *)malloc(sizeof(char) * BLOCKSIZE);
		ssufs_readDataBlock(data_block_index, block_buf);

		read_start_byte = 0;
		read_end_byte = BLOCKSIZE - 1;
		if (i == start_block_index) {
			read_start_byte = start_byte % BLOCKSIZE;
		} 
		if (i == end_block_index) {
			read_end_byte = end_byte % BLOCKSIZE;
		}

		memcpy(buf + read_bytes, block_buf + read_start_byte, read_end_byte - read_start_byte + 1);
		read_bytes += read_end_byte - read_start_byte;

		free(block_buf);
	}

	file_handle_array[file_handle].offset = end_byte + 1;

	free(tmp);
	return 0;
}

int ssufs_write(int file_handle, char *buf, int nbytes){
	/* 5 */
	struct inode_t *tmp;
	int file_size, offset;
	int write_bytes;
	int start_byte, end_byte;
	int start_block_index, end_block_index;

	if (file_handle_array[file_handle].inode_number == -1) {
		return -1;
	}

	tmp = (struct inode_t *) malloc(sizeof(struct inode_t));
	ssufs_readInode(file_handle_array[file_handle].inode_number, tmp);

	offset = file_handle_array[file_handle].offset;
	file_size = tmp->file_size;
	start_byte = offset;
	end_byte = offset + nbytes - 1;
	start_block_index = offset / BLOCKSIZE;
	end_block_index = end_byte / BLOCKSIZE;

	if (end_byte > BLOCKSIZE * MAX_FILE_SIZE) {
		free(tmp);
		return -1;
	}

	if (end_byte > file_size) {
		tmp->file_size = end_byte + 1;
	}

	for (int i = start_block_index, write_bytes = 0; i <= end_block_index; ++i) {
		char *block_buf;
		int write_start_byte, write_end_byte;
		int data_block_index = tmp->direct_blocks[i];

		block_buf = (char *)calloc(BLOCKSIZE, sizeof(char));
		if (data_block_index == -1) {
			data_block_index = tmp->direct_blocks[i] = ssufs_allocDataBlock();
			if (data_block_index == -1) {
				return -1;
			}
		} else {
			ssufs_readDataBlock(data_block_index, block_buf);
		}

		write_start_byte = 0;
		write_end_byte = BLOCKSIZE - 1;
		if (i == start_block_index) {
			write_start_byte = start_byte % BLOCKSIZE;
		} 
		if (i == end_block_index) {
			write_end_byte = end_byte % BLOCKSIZE;
		}

		memcpy(block_buf + write_start_byte, buf + write_bytes, write_end_byte - write_start_byte + 1);
		ssufs_writeDataBlock(data_block_index, block_buf);

		write_bytes += write_end_byte - write_start_byte;

		free(block_buf);
	}

	file_handle_array[file_handle].offset = end_byte + 1;
	ssufs_writeInode(file_handle_array[file_handle].inode_number, tmp);

	free(tmp);
	return 0;
}

int ssufs_lseek(int file_handle, int nseek){
	int offset = file_handle_array[file_handle].offset;

	struct inode_t *tmp = (struct inode_t *) malloc(sizeof(struct inode_t));
	ssufs_readInode(file_handle_array[file_handle].inode_number, tmp);
	
	int fsize = tmp->file_size;
	
	offset += nseek;

	if ((fsize == -1) || (offset < 0) || (offset > fsize)) {
		free(tmp);
		return -1;
	}

	file_handle_array[file_handle].offset = offset;
	free(tmp);

	return 0;
}
