
typedef struct list_s list_t;

struct list_s {
	struct list_s *next;
	struct list_s *prev;
};

#define offsetof_(type, member) ((size_t) &((type *) 0)->member)

#define containerof_(ptr, type, member) ({			\
	const typeof( ((type *)0)->member ) *__mptr = (ptr);	\
	(type *)( (char *)__mptr - offsetof_(type,member) );})

#define list_entry(ptr, type, member) \
	containerof_(ptr, type, member)

#define list_first_entry(ptr, type, member) \
	list_entry((ptr)->next, type, member)

#define list_for_each(pos, head) \
	for (pos = (head)->next; pos != (head); pos = pos->next)

#define list_for_each_safe(pos, n, head) \
	for (pos = (head)->next, n = pos->next; pos != (head); pos = n, n = pos->next)

#define list_for_each_entry(pos, head, member)				\
	for (pos = list_entry((head)->next, typeof(*pos), member);	\
	     &pos->member != (head);					\
	     pos = list_entry(pos->member.next, typeof(*pos), member))

#define list_for_each_entry_safe(pos, n, head, member)			\
	for (pos = list_entry((head)->next, typeof(*pos), member),	\
	     n = list_entry(pos->member.next, typeof(*pos), member);	\
	     &pos->member != (head); 					\
	     pos = n, n = list_entry(n->member.next, typeof(*n), member))

static inline int list_count (list_t *head)
{
	int count;
	list_t *entry;
	count = 0;
	list_for_each(entry, head) {
		count++;
	}
	return count;
}

static inline void list_add_actual (list_t *new, list_t *prev, list_t *next)
{
	next->prev = new;
	new->next = next;
	new->prev = prev;
	prev->next = new;
}

static inline void list_del_actual (list_t *prev, list_t *next)
{
	next->prev = prev;
	prev->next = next;
}

static inline void list_add_tail (list_t *new, list_t *head)
{
	list_add_actual(new, head->prev, head);
}

static inline void list_add (list_t *new, list_t *head)
{
	list_add_actual(new, head, head->next);
}

static inline void list_del (list_t *entry)
{
	list_del_actual(entry->prev, entry->next);
	entry->next = NULL;
	entry->prev = NULL;
}

static inline int list_is_first (list_t *list, list_t *head)
{
	return head->next == list;
}

static inline int list_is_last (list_t *list, list_t *head)
{
	return list->next == head;
}

static inline void list_init (list_t *head)
{
	head->next = head;
	head->prev = head;
}
