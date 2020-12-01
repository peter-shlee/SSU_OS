#include "ssufs-ops.h"

char buf[BLOCKSIZE];
char buf2[BLOCKSIZE * MAX_FILE_SIZE];

//int main()
//{
//    char str[] = "!-------32 Bytes of Data-------!!-------32 Bytes of Data-------!";
//    ssufs_formatDisk();
//
//    ssufs_create("f1.txt");
//    int fd1 = ssufs_open("f1.txt");
//
//    printf("Write Data: %d\n", ssufs_write(fd1, str, BLOCKSIZE));
//    printf("Seek: %d\n", ssufs_lseek(fd1, 0));
//    ssufs_dump();
//    ssufs_delete("f1.txt");
//    ssufs_dump();
//}

int main()
{
    char str[] = "!-------32 Bytes of Data-------!!-------32 Bytes of Data-------!";
    char str2[] = "!-20 Bytes of Data-!";
    char str3[] = "!---24 Bytes of Data---!";
    ssufs_formatDisk();

    // create and open test
    ssufs_create("f1.txt");
    int fd1 = ssufs_open("f1.txt");
    ssufs_create("f2.txt");
    int fd2 = ssufs_open("f2.txt");

    // write test
    printf ("***write test***\n");
    printf("Write Data: %d\n", ssufs_write(fd1, str, BLOCKSIZE));
    printf("Write Data: %d\n", ssufs_write(fd1, str2, 20));
    printf("Write Data: %d\n", ssufs_write(fd2, str2, 20));
    printf("Write Data: %d\n", ssufs_write(fd2, str3, 24));
    printf("Write Data: %d\n", ssufs_write(fd2, str, BLOCKSIZE));
    printf("Write Data: %d\n", ssufs_write(fd2, str2, 20));
    printf("Write Data: %d\n", ssufs_write(fd2, str3, 24));
    printf("Seek: %d\n", ssufs_lseek(fd1, 0));
    printf("Seek: %d\n", ssufs_lseek(fd2, -152));
    ssufs_dump();

    // read test
    printf ("***read test***\n");
    printf("Read Data: %d\n", ssufs_read(fd2, buf, 16));
    printf("%s\n", buf);
    printf("Read Data: %d\n", ssufs_read(fd2, buf, 16));
    printf("%s\n", buf);
    printf("Read Data: %d\n", ssufs_read(fd2, buf, 16));
    printf("%s\n", buf);
    printf("Read Data: %d\n", ssufs_read(fd2, buf, 32));
    printf("%s\n", buf);
    printf("Seek: %d\n", ssufs_lseek(fd2, -80));
    ssufs_dump();

    // read test2
    printf ("***read test2***\n");
    printf("Read Data: %d\n", ssufs_read(fd2, buf2, 152));
    printf("%s\n", buf2);
    printf("Seek: %d\n", ssufs_lseek(fd2, -152));
    ssufs_dump();

    // rewrite test
    printf ("***rewrite test***\n");
    printf("Seek: %d\n", ssufs_lseek(fd2, 62));
    printf("Write Data: %d\n", ssufs_write(fd2, "hello world", 11));
    printf("Seek: %d\n", ssufs_lseek(fd2, 10));
    printf("Write Data: %d\n", ssufs_write(fd2, "hello world", 11));
    ssufs_dump();

    // delete test
    printf ("***delete test***\n");
    ssufs_delete("f1.txt");
    ssufs_dump();
    ssufs_delete("f2.txt");
    ssufs_dump();
}
