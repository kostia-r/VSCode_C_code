/*
 * stack_ll.h
 *
 *  Created on: Aug 26, 2023
 *      Author: krudenko
 */

#ifndef STACK_LL_H_
#define STACK_LL_H_

#include <stdbool.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#define STACK_EMPTY INT_MIN

typedef struct node
{
	int value;
	struct node *next;
} node;

typedef node *stackll;

extern bool StackLL_push(stackll *mystack, int value);
extern int StackLL_pop(stackll *mystack);

extern void StackLL_Driver(void);

#endif /* STACK_LL_H_ */
