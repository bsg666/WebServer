#ifndef HTTPCONNECTION_H
#define HTTPCONNECTION_H

#include <sys/epoll.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/stat.h>   // 文件状态
#include <sys/mman.h>   // 内存映射
#include <stdarg.h>
#include <errno.h>
#include <sys/uio.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include "locker.h"
#include "lst_timer.h"
#include "log.h"


class sort_timer_lst;
class util_timer;

#define COUT_OPEN 1
const bool ET = true;
#define TIMESLOT 5      // 定时器周期：秒

// http 连接的用户数据类
class http_conn
{
public:                         // 共享对象，没有线程竞争资源，所以不需要互斥
    static int m_epoll_fd;      // 所有的socket上的事件都被注册到同一个epoll对象中
    static int m_user_cnt;      // 统计用户的数量
    static int m_request_cnt;   // 接收到的请求次数
    static sort_timer_lst m_timer_lst;// 定时器链表(对象),所有http连接共享这一个定时器链表
    // static locker m_timer_lst_locker;  // 定时器链表互斥锁

    static const int RD_BUF_SIZE = 2048;    // 读缓冲区的大小
    static const int WD_BUF_SIZE = 2048;    // 写缓冲区的大小
    static const int FILENAME_LEN = 200;    //文件名的最大长度

    util_timer* timer;              // 定时器
public:
    // HTTP请求方法，这里只支持GET
    enum METHOD {GET = 0, POST, HEAD, PUT, DELETE, TRACE, OPTIONS, CONNECT};
    
    /*
        解析客户端请求时，主状态机的状态
        CHECK_STATE_REQUESTLINE:当前正在解析请求行
        CHECK_STATE_HEADER:     当前正在解析头部字段
        CHECK_STATE_CONTENT:    当前正在解析请求体
    */
    enum CHECK_STATE { CHECK_STATE_REQUESTLINE = 0, CHECK_STATE_HEADER, CHECK_STATE_CONTENT };
    
    /*
        服务器处理HTTP请求的可能结果，报文解析的结果
        NO_REQUEST          :   请求不完整，需要继续读取客户数据
        GET_REQUEST         :   表示获得了一个完成的客户请求
        BAD_REQUEST         :   表示客户请求语法错误
        NO_RESOURCE         :   表示服务器没有资源
        FORBIDDEN_REQUEST   :   表示客户对资源没有足够的访问权限
        FILE_REQUEST        :   文件请求,获取文件成功
        INTERNAL_ERROR      :   表示服务器内部错误
        CLOSED_CONNECTION   :   表示客户端已经关闭连接了
    */
    enum HTTP_CODE { NO_REQUEST, GET_REQUEST, BAD_REQUEST, NO_RESOURCE, FORBIDDEN_REQUEST, FILE_REQUEST, INTERNAL_ERROR, CLOSED_CONNECTION };
    
    // 从状态机的三种可能状态，即行的读取状态，分别表示
    // 1.读取到一个完整的行 2.行出错 3.行数据尚且不完整
    enum LINE_STATUS { LINE_OK = 0, LINE_BAD, LINE_OPEN };

public:
    http_conn();
    ~http_conn();
    void process();     // 处理客户端的请求、对客户端的响应
    void init(int sock_fd, const sockaddr_in& addr);    // 初始化新的连接
    void conn_close();  // 关闭连接
    bool read();        // 非阻塞的读
    bool write();       // 非阻塞的写
    void del_fd();      // 定时器回调函数，被tick()调用

private:
    int m_sock_fd;                  // 该http连接的socket
    sockaddr_in m_addr;             // 通信的socket地址
    char m_rd_buf[RD_BUF_SIZE];     // 读缓冲区
    int m_rd_idx;                   // 标识读缓冲区中已经读入的客户端数据的最后一个字节的下一个位置

    int m_checked_idx;              // 当前正在分析的字符在读缓冲区的位置
    int m_line_start;               // 当前正在解析的行的起始位置

    char* m_url;                    // 请求目标文件的文件名
    char* m_version;                // 协议版本，HTPP1.1
    METHOD m_method;                // 请求方法
    char* m_host;                   // 主机名
    long m_content_len;             // HTTP请求体的消息总长度
    bool m_linger;                  // HTTP 请求是否要保持连接 keep-alive
    char m_real_file[FILENAME_LEN]; // 客户请求的目标文件的完整路径，其内容等于 doc_root + m_url, doc_root是网站根目录
    CHECK_STATE m_check_stat;       // 主状态机当前所处的状态

    struct stat m_file_stat;        // 目标文件的状态。通过它我们可以判断文件是否存在、是否为目录、是否可读，并获取文件大小等信息
    char* m_file_address;           // 客户请求的目标文件被mmap到内存中的起始位置
    char m_write_buf[WD_BUF_SIZE];  // 写缓冲区
    int m_write_idx;                // 写缓冲区中待发送的字节数
    struct iovec m_iv[2];           // writev来执行写操作，表示分散写两个不连续内存块的内容
    int m_iv_count;                 // 被写内存块的数量
    int bytes_to_send;              // 将要发送的字节
    int bytes_have_send;            // 已经发送的字节

    

private:
    void init();                    // 私有函数，初始化连接以外的信息
    HTTP_CODE process_read();                       // 解析HTTP请求
    bool process_write(HTTP_CODE ret);              // 填充HTTP应答

    // 下面这一组函数被process_read调用以分析HTTP请求
    HTTP_CODE parse_request_line(char* text);       // 解析请求首行
    HTTP_CODE parse_request_headers(char* text);    // 解析请求头部
    HTTP_CODE parse_request_content(char* text);    // 解析请求体
    LINE_STATUS parse_one_line();                   // 从状态机解析一行数据
    char* get_line(){return m_rd_buf + m_line_start;} // 获取一行数据 return m_rd_buf + m_line_start;
    HTTP_CODE do_request();                         // 处理具体请求

    // 这一组函数被process_write调用以填充HTTP应答。
    void unmap();
    bool add_response( const char* format, ... );
    bool add_content( const char* content );
    bool add_content_type();
    bool add_status_line( int status, const char* title );
    void add_headers( int content_length );
    bool add_content_length( int content_length );
    bool add_linger();
    bool add_blank_line(); 
};


#endif