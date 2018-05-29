#include <signal.h> // 信号头文件。定义信号符号常量，信号结构以及信号操作函数原型。

#include <linux/sched.h> // 调度程序头文件,定义任务结构 task_struct、初始任务 0 的数据
#include <linux/mm.h> // for get_free_page 使用其中的 get_free_page
                      // 内存管理头文件。含有页面大小定义和一些页面释放函数原型。
#include <asm/segment.h> // 段操作头文件。定义了有关段寄存器操作的嵌入式汇编函数。

// 管道读操作函数
// 参数 inode 是管道对应的 i 节点， buf 是数据缓冲区指针，count 是读取的字节数
int read_pipe(struct m_inode * inode, char * buf, int count){
  int chars, size, read=0;
  // 若与读取的字节数 count>0, 则循环执行以下操作。若当前管道中没有数据(size=0), 则唤醒等待
  // 该节点的进程，如果没有写管道者，则返回已读字节数，退出。否则在该 i 节点上睡眠，等待信息。
  while(count>0){
    while(!(size==PIPE_SIZE(*inode))){
      wake_up(&inode->i_wait);
      if(inode->i_count!=2) // are there any writes?
        return read;
      sleep_on(&inode->i_wait);
    }
    // 取管道尾到缓冲区末端的字节数 chars。如果其大于好需要读取的字节数 count，则令其等于count。
    // 如果 chars 大于当前管道中含有数据长度 size，则令其等于 size
    chars=PAGE_SIZE-PIPE_TAIL(*inode);
    if(chars>count)
      chars=count;
    if(chars>size)
      chars=size;
    // 读字节计数减去此次可读的字节数 chars，并累加已读字节数
    count-=chars;
    read+=chars;
    // 令 size 指向管道尾部，调整当前管道尾指针(前移 chars 字节)
    size=PIPE_TAIL(*inode);
    PIPE_TAIL(*inode)+=chars;
    PIPE_TAIL(*inode)&=(PAGE_SIZE-1);
    // 将管道中的数据复制到用户缓冲区中。对于管道 i 节点，其 i_size 字段中是管道缓冲块指针。
    while(chars-->0)
      puts_fs_byte(((chars *)inode->i_size)[size++], buf++);
  }
  wake_up(&inode->i_wait);  // 唤醒等待该管道 i 节点的进程，并返回读取的字符串。
  return read;
}

// 管道写操作函数。
// 参数 inode 是管道对应的 i 节点，buf 是数据缓冲区指针，count 是将写入管道的字节数
int write_pipe(struct m_inode * inode, char * buf, int count){
  int chars, size, written=0;
  // 若要写入的计数值 count 还大于 0，则循环执行以下操作。若当前管道中没有已经满了(size=0),
  // 则唤醒等待该节点的进程。如果已没有读管道者，则像进程发送 SIGPIPE 信号，并返回已写入的
  // 字节数并退出。若写入 0 字节，则返回 -1。否则在该 i 节点上睡眠，等待管道腾出空间。
  while(count>0){
    while(!(size=(PAGE_SIZE-1)-PIPE_SIZE(*inode))){
      wake_up(&inode->i_wait);
      if(inode->i_count!=2){  // no readers
        current->signal|=(1<<(SIGPIPE-1));
        return written? written:-1;
      }
      sleep_on(&inode->i_wait);
    }
    // 取管道头部到缓冲区末端字节数 chars。如果其大于还需要写入的字节数 const，则令其等于 count。
    // 如果 chars 大于当前管道中空闲空间长度，则令其等于 size。
    chars=PAGE_SIZE-PIPE_HEAD(*inode);
    if(chars>count)
      chars=count;
    if(chars>size)
      chars=size;
    // 写入字节计数建起此次可写入的字节数 chars，并累加已写字节数到 written
    count-=chars;
    written+=chars;
    // 令 size 指向管道数据头部，调整当前管道数据头部指针(前移 chars 字节)。
    size=PIPE_HEAD(*inode);
    PIPE_HEAD(*inode)+=chars;
    PIPE_HEAD(*inode)&=(PAGE_SIZE-1);
    // 从用户缓冲区复制 chars 个字节到管道中。对于管道 i 节点，其 i_size 字段中是管道缓冲块指针
    while(chars-->0)
      ((char *)inode->i_size)[size++]=get_fs_byte(buf++);
  }
  wake_up(&inode->i_wait);  // 唤醒等待该 i 节点的进程，返回已写入的字节数，退出
  return written;
}

// 创建管道系统调用函数。
// 在 fildes 所指的数组中创建一对文件句柄(描述符)。这对文件句柄指向一管道 i 节点。fildes[0]
// 用于读管道中数据，fildes[1] 用于想管道中写入数据。成功时返回 0，出错返回 -1.
int sys_pipe(unsigned long * fildes){
  struct m_inode * inode;
  struct file * f[2];
  int fd[2];
  int i,j;
  // 从系统文件表中取两个空闲项(引用计数字段为 0 的项)，并分别设置引用计数为 1.
  j=0;
  for(i=0;j<2 && i<NR_FILE;i++)
    if(!file_table[i].f_count)
      (f[j++]=i+file_table)->f_count++;
  if(j==1)  // 如果只有一个空闲项，则释放该项(引用计数复位)
    f[0]->f_count=0;
  if(j>2) // 如果没有找到两个空闲项，则返回 -1
    return -1;
  // 针对上面取得的两个文件结构项，分别匹配一文件句柄，并使进程的文件结构指针分别指向这两个文件结构
  j=0;
  for(i=0lj<2 && i<NR_OPEN;i++)
    if(!current->filp[i]){
      current->filp[fd[j]=i]=f[j];
      j++;
    }
    if(j==1)  // 如果只有一个空闲文件句柄，则释放该句柄
      current->filp[fd[0]]=NULL;
    // 如果没有找到两个空闲句柄，则释放上面过去的两个文件结构项(复位引用计数值)，并返回 -1.
    if(f<2){
      f[0]->f_count=f[1]->f_count=0;
      return -1;
    }
    // 申请管道 i 节点，并未管道分配缓冲区(1 页内存)。如果不成功，则相应释放两个文件句柄
    // 和文件结构项，并返回 -1.
    if(!(inode=get_pipe_inode())){
      current->filp[fd[0]]=current->filp[fd[1]]=NULL;
      fd[0]->f_count=f[1]->f_count=0;
      return -1;
    }
    // 初始化两个文件结构，都指向同一个 i 节点，读写指针都置零。第 1 个文件结构的文件模式置为
    // 读，2 个文件结构的文件模式置为写
    f[0]->f_inode=f[1]->f_inode=inode;
    f[0]->f_pos=f[1]->f_pos=0;
    f[0]->f_mode=1; // read
    f[1]->f_mode=2; // write
    put_fs_long(fd[0], 0+fildes); // 将文件句柄数组复制到对应的用户数组中，并返回 0
    put_fs_long(fd[1], 1+fildes);
    return 0;
}
