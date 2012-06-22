#ifndef __CRB_ATOMIC_H
#define __CRB_ATOMIC_H 1

#define crb_atomic_cmp_set(lock, old, set)                                    \
    __sync_bool_compare_and_swap(lock, old, set)

#define crb_atomic_fetch_add(value, add)                                      \
    __sync_fetch_and_add(value, add)

#define crb_atomic_fetch_sub(value, add)                                      \
    __sync_fetch_and_sub(value, add)
    
#endif /* __CRB_ATOMIC_H */
