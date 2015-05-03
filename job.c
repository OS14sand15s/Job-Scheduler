#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <time.h>
#include "job.h"
#define DEBUG
int jobid=0;
int siginfo=1;
int fifo;
int globalfd;

struct waitqueue *head=NULL;
struct waitqueue *next=NULL,*current =NULL;
//IMPORT，打印等待队列进程信息专用，注意用啊，小伙伴们。
void printWaitQueue()
{
	struct waitqueue *p;
	char timebuf[BUFLEN];
	/*
	/* 打印信息头部 */
	if(head!=NULL)
	printf("JOBID\tPID\tOWNER\tRUNTIME\tWAITTIME\tCREATTIME\t\tSTATE\n");

	for(p=head;p!=NULL;p=p->next){
		strcpy(timebuf,ctime(&(p->job->create_time)));
		timebuf[strlen(timebuf)-1]='\0';
		printf("%d\t%d\t%d\t%d\t%d\t%s\t%s\n",
			p->job->jid,
			p->job->pid,
			p->job->ownerid,
			p->job->run_time,
			p->job->wait_time,
			timebuf,
			"READY");
	}
}

/* 调度程序 */
void scheduler()
{
	struct jobinfo *newjob=NULL;
	struct jobcmd cmd;
	int  count = 0;
                bzero(&cmd,DATALEN);
//*　　功能：置字节字符串s的前n个字节为零。说明：bzero无返回值。
	if((count=read(fifo,&cmd,DATALEN))<0)//DEBUG  ????????????????????
		error_sys("read fifo failed");
#ifdef DEBUG

	if(count){
		printf("cmd cmdtype\t%d\ncmd defpri\t%d\ncmd data\t%s\n",cmd.type,cmd.defpri,cmd.data);
	}
	else
		printf("no data read\n");
#endif

        //调试任务6
        #ifdef DEBUG
	printWaitQueue();
	#endif
	/* 更新等待队列中的作业 */
	updateall();
	#ifdef DEBUG
	printWaitQueue();
	#endif
	switch(cmd.type){
	case ENQ:
		do_enq(newjob,cmd);
		break;
	case DEQ:
		do_deq(cmd);
		break;
	case STAT:
		do_stat(cmd);
		break;
	default:
		break;
	}

	/* 选择高优先级作业 */
	next=jobselect();
	/* 作业切换 */
	jobswitch();
}

int allocjid()
{
	return ++jobid;
}

void updateall()
{
	struct waitqueue *p;

	/* 更新作业运行时间 */
	if(current)         //====Global variable
		current->job->run_time += 1; /* 加1代表1000ms *//*run_time 作业运行时间 */

	/* 更新作业等待时间及优先级 */
	for(p = head; p != NULL; p = p->next){
		p->job->wait_time += 1000;//都更新了那些等待时间。????????????????????how current?
		if(p->job->wait_time >= 5000 && p->job->curpri < 3){
			p->job->curpri++;
			p->job->wait_time = 0;
		}
	}
}

struct waitqueue* jobselect()
{
	struct waitqueue *p,*prev,*select,*selectprev;
	int highest = -1;

	select = NULL;
	selectprev = NULL;
	if(head){
		/* 遍历等待队列中的作业，找到优先级最高的作业 */
		for(prev = head, p = head; p != NULL; prev = p,p = p->next)
			if(p->job->curpri > highest){
				select = p;
				selectprev = prev;
				highest = p->job->curpri;
			}
			selectprev->next = select->next;
			if (select == selectprev)
				head = NULL;
	}
	return select;
}

void jobswitch()
{
	struct waitqueue *p;
	int i;

	if(current && current->job->state == DONE){ /* 当前作业完成 */
		/* 作业完成，删除它 */
		for(i = 0;(current->job->cmdarg)[i] != NULL; i++){
			free((current->job->cmdarg)[i]);
			(current->job->cmdarg)[i] = NULL;
		}
		/* 释放空间 */
		free(current->job->cmdarg);
		free(current->job);
		free(current);

		current = NULL;
	}

	if(next == NULL && current == NULL) /* 没有作业要运行 */

		return;
	else if (next != NULL && current == NULL){ /* 开始新的作业 */

		printf("begin start new job\n");
		current = next;
		next = NULL;
		current->job->state = RUNNING;
		kill(current->job->pid,SIGCONT);
		return;
	}
	else if (next != NULL && current != NULL){ /* 切换作业 */

		printf("switch to Pid: %d\n",next->job->pid);
		kill(current->job->pid,SIGSTOP);
		current->job->curpri = current->job->defpri;
		current->job->wait_time = 0;
		current->job->state = READY;

		/* 放回等待队列 */
		if(head){
			for(p = head; p->next != NULL; p = p->next);
			p->next = current;
		}else{
			head = current;
		}
		current = next;
		next = NULL;
		current->job->state = RUNNING;
		current->job->wait_time = 0;
		kill(current->job->pid,SIGCONT);
		return;
	}else{ /* next == NULL且current != NULL，不切换 */
		return;
	}
}

void sig_handler(int sig,siginfo_t *info,void *notused)
{
	int status;
	int ret;

	switch (sig) {
case SIGVTALRM: /* 到达计时器所设置的计时间隔 */
	scheduler();
	#ifdef DEBUG
	printf("SIGVTALRM RECEIVED!\n");
	#endif
	return;
case SIGCHLD: /* 子进程结束时传送给父进程的信号 */
	ret = waitpid(-1,&status,WNOHANG);
	if (ret == 0)
		return;
	if(WIFEXITED(status)){
		current->job->state = DONE;
		printf("normal termation, exit status = %d\n",WEXITSTATUS(status));
	}else if (WIFSIGNALED(status)){
		printf("abnormal termation, signal number = %d\n",WTERMSIG(status));
	}else if (WIFSTOPPED(status)){
		printf("child stopped, signal number = %d\n",WSTOPSIG(status));
	}
	return;
	default:
		return;
	}
}

void do_enq(struct jobinfo *newjob,struct jobcmd enqcmd)
{
	struct waitqueue *newnode,*p;
	int i=0,pid;
	char *offset,*argvec,*q;
	char **arglist;
	sigset_t zeromask;

    /*typedef struct {  
      
         unsignedlong sig[_NSIG_WORDS];  
      
    } sigset_t;
    #include<signal.h>  
      
    int sigemptyset(sigset_t *set);  
      
    int sigfillset(sigset_t *set);  
      
    int sigadd(sigset_t *set,int setnumber);  
      
    int sigdelset(sigset_t *set,int setnumber);  

    nt sigismember(const sigset_t set,int signumber),
set表示信号集指针，setnumber表示信号。

sigemptyset用于将set指向的信号集设为空，即不包含任何信号。

sigfillset用于将set指向的信号集设为满，即包含所有的信号。

sigaddset用于向信号集中添加信号。

sigdelset用于向信号集中删除信号。

以上函数成功返回0，失败返回-1

i用于检测signumber是否在set中，若属于返回1，不是返回0.
      */

	sigemptyset(&zeromask);//

	/* 封装jobinfo数据结构 */
	newjob = (struct jobinfo *)malloc(sizeof(struct jobinfo));
	newjob->jid = allocjid();
	newjob->defpri = enqcmd.defpri;
	newjob->curpri = enqcmd.defpri;
	newjob->ownerid = enqcmd.owner;
	newjob->state = READY;
	newjob->create_time = time(NULL);
	newjob->wait_time = 0;
	newjob->run_time = 0;
	arglist = (char**)malloc(sizeof(char*)*(enqcmd.argnum+1));
	newjob->cmdarg = arglist;
	offset = enqcmd.data;
	argvec = enqcmd.data;
	while (i < enqcmd.argnum){
		if(*offset == ':'){
			*offset++ = '\0';
			q = (char*)malloc(offset - argvec);
			strcpy(q,argvec);
			arglist[i++] = q;
			argvec = offset;
		}else
			offset++;
	}

	arglist[i] = NULL;

#ifdef DEBUG

	printf("enqcmd argnum %d\n",enqcmd.argnum);
	for(i = 0;i < enqcmd.argnum; i++)
		printf("parse enqcmd:%s\n",arglist[i]);

#endif

	/*向等待队列中增加新的作业*/
	newnode = (struct waitqueue*)malloc(sizeof(struct waitqueue));
	newnode->next =NULL;
	newnode->job=newjob;

	if(head)
	{
		for(p=head;p->next != NULL; p=p->next);
		p->next =newnode;
	}else
		head=newnode;

	/*为作业创建进程*/
	if((pid=fork())<0)
		error_sys("enq fork failed");

	if(pid==0){
		newjob->pid =getpid();
		/*阻塞子进程,等等执行*/
		raise(SIGSTOP);
#ifdef DEBUG

		printf("begin running\n");
		for(i=0;arglist[i]!=NULL;i++)
			printf("arglist %s\n",arglist[i]);
#endif

		/*复制文件描述符到标准输出*/
		dup2(globalfd,1);
		/* 执行命令 */
		if(execv(arglist[0],arglist)<0)
			printf("exec failed\n");
		exit(1);
	}else{
		newjob->pid=pid;
	}
}

void do_deq(struct jobcmd deqcmd)
{
	int deqid,i;
	struct waitqueue *p,*prev,*select,*selectprev;
	deqid=atoi(deqcmd.data);

#ifdef DEBUG
	printf("deq jid %d\n",deqid);
#endif

	/*current jodid==deqid,终止当前作业*/
	if (current && current->job->jid ==deqid){
		printf("teminate current job\n");
		kill(current->job->pid,SIGKILL);
		for(i=0;(current->job->cmdarg)[i]!=NULL;i++){
			free((current->job->cmdarg)[i]);
			(current->job->cmdarg)[i]=NULL;
		}
		free(current->job->cmdarg);
		free(current->job);
		free(current);
		current=NULL;
	}
	else{ /* 或者在等待队列中查找deqid */
		select=NULL;
		selectprev=NULL;
		if(head){
			for(prev=head,p=head;p!=NULL;prev=p,p=p->next)
				if(p->job->jid==deqid){
					select=p;
					selectprev=prev;
					break;
				}
				selectprev->next=select->next;
				if(select==selectprev)
					head=NULL;
		}
		if(select){
			for(i=0;(select->job->cmdarg)[i]!=NULL;i++){
				free((select->job->cmdarg)[i]);
				(select->job->cmdarg)[i]=NULL;
			}
			free(select->job->cmdarg);
			free(select->job);
			free(select);
			select=NULL;
		}
	}
}

void do_stat(struct jobcmd statcmd)
{
	struct waitqueue *p;
	char timebuf[BUFLEN];
	/*
	*打印所有作业的统计信息:
	*1.作业ID
	*2.进程ID
	*3.作业所有者
	*4.作业运行时间
	*5.作业等待时间
	*6.作业创建时间
	*7.作业状态
	*/

	/* 打印信息头部 */
	printf("JOBID\tPID\tOWNER\tRUNTIME\tWAITTIME\tCREATTIME\t\tSTATE\n");
	if(current){
		strcpy(timebuf,ctime(&(current->job->create_time)));
		timebuf[strlen(timebuf)-1]='\0';
		printf("%d\t%d\t%d\t%d\t%d\t%s\t%s\n",
			current->job->jid,
			current->job->pid,
			current->job->ownerid,
			current->job->run_time,
			current->job->wait_time,
			timebuf,"RUNNING");
	}

	for(p=head;p!=NULL;p=p->next){
		strcpy(timebuf,ctime(&(p->job->create_time)));
		timebuf[strlen(timebuf)-1]='\0';
		printf("%d\t%d\t%d\t%d\t%d\t%s\t%s\n",
			p->job->jid,
			p->job->pid,
			p->job->ownerid,
			p->job->run_time,
			p->job->wait_time,
			timebuf,
			"READY");
	}
}

int main()
{
	struct timeval interval;
	struct itimerval new,old;
	struct stat statbuf;
	struct sigaction newact,oldact1,oldact2;
	#ifdef DEBUG
	printf("DEBUG IS OPEN!");
	 #endif
	if(stat("/tmp/server",&statbuf)==0){
		/* 如果FIFO文件存在,删掉 */
		if(remove("/tmp/server")<0)
			error_sys("remove failed");
	}

	if(mkfifo("/tmp/server",0666)<0)
		error_sys("mkfifo failed");
	/* 在非阻塞模式下打开FIFO */
	if((fifo=open("/tmp/server",O_RDONLY|O_NONBLOCK))<0)
		error_sys("open fifo failed");

	/* 建立信号处理函数 */
	newact.sa_sigaction=sig_handler;
	sigemptyset(&newact.sa_mask);
	newact.sa_flags=SA_SIGINFO;//此选项对信号处理程序提供了附加信息：一个指向siginfo结构的指针以及一个指向进程上下文标志符的之指针。
	sigaction(SIGCHLD,&newact,&oldact1);
	sigaction(SIGVTALRM,&newact,&oldact2);

	/* 设置时间间隔为1000毫秒 */
	interval.tv_sec=1;
	interval.tv_usec=0;

	new.it_interval=interval;
	new.it_value=interval;
	setitimer(ITIMER_VIRTUAL,&new,&old);

	while(siginfo==1);

	close(fifo);
	close(globalfd);
	return 0;
}
