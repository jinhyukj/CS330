/*Edited by Jiin-Hyuk Jang
We need to have the following functions to deal with fixed-point arithmetic*/

#define INT_MAX ((1 << 31) - 1)
#define INT_MIN (-(1 << 31))
#define F (1 << 14)
#include "stdint.h"

/*Edited by Jin-Hyuk Jang
We need the following functions to deal with fixed-point arithmetics*/

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

/*Edited by Jin-Hyuk Jang (project 1 - advanced scheduler)*/