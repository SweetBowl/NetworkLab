 /*
    TCP Echo Standard Client Demo (Multiprocess Concurrency)
    Copyright@2020 张翔（zhangx@uestc.edu.cn）All Right Reserved
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>

#define MAX_CMD_STR 100
#define bprintf(fp, format, ...) \
    if(fp == NULL){printf(format, ##__VA_ARGS__);}     \
    else{printf(format, ##__VA_ARGS__);    \
            fprintf(fp, format, ##__VA_ARGS__);fflush(fp);}

//信号的类型
int sig_type = 0;
//全局变量，初始指向NULL
FILE * fp_res = NULL;

//SIGPIEP的处理函数
void sig_pipe(int signo) {
    sig_type = signo;
    pid_t pid = getpid();
    //输出[cli](进程号) SIGPIPE is coming!
    bprintf(fp_res, "[cli](%d) SIGPIPE is coming!\n", pid);
}
//SIGCHILD的处理函数
void sig_chld(int signo) {
    sig_type = signo;
    pid_t pid = getpid(), pid_chld = 0;
    int stat;
    //输出[cli](进程号) SIGCHLD is coming!
    bprintf(fp_res, "[cli](%d) SIGCHLD is coming!\n", pid);
    while ((pid_chld = waitpid(-1, &stat, WNOHANG)) > 0){
        bprintf(fp_res, "[cli](%d) child process(%d) terminated.\n", pid, pid_chld);
    }
}

//客户端的echo功能实现函数
int echo_rqt(int sockfd, int pin)
{
    //因为只在子进程中，这里获得的是子进程的PID
    pid_t pid = getpid();
    //PDU定义：PIN(4字节) LEN(4字节) Data
    //len_h：表示主机序的PDU数据长度，len_n：表示网络序的PDU数据长度
    int len_h = 0, len_n = 0;
    //pin_h：表示主机序的进程序号，pin_n：表示网络序的进程序号
    int pin_h = pin, pin_n = htonl(pin);
    //fn_td字符数组用于存储要读取的测试文件的文件名
    char fn_td[10] = {0};
    //定义应用层PDU缓存：MAX_CMD_STR字节的字符串数据，1个字节'\0'，8字节是PDU头部，PIN和LEN
    char buf[MAX_CMD_STR+1+8] = {0};
    
    //这里的fn_td表示测试文件名td0.txt
    sprintf(fn_td, "td%d.txt", pin);
    //打开对应的测试文件，只读
    FILE * fp_td = fopen(fn_td, "r");
    if(!fp_td){//如果测试文件打开失败，向记录文件中写入[cli](进程序号) Test data read error!
        //向记录文件中写入
        bprintf(fp_res, "[cli](%d) Test data read error!\n", pin_h);
        return 0;//echo_rqt函数退出
    }

    //从测试文件读取一行测试数据，读取一行字符串，最多读100个字节，放到buf+8开始的缓冲区
    while (fgets(buf+8, MAX_CMD_STR, fp_td)) {
    // 重置pin_h & pin_n:
        pin_h = pin;//注意这里的pin是主机字节序
        pin_n = htonl(pin);//将其转换为网络字节序
    
    // 指令解析:
        // 收到指令"exit"，跳出循环并返回
        if(strncmp(buf+8, "exit", 4) == 0){
            // printf("[cli](%d) \"exit\" is found!\n", pin_h);
            break;
        }

    // 数据解析（构建应用层PDU）:
        // 将PIN写入PDU缓存（网络字节序）
        memcpy(buf, &pin_n, 4);
        // 获取数据长度
        len_h = strnlen(buf+8, MAX_CMD_STR);
        // 将数据长度写入PDU缓存（网络字节序）
        len_n = htonl(len_h);
        memcpy(buf+4, &len_n, 4);
        
        // 将读入的'\n'更换为'\0'；若仅有'\n'输入，则'\0'将被作为数据内容发出，数据长度为1
        if(buf[len_h+8-1] == '\n')
            buf[len_h+8-1] = 0; // 同'\0'

    // 发送echo_rqt数据:
        write(sockfd, buf, len_h+8);
    
    // 读取echo_rep数据:
        memset(buf, 0, sizeof(buf));
        // 读取PIN（网络字节序）
        read(sockfd, &pin_n, 4);
        // 读取服务器echo_rep数据长度（网络字节序）并转为主机字节序
        read(sockfd, &len_n, 4);
        len_h = ntohl(len_n);
        
        // 读取服务器echo_rep数据
        read(sockfd, buf, len_h);
        bprintf(fp_res,"[echo_rep](%d) %s\n", pid, buf);
    }
    return 0;
}

//主程序入口，通过命令行传递参数，程序名 服务器IP地址 端口号 最大并发数
int main(int argc, char* argv[])
{
    // 基于argc简单判断命令行指令输入是否正确；4个参数是正确的，如果不是4个参数，输出提升信息
    if(argc != 4){
        printf("Usage:%s <IP> <PORT> <CONCURRENT AMOUNT>\n", argv[0]);
        return 0;//主函数的返回值为0
    }
    
    //定义SIGPIPE信号的相关信息
    struct sigaction sigact_pipe, old_sigact_pipe;//定义两个sigaction结构体
    sigact_pipe.sa_handler = sig_pipe;//指定处理函数为sig_pipe()，信号处理函数
    sigemptyset(&sigact_pipe.sa_mask);//标准设置
    sigact_pipe.sa_flags = 0;//标准设置
    sigact_pipe.sa_flags |= SA_RESTART;//设置受影响的慢系统调用重启
    sigaction(SIGPIPE, &sigact_pipe, &old_sigact_pipe);//sigact_pipe()函数是SIGPIPE信号的执行函数
    
    //定义SIGCHLD信号的相关信息
    struct sigaction sigact_chld, old_sigact_chld;
    sigact_chld.sa_handler = &sig_chld;
    sigemptyset(&sigact_chld.sa_mask);
    sigact_pipe.sa_flags = 0;
    sigact_pipe.sa_flags |= SA_RESTART;//设置受影响的慢系统调用重启
    sigaction(SIGCHLD, &sigact_chld, &old_sigact_chld);//sig_chld()函数是SIGCHLD信号的执行函数

    struct sockaddr_in srv_addr;//定义服务器端的地址结构
    //struct sockaddr_in cli_addr;
    //int cli_addr_len;
    int connfd;                    //创建客户端的套接字描述符
    int conc_amnt = atoi(argv[3]);//定义客户端允许的最大并发进程数(6)

    // 获取当前（父）进程PID，用于后续父进程信息打印；
    pid_t pid = getpid();

    //初始化服务器端的地址结构信息
    memset(&srv_addr, 0, sizeof(srv_addr));//地址结构首先清0
    srv_addr.sin_family = AF_INET;//设置family为IPv4地址族
    inet_pton(AF_INET, argv[1], &srv_addr.sin_addr);//将指定的IP地址拷贝到服务器端地址结构对应的元素中
    srv_addr.sin_port = htons(atoi(argv[2]));//将指定的端口号拷贝到服务器端地址结构对应的元素中
    
    //for循环创建规定的并发进程，通过fork()函数，父子进程的区别是根据fork的返回值
    //返回0表示是子进程,返回非0值表示是父进程（子进程的进程号）
    for (int i = 0; i < conc_amnt - 1; i++) {
        if (!fork()) {    //子进程
            //每增加一个子进程，pin号加1
            int pin = i+1;//定义pin表示子进程的创建序号
            char fn_res[20];//用于存记录文件的名字
            // 获取当前子进程PID,用于后续子进程信息打印
            pid = getpid();//用于获得子进程的进程号PID
            
            // 打开客户端记录res文件，文件序号指定为当前子进程序号PIN；
            sprintf(fn_res, "stu_cli_res_%d.txt", pin);//拼接文件名：stu_cli_res_pin.txt
            
            //文件顺利打开后，指向该流的文件指针就会被返回
            fp_res = fopen(fn_res, "ab"); // Write only， append at the tail. Open or create a binary file;
            
            //有个保护判断，如果文件打开失败，则返回NULL，并把错误代码存在errno中
            if(!fp_res){
                //输出文件打开失败的提示信息
                printf("[cli](%d) child exits, failed to open file \"stu_cli_res_%d.txt\"!\n", pid, pin);
                exit(-1);//文件打开失败就直接退出子进程
            }
            
            //向文件中写入子进程创建的信息，具体格式[cli](进程号) child process 进行序号 is created!
            bprintf(fp_res, "[cli](%d) child process %d is created!\n", pid, pin);
            
            //创建客户端的套接字描述符
            connfd = socket(PF_INET, SOCK_STREAM, 0);
            // TODO If socket() fail.
            
            do{    //这里为什么要用循环
                //connect函数发起连接请求，TCP连接建立失败返回-1，成功返回0.
                int res = connect(connfd, (struct sockaddr*) &srv_addr, sizeof(srv_addr));
                if(!res){//TCP连接建立成功
                    char ip_str[20]={0};//创立1个临时字符数组，存储点分十进制表示的IP地址
                    
                    //在文件中写入相应的信息，[cli](进程号) server[IP:PORT] is connected!
                    bprintf(fp_res, "[cli](%d) server[%s:%d] is connected!\n", pid, \
                        inet_ntop(AF_INET, &srv_addr.sin_addr, ip_str, sizeof(ip_str)), \
                            ntohs(srv_addr.sin_port));
                    //调用echo的功能函数，注意传递参数包含了2个，1个是套接字描述符，1个是第几个子进程
                    if(!echo_rqt(connfd, pin))//echo_rqt()正常返回为0
                        break;//终止循环，执行后面关闭套接字描述符
                }
                else    //TCP连接建立失败
                    break;    //终止循环，执行后面关闭套接字描述符
            }while(1);

            //关闭套接字描述符
            close(connfd);
            //注意一定要包含向文件中写入的相关信息
            bprintf(fp_res, "[cli](%d) connfd is closed!\n", pid);
            bprintf(fp_res, "[cli](%d) child process is going to exit!\n", pid);
            
            // 关闭子进程打开的res文件
            if(fp_res){
                if(!fclose(fp_res))
                    printf("[cli](%d) stu_cli_res_%d.txt is closed!\n", pid, pin);
            }
            exit(1);//子进程直接退出
        }
        else{
            //父进程
            continue;//继续执行，直接空行也可以
        }
    }
    
    //父进程要创建套接字
    char fn_res[20];
    //父进程对应的记录文件名，stu_cli_res_0.txt
    sprintf(fn_res, "stu_cli_res_%d.txt", 0);
    fp_res = fopen(fn_res, "wb");//打开父进程对应的记录文件
    if(!fp_res){
        printf("[cli](%d) child exits, failed to open file \"stu_cli_res_0.txt\"!\n", pid);
        exit(-1);//文件打开失败就直接退出父进程
    }

    //父进程中，也需要向服务器端发送数据，因此也需要建立TCP连接
    connfd = socket(PF_INET, SOCK_STREAM, 0);
    //connect client to server
    do{
        //发起TCP连接请求
        int res = connect(connfd, (struct sockaddr*) &srv_addr, sizeof(srv_addr));
        //
        if(!res){
            char ip_str[20]={0};
            bprintf(fp_res, "[cli](%d) server[%s:%d] is connected!\n", pid, inet_ntop(AF_INET, &srv_addr.sin_addr, ip_str, sizeof(ip_str)), ntohs(srv_addr.sin_port));
            if(!echo_rqt(connfd, 0))//
                break;//成功完成字符串的发送和返回输出，终止循环，执行后面关闭套接字描述符
        }
        else
            break;//终止循环，执行后面关闭套接字描述符
    }while(1);

    // 关闭连接描述符
    close(connfd);
    bprintf(fp_res, "[cli](%d) connfd is closed!\n", pid);
    bprintf(fp_res, "[cli](%d) parent process is going to exit!\n", pid);

    // 关闭父进程res文件
    if(!fclose(fp_res))
        printf("[cli](%d) stu_cli_res_0.txt is closed!\n", pid);
        
    return 0;
}
