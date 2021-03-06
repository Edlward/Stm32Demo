#ifndef MACROS_H_
#define MACROS_H_

#include "stdint.h"

#ifdef __cplusplus
extern "C" {
#endif
	
    #define MACRO_CONS(x, y) MACRO_CONS_(x, y)
    #define MACRO_CONS_(x, y) x##y
    
    #define LIST_MAP(list, proc)  do{                                 \
        for(int list_iter_ = 0; list_iter_ < GET_ARRAY_SIZE(list); list_iter_++){        \
            proc(&(list[list_iter_]));                                      \
        }                                                              \
    }   while(0)
    
	#define GET_ARRAY_SIZE(a) 				(sizeof(a) / sizeof(a[0]))
		
	#define BIT_SET(a, n)					a = a | (((uint32_t)1) << n)
	#define BIT_RESET(a, n) 				a = a & ((((uint32_t)1) << n) ^ 0xFFFFFFFF)
	#define BIT_REVERSE(a, n) 				a = a ^ (((uint32_t)1) << n)
			
	#define MEM_NEW_OBJ(T, n) 				(T*)malloc(sizeof(T) * n)

	#define MEMBER_START_OFST(obj, m) 		((intptr_t)(&obj.m) - (intptr_t)(&obj))
	#define MEMBER_END_OFST(obj, m)			((intptr_t)(&obj.m) - (intptr_t)(&obj) + sizeof(obj.m))

	#ifdef __GNUC__ 
		#define __weak __attribute__((weak))
	#else

	#endif //__GNUC__ 
	
	#define MIN2(x, y) ((x)<(y) ? (x):(y))
	#define MIN3(x, y, z) MIN2((x), MIN2((y), (z)))


#ifdef __cplusplus
}
#endif

#endif //MACROS_H_
