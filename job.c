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
int CURRENTQUEUE=1,SELECTQUEUE=1,RUN_TIME_COUNTER=0;//global variable.
struct waitqueue *headq1=NULL,*headq2=NULL,*headq3=NULL;
struct waitqueue *next=NULL,*current =NULL;
/* 调度程序 */
void scheduler()
{
	struct jobinfo *newjob=NULL;
	struct jobcmd cmd;
	int  count = 0;
	bzero(&cmd,DATALEN);
	if((count=read(fifo,&cmd,DATALEN))<0)
		error_sys("read fifo failed");

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

	updateall();

	next=jobselect();

	if(next!=NULL)
	printf("%s\n",next->job->cmdarg[0]);
	else printf("Next JOB IS NULL\n");
	/* 作业切换 */

	jobswitch();

}

int allocjid()
{
	return ++jobid;
}

void updateall()
{
	struct waitqueue *p,*p1,*p2,*p3,*pre;
	/* 更新作业运行时间 */
	if(current!=NULL){
		current->job->run_time += 1; /* 加1代表1000ms */
		RUN_TIME_COUNTER++;
	}
	/* 更新作业等待时间及优先级 */
	for(p=headq1;p!=NULL;p=p->next){//priority=2
		p->job->wait_time+=1000;
	}
	for(p=headq2,pre=headq2;p!=NULL;){//priority=1
		p->job->wait_time+=1000;
		if(p->job->wait_time>=10000){
			p->job->curpri=2;
			p->job->wait_time=0;
			if(p==headq2){
				headq2=headq2->next;
				p->next=NULL;
				//Find the p1
				if(headq1==NULL){
					headq1=p;
				}
				else
				{
					for(p1=headq1;p1->next!=NULL;p1=p1->next);
						p1->next=p;
				}
				p=headq2;
				pre=headq2;//A new beginning;
			}
			else{
				p2=p;
				pre->next=p->next;
				p2->next=NULL;
				if(headq1==NULL){
					headq1=p2;
				}else
				{
					for(p1=headq1;p1->next!=NULL;p1=p1->next);
						p1->next=p2;
				}
				p=p->next;//pre do not be changed;
			}
		}
		else{
			pre=p;
			p=p->next;
		}
	}
	for(p=headq3,pre=headq3;p!=NULL;)
	{//priority=0;
		p->job->wait_time+=1000;
		if(p->job->wait_time>=10000)
		{
			p->job->curpri=1;
			p->job->wait_time=0;
			if(headq3==p)
			{
				headq3=headq3->next;
				p->next=NULL;
				//Find the end of q2;
				if(headq2==NULL)
				{
					headq2=p;
				}
				else
				{
					for(p2=headq2;p2->next!=NULL;p2=p2->next);
						p2->next=p;
				}
				p=headq3;
				pre=headq3;
			}
			else
			{
			p3=p;
			pre->next=p->next;
			p3->next=NULL;
			if(headq2==NULL)
			{
				headq2=p3;
			}
			else
			{
				for(p2=headq2;p2->next!=NULL;p2=p2->next);
					p2->next=p3;
			}
			p=pre->next;
		}
	}
	else{
		pre=p;
		p=p->next;
	}}

}

struct waitqueue* jobselect()
{
	struct waitqueue *select;
	select = NULL;
	//For every queue,the mode is First Come,First Service.
	if(headq1!=NULL){
		SELECTQUEUE=1;
		select=headq1;
		headq1=headq1->next;
		select->next=NULL;
	}else if(headq2!=NULL){
		SELECTQUEUE=2;
		select=headq2;
		headq2=headq2->next;
		select->next=NULL;//What a good habit!
	}else if(headq3!=NULL){
		SELECTQUEUE=3;
		select=headq3;
		headq3=headq3->next;
		select->next=NULL;
	}else{
		SELECTQUEUE=0;
		select=NULL;
	}
	return select;
}

void jobswitch()
{
	//Note the changes of CURRENTQUEUE&SELECTQUEUE
	struct waitqueue *p;
	int i;
//The current job has down.Then we can choose the next with a wonderful mood.
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
		RUN_TIME_COUNTER=0;
		current = NULL;
	}
	if(next == NULL && current == NULL) /* 没有作业要运行 */

		return;
	else if (next != NULL && current == NULL){ /* 开始新的作业 */

		printf("begin start new job\n");
		current = next;
		CURRENTQUEUE=SELECTQUEUE;
	
		current->job->state = RUNNING;
		RUN_TIME_COUNTER=0;			//usleep(2000);
	//	waitpid(-1,NULL,WUNTRACED);
		kill(current->job->pid,SIGCONT);
			next = NULL;
		return;
	}
	else if (next != NULL && current != NULL){ /* 切换作业 */
	//   QIANG ZHAN//Sorry,I am poor in English.nextmei
	//QIANGZHAN condition:
	if(SELECTQUEUE<=CURRENTQUEUE&&next->job->run_time==0&&CURRENTQUEUE==2)//condition1;
	{
		printf("qiang zhan !!switch to Pid :%d\n",next->job->pid);
		kill(current->job->pid,SIGSTOP);
		//current->job>curpri do not change.
		current->job->wait_time=0;
		current->job->state=READY;
		if(headq2)
		{
			for(p=headq2;p->next!=NULL;p=p->next);
				p->next=current;
		}
		else
		{
			headq2=current;
		}
		current=next;
	//	next=NULL;
		CURRENTQUEUE=SELECTQUEUE;
		current->job->state=RUNNING;
		RUN_TIME_COUNTER=0;
		current->job->wait_time=0;
		current->next=NULL;//what a good habit!
		kill(current->job->pid,SIGCONT);
		next = NULL;
		return;
	}
	else if(SELECTQUEUE<=3&&next->job->run_time==0&&CURRENTQUEUE==3)//QIANG ZHAN condition2;
	{
		printf("qiang zhan !!switch to Pid :%d\n",next->job->pid);
		kill(current->job->pid,SIGSTOP);
		//current->job>curpri do not change.
		current->job->wait_time=0;
		current->job->state=READY;
		if(headq3)
		{
			for(p=headq3;p->next!=NULL;p=p->next);
				p->next=current;
		}
		else
		{
			headq3=current;
		}
		current=next;
//next=NULL;
		CURRENTQUEUE=SELECTQUEUE;
		current->job->state=RUNNING;
		RUN_TIME_COUNTER=0;
		current->job->wait_time=0;
		current->next=NULL;//what a good habit!
		kill(current->job->pid,SIGCONT);
		next = NULL;
		return;
	}
	else if(CURRENTQUEUE==1){//DEGREED
		printf("switch to Pid :%d\n",next->job->pid);
		kill(current->job->pid,SIGSTOP);
		//current->job>curpri do not change.
		current->job->wait_time=0;
		current->job->state=READY;
		current->job->curpri=1;//reduce priority,
		if(headq2)
		{
			for(p=headq2;p->next!=NULL;p=p->next);
				p->next=current;
		}
		else
		{
			headq2=current;
		}
		current=next;
	//	next=NULL;
		CURRENTQUEUE=SELECTQUEUE;
		current->job->state=RUNNING;
		RUN_TIME_COUNTER=0;
		current->job->wait_time=0;
		current->next=NULL;//what a good habit!
		kill(current->job->pid,SIGCONT);
		next = NULL;
		return;
	}
	else if(CURRENTQUEUE==2)//continue;
	{
		if(RUN_TIME_COUNTER>=2)//time pies are eated out...
		{
		printf("switch to Pid :%d\n",next->job->pid);
		kill(current->job->pid,SIGSTOP);
		//current->job>curpri do not change.
		current->job->wait_time=0;
		current->job->state=READY;
		current->job->curpri=0;//reduce priority,
		if(headq3)
		{
			for(p=headq3;p->next!=NULL;p=p->next);
				p->next=current;
		}
		else
		{
			headq3=current;
		}
		current=next;
//		next=NULL;
		CURRENTQUEUE=SELECTQUEUE;
		current->job->state=RUNNING;
		RUN_TIME_COUNTER=0;
		current->job->wait_time=0;
		current->next=NULL;//what a good habit!
		kill(current->job->pid,SIGCONT);	
			next = NULL;
			
		}
		else{//continue,put the next into the origin queue
			if(SELECTQUEUE==1){
				next->next=headq1;
				headq1=next;
				next=NULL;
			}
			else if(SELECTQUEUE==2){
				next->next=headq2;
				headq2=next;
				next=NULL;
			}
			else if(SELECTQUEUE==3){
				next->next=headq3;
				headq3=next;
				next=NULL;
			}
		}
		return;
	}
	else if(CURRENTQUEUE==3)//continue;
	{
		if(RUN_TIME_COUNTER>=5)//time pies are eated out...
		{
		printf("switch to Pid :%d\n",next->job->pid);
		kill(current->job->pid,SIGSTOP);
		//current->job>curpri do not change.
		current->job->wait_time=0;
		current->job->state=READY;
		current->job->curpri=0;//reduce priority,
		if(headq3)
		{
			for(p=headq3;p->next!=NULL;p=p->next);
				p->next=current;
		}
		else
		{
			headq3=current;
		}
		current=next;
	//	next=NULL;
		CURRENTQUEUE=SELECTQUEUE;
		current->job->state=RUNNING;
		RUN_TIME_COUNTER=0;
		current->job->wait_time=0;
		current->next=NULL;//what a good habit!
		kill(current->job->pid,SIGCONT);	
		next = NULL;
		}
		else{//continue,put the next into the origin queue
			if(SELECTQUEUE==1){
				next->next=headq1;
				headq1=next;
				next=NULL;
			}
			else if(SELECTQUEUE==2){
				next->next=headq2;
				headq2=next;
				next=NULL;
			}
			else if(SELECTQUEUE==3){
				next->next=headq3;
				headq3=next;
				next=NULL;
			}
		}
		return;
	}
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
	printf("SIGVTALRM RECEIVE!\n");
	scheduler();
	printf("Agter SIGVTALRM\n");
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

	sigemptyset(&zeromask);

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



	/*向等待队列中增加新的作业*/
	newnode = (struct waitqueue*)malloc(sizeof(struct waitqueue));
	newnode->next =NULL;
	newnode->job=newjob;
	if(newjob->defpri==0){
			if(headq3)//New comer should be added in headq1.
	 			{
					for(p=headq3;p->next != NULL; p=p->next);
					p->next =newnode;
				}else
					headq3=newnode;

	}
	else if(newjob->defpri==1){
			if(headq2)//New comer should be added in headq1.
	 			{
					for(p=headq2;p->next != NULL; p=p->next);
					p->next =newnode;
				}else
					headq2=newnode;

	}else if(newjob->defpri==2){
			if(headq1)//New comer should be added in headq1.
	 		   	{
					for(p=headq1;p->next != NULL; p=p->next);
					p->next =newnode;
				}else
					headq1=newnode;
	}

	/*为作业创建进程*/
	if((pid=fork())<0)
		error_sys("enq fork failed");

	if(pid==0){
		newjob->pid =getpid();
		/*阻塞子进程,等等执行*/
		printf("raise stop\n");
		raise(SIGSTOP);


		/*复制文件描述符到标准输出*/
		dup2(globalfd,1);
		/* 执行命令 */
		if(execv(arglist[0],arglist)<0)
			printf("exec failed\n");
		exit(1);
	}else{
		newjob->pid=pid;
		wait(NULL);
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
		if(headq1){
			for(prev=headq1,p=headq1;p!=NULL;prev=p,p=p->next)
				if(p->job->jid==deqid){
					select=p;
					selectprev=prev;
					break;
				}
				if(select!=NULL&&select==headq1)//headq1 is the target;
				{
					headq1=headq1->next;
					select->next=NULL;//This is a good habit;
				}
				else if(select!=NULL){
					selectprev->next=select->next;
					select->next=NULL;
				}
		}
		if(select==NULL&&headq2){
			for(prev=headq2,p=headq2;p!=NULL;prev=p,p=p->next)
				if(p->job->jid==deqid){
					select=p;
					selectprev=prev;
					break;
				}
				if(select!=NULL&&select==headq2)
				{
					headq2=headq2->next;
					select->next=NULL;
				}else if(select!=NULL){
					selectprev->next=select->next;
					select->next=NULL;
				}
		}
		if(select==NULL&&headq3){
			for(prev=headq3,p=headq3;p!=NULL;prev=p,p=p->next)
				if(p->job->jid==deqid){
					select=p;
					selectprev=prev;
					break;
				}
				if(select!=NULL&&select==headq3){
					headq3=headq3->next;
					select->next=NULL;
				}else if(select!=NULL){
					selectprev->next=select->next;
					select->next=NULL;
				}
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
	printf("JOBID\tPID\tOWNER\tRUNTIME\tWAITTIME\tCREATTIME\t\tSTATE\tQUEUE\n");
	if(current){
		strcpy(timebuf,ctime(&(current->job->create_time)));
		timebuf[strlen(timebuf)-1]='\0';
		printf("%d\t%d\t%d\t%d\t%d\t%s\t%s\t%s\n",
			current->job->jid,
			current->job->pid,
			current->job->ownerid,
			current->job->run_time,
			current->job->wait_time,
			timebuf,"RUNNING","CURRENT");
	}
	for(p=headq1;p!=NULL;p=p->next){
		strcpy(timebuf,ctime(&(p->job->create_time)));
		timebuf[strlen(timebuf)-1]='\0';
		printf("%d\t%d\t%d\t%d\t%d\t%s\t%s\t%s\n",
			p->job->jid,
			p->job->pid,
			p->job->ownerid,
			p->job->run_time,
			p->job->wait_time,
			timebuf,
			"READY","q1");
	}
	for(p=headq2;p!=NULL;p=p->next){
		strcpy(timebuf,ctime(&(p->job->create_time)));
		timebuf[strlen(timebuf)-1]='\0';
		printf("%d\t%d\t%d\t%d\t%d\t%s\t%s\t%s\n",
			p->job->jid,
			p->job->pid,
			p->job->ownerid,
			p->job->run_time,
			p->job->wait_time,
			timebuf,
			"READY","q2");
	}
		for(p=headq3;p!=NULL;p=p->next){
		strcpy(timebuf,ctime(&(p->job->create_time)));
		timebuf[strlen(timebuf)-1]='\0';
		printf("%d\t%d\t%d\t%d\t%d\t%s\t%s\t%s\n",
			p->job->jid,
			p->job->pid,
			p->job->ownerid,
			p->job->run_time,
			p->job->wait_time,
			timebuf,
			"READY","q3");
	}
}

int main()
{
	struct timeval interval;
	struct itimerval new,old;
	struct stat statbuf;
	struct sigaction newact,oldact1,oldact2;

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
	newact.sa_flags=SA_SIGINFO;
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
