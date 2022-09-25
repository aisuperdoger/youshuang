  1 #include <stdio.h>
  2 #include <stdlib.h>
  3 #include <string.h>
  4 #include <errno.h>
 5 #include <unistd.h>
 6 #include <fcntl.h>
 7 #include <arpa/inet.h>
 8 #include <netinet/in.h>
 9 #include <sys/socket.h>
 10 #include <sys/epoll.h>
 11 
 12 /* 最大缓存区大小 */
 13 #define MAX_BUFFER_SIZE 5
 14 /* epoll最大监听数 */
 15 #define MAX_EPOLL_EVENTS 20
 16 /* LT模式 */
 17 #define EPOLL_LT 0
 18 /* ET模式 */
 19 #define EPOLL_ET 1
 20 /* 文件描述符设置阻塞 */
 21 #define FD_BLOCK 0
 22 /* 文件描述符设置非阻塞 */
 23 #define FD_NONBLOCK 1
 24 
 25 /* 设置文件为非阻塞 */
 26 int set_nonblock(int fd)
 27 {
 28     int old_flags = fcntl(fd, F_GETFL);
 29     fcntl(fd, F_SETFL, old_flags | O_NONBLOCK);
 30     return old_flags;
 31 }
 32 
 33 /* 注册文件描述符到epoll，并设置其事件为EPOLLIN(可读事件) */
 34 void addfd_to_epoll(int epoll_fd, int fd, int epoll_type, int block_type)
 35 {
 36     struct epoll_event ep_event;
 37     ep_event.data.fd = fd;
 38     ep_event.events = EPOLLIN;
 39 
 40     /* 如果是ET模式，设置EPOLLET */
 41     if (epoll_type == EPOLL_ET)
 42         ep_event.events |= EPOLLET;
 43 
 44     /* 设置是否阻塞 */
 45     if (block_type == FD_NONBLOCK)
 46         set_nonblock(fd);
 47 
 48     epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ep_event);
 49 }
 50 
 51 /* LT处理流程 */
 52 void epoll_lt(int sockfd)
 53 {
 54     char buffer[MAX_BUFFER_SIZE];
 55     int ret;
 56 
 57     memset(buffer, 0, MAX_BUFFER_SIZE);
 58     printf("开始recv()...\n");
 59     ret = recv(sockfd, buffer, MAX_BUFFER_SIZE, 0);
 60     printf("ret = %d\n", ret);
 61     if (ret > 0)
 62         printf("收到消息:%s, 共%d个字节\n", buffer, ret);
 63     else
 64     {
 65         if (ret == 0)
 66             printf("客户端主动关闭！！！\n");
 67         close(sockfd);
 68     }
 69 
 70     printf("LT处理结束！！！\n");
 71 }
 72 
 73 /* 带循环的ET处理流程 */
 74 void epoll_et_loop(int sockfd)
 75 {
 76     char buffer[MAX_BUFFER_SIZE];
 77     int ret;
 78 
 79     printf("带循环的ET读取数据开始...\n");
 80     while (1)
 81     {
 82         memset(buffer, 0, MAX_BUFFER_SIZE);
 83         ret = recv(sockfd, buffer, MAX_BUFFER_SIZE, 0);
 84         if (ret == -1)
 85         {
 86             if (errno == EAGAIN || errno == EWOULDBLOCK)
 87             {
 88                 printf("循环读完所有数据！！！\n");
 89                 break;
 90             }
 91             close(sockfd);
 92             break;
 93         }
 94         else if (ret == 0)
 95         {
 96             printf("客户端主动关闭请求！！！\n");
 97             close(sockfd);
 98             break;
 99         }
100         else
101             printf("收到消息:%s, 共%d个字节\n", buffer, ret);
102     }
103     printf("带循环的ET处理结束！！！\n");
104 }
105 
106 
107 /* 不带循环的ET处理流程，比epoll_et_loop少了一个while循环 */
108 void epoll_et_nonloop(int sockfd)
109 {
110     char buffer[MAX_BUFFER_SIZE];
111     int ret;
112 
113     printf("不带循环的ET模式开始读取数据...\n");
114     memset(buffer, 0, MAX_BUFFER_SIZE);
115     ret = recv(sockfd, buffer, MAX_BUFFER_SIZE, 0);
116     if (ret > 0)
117     {
118         printf("收到消息:%s, 共%d个字节\n", buffer, ret);
119     }
120     else
121     {
122         if (ret == 0)
123             printf("客户端主动关闭连接！！！\n");
124         close(sockfd);
125     }
126 
127     printf("不带循环的ET模式处理结束！！！\n");
128 }
129 
130 /* 处理epoll的返回结果 */
131 void epoll_process(int epollfd, struct epoll_event *events, int number, int sockfd, int epoll_type, int block_type)
132 {
133     struct sockaddr_in client_addr;
134     socklen_t client_addrlen;
135     int newfd, connfd;
136     int i;
137 
138     for (i = 0; i < number; i++)
139     {
140         newfd = events[i].data.fd;
141         if (newfd == sockfd)
142         {
143             printf("=================================新一轮accept()===================================\n");
144             printf("accept()开始...\n");
145 
146             /* 休眠3秒，模拟一个繁忙的服务器，不能立即处理accept连接 */
147             printf("开始休眠3秒...\n");
148             sleep(3);
149             printf("休眠3秒结束！！！\n");
150 
151             client_addrlen = sizeof(client_addr);
152             connfd = accept(sockfd, (struct sockaddr *)&client_addr, &client_addrlen);
153             printf("connfd = %d\n", connfd);
154 
155             /* 注册已链接的socket到epoll，并设置是LT还是ET，是阻塞还是非阻塞 */
156             addfd_to_epoll(epollfd, connfd, epoll_type, block_type);
157             printf("accept()结束！！！\n");
158         }
159         else if (events[i].events & EPOLLIN)
160         {
161             /* 可读事件处理流程 */
162 
163             if (epoll_type == EPOLL_LT)    
164             {
165                 printf("============================>水平触发开始...\n");
166                 epoll_lt(newfd);
167             }
168             else if (epoll_type == EPOLL_ET)
169             {
170                 printf("============================>边缘触发开始...\n");
171 
172                 /* 带循环的ET模式 */
173                 epoll_et_loop(newfd);
174 
175                 /* 不带循环的ET模式 */
176                 //epoll_et_nonloop(newfd);
177             }
178         }
179         else
180             printf("其他事件发生...\n");
181     }
182 }
183 
184 /* 出错处理 */
185 void err_exit(char *msg)
186 {
187     perror(msg);
188     exit(1);
189 }
190 
191 /* 创建socket */
192 int create_socket(const char *ip, const int port_number)
193 {
194     struct sockaddr_in server_addr;
195     int sockfd, reuse = 1;
196 
197     memset(&server_addr, 0, sizeof(server_addr));
198     server_addr.sin_family = AF_INET;
199     server_addr.sin_port = htons(port_number);
200 
201     if (inet_pton(PF_INET, ip, &server_addr.sin_addr) == -1)
202         err_exit("inet_pton() error");
203 
204     if ((sockfd = socket(PF_INET, SOCK_STREAM, 0)) == -1)
205         err_exit("socket() error");
206 
207     /* 设置复用socket地址 */
208     if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) == -1)
209         err_exit("setsockopt() error");
210 
211     if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
212         err_exit("bind() error");
213 
214     if (listen(sockfd, 5) == -1)
215         err_exit("listen() error");
216 
217     return sockfd;
218 }
219 
220 /* main函数 */
221 int main(int argc, const char *argv[])
222 {
223     if (argc < 3)
224     {
225         fprintf(stderr, "usage:%s ip_address port_number\n", argv[0]);
226         exit(1);
227     }
228 
229     int sockfd, epollfd, number;
230 
231     sockfd = create_socket(argv[1], atoi(argv[2]));
232     struct epoll_event events[MAX_EPOLL_EVENTS];
233 
234     /* linux内核2.6.27版的新函数，和epoll_create(int size)一样的功能，并去掉了无用的size参数 */
235     if ((epollfd = epoll_create1(0)) == -1)
236         err_exit("epoll_create1() error");
237 
238     /* 以下设置是针对监听的sockfd，当epoll_wait返回时，必定有事件发生，
239      * 所以这里我们忽略罕见的情况外设置阻塞IO没意义，我们设置为非阻塞IO */
240 
241     /* sockfd：非阻塞的LT模式 */
242     addfd_to_epoll(epollfd, sockfd, EPOLL_LT, FD_NONBLOCK);
243 
244     /* sockfd：非阻塞的ET模式 */
245     //addfd_to_epoll(epollfd, sockfd, EPOLL_ET, FD_NONBLOCK);
246 
247    
248     while (1)
249     {
250         number = epoll_wait(epollfd, events, MAX_EPOLL_EVENTS, -1);
251         if (number == -1)
252             err_exit("epoll_wait() error");
253         else
254         {
255             /* 以下的LT，ET，以及是否阻塞都是是针对accept()函数返回的文件描述符，即函数里面的connfd */
256 
257             /* connfd:阻塞的LT模式 */
258             epoll_process(epollfd, events, number, sockfd, EPOLL_LT, FD_BLOCK);
259 
260             /* connfd:非阻塞的LT模式 */
261             //epoll_process(epollfd, events, number, sockfd, EPOLL_LT, FD_NONBLOCK);
262 
263             /* connfd:阻塞的ET模式 */
264             //epoll_process(epollfd, events, number, sockfd, EPOLL_ET, FD_BLOCK);
265 
266             /* connfd:非阻塞的ET模式 */
267             //epoll_process(epollfd, events, number, sockfd, EPOLL_ET, FD_NONBLOCK);
268         }
269     }
270 
271     close(sockfd);
272     return 0;
273 }
