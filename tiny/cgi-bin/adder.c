/*
 * adder.c - a minimal CGI program that adds two numbers together
 */
/* $begin adder */
#include "csapp.h"

int main(void) {
	char *buf, *p1, *p2, *method;
	char arg1[MAXLINE], arg2[MAXLINE], content[MAXLINE];
	int n1=0, n2=0;
	method = getenv("METHOD_TYPE");

	if((buf = getenv("QUERY_STRING")) != NULL){
		p1 = strchr(buf, 'a');
		p2 = strchr(buf, 'b');

		strcpy(arg1, p1+2);
		strcpy(arg2, p2+2);
		
		n1 = atoi(arg1);
		n2 = atoi(arg2);
	}
	
	sprintf(content, "QUERY_STRING=%s", buf);
	sprintf(content, "Welcome to add.com: ");
	sprintf(content, "%sTHE Internet addition portal.\r\n<p>", buf);
	sprintf(content, "%sThe answer is: %d + %d = %d\r\n<p>", content, n1, n2, n1+n2);
	sprintf(content, "%sThanks for visiting!\r\n", content);
	
	printf("\nMethod : %s\n", method);
	printf("Contection: close\r\n");
	printf("Content-length: %d\r\n", (int)strlen(content));
	printf("Content-type: text/html\r\n\r\n");
	if(strcasecmp(method, "GET") == 0){
		printf("%s", content);
	}
	fflush(stdout);

  exit(0);
}
/* $end adder */
