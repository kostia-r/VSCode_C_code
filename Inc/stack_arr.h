/*
 * stack_arr.h
 *
 *  Created on: Aug 26, 2023
 *      Author: krudenko
 */

#ifndef STACK_ARR_H_
#define STACK_ARR_H_

#include <stdbool.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#define STACK_LENGTH 5
#define EMPTY (-1)
#define STACK_EMPTY INT_MIN

typedef struct
{
	int values [STACK_LENGTH];
	int top;
} stackArr;

extern bool StackArr_push(stackArr *mystack, int value);
extern int StackArr_pop(stackArr *mystack);

extern void StackArr_Driver(void);

#endif /* STACK_ARR_H_ */
