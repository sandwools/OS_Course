#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

// Simplifed xv6 shell.

#define MAXARGS 10

// All commands have at least a type. Have looked at the type, the code
// typically casts the *cmd to some specific cmd type.
struct cmd {
  int type;          //  ' ' (exec), | (pipe), '<' or '>' for redirection
};

struct execcmd {
  int type;              // ' '
  char *argv[MAXARGS];   // arguments to the command to be exec-ed
};

struct redircmd {
  int type;          // < or > 
  struct cmd *cmd;   // the command to be run (e.g., an execcmd)
  char *file;        // the input/output file
  int flags;         // flags for open() indicating read or write
  int fd;            // the file descriptor number to use for the file
};

struct pipecmd {
  int type;          // |
  struct cmd *left;  // left side of pipe
  struct cmd *right; // right side of pipe
};

int fork1(void);  // Fork but exits on failure.
struct cmd *parsecmd(char*);

//...mycode

void implementEcmd(struct execcmd* ecmd)
{
    //在根目录的bin下执行命令
    char path[20] = "/bin/";
    strcat(path, ecmd->argv[0]);
   
    //如果执行失败，则调整到usr用户目录下执行
    if (  execv(path, ecmd->argv)== -1) 
    {
        strcpy(path, "/usr/bin/");
        strcat(path, ecmd->argv[0]);

        //如果执行失败，则调整到当前目录下执行
        if (execv(path, ecmd->argv) == -1) 
        {
            getcwd(path, 256);
            strcat(path, ecmd->argv[0]);

            //全部失败，抛出错误
            if (execv(path, ecmd->argv) == -1)
            {
                perror(ecmd->argv[0]);
                _exit(0);
            }
        }
    }

    //或者可以用system call函数实现
    //int code=system(ecmd->argv);
}

void implementRcmd(struct redircmd* rcmd)
{
    //释放原文件描述,做错误处理
    if (close(rcmd->fd)==-1)
    {
        char err[] = "Close fd: ";
        strcat(err, rcmd->fd);
        perror(err);
        _exit(0);
    }

    //打开文件并检查权限
    //通过关闭和重新打开使得描述符不变的情况下替换了输入输出，实现重定向
    if (open(rcmd->file, rcmd->flags, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH) == -1)
    {
        char err[] = "Open ";
        strcat(err, rcmd->file);
        perror(err);
        _exit(0);
    }
}

void implementPcmd(struct pipecmd* pcmd,int p[])
{
    if (pipe(p) == -1) { perror("pipe"); } //生成管道储存在p1 p2；
    int code = fork1();
    if(code==-1){ perror("fork1"); }
    if(code==0)
    {
        close(1);
        dup(p[1]);//复制管道写入符到新的文件表中，成为新的最小描述符
        //close(p[0]);
        //close(p[1]);//关闭源管道，只留下镜像管道
        runcmd(pcmd->left);
    }
    else
    {
        close(0);
        dup(p[0]);
        //close(p[0]);
        //close(p[1]);
        runcmd(pcmd->right);
    }
}

//...



// Execute cmd.  Never returns.
void
runcmd(struct cmd *cmd)
{
  int p[2], r;//p用于缓冲区建立
  struct execcmd *ecmd;
  struct pipecmd *pcmd;
  struct redircmd *rcmd;

  if(cmd == 0)
    _exit(0);
  
  switch(cmd->type){
  default:
    fprintf(stderr, "unknown runcmd\n");
    _exit(-1);

  case ' ':
    ecmd = (struct execcmd*)cmd;
    if(ecmd->argv[0] == 0)
      _exit(0);
    implementEcmd(ecmd);
    break;

  case '>':
  case '<':
    rcmd = (struct redircmd*)cmd;
    implementRcmd(rcmd);
    runcmd(rcmd->cmd);//递归（返回）调用
    break;

  case '|':
    pcmd = (struct pipecmd*)cmd;
    implementPcmd(pcmd,p);
    break;
  }    
  _exit(0);
}

int
getcmd(char *buf, int nbuf)
{
    /*
      if (isatty(fileno(stdin)))
      fprintf(stdout, "6.828$ ");
      memset(buf, 0, nbuf);
      if(fgets(buf, nbuf, stdin) == 0)
      return -1; // EOF
      return 0;
    */

    if (isatty(fileno(stdin))) //如果是标准输入环境则显示
    {
        char pPath[256] = { 0 };
        getcwd(pPath, 256);
        fprintf(stdout, "lyc@6.828_shell:");
        fprintf(stdout, pPath);
        fprintf(stdout, "$ ");
    }
    memset(buf, 0, nbuf);
    
    if (fgets(buf, nbuf, stdin) == 0)
    {
        return -1; // EOF

    }
    return 0;
}

int
main(void)
{
  static char buf[100];
  int fd, r;

  // Read and run input commands.
  while(getcmd(buf, sizeof(buf)) >= 0){
    if(buf[0] == 'c' && buf[1] == 'd' && buf[2] == ' '){
      // Clumsy but will have to do for now.
      // Chdir has no effect on the parent if run in the child.
      buf[strlen(buf)-1] = 0;  // chop \n
      if(chdir(buf+3) < 0)
        fprintf(stderr, "cannot cd %s\n", buf+3);
      continue;
    }
    if(fork1() == 0)
      runcmd(parsecmd(buf));
    wait(&r);
  }
  exit(0);
}

int
fork1(void)
{
  int pid;
  
  pid = fork();
  if(pid == -1)
    perror("fork");
  return pid;
}

struct cmd*
execcmd(void)
{
  struct execcmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = ' ';
  return (struct cmd*)cmd;
}

struct cmd*
redircmd(struct cmd *subcmd, char *file, int type)
{
  struct redircmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = type;
  cmd->cmd = subcmd;
  cmd->file = file;
  cmd->flags = (type == '<') ?  O_RDONLY : O_WRONLY|O_CREAT|O_TRUNC;
  cmd->fd = (type == '<') ? 0 : 1;
  return (struct cmd*)cmd;
}

struct cmd*
pipecmd(struct cmd *left, struct cmd *right)
{
  struct pipecmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = '|';
  cmd->left = left;
  cmd->right = right;
  return (struct cmd*)cmd;
}

// Parsing

char whitespace[] = " \t\r\n\v";
char symbols[] = "<|>";

int
gettoken(char **ps, char *es, char **q, char **eq)
{
  char *s;
  int ret;
  
  s = *ps;
  while(s < es && strchr(whitespace, *s))
    s++;
  if(q)
    *q = s;
  ret = *s;
  switch(*s){
  case 0:
    break;
  case '|':
  case '<':
    s++;
    break;
  case '>':
    s++;
    break;
  default:
    ret = 'a';
    while(s < es && !strchr(whitespace, *s) && !strchr(symbols, *s))
      s++;
    break;
  }
  if(eq)
    *eq = s;
  
  while(s < es && strchr(whitespace, *s))
    s++;
  *ps = s;
  return ret;
}

int
peek(char **ps, char *es, char *toks)
{
  char *s;
  
  s = *ps;
  while(s < es && strchr(whitespace, *s))
    s++;
  *ps = s;
  return *s && strchr(toks, *s);
}

struct cmd *parseline(char**, char*);
struct cmd *parsepipe(char**, char*);
struct cmd *parseexec(char**, char*);

// make a copy of the characters in the input buffer, starting from s through es.
// null-terminate the copy to make it a string.
char 
*mkcopy(char *s, char *es)
{
  int n = es - s;
  char *c = malloc(n+1);
  assert(c);
  strncpy(c, s, n);
  c[n] = 0;
  return c;
}

struct cmd*
parsecmd(char *s)
{
  char *es;
  struct cmd *cmd;

  es = s + strlen(s);
  cmd = parseline(&s, es);
  peek(&s, es, "");
  if(s != es){
    fprintf(stderr, "leftovers: %s\n", s);
    exit(-1);
  }
  return cmd;
}

struct cmd*
parseline(char **ps, char *es)
{
  struct cmd *cmd;
  cmd = parsepipe(ps, es);
  return cmd;
}

struct cmd*
parsepipe(char **ps, char *es)
{
  struct cmd *cmd;

  cmd = parseexec(ps, es);
  if(peek(ps, es, "|")){
    gettoken(ps, es, 0, 0);
    cmd = pipecmd(cmd, parsepipe(ps, es));
  }
  return cmd;
}

struct cmd*
parseredirs(struct cmd *cmd, char **ps, char *es)
{
  int tok;
  char *q, *eq;

  while(peek(ps, es, "<>")){
    tok = gettoken(ps, es, 0, 0);
    if(gettoken(ps, es, &q, &eq) != 'a') {
      fprintf(stderr, "missing file for redirection\n");
      exit(-1);
    }
    switch(tok){
    case '<':
      cmd = redircmd(cmd, mkcopy(q, eq), '<');
      break;
    case '>':
      cmd = redircmd(cmd, mkcopy(q, eq), '>');
      break;
    }
  }
  return cmd;
}

struct cmd*
parseexec(char **ps, char *es)
{
  char *q, *eq;
  int tok, argc;
  struct execcmd *cmd;
  struct cmd *ret;
  
  ret = execcmd();
  cmd = (struct execcmd*)ret;

  argc = 0;
  ret = parseredirs(ret, ps, es);
  while(!peek(ps, es, "|")){
    if((tok=gettoken(ps, es, &q, &eq)) == 0)
      break;
    if(tok != 'a') {
      fprintf(stderr, "syntax error\n");
      exit(-1);
    }
    cmd->argv[argc] = mkcopy(q, eq);
    argc++;
    if(argc >= MAXARGS) {
      fprintf(stderr, "too many args\n");
      exit(-1);
    }
    ret = parseredirs(ret, ps, es);
  }
  cmd->argv[argc] = 0;
  return ret;
}
