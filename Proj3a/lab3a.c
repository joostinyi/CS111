// NAME: Justin Yi
// EMAIL: joostinyi00@gmail.com
// ID: 905123893

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include "ext2_fs.h"
#include <stdint.h>

#define BASE_OFFSET 1024 /* location of the super-block in the first group */
#define BLOCK_OFFSET(block) (BASE_OFFSET + (block - 1) * block_size)

int fd = -1, opt;
unsigned int inodes_count = 0, blocks_count = 0, block_size = 0,
             blocks_per_group = 0, inodes_per_group = 0, inode_size = 0,
             free_block_bitmap, free_inode_bitmap, group_count, first_ino, first_ino_block;

struct ext2_super_block super;
struct ext2_group_desc group;
struct ext2_inode inode;

void superBlock();
void groupScan();
void scan_free();
void inode_summary(unsigned int freeInodes);
void dirEntry(int parent, unsigned int offset);
void dir_summary(int parent, unsigned int offset);
void indirEntry(int parent, unsigned int blocks[]);

char *convert_time(unsigned int time);

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        fprintf(stderr, "usage: lab3a fsImage\n");
        exit(1);
    }
    if ((fd = open(argv[1], O_RDONLY)) < 0)
    {
        fprintf(stderr, "Unable to mount file system\n");
        exit(1);
    }

    superBlock();
    groupScan();
}

void superBlock()
{
    lseek(fd, 1024, SEEK_SET); /* position head above super-block */
    if (read(fd, &super, sizeof(super)) < 0)
    {
        fprintf(stderr, "Error Reading superblock\n");
        exit(2);
    }
    if (super.s_magic != EXT2_SUPER_MAGIC)
    {
        fprintf(stderr, "bad file system\n");
        exit(1);
    }
    inodes_count = super.s_inodes_count;
    blocks_count = super.s_blocks_count;
    block_size = 1024 << super.s_log_block_size;
    inode_size = super.s_inode_size;
    blocks_per_group = super.s_blocks_per_group;
    inodes_per_group = super.s_inodes_per_group;
    first_ino = super.s_first_ino;
    /* calculate number of block groups on the disk */
    group_count = 1 + (super.s_blocks_count - 1) / super.s_blocks_per_group;
    fprintf(stdout, "SUPERBLOCK,%u,%u,%u,%u,%u,%u,%u\n", blocks_count, inodes_count,
            block_size, inode_size, blocks_per_group, inodes_per_group, first_ino);
}

void groupScan()
{
    if (pread(fd, &group, sizeof(struct ext2_group_desc), 1024 + sizeof(struct ext2_super_block)) < 0)
    {
        fprintf(stderr, "Error Reading group\n");
        exit(2);
    }
    inodes_count = super.s_inodes_count;
    blocks_count = super.s_blocks_count;
    unsigned short freeBlocks = group.bg_free_blocks_count;
    unsigned short freeInodes = group.bg_free_inodes_count;
    free_block_bitmap = group.bg_block_bitmap;
    free_inode_bitmap = group.bg_inode_bitmap;
    first_ino_block = group.bg_inode_table;

    fprintf(stdout, "GROUP,0,%u,%u,%u,%u,%u,%u,%u\n", blocks_count, inodes_count,
            freeBlocks, freeInodes, free_block_bitmap, free_inode_bitmap, first_ino_block);
    scan_free();
    inode_summary(inodes_count);
}

void scan_free()
{
    for (unsigned int i = 1; i < blocks_count; i++)
    {
        int bit = i & 7;
        char byte;
        if (pread(fd, &byte, 1, free_block_bitmap * block_size + (i >> 3)) < 0)
        {
            fprintf(stderr, "Error Reading bitmap\n");
            exit(2);
        }
        if (((byte >> bit) & 1) == 0)
            fprintf(stdout, "BFREE,%u\n", i + 1);
    }
    for (unsigned int i = 0; i < inodes_count; i++)
    {
        int bit = i & 7;
        char byte;
        if (pread(fd, &byte, 1, free_inode_bitmap * block_size + (i >> 3)) < 0)
        {
            fprintf(stderr, "Error Reading bitmap\n");
            exit(2);
        }
        if (((byte >> bit) & 1) == 0)
            fprintf(stdout, "IFREE,%u\n", i + 1);
    }
}

void inode_summary(unsigned int inodes_count)
{
    for (unsigned int i = 0; i < inodes_count; i++)
    {
        // free check
        int bit = i & 7;
        char byte;
        if (pread(fd, &byte, 1, free_inode_bitmap * block_size + (i >> 3)) < 0)
        {
            fprintf(stderr, "Error Reading bitmap\n");
            exit(2);
        }
        if (((byte >> bit) & 1) == 0)
            continue;

        if (pread(fd, &inode, inode_size, 1024 + (first_ino_block - 1) * (block_size) + i * sizeof(struct ext2_inode)) < 0)
        {
            fprintf(stderr, "Error Reading inode\n");
            exit(2);
        }
        __u16 imode = inode.i_mode;
        if (imode == 0 || inode.i_links_count == 0)
            continue;
        char file = '?';
        if (S_ISDIR(imode))
            file = 'd';
        else if (S_ISREG(imode))
            file = 'f';
        else if (S_ISLNK(imode))
            file = 's';

        __u16 owner = inode.i_uid, group = inode.i_gid, link_count = inode.i_links_count;
        char *changeTime = convert_time(inode.i_ctime);
        char *modTime = convert_time(inode.i_mtime);
        char *accTime = convert_time(inode.i_atime);
        unsigned int size = inode.i_size;
        unsigned int blocks_count = inode.i_blocks;

        fprintf(stdout, "INODE,%d,%c,%o,%u,%u,%u,%s,%s,%s,%u,%u", i + 1, file, (imode & 0xFFF),
                owner, group, link_count, changeTime, modTime, accTime, size, blocks_count);

        for (int j = 0; j < 15; j++)
        {
            if (file == 's' && size <= 60)
                continue;
            fprintf(stdout, ",%u", inode.i_block[j]);
        }
        fprintf(stdout, "\n");

        if (file == 'd')
            // indirect blocks encapsulated in dir summary
            dir_summary(i + 1, 1024 + (first_ino_block - 1) * (block_size) + i * sizeof(struct ext2_inode));
        else
            indirEntry(i + 1, inode.i_block);
    }
}

char *convert_time(unsigned int time)
{
    char *timeStr = malloc(23 * sizeof(char));
    time_t timer = time;
    struct tm *tm = gmtime(&timer);
    strftime(timeStr, 32, "%m/%d/%y %H:%M:%S", tm);
    return timeStr;
}

void dirEntry(int parent, unsigned int offset)
{
    struct ext2_dir_entry entry;
    unsigned int iter = offset;
    while (iter < offset + block_size)
    { //size is less than the total size of the inode
        if (pread(fd, &entry, sizeof(struct ext2_dir_entry), iter) < 0)
        {
            fprintf(stderr, "Error Reading dir\n");
            exit(2);
        }
        unsigned int logOff = iter - offset, inum = entry.inode, length = entry.rec_len,
                     nameLen = entry.name_len;
        iter += length;
        if (!inum)
            continue;
        char file_name[EXT2_NAME_LEN + 1];
        memcpy(file_name, entry.name, entry.name_len);
        file_name[entry.name_len] = 0; /* append null char to the file name */
        fprintf(stdout, "DIRENT,%u,%u,%u,%u,%u,'%s'\n", parent, logOff, inum, length, nameLen, file_name);
    }
}

void dir_summary(int parent, unsigned int offset)
{
    int index, l1, l2, l3;
    unsigned int i, j, k;
    for (int i = 0; i < 12; i++)
    {
        // offset + 40 === __u32	i_block[EXT2_N_BLOCKS];/* Pointers to blocks */
        if (pread(fd, &index, 4, offset + 40 + 4 * i) < 0)
        {
            fprintf(stderr, "Error Reading dir\n");
            exit(2);
        }
        if (index)
            dirEntry(parent, block_size * index);
    }

    // 1st indirection
    if (pread(fd, &l1, 4, offset + 40 + 48) < 0)
    {
        fprintf(stderr, "Error Reading dir\n");
        exit(2);
    }
    if (l1)
        for (j = 0; j < block_size / 4; j++)
        {
            if (pread(fd, &index, 4, l1 * block_size + 4 * j) < 0)
            {
                fprintf(stderr, "Error Reading dir\n");
                exit(2);
            }
            if (index)
                dirEntry(parent, block_size * index);
        }

    // 2nd indirection
    if (pread(fd, &l2, 4, offset + 40 + 52) < 0)
    {
        fprintf(stderr, "Error Reading dir\n");
        exit(2);
    }
    if (l2)
        for (i = 0; i < block_size / 4; i++)
        {
            if (pread(fd, &l1, 4, l2 * block_size + 4 * i) < 0)
            {
                fprintf(stderr, "Error Reading dir\n");
                exit(2);
            }
            if (l1)
            {
                for (j = 0; j < block_size / 4; j++)
                {
                    if (pread(fd, &index, 4, l1 * block_size + 4 * j) < 0)
                    {
                        fprintf(stderr, "Error Reading dir\n");
                        exit(2);
                    }
                    if (index)
                        dirEntry(parent, block_size * index);
                }
            }
        }

    // 3rd indirection
    if (pread(fd, &l3, 4, offset + 40 + 56) < 0)
    {
        fprintf(stderr, "Error Reading dir\n");
        exit(2);
    }
    if (l3)
        for (i = 0; i < block_size / 4; i++)
        {
            if (pread(fd, &l2, 4, offset + 40 + 52) < 0)
            {
                fprintf(stderr, "Error Reading dir\n");
                exit(2);
            }
            if (l2)
                for (j = 0; j < block_size / 4; j++)
                {
                    if (pread(fd, &l1, 4, l2 * block_size + 4 * j) < 0)
                    {
                        fprintf(stderr, "Error Reading dir\n");
                        exit(2);
                    }
                    if (l1)
                    {
                        for (k = 0; k < block_size / 4; k++)
                        {
                            if (pread(fd, &index, 4, l1 * block_size + 4 * k) < 0)
                            {
                                fprintf(stderr, "Error Reading dir\n");
                                exit(2);
                            }
                            if (index)
                                dirEntry(parent, block_size * index);
                        }
                    }
                }
        }
}

void indirEntry(int parent, unsigned int blocks[])
{

    unsigned int offset, block_num, offset1, block_num1, offset2, block_num2, i, j, k;

    if (blocks[12])
    {
        offset = blocks[12] * block_size;
        for (i = 0; i < block_size / 4; i++)
        {
            if (pread(fd, &block_num, sizeof(unsigned int), offset + 4 * i) < 0)
            {
                fprintf(stderr, "Error Reading dir\n");
                exit(2);
            }
            if (block_num)
                fprintf(stdout, "INDIRECT,%u,%u,%u,%u,%u\n", parent, 1, 12 + i, blocks[12], block_num);
        }
    }

    if (blocks[13])
    {
        offset1 = blocks[13] * block_size;
        for (i = 0; i < block_size / 4; i++)
        {
            if (pread(fd, &block_num1, sizeof(unsigned int), offset1 + 4 * i) < 0)
            {
                fprintf(stderr, "Error Reading dir\n");
                exit(2);
            }

            if (block_num1)
            {
                offset = block_num1 * block_size;
                fprintf(stdout, "INDIRECT,%u,%u,%u,%u,%u\n", parent, 2,
                        12 + block_size / 4 + block_size / 4 * i, blocks[13], block_num1);
                for (j = 0; j < block_size / 4; j++)
                {
                    if (pread(fd, &block_num, sizeof(unsigned int), offset + 4 * j) < 0)
                    {
                        fprintf(stderr, "Error Reading dir\n");
                        exit(2);
                    }
                    if (block_num)
                        fprintf(stdout, "INDIRECT,%u,%u,%u,%u,%u\n", parent, 1,
                                12 + block_size / 4 + block_size / 4 * i + j, block_num1, block_num);
                }
            }
        }
    }

    if (blocks[14])
    {
        offset2 = blocks[14] * block_size;
        for (i = 0; i < block_size / 4; i++)
        {
            if (pread(fd, &block_num2, sizeof(unsigned int), offset2 + 4 * i) < 0)
            {
                fprintf(stderr, "Error Reading dir\n");
                exit(2);
            }
            if (block_num2)
            {
                offset1 = block_num2 * block_size;
                fprintf(stdout, "INDIRECT,%u,%u,%u,%u,%u\n", parent, 3,
                        12 + block_size / 4 + block_size / 4 * block_size / 4 + i * block_size * block_size / 8,
                        blocks[14], block_num2);
                for (j = 0; j < block_size / 4; j++)
                {
                    if (pread(fd, &block_num1, sizeof(unsigned int), offset1 + 4 * j) < 0)
                    {
                        fprintf(stderr, "Error Reading dir\n");
                        exit(2);
                    }
                    if (block_num1)
                    {
                        offset = block_num1 * block_size;
                        fprintf(stdout, "INDIRECT,%u,%u,%u,%u,%u\n", parent, 2,
                                12 + block_size / 4 + block_size / 4 * block_size / 4 + i * block_size * block_size / 8 + j * block_size / 4,
                                block_num2, block_num1);
                        for (k = 0; k < block_size / 4; k++)
                        {
                            if (pread(fd, &block_num, sizeof(unsigned int), offset + 4 * k) < 0)
                            {
                                fprintf(stderr, "Error Reading dir\n");
                                exit(2);
                            }
                            if (block_num)
                                fprintf(stdout, "INDIRECT,%u,%u,%u,%u,%u\n", parent, 1,
                                        12 + block_size / 4 + block_size / 4 * block_size / 4 + i * block_size * block_size / 8 + j * block_size / 4 + k,
                                        block_num1, block_num);
                        }
                    }
                }
            }
        }
    }
}
