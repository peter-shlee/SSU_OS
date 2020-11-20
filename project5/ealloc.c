#include "ealloc.h"

#define MAX_PAGE_COUNT 4

typedef struct node { // 메모리 관리에 사용할 링크드 리스트의 노드 구조체
	int page_index; // 해당 노드가 가리키는 메모리 영역이 속해있는 페이지의 번호
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

char *mem[MAX_PAGE_COUNT]; // heap 메모리 영역 (페이지 4개)
MemLinkedList memLinkedLists[MAX_PAGE_COUNT]; // 메모리 관리 링크드 리스트들 구조체(페이지당 하나씩)
Node *management_mem; // 메모리 관리 링크드 리스트의 노드들을 할당할 메모리 영역

int init_alloc_one_page(int i);
int cleanup_one_page(int i);
char *alloc_one_page(int i, int size);
void dealloc_one_page(int i, char *dealloc_ptr);
Node *getNewNode();
void removeNode(Node *node);
int checkallocedatpage(int page_num, char *addr);
void printallnode(int i);

void init_alloc() {
	management_mem = mmap(NULL, PAGESIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0); // 메모리 관리에 사용할 메모리 영역을 할당받는다
}


char *alloc(int size) {
	int i;
	char *new_alloced_mem = NULL;

	for (i = 0; i < MAX_PAGE_COUNT; ++i) {
		if (!mem[i]) { // 해당 heap 메모리 페이지가 아직 할당되지 않았다면
			init_alloc_one_page(i); // 한페이지 할당
		}

		new_alloced_mem = alloc_one_page(i, size); // i번 페이지에서 메모리 할당

		if (new_alloced_mem) { // 할당 되었다면
			break; // break
		}
		// 할당 안됐으면 다음 페이지에서 할당 시도
	}

	return new_alloced_mem;
}

void dealloc(char *dealloc_ptr) {
	int i;

	for (i = 0; i < MAX_PAGE_COUNT; ++i) {
		if (checkallocedatpage(i, dealloc_ptr)) { // 해제할 메모리가 해당 페이지에 있는지 확인
			dealloc_one_page(i, dealloc_ptr); // 해당 페이지에서 메모리 해제
			break;
		}
	}
}


void cleanup() {
	int i = 0;
	// 메모리 가리키는 포인터와 관리 링크드 리스트들 초기화
	for (i = 0; i < MAX_PAGE_COUNT; ++i) {
		mem[i] = NULL;
		memLinkedLists[i].mem_in_use_head = NULL;
		memLinkedLists[i].mem_not_in_use_head = NULL;
		management_mem = NULL;
	}
	
	return;
}

int checkallocedatpage(int page_num, char *addr) { // 전달된 주소가 해당 페이지에 할당되어 있는 메모리 영역의 주소인지 확인하는 함수
	int i;
	Node *node = memLinkedLists[page_num].mem_in_use_head;
	char *base = mem[page_num];

	if (!base) return 0;

	while (node) {
		if (base + node->start_addr == addr) { // 주소 일치하면
			return 1; // 1리턴
		}

		node = node->next_node; // 다음 노드 확인
	}

	return 0; // 일치하는 주소 없으면 0 리턴
}

////////////////////////////////////////////////////////
// 아래는 과제 (1)의 코드를 재활용함

int init_alloc_one_page(int i) {
	Node *new_node;
	
	mem[i] = mmap(NULL, PAGESIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (mem[i] == MAP_FAILED) return -1;

	new_node = getNewNode();
	new_node->page_index = i;
	new_node->start_addr = 0;
	new_node->size = PAGESIZE;
	new_node->next_node = NULL;
	new_node->prev_node = NULL;

	// 해당 페이지에 대한 리스트를 따로 만들어 관리
	memLinkedLists[i].mem_in_use_head = NULL;
	memLinkedLists[i].mem_not_in_use_head = new_node;

	return 0;
}

int cleanup_one_page(int i) {
	Node *node;

	// mem in use list 정리
	// 해당 페이지에 대한 리스트를 정리하도록 수정
	node = memLinkedLists[i].mem_in_use_head;
	while (node) {
		removeNode(node);
		node = node->next_node;
	}

	// mem not in use list 정리
	node = memLinkedLists[i].mem_not_in_use_head;
	while (node) {
		removeNode(node);
		node = node->next_node;
	}

	return 0;
}

char *alloc_one_page(int i, int size) {
	Node *new_node;
	Node *new_alloced_mem;
	Node * mem_not_in_use;

	if (size % MINALLOC) {
		return NULL;
	}

	mem_not_in_use = memLinkedLists[i].mem_not_in_use_head;
	while (1) {
		if (!mem_not_in_use) {
			return NULL;
		}

		if (mem_not_in_use->size > size) {
			// 사용할 만큼 할당
			// not_in_use node의 size 변경
			mem_not_in_use->size -= size;

			// in_use list에 넣을 새로운 노드 생성
			new_node = getNewNode();
			new_node->size = size;
			new_node->start_addr = mem_not_in_use->start_addr + mem_not_in_use->size; // 앞에서 mem_not_in_use의 size 줄였기 때문에 그냥 size만 더해주면 됨
			new_node->prev_node = NULL;
			new_node->next_node = memLinkedLists[i].mem_in_use_head;

			// 새로운 노드를 in_use_list의 앞부분에 넣음
			if (memLinkedLists[i].mem_in_use_head) {
				memLinkedLists[i].mem_in_use_head->prev_node = new_node;
			}
			memLinkedLists[i].mem_in_use_head = new_node;

			new_alloced_mem = new_node;

			break;
		} else if (mem_not_in_use->size == size) {
			// not_in_use 리스트에서 해당 노드 제거
			if (mem_not_in_use->prev_node) {
				mem_not_in_use->prev_node->next_node = mem_not_in_use->next_node;
			} else {
				memLinkedLists[i].mem_not_in_use_head = mem_not_in_use->next_node;
			}

			if (mem_not_in_use->next_node) {
				mem_not_in_use->next_node->prev_node = mem_not_in_use->prev_node;
			}


			// in_use로 이동할 노드 setting
			mem_not_in_use->prev_node = NULL;
			mem_not_in_use->next_node = memLinkedLists[i].mem_in_use_head;

			// in_use_list의 앞부분에 집어넣음
			if (memLinkedLists[i].mem_in_use_head) {
				memLinkedLists[i].mem_in_use_head->prev_node = mem_not_in_use;
			}
			memLinkedLists[i].mem_in_use_head = mem_not_in_use;

			new_alloced_mem = mem_not_in_use;

			break;
		}

		mem_not_in_use = mem_not_in_use->next_node;
	}


	return mem[i] + new_alloced_mem->start_addr;
}

void dealloc_one_page(int i, char *dealloc_ptr) {
	int mem_index = dealloc_ptr - mem[i];
	Node *node;
	Node *mem_not_in_use;
	Node *tmp_node;
	Node *prev_node = NULL;

	if (mem_index < 0 || 4096 <= mem_index) {
		return;
	}


	node = memLinkedLists[i].mem_in_use_head;
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
		memLinkedLists[i].mem_in_use_head = node->next_node;
	}

	if (node->next_node) {
		node->next_node->prev_node = node->prev_node;
	}

	// mem_not_in_use 리스트에 넣는다
	mem_not_in_use = memLinkedLists[i].mem_not_in_use_head;
	if (!mem_not_in_use) {
		node->prev_node = NULL;
		node->next_node = NULL;
		memLinkedLists[i].mem_not_in_use_head = node;
	} else {
		while (mem_not_in_use) {
			if (mem_not_in_use->start_addr > node->start_addr) {
				// mem_not_in_list에 node 삽입
				node->prev_node = mem_not_in_use->prev_node;
				node->next_node = mem_not_in_use;
				if (node->prev_node) {
					node->prev_node->next_node = node;
				} else {
					memLinkedLists[i].mem_not_in_use_head = node;	
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

Node *getNewNode() {
	while (management_mem->is_valid) {
		management_mem += 1;
	}
	management_mem->is_valid = 1;

	return management_mem;
}

void removeNode(Node *node) {
	node->is_valid = 0;
}

void printAllNode(int i) {
	Node *node;
	printf("\n******************print all nodes*******************\n");
	printf("mem in use:\n");
	node = memLinkedLists[i].mem_in_use_head;
	while(node) {
		printf("index: %d, size: %d\n", node->start_addr, node->size);

		node = node->next_node;
	}

	printf("mem not in use:\n");
	node = memLinkedLists[i].mem_not_in_use_head;
	while(node) {
		printf("index: %d, size: %d\n", node->start_addr, node->size);

		node = node->next_node;
	}
	printf("******************print all nodes end****************\n\n");

	return;
}
