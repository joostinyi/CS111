#!/usr/bin/env python3

# NAME: Justin Yi
# EMAIL: joostinyi00@gmail.com
# ID: 905123893

import sys, csv, math
sb = None ; group = None
max_block = None
max_inode = None
first_inode = None
first_unreserved = 0
ret = True
inodes = []; free_inodes = []; free_blocks = []; dir_entries = []
indir_entries = []; inode_nums = []
my_block_bitmap = {}


class Super_Block:
    def __init__(self, args):
        self.num_blocks = int(args[1])
        self.num_inodes  = int(args[2])
        self.block_size = int(args[3])
        self.inode_size = int(args[4])
        self.inodes_per_group = int(args[6])
        self.first_inode = int(args[7])

        global max_block
        max_block = self.num_blocks - 1
        global max_inode
        max_inode = self.num_inodes
        global first_inode
        first_inode = self.first_inode



class Group:
    def __init__(self, args):
        self.group = int(args[1])
        self.num_blocks  = int(args[2])
        self.num_inodes  = int(args[3])
        self.bitmap = int(args[6])
        self.first_block = int(args[8])

class Inode:
    def __init__(self, args):
        self.inode = int(args[1])
        self.type = args[2]
        self.mode = int(args[3])
        self.link_count = int(args[6])
        self.blocks = self.parse_blocks(args)
        self.block_pointers = self.parse_block_pointers(args)

    def parse_blocks(self, args):
        ret = []
        if self.type != 's':
            for i in range(12, 24):
                ret.append(int(args[i]))
        return ret
    
    def parse_block_pointers(self, args):
        ret = []
        if self.type != 's':
            for i in range(24, 27):
                ret.append(int(args[i]))
        return ret


class Dirent:
    def __init__(self, args):
        self.parent_inode = int(args[1])
        self.offset = int(args[2])
        self.inode = int(args[3])
        self.entry_len = int(args[4])
        self.name_len = int(args[5])
        self.name = str(args[6])


class Indirect:
    def __init__(self, args):
        self.inode = int(args[1])
        self.indirection = int(args[2])
        self.offset = int(args[3])
        self.block = int(args[5])
        

def parse_csv(file):
    global sb, group, inodes, free_inodes, free_blocks, dir_entries, indir_entries
    try:
        parser = csv.reader(open(file))
        for row in parser:
            if row[0] == 'SUPERBLOCK':
                sb = Super_Block(row)
            if row[0] == 'GROUP':
                group = Group(row)
            if row[0] == 'BFREE':
                free_blocks.append(int(row[1]))
            if row[0] == 'IFREE':
                free_inodes.append(int(row[1]))
            if row[0] == 'DIRENT':
                dir_entries.append(Dirent(row))
            if row[0] == 'INODE':
                inodes.append(Inode(row))
            if row[0] == 'INDIRECT':
                indir_entries.append(Indirect(row))

    except IOError:
        print('Bad csv file', file = sys.stderr)
        sys.exit(1)

    global first_unreserved 
    first_unreserved = group.first_block + math.ceil((sb.num_inodes * sb.inode_size) / sb.block_size)

    global inode_nums
    for i in range(len(inodes)):
        inode_nums.append(inodes[i].inode) # block numbers used by Boot Block, SB, .…

    

def data_block_audit():
    global my_block_bitmap; global max_block; global first_unreserved
    indirect_map = {0: '', 1: 'INDIRECT ', 2: 'DOUBLE INDIRECT ', 3: 'TRIPLE INDIRECT '} #map of levels to text for output
    offset_map = {1: 12, 2: (12 + 1024//4) , 3: (12 + 1024//4 + (1024//4)**2)}
    
    for inode in inodes:
        # print('INODE {}'.format(inode.inode))
        # print(*inode.blocks, sep=',')
        if inode.inode == 0: continue
        offset = 0
        level = indirect_map[0]
        for block in inode.blocks:
            if block != 0:
                if block < 0 or block > max_block:
                    wrappedPrint('INVALID BLOCK {} IN INODE {} AT OFFSET {}'.format(block, inode.inode, offset))
                if block < first_unreserved:
                    wrappedPrint('RESERVED BLOCK {} IN INODE {} AT OFFSET {}'.format(block, inode.inode, offset))
                elif block in my_block_bitmap:
                    my_block_bitmap[block].append([level, block, inode.inode, offset])
                else:
                    my_block_bitmap[block] = [[level, block, inode.inode, offset]]
            offset += 1


        # print('pointers')
        # print(*inode.block_pointers, sep=',')
        
        for i in range(len(inode.block_pointers)):
            pointer = inode.block_pointers[i]
            level = indirect_map[i+1]
            offset = offset_map[i+1]
            if pointer != 0:
                if pointer < 0 or pointer > max_block:
                    wrappedPrint('INVALID {}BLOCK {} IN INODE {} AT OFFSET {}'.format(level, pointer, inode.inode, offset))
                if pointer < first_unreserved:
                    wrappedPrint('RESERVED {}BLOCK {} IN INODE {} AT OFFSET {}'.format(level, pointer, inode.inode, offset))
                elif pointer in my_block_bitmap:
                    my_block_bitmap[pointer].append([level, pointer, inode.inode, offset])
                else:
                    my_block_bitmap[pointer] = [[level, pointer, inode.inode, offset]]
    
    

    for i in range(len(indir_entries)):
        indirent = indir_entries[i]
        level = indirect_map[indirent.indirection]
        offset = indirent.offset
        # print('INDIRENT {}'.format(indirent.inode))
        # print(indirent.block)
        if indirent.block != 0:
            if indirent.block < 0 or indirent.block > max_block:
                wrappedPrint('INVALID {}BLOCK {} IN INODE {} AT OFFSET {}'.format(level, indirent.block, indirent.inode, offset))
            if indirent.block < first_unreserved:
                wrappedPrint('RESERVED {}BLOCK {} IN INODE {} AT OFFSET {}'.format(level, indirent.block, inode.inode, offset))
            elif indirent.block in my_block_bitmap:
                my_block_bitmap[indirent.block].append([level, indirent.block, indirent.inode, offset])
            else:
                my_block_bitmap[indirent.block] = [[level, indirent.block, indirent.inode, offset]]


    # handle unreferenced, allocated, and duplicate inconsistencies
    for block in range(first_unreserved,max_block):
        # print(block)
        # print(my_block_bitmap)
        # print(free_blocks)
        if block not in my_block_bitmap and block not in free_blocks:
            wrappedPrint('UNREFERENCED BLOCK {}'.format(block))
            x = 1
        elif block in my_block_bitmap and block in free_blocks:
            wrappedPrint('ALLOCATED BLOCK {} ON FREELIST'.format(block))
        elif block in my_block_bitmap:
            dups = my_block_bitmap[block]
            if len(dups) > 1:
                for i in range(len(dups)):
                    wrappedPrint('DUPLICATE {}BLOCK {} IN INODE {} AT OFFSET {}'.format(dups[i][0], dups[i][1], dups[i][2], dups[i][3]))



def inode_audit():
    for inode in inodes:
        if inode.mode != 0 and inode.inode in free_inodes:
            wrappedPrint('ALLOCATED INODE {} ON FREELIST'.format(inode.inode))
    # print(range(first_inode, max_inode))
    # print(inodes)
    # print(free_inodes)
    for inode in range(first_inode, max_inode):   
        if inode not in inode_nums and inode not in free_inodes:
            wrappedPrint('UNALLOCATED INODE {} NOT ON FREELIST'.format(inode))

def dir_audit():
    global max_inode
    inode_links = {}
    for i in range(1, max_inode):
        inode_links[i] = 0
    inode_parents = {}
    for dir_entry in dir_entries:
        if dir_entry.inode < 0 or dir_entry.inode > max_inode:
            wrappedPrint('DIRECTORY INODE {} NAME {} INVALID INODE {}'.format(dir_entry.parent_inode, dir_entry.name, dir_entry.inode))
        elif dir_entry.inode not in inode_nums:
            wrappedPrint('DIRECTORY INODE {} NAME {} UNALLOCATED INODE {}'.format(dir_entry.parent_inode, dir_entry.name, dir_entry.inode))
        elif dir_entry.name == "'.'" and dir_entry.parent_inode != dir_entry.inode:
            wrappedPrint('DIRECTORY INODE {} NAME {} LINK TO INODE {} SHOULD BE {}'.format(dir_entry.parent_inode, dir_entry.name, dir_entry.inode, dir_entry.parent_inode))
        else:
            inode_links[dir_entry.inode] += 1

    #  root level parent dir check
    for dir_entry in list(filter(lambda x: x.parent_inode == 2 and x.name == "'..'", dir_entries)):
        if dir_entry.parent_inode != dir_entry.inode:
            wrappedPrint('DIRECTORY INODE {} NAME {} LINK TO INODE {} SHOULD BE {}'.format(dir_entry.parent_inode, dir_entry.name, dir_entry.inode, dir_entry.parent_inode))
    
    # general case parent dir check
    for dir_entry in list(filter(lambda x: x.parent_inode != 2 and x.name == "'..'", dir_entries)):
        for parent_dir in list(filter(lambda x: dir_entry.parent_inode == x.inode and x.parent_inode != dir_entry.parent_inode, dir_entries)):
            if dir_entry.inode != parent_dir.parent_inode:
                wrappedPrint('DIRECTORY INODE {} NAME {} LINK TO INODE {} SHOULD BE {}'.format(dir_entry.parent_inode, dir_entry.name, dir_entry.inode, parent_dir.parent_inode))

    for inode in inodes:
        if inode.inode in inode_links:
            if inode.link_count != inode_links[inode.inode]:
                wrappedPrint('INODE {} HAS {} LINKS BUT LINKCOUNT IS {}'.format(inode.inode, inode_links[inode.inode], inode.link_count))


def wrappedPrint(message):
    global ret
    ret = False
    print(message, file=sys.stdout)

if __name__ == '__main__':
    if len(sys.argv) != 2:
        print('Usage: lab3b summary', file = sys.stderr)
        sys.exit(1)
    
    parse_csv(sys.argv[1])
    data_block_audit()
    inode_audit()
    dir_audit()

    exitStatus = 0 if ret else 1 # inconsistency flag
    sys.exit(exitStatus)