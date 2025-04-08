/*
 * stack_ll.c
 *
 *  Created on: Aug 26, 2023
 *      Author: krudenko
 */

#include "stack_ll.h"

bool StackLL_push(stackll *mystack, int value)
{
	node *newnode = malloc(sizeof(node));

	if (newnode == NULL)
	{
		return false;
	}

	newnode->value = value;
	newnode->next = *mystack;
	*mystack = newnode;
	return true;
}

int StackLL_pop(stackll *mystack)
{
	if (*mystack == NULL)
	{
		return STACK_EMPTY;
	}

	int result = (*mystack)->value;
	node *tmp = *mystack;
	*mystack = (*mystack)->next;
	free(tmp);
	return result;
}

void StackLL_Driver(void)
{
	/* Stack - linker-list-based implementation START */
	stackll s1 = NULL, s2 = NULL, s3 = NULL;
	StackLL_push(&s1, 56);
	StackLL_push(&s2, 78);
	StackLL_push(&s2, 23);
	StackLL_push(&s2, 988);
	StackLL_push(&s3, 13);

	int t;
	while ((t = StackLL_pop(&s2)) != STACK_EMPTY)
	{
		printf("t = %d\n", t);
	}
	/* Stack - linker-list-based implementation END */
}
