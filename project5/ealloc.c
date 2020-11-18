#include "ealloc.h"

#define MAX_PAGE_COUNT 4

typedef struct node {
	int page_index;
	int is_valid;
	int start_addr;
	int size;
	struct node *next_node;
	struct node *prev_node;
} Node;

typedef struct mem_list {
	Node *mem_in_use_head;
	Node *mem_not_in_use_head;
} MemLinkedList;

char *mem[MAX_PAGE_COUNT];
MemLinkedList memLinkedLists[MAX_PAGE_COUNT];
Node *management_mem;

int init_alloc_one_page(int i);
int cleanup_one_page(int i);
char *alloc_one_page(int i, int size);
void dealloc_one_page(int i, char *dealloc_ptr);
Node *getNewNode();
void removeNode(Node *node);
int checkAllocedAtPage(int page_num, char *addr);
void printAllNode(int i);

//////////////////////////////////////////////////////

int current_page;

void init_alloc() {
	current_page = 0;
	management_mem = mmap(NULL, PAGESIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0); // 메모리 관리에 사용할 메모리 영역을 할당받는다
}


char *alloc(int size) {
	int i;
	char *new_alloced_mem = NULL;

	//printf("1111\n");
	for (i = 0; i < MAX_PAGE_COUNT; ++i) {
		//printf("1111-2\n");
		if (!mem[i]) {
			//printf("1111-3\n");
			init_alloc_one_page(i);
			//printf("1111-4\n");
		}

		//printAllNode(i);
		//printf("1111-5\n");
		new_alloced_mem = alloc_one_page(i, size);
		//printf("1111-6\n");

		if (new_alloced_mem) {
			break;
		}
	}
	//printf("1111 end\n");

	return new_alloced_mem;
}

void dealloc(char *dealloc_ptr) {
	int i;

	//printf("dealloc start\n");
	//printf("4444-1\n");
	for (i = 0; i < MAX_PAGE_COUNT; ++i) {
		//printf("4444-2\n");
		if (checkAllocedAtPage(i, dealloc_ptr)) {
			dealloc_one_page(i, dealloc_ptr);
			//printAllNode(i);
			break;
		}
	}
}


void cleanup() {
	int i = 0;
	for (i = 0; i < MAX_PAGE_COUNT; ++i) {
		mem[i] = NULL;
		memLinkedLists[i].mem_in_use_head = NULL;
		memLinkedLists[i].mem_not_in_use_head = NULL;
		management_mem = NULL;
	}
	
	return;
}

int checkAllocedAtPage(int page_num, char *addr) {
	int i;
	Node *node = memLinkedLists[page_num].mem_in_use_head;
	char *base = mem[page_num];

	if (!base) return 0;

	while (node) {
		if (base + node->start_addr == addr) {
			return 1;
		}

		node = node->next_node;
	}

	return 0;
}

////////////////////////////////////////////////////////

int init_alloc_one_page(int i) {
	Node *new_node;
	
	//printf("page size: %d\n", PAGESIZE);

	mem[i] = mmap(NULL, PAGESIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (mem[i] == MAP_FAILED) return -1;

//	management_mem = mmap(NULL, PAGESIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
//	if (management_mem == MAP_FAILED) return -1; -> init_alloc에서 초기화해서 모든 페이지가 공유해서 사용

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
	//node = mem_linked_list.mem_in_use_head;
	node = memLinkedLists[i].mem_in_use_head;
	while (node) {
		removeNode(node);
		node = node->next_node;
	}

	// mem not in use list 정리
	//node = mem_linked_list.mem_not_in_use_head;
	node = memLinkedLists[i].mem_not_in_use_head;
	while (node) {
		removeNode(node);
		node = node->next_node;
	}

	// cleanup()에서 한번에 정리해야함
	//if (!munmap(management_mem, PAGESIZE)) return -1;

	// mem은 unmap 하지 않아도 됨
	//if (!munmap(mem[i], PAGESIZE)) return -1;

	return 0;
}

char *alloc_one_page(int i, int size) {
	Node *new_node;
	Node *new_alloced_mem;
	Node* mem_not_in_use;

	if (size % MINALLOC) {
		return NULL;
	}

	//printf("2222-1\n");
	//mem_not_in_use = mem_linked_list.mem_not_in_use_head;
	mem_not_in_use = memLinkedLists[i].mem_not_in_use_head;
	while (1) {
		if (!mem_not_in_use) {
			return NULL;
		}

		//printf("2222-2\n");
		//printf("mem size: %d, req size: %d\n", mem_not_in_use->size, size);
		if (mem_not_in_use->size > size) {
			// 사용할 만큼 할당
			// not_in_use Node의 size 변경
			mem_not_in_use->size -= size;

			//printf("2222-3\n");
			// in_use list에 넣을 새로운 노드 생성
			new_node = getNewNode();
			new_node->size = size;
			new_node->start_addr = mem_not_in_use->start_addr + mem_not_in_use->size; // 앞에서 mem_not_in_use의 size 줄였기 때문에 그냥 size만 더해주면 됨
			new_node->prev_node = NULL;
			//new_node->next_node = mem_linked_list.mem_in_use_head;
			new_node->next_node = memLinkedLists[i].mem_in_use_head;

			//printf("2222-4\n");
			// 새로운 노드를 in_use_list의 앞부분에 넣음
			if (memLinkedLists[i].mem_in_use_head) {
				memLinkedLists[i].mem_in_use_head->prev_node = new_node;
			}
			memLinkedLists[i].mem_in_use_head = new_node;

			new_alloced_mem = new_node;

			//printf("2222-5\n");
			break;
		} else if (mem_not_in_use->size == size) {
			// not_in_use 리스트에서 해당 노드 제거
			if (mem_not_in_use->prev_node) {
				//printf("2222-6\n");
				mem_not_in_use->prev_node->next_node = mem_not_in_use->next_node;
			} else {
				//printf("2222-7\n");
				memLinkedLists[i].mem_not_in_use_head = mem_not_in_use->next_node;
			}

			if (mem_not_in_use->next_node) {
				//printf("2222-8\n");
				mem_not_in_use->next_node->prev_node = mem_not_in_use->prev_node;
			}


			//printf("2222-9\n");
			// in_use로 이동할 노드 setting
			mem_not_in_use->prev_node = NULL;
			mem_not_in_use->next_node = memLinkedLists[i].mem_in_use_head;

			//printf("2222-10\n");
			// in_use_list의 앞부분에 집어넣음
			if (memLinkedLists[i].mem_in_use_head) {
				memLinkedLists[i].mem_in_use_head->prev_node = mem_not_in_use;
			}
			memLinkedLists[i].mem_in_use_head = mem_not_in_use;

			new_alloced_mem = mem_not_in_use;
			//printf("2222-11\n");


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


	//printf("3333-1\n");
	node = memLinkedLists[i].mem_in_use_head;
	while (node) {
		if (node->start_addr == mem_index) {
			break;
		}

		node = node->next_node;
	}


	//printf("3333-2\n");
	if (!node) return; // 인자로 전달된 주소에 해당되는 노드를 찾지 못했으면 종료

	// mem_in_use 리스트를 정리한다
	if (node->prev_node) { // 헤드노드가 아닐 때
		node->prev_node->next_node = node->next_node;
	} else { // 헤드노드일 때
		memLinkedLists[i].mem_in_use_head = node->next_node;
	}

	//printf("3333-3\n");
	if (node->next_node) {
		node->next_node->prev_node = node->prev_node;
	}


	//printf("3333-4\n");
	// mem_not_in_use 리스트에 넣는다
	mem_not_in_use = memLinkedLists[i].mem_not_in_use_head;
	if (!mem_not_in_use) {
		node->prev_node = NULL;
		node->next_node = NULL;
		memLinkedLists[i].mem_not_in_use_head = node;
	} else {
		while (mem_not_in_use) {
			//printf("3333-5\n");
			if (mem_not_in_use->start_addr > node->start_addr) {
				//printf("3333-6\n");
				// mem_not_in_list에 node 삽입
				node->prev_node = mem_not_in_use->prev_node;
				node->next_node = mem_not_in_use;
				if (node->prev_node) {
					node->prev_node->next_node = node;
				} else {
					memLinkedLists[i].mem_not_in_use_head = node;	
				}
				mem_not_in_use->prev_node = node;


				//printf("3333-7\n");
				// 뒤쪽 노드와 합칠 수 있는지 확인
				if (node->next_node) {
					//printf("3333-7-1\n");
					if (node->start_addr + node->size == node->next_node->start_addr) {
						//printf("3333-7-2\n");
						tmp_node = node->next_node;
						node->size += node->next_node->size;
						node->next_node = node->next_node->next_node;
						if (node->next_node) {
							node->next_node->prev_node = node;
						}

						// 필요 없어진 노드 제거
						removeNode(tmp_node);
						//printf("3333-7-3\n");
					}
				}
				
				//printf("3333-8\n");
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

		//printf("3333-9\n");
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
