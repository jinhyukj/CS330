/*Edited by Jin-Hyuk Jang
We need the following functions to deal with fixed-point arithmetics*/

#include "threads/fp-arithmetic.h"
#include "stdint.h"

int itf(int n)
{
    return n * F;
}

int fti(int x)
{
    return x / F;
}

int ftir(int x)
{
    if (x >= 0)
        return (x + F / 2) / F;

    else
        return (x - F / 2) / F;
}

int addf(int x, int y)
{
    return x + y;
}

int subf(int x, int y)
{
    return x - y;
}

int addif(int x, int n)
{
    return x + n * F;
}

int subif(int x, int n)
{
    return x - n * F;
}

int multf(int x, int y)
{
    return ((int64_t)x) * y / F;
}

int multif(int x, int n)
{
    return x * n;
}

int divf(int x, int y)
{
    return ((int64_t)x) * F / y;
}

int divif(int x, int n)
{
    return x / n;
}

/*Edited by Jin-Hyuk Jang(project 1 - advanced scheduler)*/