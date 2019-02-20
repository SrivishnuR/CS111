#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include "ext2_fs.h"

int block_offset(int blockNum);
int get_bit(char byte, int bitNum);
void get_filetype(char* type, int imode);
void get_time(char* timeString, time_t sec);
int print_dir_block(int owner, int block, int logicalBlock);
int print_dir_indirect(int owner, int level, int blockNum, int logicalBlockOffset);
void print_indirect(int owner, int level, int blockNum, int logicalBlockOffset);

int blockSize = EXT2_MIN_BLOCK_SIZE;
int id;

int main(int argc, char** argv) {
	if(argc != 2) {
		fprintf(stderr, "Error with ammount of arguments\n");
		exit(1);
	}

	if((id = open(argv[1], O_RDONLY)) == -1) {
		fprintf(stderr, "Error with opening file\n");
		exit(1);
	}

	int i, j;
	char* block;

	// SUPERBLOCK
	if((block = (char *)malloc(blockSize * sizeof(char))) == NULL) {
		fprintf(stderr, "Error allocating memory\n");
		exit(1);
	}

	struct ext2_super_block superblock;
	pread(id, block, blockSize, block_offset(1));
	memcpy(&superblock, block, sizeof(superblock));

	blockSize = EXT2_MIN_BLOCK_SIZE << superblock.s_log_block_size;

	printf("SUPERBLOCK,%d,%d,%d,%d,%d,%d,%d\n",
			superblock.s_blocks_count, superblock.s_inodes_count,
			blockSize, superblock.s_inode_size, superblock.s_blocks_per_group, 
			superblock.s_inodes_per_group, superblock.s_first_ino);

	if((block = (char *)realloc(block, blockSize * sizeof(char))) == NULL) {
		fprintf(stderr, "Error allocating memory\n");
		exit(1);
	}

	// GROUP DESCRIPTORS (ASSUMING ONLY 1 GROUP)	
	struct ext2_group_desc groupdesc;
	pread(id, block, blockSize, block_offset(2));
	memcpy(&groupdesc, block, sizeof(groupdesc));

	printf("GROUP,0,%d,%d,%d,%d,%d,%d,%d\n",
		superblock.s_blocks_count, superblock.s_inodes_count,
		groupdesc.bg_free_blocks_count, groupdesc.bg_free_inodes_count,
		groupdesc.bg_block_bitmap, groupdesc.bg_inode_bitmap,
		groupdesc.bg_inode_table);

	// FREE BLOCK ENTRIES
	pread(id, block, blockSize, block_offset(groupdesc.bg_block_bitmap));

	for(i = 0; i < (int)superblock.s_blocks_count; i++) {
		for(j = 0; j < 8; j++) {
			if(!get_bit(block[i], j))
				printf("BFREE,%d\n", (8 * i) + j + 1);
		}
	}

	// FREE I-NODE ENTRIES
	pread(id, block, blockSize, block_offset(groupdesc.bg_inode_bitmap));

	for(i = 0; i < (int)superblock.s_inodes_count; i++) {
		for(j = 0; j < 8; j++) {
			if(!get_bit(block[i], j))
				printf("IFREE,%d\n", (8 * i) + j + 1);
		}
	}

	// I-NODE SUMMARY
	int inodeOffset, contFlag;
	char fileType;
	char changTime[20], modTime[20], accesTime[20];
	
	struct ext2_inode inode;
	char inodeData[sizeof(inode)];	
	
	for(i = 0; i < (int)superblock.s_inodes_count; i++) {
		fileType = '?';
		inodeOffset = block_offset((int)groupdesc.bg_inode_table) + (i * sizeof(inode));
		pread(id, inodeData, sizeof(inode), inodeOffset);
		memcpy(&inode, inodeData, sizeof(inode));

		if(inode.i_mode && inode.i_links_count) {
			get_filetype(&fileType, inode.i_mode);
			get_time(changTime, inode.i_ctime);
			get_time(modTime, inode.i_mtime);
			get_time(accesTime, inode.i_atime);

			printf("INODE,%d,%c,%o,%d,%d,%d,%s,%s,%s,%ld,%d",
					(i + 1), fileType, (inode.i_mode & 0x0FFF),
					inode.i_uid, inode.i_gid, inode.i_links_count,
					changTime, modTime, accesTime, 
					((unsigned long) inode.i_size) | (((unsigned long) inode.i_dir_acl) << 32),
					inode.i_blocks);
	
			if(fileType == 'd' || fileType == 'f' || (fileType == 's' && inode.i_size > 60))
				printf(",%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u",
					inode.i_block[0], inode.i_block[1], inode.i_block[2],
					inode.i_block[3], inode.i_block[4], inode.i_block[5],
					inode.i_block[6], inode.i_block[7], inode.i_block[8],
					inode.i_block[9], inode.i_block[10], inode.i_block[11],
					inode.i_block[12], inode.i_block[13], inode.i_block[14]);

			printf("\n");
		}	

		if(fileType == 'd') {
			for(j = 0; j < 12; j++) {
				contFlag = print_dir_block(i + 1, inode.i_block[j], j);
				if(!contFlag)
					break;
			}
		       
			if(!contFlag)
				contFlag = print_dir_indirect(i + 1, 1, inode.i_block[12], 12);
			
			if(!contFlag)
				contFlag = print_dir_indirect(i + 1, 2, inode.i_block[13], 12 + (blockSize / 4));
			
			if(!contFlag)
				print_dir_indirect(i + 1, 3, inode.i_block[14], 
						12 + (blockSize / 4) + ((blockSize / 4) * (blockSize / 4)));
		}

		if(fileType == 'd' || fileType == 'f') {
			print_indirect(i + 1, 1, inode.i_block[12], 12);	
			print_indirect(i + 1, 2, inode.i_block[13], 12 + (blockSize / 4));
			print_indirect(i + 1, 3, inode.i_block[14], 
					12 + (blockSize / 4) + ((blockSize / 4) * (blockSize / 4)));
		}
	}

	return 0;
}

int block_offset(int blockNum) {
	return blockNum * blockSize;
}

int get_bit(char byte, int bitNum) {
	return (byte & (1 << bitNum));
}

void get_filetype(char* type, int imode) {
	if((imode & 0xA000) == 0xA000)
		*type = 's';
	else if((imode & 0x8000) == 0x8000)
		*type = 'f';
	else if((imode & 0x4000) == 0x4000)
		*type = 'd';
	else 
		*type = '?';
}

void get_time(char* timeString, time_t sec) {
	struct tm *time = gmtime(&sec);
	strftime(timeString, 20, "%m/%d/%y %H:%M:%S", time);
}

int print_dir_block(int owner, int blockNum, int logicalBlock) {
	int baseOffset = block_offset(blockNum);
	int dirOffset = baseOffset;
	int logicalOffset = block_offset(logicalBlock);

	struct ext2_dir_entry dir;
	char dirData[sizeof(dir)];
	pread(id, dirData, sizeof(dir), dirOffset);
	memcpy(&dir, dirData, sizeof(dir));

	while(((dirOffset - baseOffset) < blockSize)) {
		if(dir.inode == 0)
			return -1;

		printf("DIRENT,%d,%d,%d,%d,%d,'%s'\n",
				owner, logicalOffset, dir.inode, 
				dir.rec_len, dir.name_len, dir.name);

		dirOffset += dir.rec_len;
		logicalOffset += dir.rec_len;
		pread(id, dirData, sizeof(dir), dirOffset);
		memcpy(&dir, dirData, sizeof(dir));
	}

	return 0;
}

int print_dir_indirect(int owner, int level, int blockNum, int logicalBlockOffset) {
	int i, multOffset, logicalBlock;
	int *blockData = (int*)malloc((blockSize / 4) * sizeof(int));
	pread(id, blockData, blockSize, block_offset(blockNum));
	
	if(level == 1)
		multOffset = 1;
	else if (level == 2)
		multOffset = blockSize / 4;
	else if (level == 3)
		multOffset = (blockSize / 4) * (blockSize / 4);

	for(i = 0; i < blockSize / 4; i++) {
		logicalBlock = logicalBlockOffset + (i * multOffset);
		if(level == 1) {
			if(print_dir_block(owner, blockData[i], logicalBlock) == -1) {
				free(blockData);
				return -1;
			}
		} else
			print_dir_indirect(owner, level - 1, blockData[i], logicalBlock);
	}

	free(blockData);
	return 0;
}

void print_indirect(int owner, int level, int blockNum, int logicalBlockOffset) {
	if(blockNum == 0)
		return;
	
	int i, multOffset, logicalBlock;
	int *blockData = (int*)malloc((blockSize / 4) * sizeof(int));
	pread(id, blockData, blockSize, block_offset(blockNum));

	if(level == 1)
		multOffset = 1;
	else if (level == 2)
		multOffset = blockSize / 4;
	else if (level == 3)
		multOffset = (blockSize / 4) * (blockSize / 4);

	for(i = 0; i < blockSize / 4; i++) {
		if(blockData[i] != 0) {
			logicalBlock = logicalBlockOffset + (i * multOffset);
			printf("INDIRECT,%d,%d,%d,%d,%d\n",
				owner, level, logicalBlock, blockNum,
				blockData[i]);
			
			if(level > 1) 
				print_indirect(owner, level - 1, blockData[i], logicalBlock);
		}
	}

	free(blockData);
}
