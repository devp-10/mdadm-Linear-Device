#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <err.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include "net.h"
#include "jbod.h"

/* the client socket descriptor for the connection to the server */
int cli_sd = -1;

/* attempts to read n bytes from fd; returns true on success and false on
 * failure */
static bool nread(int fd, int len, uint8_t *buf) {
  int bytes_read = 0;
  while (bytes_read < len) {
    int num = read(fd, &buf[bytes_read], len-bytes_read);
    if (num < 0)
      return false;
    else
      bytes_read += num;
  }
  return true;
}

/* attempts to write n bytes to fd; returns true on success and false on
 * failure */
static bool nwrite(int fd, int len, uint8_t *buf) {
  int bytes_written = 0;
  while (bytes_written < len) {
    int num = write(fd, &buf[bytes_written], len-bytes_written);
    if (num < 0)
      return false;
    else
      bytes_written += num;
  }
  return true;
}

/* attempts to receive a packet from fd; returns true on success and false on
 * failure */
static bool recv_packet(int fd, uint32_t *op, uint16_t *ret, uint8_t *block) {
  int SIZEOF_LEN = 2, SIZEOF_OP = 4, SIZEOF_RET = 2;
  uint16_t len;
  uint8_t tmp[HEADER_LEN];
  if (!nread(fd, HEADER_LEN, tmp)) { 
      return false;
  }
  memcpy(&len, tmp, SIZEOF_LEN);
  memcpy(op, tmp+SIZEOF_LEN, SIZEOF_OP);
  memcpy(ret, tmp+SIZEOF_LEN+SIZEOF_OP, SIZEOF_RET);
  len = ntohs(len);
  *op = ntohl(*op);
  *ret = ntohs(*ret);
  if(len>HEADER_LEN) {
    return nread(fd, JBOD_BLOCK_SIZE, block);
  }
}

/* attempts to send a packet to sd; returns true on success and false on
 * failure */
static bool send_packet(int sd, uint32_t op, uint8_t *block) {
  int SIZEOF_LEN = 2, SIZEOF_OP = 4;
  uint16_t len = HEADER_LEN;
  uint16_t net_len;
  uint8_t tmp[HEADER_LEN + JBOD_BLOCK_SIZE];
  if ((op>>26) == JBOD_WRITE_BLOCK) {
    len += JBOD_BLOCK_SIZE;
  }
  net_len = htons(len);
  op = htonl(op);
  memcpy(tmp, &net_len, SIZEOF_LEN);
  memcpy(tmp+2, &op, SIZEOF_OP);
  if (len>HEADER_LEN) {
    memcpy(tmp+HEADER_LEN, block, JBOD_BLOCK_SIZE);
  }
  return nwrite(sd, len, tmp);
}

/* attempts to connect to server and set the global cli_sd variable to the
 * socket; returns true if successful and false if not. */
bool jbod_connect(const char *ip, uint16_t port) {
  int sockfd = socket(AF_INET, SOCK_STREAM, 0);
  struct sockaddr_in caddr;
  caddr.sin_family = AF_INET;
  caddr.sin_port = htons(port);
  inet_aton(ip, &caddr.sin_addr);
  if (connect(sockfd, (const struct sockaddr*)&caddr, sizeof(caddr)) == -1) {
    return false;
  }
  else {
    cli_sd = sockfd;
    return true;
  }
}

/* disconnects from the server and resets cli_sd */
void jbod_disconnect(void) {
  close(cli_sd);
  cli_sd = -1;
}

/* sends the JBOD operation to the server and receives and processes the
 * response. */
int jbod_client_operation(uint32_t op, uint8_t *block) {
  uint16_t ret;
  bool send = send_packet(cli_sd,op,block);
  bool recv = recv_packet(cli_sd,&op,&ret,block);
  if (send == true || recv == true)
    return 0;
  else
    return -1;
}
