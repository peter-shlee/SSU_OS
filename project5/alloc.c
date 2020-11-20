#include "alloc.h"

typedef struct node { // 메모리 관리에 사용할 링크드 리스트의 노드 구조체
	int is_valid; // 해당 노드가 유효하면(해당 노드 사용중) 1, 아니면 0. 이게 1이면 이 노드가 저장된 메모리를 다른 노드가 사용하면 안됨.
	int start_addr; // 할당된 메모리 영역의 시작 주소(0 부터 시작, 1바이트 단위)
	int size; // 할당된 메모리 영역의 크기
	struct node *next_node; // 다음 메모리 관리 노드의 주소
	struct node *prev_node; // 이전 메모리 관리 노드의 주소
} Node;

typedef struct mem_list { // 메모리 관리 링크드 리스트들 저장하는 구조체
	Node *mem_in_use_head; // 할당되어 사용중인 메모리 영역 관리 링크드 리스트
	Node *mem_not_in_use_head; // 사용중이지 않은 메모리 영역 관리 링크드 리스트
} MemLinkedList;

char *mem; // heap 메모리 영역
Node *management_mem; // 메모리 관리 링크드 리스트의 노드들을 할당할 메모리 영역
MemLinkedList mem_linked_list; // 메모리 관리 링크드 리스트들 구조체

Node *getNewNode(); // 새로운 노드를 메모리에 할당하여 리턴하는 함수
void removeNode(Node *node); // 더이상 사용하지 않는 노드를 메모리에서 해제하는 함수

int init_alloc() {
	Node *new_node;
	
	mem = mmap(NULL, PAGESIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0); // heap으로 사용할 메모리 영역을 mmap으로 할당함
	if (mem == MAP_FAILED) return -1; // mmap 실패시 -1 리턴

	management_mem = mmap(NULL, PAGESIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0); // 메모리 관리 링크드 리스트에 사용할 메모리 영역을 mmap으로 할당
	if (management_mem == MAP_FAILED) return -1; // mmap 실패시 -1 리턴

	// 초기에는 메모리의 모든 공간이 할당 가능하므로 사용하지 않는 메모리 영역의 링크드 리스트에 메모리 전체 크기를 나타내는 노드를 넣는다
	new_node = getNewNode(); // 새로운 노드 생성
	new_node->start_addr = 0; // 시작 주소 0으로 설정
	new_node->size = PAGESIZE; // 크기는 PAGESIZE로 설정
	new_node->next_node = NULL; // 다음 노드 없음
	new_node->prev_node = NULL; // 이전 노드 없음

	mem_linked_list.mem_in_use_head = NULL; // 사용중인 메모리 없음
	mem_linked_list.mem_not_in_use_head = new_node; // 사용중이지 않은 메모리 관리 리스트에 위에서 생성한 노드를 넣는다.

	return 0;
}

int cleanup() {
	Node *node;

	// mem in use list 정리
	node = mem_linked_list.mem_in_use_head;
	while (node) {
		removeNode(node);
		node = node->next_node;
	}

	// mem not in use list 정리
	node = mem_linked_list.mem_not_in_use_head;
	while (node) {
		removeNode(node);
		node = node->next_node;
	}

	if (!munmap(management_mem, PAGESIZE)) return -1;
	if (!munmap(mem, PAGESIZE)) return -1;

	return 0;
}

char *alloc(int size) {
	Node *new_node;
	Node *new_alloced_mem;
	Node* mem_not_in_use;

	if (size % MINALLOC) { // 요청된 크기가 8의 배수가 아니면 NULL을 리턴한다
		return NULL;
	}

	mem_not_in_use = mem_linked_list.mem_not_in_use_head;
	while (1) {
		if (!mem_not_in_use) { // 여유 공간이 부족한 경우 NULL을 리턴한다
			return NULL;
		}

		if (mem_not_in_use->size > size) {
			// 사용할 만큼 할당
			// not_in_use Node의 size 변경
			mem_not_in_use->size -= size;

			// in_use list에 넣을 새로운 노드 생성
			new_node = getNewNode();
			new_node->size = size;
			new_node->start_addr = mem_not_in_use->start_addr + mem_not_in_use->size; // 앞에서 mem_not_in_use의 size 줄였기 때문에 그냥 size만 더해주면 됨
			new_node->prev_node = NULL;
			new_node->next_node = mem_linked_list.mem_in_use_head;

			// 새로운 노드를 in_use_list의 앞부분에 넣음
			if (mem_linked_list.mem_in_use_head) {
				mem_linked_list.mem_in_use_head->prev_node = new_node;
			}
			mem_linked_list.mem_in_use_head = new_node;

			new_alloced_mem = new_node;

			break;
		} else if (mem_not_in_use->size == size) {
			// not_in_use 리스트에서 해당 노드 제거
			if (mem_not_in_use->prev_node) {
				mem_not_in_use->prev_node->next_node = mem_not_in_use->next_node;
			} else {
				mem_linked_list.mem_not_in_use_head = mem_not_in_use->next_node;
			}

			if (mem_not_in_use->next_node) {
				mem_not_in_use->next_node->prev_node = mem_not_in_use->prev_node;
			}


			// in_use로 이동할 노드 setting
			mem_not_in_use->prev_node = NULL;
			mem_not_in_use->next_node = mem_linked_list.mem_in_use_head;


			// in_use_list의 앞부분에 집어넣음
			if (mem_linked_list.mem_in_use_head) {
				mem_linked_list.mem_in_use_head->prev_node = mem_not_in_use;
			}
			mem_linked_list.mem_in_use_head = mem_not_in_use;

			new_alloced_mem = mem_not_in_use;


			break;
		}

		mem_not_in_use = mem_not_in_use->next_node; // 다음 노드에 할당 가능한지 확인
	}


	return mem + new_alloced_mem->start_addr; // 할당된 메모리 주소를 리턴한다
}

void dealloc(char *dealloc_ptr) {
	int mem_index = dealloc_ptr - mem; // dealloc할 메모리의 주소
	Node *node;
	Node *mem_not_in_use;
	Node *tmp_node;
	Node *prev_node = NULL;

	if (mem_index < 0 || 4096 <= mem_index) { // 범위를 벗어난 주소가 전달됐을 때
		return;
	}


	node = mem_linked_list.mem_in_use_head;
	while (node) {
		if (node->start_addr == mem_index) {
			break;
		}

		node = node->next_node;
	}


	if (!node) return; // 인자로 전달된 주소에 해당되는 노드를 찾지 못했으면 종료

	// mem_in_use 리스트를 정리한다
	if (node->prev_node) { // 헤드노드가 아닐 때
		node->prev_node->next_node = node->next_node;
	} else { // 헤드노드일 때
		mem_linked_list.mem_in_use_head = node->next_node;
	}

	if (node->next_node) {
		node->next_node->prev_node = node->prev_node;
	}


	// mem_not_in_use 리스트에 넣는다
	mem_not_in_use = mem_linked_list.mem_not_in_use_head;
	if (!mem_not_in_use) {
		node->prev_node = NULL;
		node->next_node = NULL;
		mem_linked_list.mem_not_in_use_head = node;
	} else {
		while (mem_not_in_use) {
			if (mem_not_in_use->start_addr > node->start_addr) {
				// mem_not_in_list에 node 삽입
				node->prev_node = mem_not_in_use->prev_node;
				node->next_node = mem_not_in_use;
				if (node->prev_node) {
					node->prev_node->next_node = node;
				} else {
					mem_linked_list.mem_not_in_use_head = node;	
				}
				mem_not_in_use->prev_node = node;


				// 뒤쪽 노드와 합칠 수 있는지 확인
				if (node->next_node) {
					if (node->start_addr + node->size == node->next_node->start_addr) {
						tmp_node = node->next_node;
						node->size += node->next_node->size;
						node->next_node = node->next_node->next_node;
						if (node->next_node) {
							node->next_node->prev_node = node;
						}

						// 필요 없어진 노드 제거
						removeNode(tmp_node);
					}
				}
				
				
				// 앞쪽 노드와 합칠 수 있는지 확인
				if (node->prev_node) {
					if (node->prev_node->start_addr + node->prev_node->size == node->start_addr) {
						node->prev_node->next_node = node->next_node;
						if (node->next_node) {
							node->next_node->prev_node = node->prev_node;
						}
						node->prev_node->size += node->size;

						// 필요 없어진 노드 제거
						removeNode(node);
					}

				}

				break;
			}

			prev_node = mem_not_in_use;
			mem_not_in_use = mem_not_in_use->next_node;
		}

		if (!mem_not_in_use) {
			// mem_not_in_use list의 맨 끝에 삽입해야 하는 경우
			prev_node->next_node = node;
			node->prev_node = prev_node;
			node->next_node = NULL;

			// 앞쪽 노드와 합칠 수 있는지 확인
			if (node->prev_node) {
				if (node->prev_node->start_addr + node->prev_node->size == node->start_addr) {
					node->prev_node->next_node = node->next_node;
					node->prev_node->size += node->size;

					// 필요 없어진 노드 제거
					removeNode(node);
				}

			}
		}
	}


	return;
}

Node *getNewNode() { // 새로운 노드 할당하고 주소 리턴하는 함수
	while (management_mem->is_valid) { // 해당 메모리가 사용중이라면
		management_mem += 1; // 다음 위치 확인
	}
	management_mem->is_valid = 1; // 새로 할당할 위치 사용중이라고 표시

	return management_mem; // 주소 리턴
}

void removeNode(Node *node) { // 사용 끝난 노드 메모리 해제하는 함수
	node->is_valid = 0; // 유효하지 않다고 표시하면 끝
}

void printAllNode() { // 링크드 리스트의 모든 노드 내용 출력하는 함수, 디버깅용으로 사용
	Node *node;
	printf("\n******************print all nodes*******************\n");
	printf("mem in use:\n");
	node = mem_linked_list.mem_in_use_head;
	while(node) {
		printf("index: %d, size: %d\n", node->start_addr, node->size);

		node = node->next_node;
	}

	printf("mem not in use:\n");
	node = mem_linked_list.mem_not_in_use_head;
	while(node) {
		printf("index: %d, size: %d\n", node->start_addr, node->size);

		node = node->next_node;
	}
	printf("******************print all nodes end****************\n\n");

	return;
}
