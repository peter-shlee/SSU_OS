#include "alloc.h"

typedef struct node {
	int is_valid;
	int start_index;
	int size;
	struct node *next_node;
	struct node *prev_node;
} Node;

typedef struct mem_list {
	Node *mem_in_use_head;
	Node *mem_not_in_use_head;
} MemLinkedList;

char *mem;
Node *management_mem;
MemLinkedList mem_linked_list;

Node *getNewNode();
void removeNode(Node *node);

int init_alloc() {
	Node *new_node;
	
	mem = mmap(NULL, PAGESIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (mem == MAP_FAILED) return -1;

	management_mem = mmap(NULL, PAGESIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (management_mem == MAP_FAILED) return -1;

	new_node = getNewNode();
	new_node->start_index = 0;
	new_node->size = PAGESIZE;
	new_node->next_node = NULL;
	new_node->prev_node = NULL;

	mem_linked_list.mem_in_use_head = NULL;
	mem_linked_list.mem_not_in_use_head = new_node;

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

	if (size % MINALLOC) {
		return NULL;
	}

	mem_not_in_use = mem_linked_list.mem_not_in_use_head;
	while (1) {
		if (!mem_not_in_use) {
			return NULL;
		}

		if (mem_not_in_use->size > size) {
			// 사용할 만큼 할당
			// not_in_use Node의 size 변경
			mem_not_in_use->size -= size;

			// in_use list에 넣을 새로운 노드 생성
			new_node = getNewNode();
			new_node->size = size;
			new_node->start_index = mem_not_in_use->start_index + mem_not_in_use->size; // 앞에서 mem_not_in_use의 size 줄였기 때문에 그냥 size만 더해주면 됨
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

		mem_not_in_use = mem_not_in_use->next_node;
	}


	return mem + new_alloced_mem->start_index;
}

void dealloc(char *dealloc_ptr) {
	int mem_index = dealloc_ptr - mem;
	Node *node;
	Node *mem_not_in_use;
	Node *tmp_node;
	Node *prev_node = NULL;

	if (mem_index < 0 || 4096 <= mem_index) {
		return;
	}


	node = mem_linked_list.mem_in_use_head;
	while (node) {
		if (node->start_index == mem_index) {
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
			if (mem_not_in_use->start_index > node->start_index) {
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
					if (node->start_index + node->size == node->next_node->start_index) {
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
					if (node->prev_node->start_index + node->prev_node->size == node->start_index) {
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
				if (node->prev_node->start_index + node->prev_node->size == node->start_index) {
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

void printAllNode() {
	Node *node;
	printf("\n******************print all nodes*******************\n");
	printf("mem in use:\n");
	node = mem_linked_list.mem_in_use_head;
	while(node) {
		printf("index: %d, size: %d\n", node->start_index, node->size);

		node = node->next_node;
	}

	printf("mem not in use:\n");
	node = mem_linked_list.mem_not_in_use_head;
	while(node) {
		printf("index: %d, size: %d\n", node->start_index, node->size);

		node = node->next_node;
	}
	printf("******************print all nodes end****************\n\n");

	return;
}
