#include "module.h"

#include <stdio.h>
#include <stdint.h>

void print_hello(void)
{
  printf("Hello, World in debug!\n");
}

void Host_Info(void)
{
	uint32_t word = 0x12345678;
	uint8_t *word_ptr = (uint8_t*) (&word);
	printf("TARGET: %llu-BIT ", (unsigned long long)(sizeof(void*) * 8));

	if ((*(word_ptr) & 0xF) == 0x8)
	{
		printf("\t\tLittle-endian\r\n");
	}
	else
	{
		printf("\t\tBig-endian\r\n");
	}

	printf("\tWORD: \t\t0x%X 0x%X 0x%X 0x%X\r\n", (word & 0xFFFFFFFF) >> 24,
			(word & 0xFFFFFF) >> 16, (word & 0xFFFF) >> 8, (word & 0xFF));
	printf("\tIN MEMORY:\t");

	for (uint8_t i = 0; i < sizeof(int); i++)
	{
		printf("0x%X ", *((uint8_t*) &word + i));
	}
	printf("\r\n\r\n");
}

