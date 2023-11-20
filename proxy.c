#include <stdio.h>
#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

//---------------Proxy----------------------------
void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *host, char *port, char *path);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
		char *longmsg);

//--------------Thread----------------------------
void *thread(void *vargp);

//---------------Cache----------------------------

//cache struct
typedef struct {
    char *url;        // URi
    char *data;       // data
    size_t size;      // data size
} cache_entry;

//------------------------------------------------
/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
"User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
"Firefox/10.0.3\r\n";

int main(int argc, char **argv) {
	int listenfd, *connfdp;
	char hostname[MAXLINE], port[MAXLINE];
	socklen_t clientlen;
	struct sockaddr_storage clientaddr;
	pthread_t tid;

	/* Check command line args */
	if (argc != 2) {
		fprintf(stderr, "usage: %s <port>\n", argv[0]);
		exit(1);
	}

	listenfd = Open_listenfd(argv[1]);
	while (1) {
		clientlen = sizeof(clientaddr);
		connfdp = malloc(sizeof(int));
		*connfdp = Accept(listenfd, (SA *)&clientaddr,&clientlen);  // line:netp:tiny:accept
		Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE,0);
		printf("Accepted connection from (%s, %s)\n", hostname, port);
		Pthread_create(&tid, NULL, thread, connfdp);
	}
}

void *thread(void *vargp){ //p.954 12.14
  int connfd = *((int *)vargp);
  Pthread_detach(pthread_self());
  Free(vargp);
	doit(connfd);   // line:netp:tiny:doit
	Close(connfd);  // line:netp:tiny:close
  return NULL;
}

void doit(int fd){
	int target_serverfd; 
	struct stat sbuf;
	char buf[MAXLINE];
	char buf_res[MAXLINE];
	char version[MAXLINE];
	//URI parse value
	char method[MAXLINE], uri[MAXLINE], host[MAXLINE], path[MAXLINE], port[MAXLINE];
	char filename[MAXLINE], cgiargs[MAXLINE];
	rio_t rio;
	Rio_readinitb(&rio, fd);
	Rio_readlineb(&rio, buf, MAXLINE);
	printf("Request headers:\n");
	printf("%s", buf);
	sscanf(buf, "%s %s %s", method, uri, version);
	
	parse_uri(uri, host, port, path);
	printf("-----------------------------\n");	
	printf("\nClient Request Info : \n");
	printf("mothed : %s\n", method);
	printf("URI : %s\n", uri);
	printf("hostName : %s\n", host);
	printf("port : %s\n", port);
	printf("path : %s\n", path);
	printf("-----------------------------\n");
/*
	if (strcasecmp(method, "GET") != 0 && strcasecmp(method, "HEAD") != 0){
		//clienterror(fd, method, "501", "Not implemented", "Tiny does not implement this method");
		return;
	}

	//read_requesthdrs(&rio);

	if(stat(filename, &sbuf) < 0){
		//clienterror(fd, filename, "404", "Not found", "Tiny couldn't find this file");
		return;
	}
*/
	target_serverfd = Open_clientfd(host, port);
	request(target_serverfd, host, path);
	response(target_serverfd, fd);
	Close(target_serverfd);
}

void request(int target_fd, char *host, char *path){
	char *version = "HTTP/1.0";
	char buf[MAXLINE];

	sprintf(buf, "GET %s %s\r\n", path, version);
	/*Set header*/
	sprintf(buf, "%sHost: %s\r\n", buf, host);    
  sprintf(buf, "%s%s", buf, user_agent_hdr);
  sprintf(buf, "%sConnections: close\r\n", buf);
  sprintf(buf, "%sProxy-Connection: close\r\n\r\n", buf);

	Rio_writen(target_fd, buf, (size_t)strlen(buf));	
}

void response(int target_fd, int fd){
	char buf[MAX_CACHE_SIZE];
	rio_t rio;
	int content_length;
	char *ptr;

	Rio_readinitb(&rio, target_fd);
	while (strcmp(buf, "\r\n")){
    Rio_readlineb(&rio, buf, MAX_CACHE_SIZE);
    if (strstr(buf, "Content-length")) 
      content_length = atoi(strchr(buf, ':') + 1);
    Rio_writen(fd, buf, strlen(buf));
  }

	ptr = malloc(content_length);
	Rio_readnb(&rio, ptr, content_length);
	printf("server received %d bytes\n", content_length);
	Rio_writen(fd, ptr, content_length);
}

void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg){
	char buf[MAXLINE], body[MAXBUF];

	sprintf(body, "<html><title>Tiny Error</title>");
	sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
	sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
	sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
	sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

	sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
	Rio_writen(fd, buf, strlen(buf));
	sprintf(buf, "Content-type: text/html\r\n");
	Rio_writen(fd, buf, strlen(buf));
	sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
	Rio_writen(fd, buf, strlen(buf));
	Rio_writen(fd, body, strlen(body));
}

void read_requesthdrs(rio_t *rp){
	char buf[MAXLINE];

	Rio_readlineb(rp, buf, MAXLINE);
	while(strcmp(buf, "\r\n")){
		Rio_readlineb(rp, buf, MAXLINE);
		printf("%s", buf);
	}
	return;
}

int parse_uri(char *uri, char *host, char *port, char *path){
	char *parse_ptr = strstr(uri, "//") ? strstr(uri, "//") + 2 : uri;

	//www.github.com:80/bckim9489.html
	strcpy(host, parse_ptr);
	
	strcpy(path, "/"); //path = /
	
	parse_ptr = strchr(host, '/');
	if(parse_ptr){
		//path = /bckim9489.html
		*parse_ptr = '\0';
		parse_ptr +=1;
		strcat(path, parse_ptr);
	}
	
	//www.github.com:80
	parse_ptr = strchr(host, ':');
	if(parse_ptr){
		//port = 80
		*parse_ptr = '\0';
		parse_ptr +=1;
		strcpy(port, parse_ptr);
	} else {
		strcpy(port, "80");
	}

	return 0;
}
