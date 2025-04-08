#include "SysCall_learning.h"

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>

static const char *filename = "myfile.txt";
static const char *someString = "Hello World! To Be Written to the file\n";
static char buffer[100];

void syscall_file_manipulations(void)
{
    int file, retVal, readCnt, writeCnt;

    // Task 1. Using system call
    retVal = write(STDOUT_FILENO, "HelloWorld using system call!\n", sizeof("HelloWorld using system call!\n"));

    // permissions
    // 0  USER     GROUP    OTHER
    // 0  R: 4     W: 2     X: 1
    file = open("newfile.txt", O_RDWR | O_CREAT, 0750);

    if (file < 0)
    {
        printf("Error\n");
        exit(1);
    }

    retVal = remove("newfile.txt");
    if (retVal < 0)
    {
        printf("Error remove %d", retVal);
        //exit(1);
    }

    /* Read from file */
     file = open("readFile.txt", O_RDONLY, 0777);

     if (file < 0)
     {
         printf("File opening error!\n");
         exit(1);
     }


     readCnt = read(file, buffer, 99);

     while(readCnt > 0)
     {
         printf("%.*s", readCnt, buffer);
         readCnt = read(file, buffer, 99);
    }

     printf("\n");

    if (readCnt < 0)
    {
        printf("File reading error!\n");
        exit(1);
    }
    else
    {
        printf("%s\n", buffer);
    }

    // 0  USER   GROUP   OTHER
    // 0  RWX    RX      RX
    // R:4;W:2;X:1
    file = open(filename, O_RDWR | O_CREAT, 0755);

     if (file < 0)
     {
         printf("Error Openinig/Creating file!\n");
         exit(1);
     }

     writeCnt = write(file, someString, strlen(someString));

     if (writeCnt < 0)
     {
         printf("Error Writing to file!\n");
         exit(1);
     }

     retVal = close(file);
     if (retVal < 0)
     {
         printf("Error close %d", retVal);
         exit(1);
    }
}
