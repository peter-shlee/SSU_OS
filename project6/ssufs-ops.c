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

	if (open_namei(filename) != -1) { // 동일한 이름의 파일이 존재하는지 확인한다.
		return -1;
	}

	if ((inode_number = ssufs_allocInode()) == -1) { // 새로운 inode를 할당한다
		return -1;
	}

	tmp = (struct inode_t *) malloc(sizeof(struct inode_t)); // 임시로 inode 내용 저장할 메모리 공간 할당
	ssufs_readInode(inode_number, tmp);
	tmp->status = INODE_IN_USE; // inode의 status 변경
	strcpy(tmp->name, filename); // 파일명 저장
	tmp->file_size = 0; // 새로 생성된 파일이므로 크기 0으로 초기화
	ssufs_writeInode(inode_number, tmp); // 새로운 inode 내용을 inode block에 저장

	return inode_number;
}

void ssufs_delete(char *filename){
	/* 2 */
	int inode_number;

	if (strlen(filename) > MAX_NAME_STRLEN) {
		return;
	}

	if ((inode_number = open_namei(filename)) == -1) { // 해당 파일의 inode 번호를 구한다
		return; // 해당 파일이 존재하지 않으면 종료한다
	}

	ssufs_freeInode(inode_number); // inode free

	return;
}

int ssufs_open(char *filename){
	/* 3 */
	int inode_number;
	int new_handle_index = -1;

	if ((inode_number = open_namei(filename)) == -1) { // 해당 파일의 inode 번호 구함
		return -1; // 파일 존재하지 않으면 -1 리턴
	}

	if ((new_handle_index = ssufs_allocFileHandle()) == -1) { // 새로운 file handle 할당
		return -1; // file handle을 할당받지 못했다면 -1 리턴
	}
	file_handle_array[new_handle_index].inode_number = inode_number; // file handle에 inode 번호 저장
	file_handle_array[new_handle_index].offset = 0; // offset 0으로 초기화

	return new_handle_index; // 새로운 file handle의 index를 리턴함
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

	if (file_handle_array[file_handle].inode_number == -1) { // 잘못된 file_handle 번호를 전달받았으면 -1 리턴
		return -1;
	}

	tmp = (struct inode_t *) malloc(sizeof(struct inode_t));
	ssufs_readInode(file_handle_array[file_handle].inode_number, tmp); // inode 읽어옴

	offset = file_handle_array[file_handle].offset;
	file_size = tmp->file_size;
	start_byte = offset; // 읽기 시작할 위치의 오프셋
	end_byte = offset + nbytes - 1; // 읽기 종료할 위치의 오프셋 (여기까지 읽고 종료)
	start_block_index = offset / BLOCKSIZE; // 읽기 시작할 블록 index
	end_block_index = end_byte / BLOCKSIZE; // 읽기 종료할 블록 index

	if (offset + nbytes > file_size) { // 파일 크기를 넘어서 읽으려고 하는 경우에는 아무것도 읽지 않아야함 -> -1 리턴하며 함수 종료
		free(tmp);
		return -1;
	}

	for (int i = start_block_index, read_bytes = 0; i <= end_block_index; ++i) {
		int read_start_byte, read_end_byte;
		int data_block_index = tmp->direct_blocks[i];

		block_buf = (char *)calloc(BLOCKSIZE, sizeof(char)); // 읽은 데이터 임시로 저장할 공간 할당
		ssufs_readDataBlock(data_block_index, block_buf); // 블럭 통째로 데이터 읽어옴

		// 읽어온 블럭의 데이터들 중에서 우리에게 필요한 데이터의 시작위치, 끝 위치를 구한다
		read_start_byte = 0;
		read_end_byte = BLOCKSIZE - 1;
		if (i == start_block_index) {
			read_start_byte = start_byte % BLOCKSIZE;
		} 
		if (i == end_block_index) {
			read_end_byte = end_byte % BLOCKSIZE;
		}

		memcpy(buf + read_bytes, block_buf + read_start_byte, read_end_byte - read_start_byte + 1); // 데이터를 buf에 copy
		read_bytes += read_end_byte - read_start_byte + 1; // 이번 블럭에서 읽어온 데이터의 크기를 구한다

		free(block_buf);
	}

	file_handle_array[file_handle].offset = end_byte + 1; // 새로운 offset 저장

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

	if (file_handle_array[file_handle].inode_number == -1) { // 잘못된 file_handle 번호를 전달받았으면 -1 리턴
		return -1;
	}

	tmp = (struct inode_t *) malloc(sizeof(struct inode_t));
	ssufs_readInode(file_handle_array[file_handle].inode_number, tmp); // inode 읽어옴

	offset = file_handle_array[file_handle].offset;
	file_size = tmp->file_size;
	start_byte = offset; // 쓰기 시작할 위치의 오프셋
	end_byte = offset + nbytes - 1; // 쓰기 종료할 위치의 오프셋 (여기까지 쓰고 종료)
	start_block_index = offset / BLOCKSIZE; // 쓰기 시작할 블록의 index
	end_block_index = end_byte / BLOCKSIZE; // 쓰기 종료할 블록의 index

	if (end_byte > BLOCKSIZE * MAX_FILE_SIZE) { // 요청된 바이트 수를 쓰면 최대 파일 크기 제한을 초과하는 경우 -1 리턴하고 함수 종료
		free(tmp);
		return -1;
	}

	int new_alloced_data_blocks[MAX_FILE_SIZE] = {0,};
	for (int i = start_block_index; i <= end_block_index; ++i) {
		int data_block_index = tmp->direct_blocks[i];

		if (data_block_index == -1) {
			data_block_index = tmp->direct_blocks[i] = ssufs_allocDataBlock(); // 새로운 data block이 필요하면 새로 할당함
			if (data_block_index == -1) { // 할당 실패 시
				// 앞서 새로 할당했던 data block들 다시 반환하여 쓰기 하기 전 상태를 유지하도록 한다
				for (int j = 0; j < MAX_FILE_SIZE; ++j) {
					if (new_alloced_data_blocks[j]) {
						ssufs_freeDataBlock(tmp->direct_blocks[j]);
					}
				}

				return -1; // -1 리턴하며 종료

			} else { // 할당 성공 시 새로 할당된 데이터 블럭임을 표시
				new_alloced_data_blocks[i] = 1;
			}
		}	
	}

	for (int i = start_block_index, write_bytes = 0; i <= end_block_index; ++i) {
		char *block_buf;
		int write_start_byte, write_end_byte;
		int data_block_index = tmp->direct_blocks[i];

		block_buf = (char *)calloc(BLOCKSIZE, sizeof(char)); // write할 데이터 임시로 저장할 공간 할당

		if (!new_alloced_data_blocks[i]) {
			ssufs_readDataBlock(data_block_index, block_buf);
		}

		// 데이터를 쓸 위치의 인덱스를 구한다
		write_start_byte = 0;
		write_end_byte = BLOCKSIZE - 1;
		if (i == start_block_index) {
			write_start_byte = start_byte % BLOCKSIZE;
		} 
		if (i == end_block_index) {
			write_end_byte = end_byte % BLOCKSIZE;
		}

		memcpy(block_buf + write_start_byte, buf + write_bytes, write_end_byte - write_start_byte + 1); // write
		ssufs_writeDataBlock(data_block_index, block_buf); // write한 내용 디스크에 쓴다

		write_bytes += write_end_byte - write_start_byte + 1; // write한 바이트 수 계산

		free(block_buf);
	}

	if (end_byte > file_size) { // write 후에 파일의 크기가 커진 경우 file size 증가시킴
		tmp->file_size = end_byte + 1;
	}

	file_handle_array[file_handle].offset = end_byte + 1; // 새로운 offset 저장
	ssufs_writeInode(file_handle_array[file_handle].inode_number, tmp); // 변경된 idnode 내용 저장

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
