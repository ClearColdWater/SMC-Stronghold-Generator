#pragma once

#ifdef _OPENMP
#include<omp.h>
#else
constexpr inline int omp_get_thread_num(){ return 0; }
constexpr inline int omp_get_max_threads(){ return 1; }
#endif