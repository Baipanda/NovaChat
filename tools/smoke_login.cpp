/* Minimal smoke client: login alice then quit. Compile: g++ -o smoke_login smoke_login.cpp */
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstdio>
#include <cstring>
#include <string>

int main() {
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(6666);
  inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
  if (connect(fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
    perror("connect");
    return 1;
  }
  // msgid=1 LOGIN
  const char* req = "{\"msgid\":1,\"id\":1,\"password\":\"123456\"}";
  if (send(fd, req, strlen(req) + 1, 0) < 0) {
    perror("send");
    return 1;
  }
  char buf[4096] = {0};
  ssize_t n = recv(fd, buf, sizeof(buf) - 1, 0);
  if (n <= 0) {
    perror("recv");
    return 1;
  }
  printf("recv(%zd): %s\n", n, buf);
  close(fd);
  return 0;
}
