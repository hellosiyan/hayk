#ifndef __HK_ATOMIC_H
#define __HK_ATOMIC_H 1

#define hk_atomic_cmp_set(lock, old, set)                                    \
    __sync_bool_compare_and_swap(lock, old, set)

#define hk_atomic_fetch_add(value, add)                                      \
    __sync_fetch_and_add(value, add)

#define hk_atomic_add_fetch(value, add)                                      \
    __sync_add_and_fetch(value, add)

#define hk_atomic_fetch_sub(value, add)                                      \
    __sync_fetch_and_sub(value, add)

#define hk_atomic_sub_fetch(value, add)                                      \
    __sync_sub_and_fetch(value, add)
    
#endif /* __HK_ATOMIC_H */
