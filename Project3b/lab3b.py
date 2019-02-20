import sys
import csv
import math

if len(sys.argv) != 2:
	sys.stderr.write("Invalid number of arguments")
	sys.exit(1)

try:
	file = open(sys.argv[1], mode='r');
except OSError:
	sys.stderr.write("Error opening file")
	sys.exit(1)

data = csv.reader(file)

totalblocks = 0;
totalinodes = 0;
blocksize = 0;
reservedblocks = 0;
reservedinodes = 0;
bfree = set()
ifree = set()
inodes = []
dirents = []
inds = []

blockmap = {}
parentmap = {2: 2}
linkmap = {}

for row in data: ## Assuming 1 group
	if row[0] == "SUPERBLOCK":
		totalblocks = int(row[1])
		totalinodes = int(row[2])
		blocksize = int(row[3])
		reservedinodes = int(row[7]) - 1

	if row[0] == "GROUP":
		inodetable = int(row[8])	

	if row[0] == "BFREE":
		bfree.add(int(row[1]))

	if row[0] == "IFREE":
		ifree.add(int(row[1]))

	if row[0] == "INODE":
		inodes.append((row[0], int(row[1]), row[2], row[3], int(row[4]), 
			int(row[5]), int(row[6]), row[7], row[8], row[9], int(row[10]), 
			int(row[11]), int(row[12]), int(row[13]), int(row[14]), int(row[15]), 
			int(row[16]), int(row[17]), int(row[18]), int(row[19]), int(row[20]), 
			int(row[21]), int(row[22]), int(row[23]), int(row[24]), int(row[25]),
			int(row[26])))
		
		linkmap.update({int(row[1]): [int(row[6]), 0]})

	if row[0] == "DIRENT":
		dirents.append((row[0], int(row[1]), int(row[2]), int(row[3]), int(row[4]),
			int(row[5]), row[6]))
		
		if row[6] != "'.'" and row[6] != "'..'":
			parentmap.update({int(row[3]): int(row[1])})

	if row[0] == "INDIRECT":
		inds.append((row[0], int(row[1]), int(row[2]), int(row[3]), int(row[4]),
			int(row[5])))

reservedblocks = inodetable + (totalinodes * 128 / blocksize)
reservedblocks = math.ceil(reservedblocks)

for inode in inodes:
	if not (inode[2] == "d" and  int(inode[10]) <= 60):
		for i in range(12, 27):
			offset = i - 12
			level = 0

			if i == 24:
				level = 1
			elif i == 25:
				offset = 268
				level = 2
			elif i == 26:
				offset = 65804
				level = 3

			if inode[i] != 0 and (inode[i] < reservedblocks or inode[i] >= totalblocks):
				print("RESERVED " if inode[i] < reservedblocks else "INVALID ", end='')	
				print("INDIRECT " if i == 24 else "", end='')
				print("DOUBLE INDIRECT " if i == 25 else "", end='')
				print("TRIPLE INDIRECT " if i == 26 else "", end='')
				print("BLOCK", inode[i], "IN INODE", inode[1],
					"AT OFFSET", offset)
			else:
				if inode[i] in blockmap:
					blockmap[inode[i]].append((inode[1], offset, level))
				else:
					blockmap.update({inode[i]: [(inode[1], offset, level)]})

for ind in inds:
	if ind[5] != 0 and (ind[5] < reservedblocks or ind[5] >= totalblocks):
		print("RESERVED " if ind[4] < reservedblocks else "INVALID ", end='')
		print("DOUBLE INDIRECT " if ind[2] == 3 else "", end='')
		print("INDIRECT " if ind[2] == 2 else "", end='')
		print("BLOCK", ind[4], "IN INODE", ind[1], "AT OFFSET", ind[3])
	else:
		if ind[5] in blockmap:
			blockmap[ind[5]].append((ind[1], ind[3], ind[2] - 1))
		else:
			blockmap.update({ind[5]: [(ind[1], ind[3], ind[2] - 1)]})

for i in range(reservedblocks, totalblocks):
	if(i in blockmap and i in bfree):
		print("ALLOCATED BLOCK", i, "ON FREELIST")

	if(not (i in blockmap or i in bfree)):
		print("UNREFERENCED BLOCK", i)

	if(i in blockmap and (len(blockmap[i]) > 1)):
		for j in range(len(blockmap[i])):
			print("DUPLICATE ", end='')
			print("INDIRECT " if blockmap[i][j][2] == 1 else "", end='')
			print("DOUBLE INDIRECT " if blockmap[i][j][2] == 2 else "", end='')
			print("TRIPLE INDIRECT " if blockmap[i][j][2] == 3 else "", end='')
			print("BLOCK", i, "IN INODE", blockmap[i][j][0], "AT OFFSET", blockmap[i][j][1])

for i in range(2, totalinodes + 1):
	if(i < 3 or i > reservedinodes): # We check 2 as it is the root inode
		if(i in linkmap and i in ifree):
			print("ALLOCATED INODE", i, "ON FREELIST")

		if(not (i in linkmap or i in ifree)):
			print("UNALLOCATED INODE", i, "NOT ON FREELIST")

for dirent in dirents:
	if dirent[6] == "'.'" and dirent[1] != dirent[3]:
		print("DIRECTORY INODE", dirent[1], "NAME", dirent[6], "LINK TO INODE", dirent[3],
			"SHOULD BE", dirent[1])

	if dirent[6] == "'..'":
		if dirent[1] == 2 and dirent[3] != 2:
			print("DIRECTORY INODE", dirent[1], "NAME", dirent[6], "LINK TO INODE",
				dirent[3], "SHOULD BE", dirent[1])
		elif dirent[1] != 2 and dirent[3] != parentmap[dirent[1]]:
			print("DIRECTORY INODE", dirent[1], "NAME", dirent[6], "LINK TO INODE",
				dirent[3], "SHOULD BE", parentmap[dirent[1]])
				
	if dirent[3] < 1 or dirent[3] > totalinodes:
		print("DIRECTORY INODE", dirent[1], "NAME", dirent[6], "INVALID INODE", dirent[3])
	elif not dirent[3] in linkmap:
		print("DIRECTORY INODE", dirent[1], "NAME", dirent[6], "UNALLOCATED INODE", dirent[3])
	else:
		linkmap[dirent[3]][1] += 1

for inode in linkmap:
	if linkmap[inode][0] != linkmap[inode][1]:
		print("INODE", inode, "HAS", linkmap[inode][1], "LINKS BUT LINKCOUNT IS", linkmap[inode][0])
