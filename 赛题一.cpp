//======================lab1==========================
//sleep
#include "kernel/types.h"
#include "user/user.h"
int main(int argc, char *argv[]) {
  if (argc != 2) {
    fprintf(2, "usage: sleep [ticks num]\n");
    exit(1);
  }
  int ticks = atoi(argv[1]);
  int ret = sleep(ticks);
  exit(ret);
}
//pingpong
#include "kernel/types.h"
#include "user/user.h"
int main(int argc, char* argv[]) {
    int parent_pid, child_pid;
    int parent_to_child[2], child_to_parent[2];
    char message = 'a';
    pipe(parent_to_child);
    pipe(child_to_parent);
    child_pid = fork();
    if (child_pid == 0) {       
        parent_pid = getpid();       
        close(parent_to_child[1]);
        close(child_to_parent[0]);     
        read(parent_to_child[0], &message, 1);
        printf("%d: received ping\n", parent_pid);       
        write(child_to_parent[1], &message, 1);
        exit(0);
    }
    else { 
        parent_pid = getpid(); 
        close(parent_to_child[0]);
        close(child_to_parent[1]);
        write(parent_to_child[1], &message, 1);
        read(child_to_parent[0], &message, 1);
        printf("%d: received pong\n", parent_pid);
        exit(0);
    }
}

//primes
#include "kernel/types.h"
#include "user/user.h"
void run_prime_sieve(int listenfd) {
    int prime = 0;        
    while (1) {
        if (read(listenfd, &candidate, sizeof(int)) == 0) {
            close(listenfd);

            if (has_child) {
                close(child_pipe[1]);    
                wait(0);                
            }
            exit(0);
        }
        if (prime == 0) {
            prime = candidate;
            printf("prime %d\n", prime);
            continue;
        }
        if (candidate % prime != 0) {
            if (!has_child) {
                pipe(child_pipe);
                has_child = 1;
                if (fork() == 0) {
                    close(child_pipe[1]);
                    close(listenfd);
                    run_prime_sieve(child_pipe[0]);
                }
                else {
                    close(child_pipe[0]);
                }
            }
            write(child_pipe[1], &candidate, sizeof(int));
        }
    }
}

int main(int argc, char* argv[]) {
    int initial_pipe[2];
    pipe(initial_pipe);
    for (int num = 2; num <= 35; num++) {
        write(initial_pipe[1], &num, sizeof(int));
    }
    close(initial_pipe[1]);

    run_prime_sieve(initial_pipe[0]);
    exit(0);
}

//find
#include "kernel/types.h"
#include "kernel/fcntl.h"
#include "kernel/fs.h"
#include "kernel/stat.h"
#include "user/user.h"

char* basename(char* pathname) {
    char* prev = 0;
    char* curr = strchr(pathname, '/');
    while (curr != 0) {
        prev = curr;
        curr = strchr(curr + 1, '/');
    }
    return prev;
}

void find(char* curr_path, char* target) {
    char buf[512], * p;
    int fd;
    struct dirent de;
    struct stat st;
    if ((fd = open(curr_path, O_RDONLY)) < 0) {
        fprintf(2, "find: cannot open %s\n", curr_path);
        return;
    }

    if (fstat(fd, &st) < 0) {
        fprintf(2, "find: cannot stat %s\n", curr_path);
        close(fd);
        return;
    }

    switch (st.type) {

    case T_FILE: {
        char* f_name = basename(curr_path);
        int match = 1;
        if (f_name == 0 || strcmp(f_name + 1, target) != 0) {
            match = 0;
        }
        if (match)
            printf("%s\n", curr_path);
        close(fd);
        break;
    }
    case T_DIR: {
        memset(buf, 0, sizeof(buf));
        uint curr_path_len = strlen(curr_path);
        memcpy(buf, curr_path, curr_path_len);
        buf[curr_path_len] = '/';
        p = buf + curr_path_len + 1;
        while (read(fd, &de, sizeof(de)) == sizeof(de)) {
            if (de.inum == 0 || strcmp(de.name, ".") == 0 ||
                strcmp(de.name, "..") == 0)
                continue;
            memcpy(p, de.name, DIRSIZ);
            p[DIRSIZ] = 0;
            find(buf, target);
        }
        close(fd);
        break;
    }
    }
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        fprintf(2, "usage: find [directory] [target filename]\n");
        exit(1);
    }
    find(argv[1], argv[2]);
    exit(0);
}

//xargs
#include "kernel/param.h"
#include "kernel/types.h"
#include "user/user.h"

#define buf_size 512

int main(int argc, char* argv[]) {
    char buf[buf_size + 1] = { 0 };
    uint occupy = 0;
    char* xargv[MAXARG] = { 0 };
    int stdin_end = 0;
    for (int i = 1; i < argc; i++) {
        xargv[i - 1] = argv[i];
    }

    while (!(stdin_end && occupy == 0)) {
        if (!stdin_end) {
            int remain_size = buf_size - occupy;
            int read_bytes = read(0, buf + occupy, remain_size);
            if (read_bytes < 0) {
                fprintf(2, "xargs: read returns -1 error\n");
            }
            if (read_bytes == 0) {
                close(0);
                stdin_end = 1;
            }
            occupy += read_bytes;
        }

        char* line_end = strchr(buf, '\n');
        while (line_end) {
            char xbuf[buf_size + 1] = { 0 };
            memcpy(xbuf, buf, line_end - buf);
            xargv[argc - 1] = xbuf;
            int ret = fork();
            if (ret == 0) {
                if (!stdin_end) {
                    close(0);
                }
                if (exec(argv[1], xargv) < 0) {
                    fprintf(2, "xargs: exec fails with -1\n");
                    exit(1);
                }
            }
            else {
                memmove(buf, line_end + 1, occupy - (line_end - buf) - 1);
                occupy -= line_end - buf + 1;
                memset(buf + occupy, 0, buf_size - occupy);
                int pid;
                wait(&pid);
                line_end = strchr(buf, '\n');
            }
        }
    }
    exit(0);
}

//============================lab2=============================
//trace
#include "kernel/param.h"
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  int i;
  char *nargv[MAXARG];

  if(argc < 3 || (argv[1][0] < '0' || argv[1][0] > '9')){
    fprintf(2, "Usage: %s mask command\n", argv[0]);
    exit(1);
  }

  if (trace(atoi(argv[1])) < 0) {
    fprintf(2, "%s: trace failed\n", argv[0]);
    exit(1);
  }
  
  for(i = 2; i < argc && i < MAXARG; i++){
    nargv[i-2] = argv[i];
  }
  exec(nargv[0], nargv);
  exit(0);
}

//sysinfo
//在 kernel/sysproc.c 中实现 sys_sysinfo() 函数
#include "sysinfo.h"
uint64
sys_sysinfo(void)
{
  struct sysinfo info;
  uint64 addr; // 用户空间指针 
  if(argaddr(0, &addr) < 0)
    return -1;
  info.freemem = kfreemem();
  info.nproc = procnum();
  if(copyout(myproc()->pagetable, addr, (char *)&info, sizeof(info)) < 0)
    return -1;
  return 0;
}
//获取空闲内存量,在 kernel/kalloc.c 中添加:
uint64
kfreemem(void)
{
  struct run *r;
  uint64 n = 0;  
  acquire(&kmem.lock);
  r = kmem.freelist;
  while(r){
    n += PGSIZE;
    r = r->next;
  }
  release(&kmem.lock);  
  return n;
}
//获取进程数量,在 kernel/proc.c 中添加:
uint64
procnum(void)
{
  struct proc *p;
  uint64 n = 0;  
  for(p = proc; p < &proc[NPROC]; p++){
    acquire(&p->lock);
    if(p->state != UNUSED){
      n++;
    }
    release(&p->lock);
  }
  return n;
}  
#include "kernel/types.h"
#include "kernel/sysinfo.h"
#include "user/user.h"
int
main(int argc, char *argv[])
{
  struct sysinfo info;  
  if(sysinfo(&info) < 0){
    printf("sysinfo failed\n");
    exit(1);
  }  
  printf("free memory: %d bytes\n", info.freemem);
  printf("process count: %d\n", info.nproc);  
  exit(0);
}
