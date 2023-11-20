#include <stdio.h>
#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
#define INITIAL_HASHMAP_SIZE 10
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
	time_t last_access;
} cache_entry;

//--------------hash 전방 수류탄-----------------------------
//hashmap
typedef struct {
	cache_entry **table; // hash table
	size_t size;         // size of hash table
} hashmap;

hashmap cache_map;

unsigned long djb2_hash(char *str);
unsigned long sdbm_hash(char *str);
unsigned int double_hashing(char *str, unsigned int table_size, unsigned int collision_cnt);
void hashmap_init(hashmap *hash_map, size_t size);
void hashmap_insert(hashmap *map, cache_entry *entry);
void hashmap_lru(hashmap *map);
cache_entry *hashmap_search(hashmap *map, char *url);
void hashmap_delete(hashmap *map, char *url);

//--------------------hashing func----------------
//Hash func 1
unsigned long djb2_hash(char *str) {
	unsigned long hash = 5381;
	int c;

	while ((c = *str++))
		hash = ((hash << 5) + hash) + c; // hash * 33 + c

	return hash;
}

//Hash func 2
unsigned long sdbm_hash(char *str) {
	unsigned long hash = 0;
	int c;

	while ((c = *str++))
		hash = c + (hash << 6) + (hash << 16) - hash;

	return hash;
}

//double hashing
unsigned int double_hashing(char *str, unsigned int table_size, unsigned int collision_cnt) {
	unsigned long hash1 = djb2_hash(str) % table_size;
	unsigned long hash2 = sdbm_hash(str) % table_size;
	return (hash1 + collision_cnt * (hash2 + 1)) % table_size; // hash2 + 1 ->  always not return 0
}

//Init hashmap func
void hashmap_init(hashmap *hash_map, size_t size) {
	hash_map->size = size;
	hash_map->table = malloc(sizeof(cache_entry*) * size);
	for (size_t i = 0; i < size; i++) {
		hash_map->table[i] = NULL;
	}
}

//insert func
void hashmap_insert(hashmap *map, cache_entry *entry) {
	unsigned int collision_cnt = 0;
	unsigned int index;

	do {
		index = double_hashing(entry->url, map->size, collision_cnt);
		collision_cnt++;
	} while (map->table[index] != NULL && collision_cnt < map->size);//충돌시 한바꾸 돌림

	if (collision_cnt < map->size) {
		map->table[index] = entry;
	} else {
		// 삽일 할 곳이 읍을 때 or 해시테이블 공간 부족 시
		hashmap_lru(map);
		hashmap_insert(map, entry); // 2트
	}
}

void hashmap_lru(hashmap *map){
	time_t oldest = time(NULL); //현재는 과거다!
	int oldest_index = -1;

	for (size_t i = 0; i < map->size; i++) {
		if (map->table[i] != NULL && map->table[i]->last_access < oldest) {
			oldest = map->table[i]->last_access;
			oldest_index = i;
		}
	}

	if (oldest_index != -1) {
		// 가장 오래된 거 제거
		free(map->table[oldest_index]->data);
		free(map->table[oldest_index]);
		map->table[oldest_index] = NULL;
	}
}


//search func
cache_entry *hashmap_search(hashmap *map, char *url) {
	unsigned int collision_cnt = 0;
	unsigned int index;

	do {
		index = double_hashing(url, map->size, collision_cnt);
		if (map->table[index] != NULL && strcmp(map->table[index]->url, url) == 0) {
			return map->table[index]; // 찾음
		}
		collision_cnt++;
	} while (map->table[index] != NULL && collision_cnt < map->size);

	return NULL; // 찾지 못함
}

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

	//hash init
	hashmap_init(&cache_map, INITIAL_HASHMAP_SIZE);

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

	cache_entry *cached_content = hashmap_search(&cache_map, uri);
	
	if(cached_content != NULL){ //cache hit!
		cached_content->last_access = time(NULL);
		//set Header 
		sprintf(buf_res, "HTTP/1.0 200 OK\r\n");
		sprintf(buf_res, "%sServer: Tiny Web Server\r\n", buf_res);
		sprintf(buf_res, "%sConnection: close\r\n", buf_res);
		sprintf(buf_res, "%sContent-length: %d\r\n\r\n", buf_res, cached_content->size);
		Rio_writen(fd, buf_res, strlen(buf_res));

		Rio_writen(fd, cached_content->data, cached_content->size);
	} else { //cache miss...
		target_serverfd = Open_clientfd(host, port);
		request(target_serverfd, host, path);
		response(target_serverfd, fd, uri);
		Close(target_serverfd);
	}
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

void response(int target_fd, int fd, char* uri){
	char buf[MAX_CACHE_SIZE];
	rio_t rio;
	int content_length;

	char *ptr, *cached_data;
	int total_size = 0;

	Rio_readinitb(&rio, target_fd);
	//헤더 만들기
	while (strcmp(buf, "\r\n")){
		Rio_readlineb(&rio, buf, MAX_CACHE_SIZE);
		if (strstr(buf, "Content-length")) //사이즈 뽑아내기 
			content_length = atoi(strchr(buf, ':') + 1);
		Rio_writen(fd, buf, strlen(buf));
	}

	total_size = content_length;
	cached_data = malloc(total_size);
	ptr = cached_data;
	
	//바디 쏘기
	while (total_size > 0) {
		int bytes_read = Rio_readnb(&rio, ptr, total_size);
		Rio_writen(fd, ptr, bytes_read);
		ptr += bytes_read;
		total_size -= bytes_read;
	}
	
	//사이즈 작으면 캐시해줌
	if(content_length <= MAX_OBJECT_SIZE){
		cache_entry *new_entry = malloc(sizeof(cache_entry));
		new_entry->url = strdup(uri);
		new_entry->data = cached_data;
		new_entry->size = content_length + total_size;
		hashmap_insert(&cache_map, new_entry);	
	}
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
