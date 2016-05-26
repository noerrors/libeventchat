#include<iostream>
#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<unistd.h>
#include<fcntl.h>
#include<sys/socket.h>
#include<sys/types.h>
#include<event.h>
#include<map>
#include<string>
#include<json/json.h>
using namespace std;

#define PORT        8888
#define BACKLOG     5
#define MAX 2048
#define MEM_SIZE    1024
int fd[MAX];
map<string,int> fdmap;
static int num = 0;

struct event_base* base;
struct sock_ev{
   struct event* read_ev;
   struct event* write_ev;
   char* buffer;
};

void release_sock_event(struct sock_ev* ev){
	event_del(ev->read_ev);
	free(ev->read_ev);
	free(ev->write_ev);
	free(ev->buffer);
	free(ev);
}

void on_write(int sock,short event,void* arg){
	char* buf = (char*)arg;
	for(int i =0;i< num;i++){
	 if(fd[i]!= sock){
	   write(fd[i],buf,strlen(buf)+1);
	 }
	}
	//write(sock,buf,strlen(buf)+1);
	free(buf);
}

void on_read(int sock,short event,void* arg){
	struct event* sock_write;
	struct sock_ev* ev=(struct sock_ev*)arg;
	ev->buffer = (char*)malloc(MEM_SIZE);
	bzero(ev->buffer, MEM_SIZE);
	int size = recv(sock,ev->buffer,MEM_SIZE,0);
	printf("recv:%s\n",ev->buffer);
	if (size == 0) {
		release_sock_event(ev);
		close(sock);
		return;
	}
	event_set(ev->write_ev, sock, EV_WRITE, on_write, ev->buffer);
	event_base_set(base, ev->write_ev);
	event_add(ev->write_ev, NULL);
}

void on_accept(int sock,short event,void* arg){
	struct sockaddr_in cli_addr;
	int newfd, sin_size;
	struct sock_ev* ev = (struct sock_ev*)malloc(sizeof(struct sock_ev));
	ev->read_ev = (struct event*)malloc(sizeof(struct event));
	ev->write_ev = (struct event*)malloc(sizeof(struct event));
	sin_size = sizeof(struct sockaddr_in);
	newfd = accept(sock, (struct sockaddr*)&cli_addr, &sin_size);
    fd[num++]=newfd;
	printf("connect fd:%d\n",newfd);
	//获取建立的sockfd的当前状态（非阻塞）
	int flags = fcntl(newfd,F_GETFL,0);
	fcntl(newfd,F_SETFL,flags|O_NONBLOCK);//将当前sockfd设置为非阻塞
	event_set(ev->read_ev, newfd, EV_READ|EV_PERSIST, on_read, ev);
	event_base_set(base, ev->read_ev);
	event_add(ev->read_ev, NULL);
}

int main(){
  struct sockaddr_in my_addr;
  int sock;
  sock = socket(AF_INET,SOCK_STREAM,0);
  int flag = 1;
  setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,&flag,sizeof(int));
  memset(&my_addr, 0, sizeof(my_addr));
  my_addr.sin_family = AF_INET;
  my_addr.sin_port = htons(PORT);
  my_addr.sin_addr.s_addr = INADDR_ANY;
  bind(sock, (struct sockaddr*)&my_addr, sizeof(struct sockaddr));
  listen(sock, BACKLOG);
  struct event listen_ev;
  base = event_base_new();
  event_set(&listen_ev, sock, EV_READ|EV_PERSIST, on_accept, NULL);
  event_base_set(base, &listen_ev);
  event_add(&listen_ev, NULL);
  event_base_dispatch(base);
  return 0;
}
