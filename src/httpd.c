// fork from Tinyhttpd.
// Tinyhttpd's author: J. David Blackstone
// homepage: https://tinyhttpd.sourceforge.net/

#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <ctype.h>
#include <strings.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <stdint.h>

// isspace可以判断空格(\0)、换行(\f)、换行(\n)、回车(\r)、水平制表符(\t)和垂直制表符(\v)
#define ISspace(x) isspace((int)(x))	// 函数宏，isspace的参数为int

#define SERVER_STRING "Server: C_Tinyhttpd/0.1.0\r\n"
#define STDIN   0
#define STDOUT  1
#define STDERR  2

// 函数声明
void accept_request(void*);
void bad_request(int);
void cat(int, FILE*);
void cannot_execute(int);
void error_die(const char*);
void execute_cgi(int, const char*, const char*, const char*);
int get_line(int, char*, int);
void headers(int, const char*);
void not_found(int);
void serve_file(int, const char*);
int startup(u_short*);
void unimplemented(int);

/**********************************************************************/
/* A request has caused a call to accept() on the server port to
 * return.  Process the request appropriately.
 * Parameters: the socket connected to the client */
 /**********************************************************************/
void accept_request(void* arg) {
	int client = (intptr_t)arg;	// 通信套接字
	int cgi = 0;				// CGI标志位，如果判断为CGI标准通信则为true(1)

	// 从套接字中接收一行字符(该字符以\0结尾)
	char buf[1024];				// 接收数据流的容器
	size_t numchars;			// 接收的数据量
	numchars = get_line(client, buf, sizeof(buf));

	// HTTP第一行的格式为：方法 URI 协议版本
	char method[255];			// 方法
	char url[255];				// URL

	// 作者用了双指针的方法来逐步解析第一行的字符串(不得不说纯C对字符串的操作是真的繁琐)
	size_t i = 0, j = 0;
	// 从第一行中提取方法
	while (!ISspace(buf[i]) && (i < sizeof(method) - 1)) {
		method[i] = buf[i];
		i++;
	}
	j = i;
	method[i] = '\0';
	// strcasecmp()用于对比两个字符串，字符串相等时返回值为0
	if (strcasecmp(method, "GET") && strcasecmp(method, "POST")) {
		// 当HTTP方法既不为GET又不为POST时，给客户端回应501
		unimplemented(client);
		return;		// ====================================================>为什么这里不用清空缓冲区？也不用close(socket_fd)？
	}
	if (strcasecmp(method, "POST") == 0) cgi = 1;

	i = 0;
	// 去除空格
	while (ISspace(buf[j]) && (j < numchars)) {
		j++;
	}
	// 从第一行中提取URL
	while (!ISspace(buf[j]) && (i < sizeof(url) - 1) && (j < numchars)) {
		url[i] = buf[j];
		i++;
		j++;
	}
	url[i] = '\0';

	struct stat st;				// 保存文件信息的结构体变量，用于保存客户端请求的文件
	char* query_string = NULL;	// 指向URL的指针
	// 如果HTTP请求方法为GET，判断是否为CGI请求。
	// 一个CGI请求可能为https://www.example.com/search?keyword=1&search_source=5
	// 此时HTTP报头的格式为GET /search?keyword=1&search_source=5 HTTP/1.1
	// 其中，/search是路径部分，用于表示资源的路径
	// keyword=1&search_source=5是查询字符串部分，用于向服务器传递额外的参数
	if (strcasecmp(method, "GET") == 0) {
		query_string = url;
		while ((*query_string != '?') && (*query_string != '\0')) {
			query_string++;
		}
		if (*query_string == '?') {
			cgi = 1;
			*query_string = '\0';	// 用空字符\0分隔路径和查询字符串，这样前面的路径可以直接当字符串使用
			query_string++;
		}
	}
	// 将url拼接成完整的相对路径，如果路径是个文件夹，在后面加上index.html
	char path[512];				// 路径
	sprintf(path, "res%s", url);
	if (path[strlen(path) - 1] == '/') {
		strcat(path, "index.html");
	}
	// 判断文件是否可读
	if (stat(path, &st) == -1) {
		// 如果文件读取失败->文件不存在/无权限读取/路径错误/系统错误，清空套接字缓冲区，并且给客户端回应404
		// 清空套接字缓冲区很重要，它会影响TCP连接的断开，如果缓冲区存在数据，客户端可能会收到RST报文为不是FIN报文
		while ((numchars > 0) && strcmp("\n", buf)) {
			numchars = get_line(client, buf, sizeof(buf));
		}
		not_found(client);
	}
	else {
		// 如果路径是个文件夹，在后面加上index.html
		if ((st.st_mode & S_IFMT) == S_IFDIR) strcat(path, "/index.html");
		// 检测文件权限
		if ((st.st_mode & S_IXUSR) || (st.st_mode & S_IXGRP) || (st.st_mode & S_IXOTH)) cgi = 1;
		if (!cgi) {
			// 对于非CGI请求，直接使用HTTP常规回应
			serve_file(client, path);
		}
		else {
			// 对于CGI请求，直接使用HTTP常规回应
			execute_cgi(client, path, method, query_string);
		}
	}
	// 通信结束
	close(client);
}

/**********************************************************************/
/* Put the entire contents of a file out on a socket.  This function
 * is named after the UNIX "cat" command, because it might have been
 * easier just to do something like pipe, fork, and exec("cat").
 * Parameters: the client socket descriptor
 *             FILE pointer for the file to cat */
 /**********************************************************************/
void cat(int client, FILE* resource) {
	char buf[1024];

	fgets(buf, sizeof(buf), resource);
	while (!feof(resource)) {
		send(client, buf, strlen(buf), 0);
		fgets(buf, sizeof(buf), resource);
	}
}

/**********************************************************************/
/* Print out an error message with perror() (for system errors; based
 * on value of errno, which indicates system call errors) and exit the
 * program indicating an error. */
 /**********************************************************************/
void error_die(const char* sc) {
	perror(sc);
	exit(1);
}

/**********************************************************************/
/* Execute a CGI script.  Will need to set environment variables as
 * appropriate.
 * Parameters: client socket descriptor
 *             path to the CGI script */
 /**********************************************************************/
void execute_cgi(int client, const char* path,
	const char* method, const char* query_string) {
	// client：和客户端通信的套接字
	// path：请求的资源路径
	// method：请求方法
	// query_string：指向查询字符串的指针(url的一部分)
	char buf[1024];

	int numchars = 1;
	int content_length = -1;
	// 如果是GET，清空缓冲区，buf[0] = 'A'是为了使strcmp("\n", buf) > 0
	buf[0] = 'A'; buf[1] = '\0';
	if (strcasecmp(method, "GET") == 0) {
		while ((numchars > 0) && strcmp("\n", buf)) {
			numchars = get_line(client, buf, sizeof(buf));
		}
	}
	else if (strcasecmp(method, "POST") == 0) {
		// 如果是POST，则需要接收处理后面的内容
		numchars = get_line(client, buf, sizeof(buf));
		while ((numchars > 0) && strcmp("\n", buf)) {
			// 获取Content-Length
			buf[15] = '\0';
			if (strcasecmp(buf, "Content-Length:") == 0) content_length = atoi(&(buf[16]));
			numchars = get_line(client, buf, sizeof(buf));
		}
		if (content_length == -1) {
			bad_request(client);
			return;		// ====================================================>为什么这里不用close(socket_fd)？
		}
	}
	else{
		// 其他请求方式的实现
	}

	int cgi_output[2];
	int cgi_input[2];
	pid_t pid;
	int status;

	// 创建输出管道
	if (pipe(cgi_output) < 0) {
		cannot_execute(client);
		return;
	}
	// 创建输入管道
	if (pipe(cgi_input) < 0) {
		cannot_execute(client);
		return;
	}
	// 创建子进程
	if ((pid = fork()) < 0) {
		cannot_execute(client);
		return;
	}
	// 给客户端回应200 OK ====================================================>为什么要在fork()之后回应？是否会导致回应重复？
	sprintf(buf, "HTTP/1.0 200 OK\r\n");	
	send(client, buf, strlen(buf), 0);

	char c;
	if (pid == 0) {	// 子进程执行
		char meth_env[255];
		char query_env[255];
		char length_env[255];

		dup2(cgi_output[1], STDOUT);
		dup2(cgi_input[0], STDIN);
		close(cgi_output[0]);
		close(cgi_input[1]);
		sprintf(meth_env, "REQUEST_METHOD=%s", method);
		putenv(meth_env);
		if (strcasecmp(method, "GET") == 0) {
			sprintf(query_env, "QUERY_STRING=%s", query_string);
			putenv(query_env);
		}
		else {   /* POST */
			sprintf(length_env, "CONTENT_LENGTH=%d", content_length);
			putenv(length_env);
		}
		execl(path, NULL);
		exit(0);
	}
	else {	// 父进程执行
		close(cgi_output[1]);
		close(cgi_input[0]);
		if (strcasecmp(method, "POST") == 0)
			for (int i = 0; i < content_length; i++) {
				recv(client, &c, 1, 0);
				write(cgi_input[1], &c, 1);
			}
		while (read(cgi_output[0], &c, 1) > 0)
			send(client, &c, 1, 0);

		close(cgi_output[0]);
		close(cgi_input[1]);
		waitpid(pid, &status, 0);
	}
}

/**********************************************************************/
/* Get a line from a socket, whether the line ends in a newline,
 * carriage return, or a CRLF combination.  Terminates the string read
 * with a null character.  If no newline indicator is found before the
 * end of the buffer, the string is terminated with a null.  If any of
 * the above three line terminators is read, the last character of the
 * string will be a linefeed and the string will be terminated with a
 * null character.
 * Parameters: the socket descriptor
 *             the buffer to save the data in
 *             the size of the buffer
 * Returns: the number of bytes stored (excluding null) */
 /**********************************************************************/
int get_line(int sock, char* buf, int size) {
	int i = 0;
	char c = '\0';
	int n;

	while ((i < size - 1) && (c != '\n')) {
		n = recv(sock, &c, 1, 0);	// 从缓存区中接收一个字符
		if (n > 0) {				// 如果成功接收到一个字符，对该字符进行判断
			// 1.该字符为\r且下一个是\n，将该字符改为\n，且清除掉缓存区中的下一个\n
			// 2.该字符为单个\r，将该字符改为\n
			// 3.该字符为\n，不动
			if (c == '\r') {
				n = recv(sock, &c, 1, MSG_PEEK);
				if ((n > 0) && (c == '\n')) {
					recv(sock, &c, 1, 0);
				}
				else {
					c = '\n';
				}
			}
			buf[i] = c;				// 将该字符放到容器内
			i++;
		}
		else {						// 如果没有接收到字符，将该字符改为\n，准备跳出while循环
			c = '\n';
		}
	}
	buf[i] = '\0';					// 读取一行后，在末尾添加\0
	return i;
}

/**********************************************************************/
/* Send a regular file to the client.  Use headers, and report
 * errors to client if they occur.
 * Parameters: a pointer to a file structure produced from the socket
 *              file descriptor
 *             the name of the file to serve */
 /**********************************************************************/
void serve_file(int client, const char* filename) {
	// 清空缓冲区，buf[0] = 'A'是为了使strcmp("\n", buf) > 0
	int numchars = 1;
	char buf[1024];
	buf[0] = 'A'; buf[1] = '\0';
	while ((numchars > 0) && strcmp("\n", buf)) {
		numchars = get_line(client, buf, sizeof(buf));
	}
	// 打开目标文件
	FILE* resource = NULL;
	resource = fopen(filename, "r");
	if (resource == NULL) {
		// 如果不能打开文件，返回404
		not_found(client);
	}
	else {
		// 如果打开了文件，则给客户端回应200 OK报头+资源内容
		headers(client, filename);
		cat(client, resource);
	}
	fclose(resource);
}

/**********************************************************************/
/* This function starts the process of listening for web connections
 * on a specified port.  If the port is 0, then dynamically allocate a
 * port and modify the original port variable to reflect the actual
 * port.
 * Parameters: pointer to variable containing the port to connect on
 * Returns: the socket */
 /**********************************************************************/
int startup(u_short* port) {
	// 得到TCP套接字
	int httpd = 0;
	httpd = socket(PF_INET, SOCK_STREAM, 0);
	if (httpd == -1)
		error_die("socket");
	// 创建套接字地址和端口的结构体变量
	struct sockaddr_in name;
	memset(&name, 0, sizeof(name));
	name.sin_family = AF_INET;
	name.sin_port = htons(*port);
	name.sin_addr.s_addr = htonl(INADDR_ANY);
	// 设置端口复用
	int opt = 1;
	if ((setsockopt(httpd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) < 0) {
		error_die("setsockopt failed");
	}
	// 绑定监听端口
	if (bind(httpd, (struct sockaddr*)&name, sizeof(name)) < 0)
		error_die("bind");
	// 如果为动态监听端口，得到动态监听的端口
	if (*port == 0) {
		socklen_t namelen = sizeof(name);
		if (getsockname(httpd, (struct sockaddr*)&name, &namelen) == -1)
			error_die("getsockname");
		*port = ntohs(name.sin_port);
	}
	// 开始监听，监听队列大小设置为5
	if (listen(httpd, 5) < 0)
		error_die("listen");
	return(httpd);
}

int main(void) {
	int server_sock = -1;			// 监听套接字
	u_short port = 4000;			// 监听端口
	int client_sock = -1;			// 通信套接字
	struct sockaddr_in client_name;	// 通信套接字地址结构体
	socklen_t client_name_len = sizeof(client_name);

	server_sock = startup(&port);	// 创建监听
	printf("Tinyhttpd++ running on port %d.\n", port);

	// 循环通信
	while (1) {
		client_sock = accept(server_sock, (struct sockaddr*)&client_name, &client_name_len);
		if (client_sock == -1) {
			error_die("accept");
		}
		accept_request(&client_sock);
	}

	close(server_sock);
	return(0);
}

/*******************************400*************************************/
/* Inform the client that a request it has made has a problem.
 * Parameters: client socket */
 /**********************************************************************/
void bad_request(int client) {
	char buf[1024];

	sprintf(buf, "HTTP/1.0 400 BAD REQUEST\r\n");
	send(client, buf, sizeof(buf), 0);
	sprintf(buf, "Content-type: text/html\r\n");
	send(client, buf, sizeof(buf), 0);
	sprintf(buf, "\r\n");
	send(client, buf, sizeof(buf), 0);
	sprintf(buf, "<P>Your browser sent a bad request, ");
	send(client, buf, sizeof(buf), 0);
	sprintf(buf, "such as a POST without a Content-Length.\r\n");
	send(client, buf, sizeof(buf), 0);
}

/*******************************500*************************************/
/* Inform the client that a CGI script could not be executed.
 * Parameter: the client socket descriptor. */
 /**********************************************************************/
void cannot_execute(int client) {
	char buf[1024];

	sprintf(buf, "HTTP/1.0 500 Internal Server Error\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "Content-type: text/html\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "<P>Error prohibited CGI execution.\r\n");
	send(client, buf, strlen(buf), 0);
}

/*******************************200*************************************/
/* Return the informational HTTP headers about a file. */
/* Parameters: the socket to print the headers on
 *             the name of the file */
 /**********************************************************************/
void headers(int client, const char* filename) {
	char buf[1024];
	// 作者提示你可以使用filename来确定文件类型
	// (void)filename是为了让编译器不报参数未使用的警告
	(void)filename;  /* could use filename to determine file type */

	strcpy(buf, "HTTP/1.0 200 OK\r\n");
	send(client, buf, strlen(buf), 0);
	strcpy(buf, SERVER_STRING);
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "Content-Type: text/html\r\n");
	send(client, buf, strlen(buf), 0);
	strcpy(buf, "\r\n");
	send(client, buf, strlen(buf), 0);
}

/*******************************404*************************************/
/* Give a client a 404 not found status message. */
/**********************************************************************/
void not_found(int client) {
	char buf[1024];

	sprintf(buf, "HTTP/1.0 404 NOT FOUND\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, SERVER_STRING);
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "Content-Type: text/html\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "<HTML><TITLE>Not Found</TITLE>\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "<BODY><P>The server could not fulfill\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "your request because the resource specified\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "is unavailable or nonexistent.\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "</BODY></HTML>\r\n");
	send(client, buf, strlen(buf), 0);
}

/*******************************501*************************************/
/* Inform the client that the requested web method has not been
 * implemented.
 * Parameter: the client socket */
 /**********************************************************************/
void unimplemented(int client) {
	char buf[1024];

	sprintf(buf, "HTTP/1.0 501 Method Not Implemented\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, SERVER_STRING);
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "Content-Type: text/html\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "<HTML><HEAD><TITLE>Method Not Implemented\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "</TITLE></HEAD>\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "<BODY><P>HTTP request method not supported.\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "</BODY></HTML>\r\n");
	send(client, buf, strlen(buf), 0);
}