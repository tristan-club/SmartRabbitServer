#include "array_map.h"

#ifdef DEBUG_ARRAY_MAP
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define cast(t, p)	((t)(p))
#define RMALLOC(r, t, c)	(t*)malloc(sizeof(t)*(c))
#define RFREEVECTOR(r, p, c)	free(p)

#endif	// DEBUG_ARRAY_MAP

#define array_map_at_raw(am, pos) &((am)->p[(am)->node_size * (pos)])
#define array_internal_at(am, pos) cast(struct internal *, (array_map_at_raw(am, pos) + (am)->elem_size))

struct internal {
	struct list_head link;
	int flag;
};

struct array_map *
array_map_create(rabbit * r, int elem_size, int elem_count)
{
	int node_size = elem_size + sizeof(struct internal);

	char * p = RMALLOC(r, char, sizeof(struct array_map) + node_size * elem_count);
	struct array_map * am = cast(struct array_map *, p);

	am->r = r;
	am->p = p + sizeof(struct array_map); 
	am->node_size = node_size; 
	am->elem_size = elem_size;
	am->elem_count = elem_count;
	list_init(&am->head_busy);
	list_init(&am->head_idle);

	memset(am->p, 0, node_size * elem_count);

	int i;
	for (i = 0 ; i < elem_count ; i++) {
		list_init(&array_internal_at(am, i)->link);
		list_insert_tail(&am->head_idle, &array_internal_at(am, i)->link);
		array_internal_at(am, i)->flag = 0;
	}

	return am;
}

void
array_map_destroy(struct array_map * am)
{
	RFREEVECTOR(am->r, cast(char *, am), sizeof(struct array_map) + am->node_size * am->elem_count);
}

int
array_map_push(struct array_map * am, int pos)
{
	if (pos < 0 || pos >= am->elem_count) {
		if (!list_empty(&am->head_idle)) {
			struct list_head * valid = list_first_entry(&am->head_idle);
			list_del(valid);

			list_insert_tail(&am->head_busy, valid);
			//*cast(int *, cast(char *, valid) + sizeof(struct list_head)) = 1;
			struct internal * in = list_entry(valid, struct internal, link); 

			in->flag = 1;

			assert(am->node_size != 0);
			return (cast(char *, in)- am->elem_size - am->p) / am->node_size;
		}

		return -1;
	}

	list_del(&array_internal_at(am, pos)->link);

	list_insert_tail(&am->head_busy, &array_internal_at(am, pos)->link);
	array_internal_at(am, pos)->flag = 1;
	return pos;
}

int
array_map_is_empty(struct array_map * am, int pos)
{
	if (pos < 0 || pos >= am->elem_count) {
		return 1;
	}

	if (array_internal_at(am, pos)->flag == 0) {
		return 1;
	} 	

	return 0;
}

void *
array_map_at(struct array_map * am, int pos)
{
	if (pos < 0 || pos >= am->elem_count) {
		return NULL;
	}

	return array_map_at_raw(am, pos);
}

int
array_map_id(struct array_map * am, void * p)
{
	ptrdiff_t ptr = cast(ptrdiff_t, p);
	ptrdiff_t start = cast(ptrdiff_t, am->p);

	ptrdiff_t diff = ptr - start;

	if(diff < 0) {
		kLOG(am->r, 0, "[Warning] %s : 指针不在array_map内！\n", __FUNCTION__);
		return -1;
	}

	if((diff % am->node_size) != 0) {
		kLOG(am->r, 0, "[Warning] %s : 不是整位置！\n", __FUNCTION__);
	}

	int id = diff / am->node_size;

	if(id < 0 || id >= am->elem_count) {
		kLOG(am->r, 0, "[Warning] %s : 位置id(%d)超出am->elem_count(%d)\n", __FUNCTION__, id, am->elem_count);
		return -1;
	}
	return id;
}

void
array_map_free(struct array_map * am, int pos)
{
	if (pos < 0 || pos >= am->elem_count) {
		return;
	}

	if (!array_map_is_empty(am, pos)) {
		list_del(&array_internal_at(am, pos)->link);
		list_insert_tail(&am->head_idle, &array_internal_at(am, pos)->link);
		array_internal_at(am, pos)->flag = 0;
	}
}

void *
array_map_list_to_value(struct array_map * am, struct list_head * p)
{
	return cast(void *, p) - am->elem_size;	
}

#ifdef DEBUG_ARRAY_MAP
int main(int argc, char * argv[])
{
	struct AABB {
		int a;
		int b;
	};

	rabbit * r;

	int count = 16;

	struct array_map * am = array_map_create(r, sizeof(struct AABB), count);

	int i, pos, j, pushnum;
	srand(time(0));

	struct timeval tm_s, tm_e;
	gettimeofday(&tm_s, NULL);
	for (i = 0 ; i < 100 ; i++) {
		pushnum = rand() % count;
		//pushnum = i % count;
		for (j = 0 ; j < pushnum ; j++) {
			pos = rand() % count;
			//pos = j + pushnum;
			struct AABB * ab = array_map_at(am, array_map_push(am, pos));
			if (ab == NULL ) {
				continue;
			}
			ab->a = pos;
			ab->b = pos * 10;

		}

		pushnum = rand() % count;
		//pushnum /= 2;

		for (j = 0 ; j < pushnum ; j++) {
			pos = rand() % count;
			//pos = j + pushnum;
			array_map_free(am, pos);	
		}
	}
	gettimeofday(&tm_e, NULL);

	struct list_head *it, *tmp;	
	list_foreach_safe(it, tmp, &am->head_busy) {
		struct AABB * ab = array_map_list_to_value(am, it);
		fprintf(stderr, "array[%d] : a = %d, b = %d\n", (cast(char *, ab) - am->p) / am->node_size, ab->a, ab->b);
	}

	array_map_destroy(am);

	fprintf(stderr, "Time cost : %ld\n", (tm_e.tv_sec- tm_s.tv_sec) * 1000 + tm_e.tv_usec - tm_s.tv_usec);

	return 0;

}
#endif // DEBUG_ARRAY_MAP
