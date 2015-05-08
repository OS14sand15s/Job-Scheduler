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
struct waitqueue *head=NULL,*headq1=NULL,*headq2=NULL,*headq3=NULL;//q1 has the highest priority!You can delete ptr head.
struct waitqueue *next=NULL,*current =NULL;


/* µ÷¶È³ÌÐò */
void scheduler()
{
	struct jobinfo *newjob=NULL;
	struct jobcmd cmd;
	int  count = 0;
	bzero(&cmd,DATALEN);
	if((count=read(fifo,&cmd,DATALEN)) < 0)
		error_sys("read fifo failed");
	#ifdef DEBUG
        printf("Task 3:Reading whether other process send command!\n");
	if(count){
		printf("cmd cmdtype\t%d\ncmd defpri\t%d\ncmd data\t%s\n",cmd.type,cmd.defpri,cmd.data);
	}
	else
		printf("no data read\n");
	#endif
	/* žüÐÂµÈŽý¶ÓÁÐÖÐµÄ×÷Òµ */
	#ifdef DEBUG
        printf("Task 6: Before Updateall\n");
	#endif
	#ifdef DEBUG
        printf("Task 3:Update jobs in wait queue!\n");
	#endif       
	updateall();
	#ifdef DEBUG
        printf("Task 6: After Updateall\n");
	#endif
	switch(cmd.type){
	case ENQ:
                #ifdef DEBUG 
                printf("Task 3:Execute enq!\n");
                #endif
                    #ifdef DEBUG
                    printf("Task 7:Before ENQ\n");
                    #endif
		do_enq(newjob,cmd);
	              #ifdef DEBUG
                    printf("Task 7:After ENQ\n");
                    #endif
		break;
	case DEQ:
                #ifdef DEBUG 
                printf("Task 3:Execute deq!\n");
                #endif
                    #ifdef DEBUG
                    printf("Task 7:Before DEQ\n");
                    #endif
		do_deq(cmd);
                    #ifdef DEBUG
                    printf("Task 7:After DEQ\n");
                    #endif
		break;
	case STAT:
                #ifdef DEBUG 
                printf("Task 3:Execute stat!\n");
                #endif
                    #ifdef DEBUG
                    printf("Task 7:Before STAT\n");
                    #endif
		do_stat(cmd);
                    #ifdef DEBUG
                    printf("Task 7:After STAT\n");
                    #endif
		break;
	default:
		break;
	}
        #ifdef DEBUG
        printf("Task 3:Select which job to run next!\n");
        #endif
	/* Ñ¡ÔñžßÓÅÏÈŒ¶×÷Òµ */
	next=jobselect();
        #ifdef DEBUG
        printf("Task 3:Switch to the next!\n");
        #endif
	/* ×÷ÒµÇÐ»» */
      
	jobswitch();
       
}
int allocjid()
{
	return ++jobid;
}
void updateall()
{
	struct waitqueue *p;
	/* žüÐÂ×÷ÒµÔËÐÐÊ±Œä */
	if(current)
		current->job->run_time += 1; /* ŒÓ1Žú±í1000ms */
	/* žüÐÂ×÷ÒµµÈŽýÊ±ŒäŒ°ÓÅÏÈŒ¶ */
	for(p = head; p != NULL; p = p->next){
		p->job->wait_time += 1000;
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
		/* ±éÀúµÈŽý¶ÓÁÐÖÐµÄ×÷Òµ£¬ÕÒµœÓÅÏÈŒ¶×îžßµÄ×÷Òµ */
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
	if(current && current->job->state == DONE){ /* current job complished*/ 
		/* delete it*/ 
		for(i = 0;(current->job->cmdarg)[i] != NULL; i++){ 
 			free((current->job->cmdarg)[i]); 
			(current->job->cmdarg)[i] = NULL; 
		} 
 		/* free */ 
		free(current->job->cmdarg); 
		free(current->job); 
		free(current); 
		current = NULL; 
	} 
	if(next == NULL && current == NULL) /* none to run*/ 

		return; 
 	else if (next != NULL && current == NULL){ /* start new job*/ 
		printf("begin start new job\n"); 
		current = next; 
 		next = NULL; 
		current->job->state = RUNNING; 
		current->next=NULL;//important 
		//usleep(2000); 
		waitpid(-1,NULL,WUNTRACED); 
		kill(current->job->pid,SIGCONT); 
		return; 
	} 
	else if (next != NULL && current != NULL){ /* switch job*/ 
		printf("switch to Pid: %d\n",next->job->pid); 
		kill(current->job->pid,SIGSTOP); 
		current->job->curpri = current->job->defpri; 
		current->job->wait_time = 0; 
		current->job->state = READY; 
		current->next=NULL; 
 		/*back to the waiting queue */ 
		if(head){ 
 			for(p = head; p->next != NULL; p = p->next); 
 			p->next = current; 
 		}else{ 
			head = current; 
		} 
		current = next; 
 		current->next=NULL; 
 		next = NULL; 
		current->job->state = RUNNING; 
 		current->job->wait_time = 0; 
 	       // waitpid(-1,NULL,WUNTRACED); 
		kill(current->job->pid,SIGCONT); 
 		return; 
 	}else{ /* next == NULL&&current != NULL，no switch */ 
		return; 
 	} 
} 

void sig_handler(int sig,siginfo_t *info,void *notused)
{
	int status;
	int ret;
	switch (sig) {
case SIGVTALRM: /* µœŽïŒÆÊ±Æ÷ËùÉèÖÃµÄŒÆÊ±Œäžô */
	scheduler();
        /*debug 2*/
        #ifdef DEBUG 
	printf("Task 2:SIGVTALRM RECEIVED!\n"); 
	#endif 
	return;
case SIGCHLD: /* ×Óœø³ÌœáÊøÊ±Ž«ËÍžøžžœø³ÌµÄÐÅºÅ */
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
        #ifdef DEBUG
	char timebuf[BUFLEN];
       printf("Task 10:\nThe current job:\nJOBID\tPID\tOWNER\tRUNTIME\tWAITTIME\tCREATTIME\t\n"); 
	strcpy(timebuf,ctime(&(current->job->create_time))); 
	timebuf[strlen(timebuf)-1]='\0'; 
 	printf("%d\t%d\t%d\t%d\t%d\t%s\n", 
		current->job->jid, 
		current->job->pid, 
		current->job->ownerid, 
		current->job->run_time, 
		current->job->wait_time, 
		timebuf);
        printf("Task 10:\nThe information of the queue:\n");
        printWaitQueue();
        #endif
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
	/* ·â×°jobinfoÊýŸÝœá¹¹ */
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
	/*ÏòµÈŽý¶ÓÁÐÖÐÔöŒÓÐÂµÄ×÷Òµ*/
	newnode = (struct waitqueue*)malloc(sizeof(struct waitqueue));
	newnode->next =NULL;
	newnode->job=newjob;
	if(head)
	{
		for(p=head;p->next != NULL; p=p->next);
		p->next =newnode;
	}else
		head=newnode;
	/*Îª×÷ÒµŽŽœšœø³Ì*/

	if((pid=fork())<0)

		error_sys("enq fork failed");



	if(pid==0){
		newjob->pid =getpid();
		/*×èÈû×Óœø³Ì,µÈµÈÖŽÐÐ*/
		raise(SIGSTOP);
#ifdef DEBUG
		printf("begin running\n");
		for(i=0;arglist[i]!=NULL;i++)
			printf("arglist %s\n",arglist[i]);

#endif
		/*žŽÖÆÎÄŒþÃèÊö·ûµœ±ê×ŒÊä³ö*/
		dup2(globalfd,1);
		/* ÖŽÐÐÃüÁî */
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

	/*current jodid==deqid,ÖÕÖ¹µ±Ç°×÷Òµ*/
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
	else{ /* »òÕßÔÚµÈŽý¶ÓÁÐÖÐ²éÕÒdeqid */
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

	/* ŽòÓ¡ÐÅÏ¢Í·²¿ */
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
        /*debug 1*/
        #ifdef DEBUG 
	printf("Task 1: DEBUG IS OPEN!\n"); 
	#endif 
	if(stat("/tmp/server",&statbuf)==0){
		/* Èç¹ûFIFOÎÄŒþŽæÔÚ,ÉŸµô */
		if(remove("/tmp/server")<0)
			error_sys("remove failed");
	}
	if(mkfifo("/tmp/server",0666)<0)
		error_sys("mkfifo failed");
	/* ÔÚ·Ç×èÈûÄ£ÊœÏÂŽò¿ªFIFO */
	if((fifo=open("/tmp/server",O_RDONLY|O_NONBLOCK))<0)
		error_sys("open fifo failed");
	/* œšÁ¢ÐÅºÅŽŠÀíº¯Êý */
	newact.sa_sigaction=sig_handler;
	sigemptyset(&newact.sa_mask);
	newact.sa_flags=SA_SIGINFO;
	sigaction(SIGCHLD,&newact,&oldact1);
	sigaction(SIGVTALRM,&newact,&oldact2);
	/* ÉèÖÃÊ±ŒäŒäžôÎª1000ºÁÃë */
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
