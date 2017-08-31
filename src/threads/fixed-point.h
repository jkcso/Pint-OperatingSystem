#ifndef THREADS_FIXED_POINT_H
#define THREADS_FIXED_POINT_H

#define P 17
#define Q 14
#define F 1 << (Q)

/* Below x and y are fixed-point numbers and n is an integer. */

#define INT_TO_FIXED_POINT(n, d) ((n) * (F) / (d))
#define FIXED_POINT_TO_INT_TO_ZERO(x) ((x) / (F))
#define FIXED_POINT_TO_INT_TO_NEAREST(x) ((x) >= 0 ? (((x) + (F) / 2) / (F)) : \
                                                   (((x) - (F) / 2) / (F)))
#define ADD(x, y) ((x) + (y))
#define SUB(x, y) ((x) - (y))
#define MUL(x, y) (((int64_t) (x)) * (y) / (F))
#define DIV(x, y) (((int64_t) (x)) * (F) / (y))
#define ADD_FIXED_POINT_INT(x, n) ((x) + (n) * (F))
#define SUB_FIXED_POINT_INT(x, n) ((x) - (n) * (F))
#define MUL_FIXED_POINT_INT(x, n) ((x) * (n))
#define DIV_FIXED_POINT_INT(x, n) ((x) / (n))

#endif /* threads/fixed-point.h */
