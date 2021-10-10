#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "mdadm.h"
#include "jbod.h"
#include "cache.h"

// Helper function to get the operation which is used in jbod_operation function
uint32_t encode_operation(jbod_cmd_t cmd, int disk_num, int block_num) {
  uint32_t op = cmd<<26 | disk_num<<22 | block_num;
  return op;
}

// Helper function to get which offset and block of which disk contains an address
void translate_address(uint32_t addr, int *disk_num, int *block_num, int *offset) {
  *disk_num = addr/JBOD_DISK_SIZE;
  *block_num = (addr%JBOD_DISK_SIZE)/JBOD_BLOCK_SIZE;
  *offset = (addr%JBOD_DISK_SIZE)%JBOD_BLOCK_SIZE;
}

// Helper function to seek to a disk or a block using jbod_operation
int seek(int disk_num, int block_num) {
  int seek_to_disk = jbod_client_operation(encode_operation(JBOD_SEEK_TO_DISK, disk_num, 0), NULL);
  int seek_to_block = jbod_client_operation(encode_operation(JBOD_SEEK_TO_BLOCK, 0, block_num), NULL);
  if ((seek_to_disk == 0) && (seek_to_block == 0)) {
	  return 1;
  }  
  else {
    return -1;
  }
}

int mdadm_mount(void) {
  if ((jbod_client_operation(encode_operation(JBOD_MOUNT, 0, 0), NULL))==0) {
      return 1;
  }
  else {
    return -1;
  }
}

int mdadm_unmount(void) {
  if ((jbod_client_operation(encode_operation(JBOD_UNMOUNT, 0, 0), NULL))==0) {
    return 1;
  }
  else {
    return -1;
  }
}

int mdadm_read(uint32_t addr, uint32_t len, uint8_t *buf) {
  int disk_num;
  int block_num;
  int offset;
  uint32_t current_addr = addr;
  int buf_tracker = 0; // tracks the number of bytes read
  if (addr<=JBOD_DISK_SIZE*JBOD_NUM_DISKS && len<=1024 && (addr+len)<=JBOD_DISK_SIZE*JBOD_NUM_DISKS) {
    // Cannot read if length to be read is 0 and data to be read is not there
    if (len!=0 && buf==NULL) {                                                
      return -1;
    }
    // Loop runs until len bytes are read from addr to buf
    while (current_addr < (addr+len)) {
      translate_address(current_addr, &disk_num, &block_num, &offset);
      if (seek(disk_num, block_num) == -1) {
        return -1;
      }
      uint8_t tmp[JBOD_BLOCK_SIZE];
      // Checking if the block exists in cache or not
      if (cache_enabled() == true) {
	if (cache_lookup(disk_num, block_num, tmp) == -1) {
	  jbod_client_operation(encode_operation(JBOD_READ_BLOCK, 0, 0), tmp);
	  cache_insert(disk_num, block_num, tmp);
	}
      }
      else {
	jbod_client_operation(encode_operation(JBOD_READ_BLOCK, 0, 0), tmp);
      }
      if (offset == 0) {
	// |(*****)***|
        if (len-buf_tracker < JBOD_BLOCK_SIZE) {
          memcpy(&buf[buf_tracker], tmp, len-buf_tracker);
	  current_addr += (len-buf_tracker);
	  buf_tracker += (len-buf_tracker);
        }
	// |(********)|
        else {
	  memcpy(&buf[buf_tracker], tmp, JBOD_BLOCK_SIZE);
	  current_addr += JBOD_BLOCK_SIZE;
	  buf_tracker += JBOD_BLOCK_SIZE;   
        }
      }
      if (offset != 0) {
	// |**(****)**|
        if (len+offset < JBOD_BLOCK_SIZE) {
	  memcpy(buf, tmp+offset, len);
	  current_addr+= len;
        }
	// |***(*****)|
        else {
	  memcpy(buf, tmp+offset, JBOD_BLOCK_SIZE-offset);
          current_addr += (JBOD_BLOCK_SIZE-offset);
          buf_tracker += (JBOD_BLOCK_SIZE-offset);
        }
      }
    }
    return len;
  }
  else {
    return -1;
  }
}

int mdadm_write(uint32_t addr, uint32_t len, const uint8_t *buf) {
  int disk_num;
  int block_num;
  int offset;
  uint32_t current_addr = addr;
  int buf_tracker = 0; // tracks the number of bytes written
  if (addr<=JBOD_DISK_SIZE*JBOD_NUM_DISKS && len<=1024 && (addr+len)<=JBOD_DISK_SIZE*JBOD_NUM_DISKS) {
    // Cannot write if 0 bytes are to be written and data to write is not there
    if (len!=0 && buf==NULL) {                                                
      return -1;
    }
    // Loop runs until len bytes are written into addr from buf
    while (current_addr < (addr+len)) {
      translate_address(current_addr, &disk_num, &block_num, &offset);
      if (seek(disk_num, block_num) == -1) {
        return -1;
      }
      uint8_t tmp[JBOD_BLOCK_SIZE];
      if (jbod_client_operation(encode_operation(JBOD_READ_BLOCK, 0, 0), tmp) == -1){
	return -1;
      }
      if (seek(disk_num, block_num) == -1) {
        return -1;
      }
      if (offset == 0) {
	// |(*****)***|
        if (len-buf_tracker < JBOD_BLOCK_SIZE) {
          memcpy(tmp, &buf[buf_tracker], len-buf_tracker);
	  current_addr += (len-buf_tracker);
	  buf_tracker += (len-buf_tracker);
        }
	// |(********)|
        else {
	  memcpy(tmp, &buf[buf_tracker], JBOD_BLOCK_SIZE);
	  current_addr += JBOD_BLOCK_SIZE;
	  buf_tracker += JBOD_BLOCK_SIZE;   
        }
      }
      if (offset != 0) {
        // |**(****)**|
        if (len+offset < JBOD_BLOCK_SIZE) {
	  memcpy(tmp+offset, buf, len);
	  current_addr+= len;
        }
	// |***(*****)|
        else {
	  memcpy(tmp+offset, buf, JBOD_BLOCK_SIZE-offset);
          current_addr += (JBOD_BLOCK_SIZE-offset);
          buf_tracker += (JBOD_BLOCK_SIZE-offset);
        }
      }
      if (jbod_client_operation(encode_operation(JBOD_WRITE_BLOCK, 0, 0), tmp)== -1){
	return -1;
      }
    }
    return len;
  }
  else {
    return -1;
  }
}
