//////////////////////////////////////////////////////////////////////////////////////////////////
// File Name     :   proxy_cache.c
// Date          :   2022/05/27
// OS            :   Ubuntu 16.04 LTS 64bits
// Author        :   Lee Jun Hyeong
// Student ID    :   2018202060
// ---------------------------------------------------------------------------------------------//
// Title : System Programming Assignment #3-2 (proxy server)                                    //
// Description : semaphore, thread, HTTP request, response Create directory, Log text File and distinguish Hit or Miss //
//////////////////////////////////////////////////////////////////////////////////////////////////

#include<stdio.h>
#include<string.h>
#include<openssl/sha.h>
#include<pwd.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<stdlib.h>
#include<fcntl.h>
#include<dirent.h>
#include<time.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<signal.h>
#include<netdb.h>
#include<sys/ipc.h>
#include<sys/sem.h>
#include<pthread.h>

#define BUFFSIZE	1024
#define PORTNO		39999


char inputurl[BUFFSIZE]; //input url 
char inputurlfile[BUFFSIZE]; //cache file path
char checkurl[BUFFSIZE];
char global_home[100];
int process_count; //child process count
int write_state; //write state
int check;	
pid_t received_pid; //received pid
time_t current_time; //current time

void p(int semid);
void v(int semid);


///////////////////////////////////////////////////////////////////////
// sha1_hash                                                         //
// ================================================================= //
// Input: input_url -> user puts url                                 //
// hashed_url -> input url is changed into hashed url                //
// Output: char* hashed_url -> input url is hashed by sha-1 algorithm//
// 0 fail                                                            //
// input_url will be hashed by sha-1 algorithm into hashing_url      //
// Purpose: hashing url                                              //
///////////////////////////////////////////////////////////////////////

char *sha1_hash(char* input_url, char* hashed_url)
{//sha1 to extract cache
    unsigned char hashed_160bits[20]; //hashed 160bits values
    char hashed_hex[41]; // changed hashed_160bits into hexadecimal
    int i; //for counting

    SHA1(input_url, strlen(input_url), hashed_160bits); //use SHA1-algorithm

    for(i=0;i<sizeof(hashed_160bits);i++)
        sprintf(hashed_hex+i*2, "%02x", hashed_160bits[i]); //convert hashed_160bits into hex
        
    strcpy(hashed_url, hashed_hex); //to return hash

    return hashed_url; // and return 
}

///////////////////////////////////////////////////////////////////////
// getHomeDir                                                        //
// ================================================================= //
// Input: char* home -> it will be saved to home_directory           //
// Output: char* home -> home_directory                              //
// 0 fail                                                            //
// will find home_directory                                          //
// Purpose: find home_directory                                      //
///////////////////////////////////////////////////////////////////////

char *getHomeDir(char *home)
{
    struct passwd *usr_info=getpwuid(getuid());//get home directory
    strcpy(home, usr_info->pw_dir);//string copy home directory

    return home;
}

///////////////////////////////////////////////////////////////////////
// getIPAddr                                                         //
// ================================================================= //
// Input: char *request -> get url link                              //
// Output: IP Address                                                //
// 0 fail                                                            //
// convert url link into IP address                                  //
// Purpose: convert url link into IP address                         //
///////////////////////////////////////////////////////////////////////

char* getIPAddr(char* addr) {
	struct hostent* hent;
	char* haddr;
	int l = strlen(addr);
	if ((hent = (struct hostent*)gethostbyname(addr)) != NULL){
		haddr = inet_ntoa(*((struct in_addr*)hent->h_addr_list[0]));
	}
	return haddr;
}

///////////////////////////////////////////////////////////////////////
// ALRMhandler                                                       //
// ================================================================= //
// Input: int -> check SIGALRM or not                                //
// Output:                                                           //
// 0 fail                                                            //
// alarm                                                             //
// Purpose: if alarm(n), count n seconds                             //
///////////////////////////////////////////////////////////////////////

void ALRMhandler(int sig){
	pid_t child;
	child=getpid();	//get child process ID
	printf("========No response========");
	kill(child,SIGKILL); //child process kill
}

///////////////////////////////////////////////////////////////////////
// CHILDhandler                                                       //
// ================================================================= //
// Input: int -> check SIGALRM or not                                //
// Output:                                                           //
// 0 fail                                                            //
// alarm                                                             //
// Purpose: wait exiting child process		                         //
///////////////////////////////////////////////////////////////////////
void CHLDhandler(int sig){
	int status;
	pid_t child;
	while((child=waitpid(-1,&status,WNOHANG))>0); //waitpid
	
}

///////////////////////////////////////////////////////////////////////
// INThandler                                                        //
// ================================================================= //
// Input: int -> check SIGINT or not                                 //
// Output:                                                           //
// 0 fail                                                            //
// ctrl+c and exit                                                   //
// Purpose: if we give ctrl+c signal, write logfile                  //
///////////////////////////////////////////////////////////////////////

void INThandler(int sig){
	if(getpid()==received_pid){
		char str[1024];
		time_t end_time;
		time(&end_time);
		FILE* fp=fopen(global_home,"at");//open logfile
		sprintf(str,"**SERVER** [Terminated] run time: %ld sec. #sub process : %d\n",end_time-current_time, process_count);	//termination information
		fputs(str, fp);	//write termination information
		fclose(fp);
	}	
	exit(0);
}

///////////////////////////////////////////////////////////////////////
// p                                                       			 //
// ================================================================= //
// Input: int semid(semaphore id)	                                 //
// Output:                                                           //
// 0 fail                                                            //
// like wait function                                                //
// Purpose: wait semaphore							                 //
///////////////////////////////////////////////////////////////////////
void p(int semid)
{
    struct sembuf pbuf;
    pbuf.sem_num = 0;
    pbuf.sem_op = -1;
    pbuf.sem_flg = SEM_UNDO;

    if((semop(semid, &pbuf, 1))== - 1)
    { //semaphore error
        perror("p : semop failed");
        exit(1);
    } 
}

///////////////////////////////////////////////////////////////////////
// v                                                       			 //
// ================================================================= //
// Input: int semid(semaphore id)	                                 //
// Output:                                                           //
// 0 fail                                                            //
// like signal function                                              //
// Purpose: signal semaphore						                 //
///////////////////////////////////////////////////////////////////////
void v(int semid)
{
    struct sembuf vbuf;
    vbuf.sem_num = 0;
    vbuf.sem_op = 1;
    vbuf.sem_flg = SEM_UNDO;
    if((semop(semid, &vbuf, 1))==-1)
    { //semaphore error
        perror("v : semop failed");
        exit(1);
    }

}

//////////////////////////////////////////////////////////
// thr_fn												//
// =====================================================//
// Input: void* buf : buffer        					//
// Output:                                              //
// 0 fail                                               //
// Purpose: Print Tid                           		//
//////////////////////////////////////////////////////////
void *thr_fn(void* buf){
	printf("*PID# %d create the *TID# %lu. \n",getpid(),pthread_self());//print create message
}

int main(){

	char home[50],dirname[50],filename[50];//directory path, file path
	char hashed[50]; //hashed url
	char cache[]="/cache";
	char slash[]="/";

	char cache_directory[4];//directory name
	char cache_file[50];//file name
	char time_info[50];//time information
	char buf[BUFFSIZE]={0,};
	char tmp[BUFFSIZE]={0,};
	char method[20]={0,};
	char url[BUFFSIZE]={0,};
	char real_url[BUFFSIZE]={0,};

	char *tok=NULL;	
	char *host_name =malloc(50);
	char *IPAddr=NULL;//IP address by gethostbyname

	int hit_or_miss=0; //hit or miss state
	int find=0;//find state
	int client_fd, socket_fd, server_fd;
	int len, len1;	
    int semid, i;
    int err;		//error check
	void *tret;		//contains the exit status of the target thread

    union semun{
        int val;
        struct semid_ds *buf;
        unsigned short int *array;
    } arg;

	pthread_t tid;		//thread id
	struct sockaddr_in p_server_addr,client_addr,server_addr;
	struct dirent *d; //directory entry
	struct in_addr inet_client_address;	
	struct hostent* hent;

	pid_t pid; //process ID
	DIR *dir; //directory position pointer

    if((semid = semget((key_t)39999, 1, IPC_CREAT|0666)) == -1)
    {//create semaphore
        perror("semget failed");
        exit(1);
    }

    arg.val = 1;

    if((semctl(semid, 0, SETVAL, arg))==-1)
    {//control semaphore
        perror("semctl failed");
        exit(1);
    }

	umask(0); //set umask 0
	received_pid=getpid(); //global variable setting
	getHomeDir(home);
	getHomeDir(global_home);//gethome directory
	strcat(home,cache);
	mkdir(home, 0777);	//create cache directory(777)
	strcat(home,slash); // home = /home/kw2018202060/

	strcat(global_home, "/logfile"); // global_home = /home/kw2018202060/logfile
	mkdir(global_home, 0777);//make directory at /home/kw2018202060/logfile
	strcat(global_home, "/logfile.txt"); //global_home = /home/kw2018202060/logfile/logfile.txt
	creat(global_home,0777);//create logfile.txt

    if((socket_fd = socket(PF_INET, SOCK_STREAM, 0))<0)
    {//can't socket
        printf("Server : Can't open stream socket\n");
        return 0;
    }
    bzero((char*)&server_addr, sizeof(server_addr)); //filling zero
    server_addr.sin_family = AF_INET; //server address hieracry
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY); // 32bits ip address
    server_addr.sin_port = htons(PORTNO);//port number setting

    int opt=1;
    setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)); //prevent bind

    if(bind(socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr))<0) //binding
    {
        printf("Server : Can't bind local address\n");
        return 0;
    }

    listen(socket_fd, 5);//listen
	time(&current_time); //global time setting

	signal(SIGALRM,ALRMhandler); //SIGALRM handler
	signal(SIGCHLD,CHLDhandler); //SIGCHLD handler
	signal(SIGINT,INThandler);	//SIGINT handler

	while(1){
		len=sizeof(client_addr);//client address length
 		client_fd = accept(socket_fd, (struct sockaddr *)&client_addr, &len); //client accpeting

		if(client_fd<0)
		{// if accept return is negative
			printf("Server : accept failed\n");
			return 0;
		}
		inet_client_address.s_addr = client_addr.sin_addr.s_addr; // change address format

		len1=read(client_fd, buf, BUFFSIZE);
		strcpy(tmp, buf); // string copy
		tok=strtok(tmp, " ");//method checking
		strcpy(method, tok);
		if(strcmp(method, "GET")==0)
		{//if method is GET
			tok=strtok(NULL, " ");
			strcpy(url, tok);
		}
		sha1_hash(url,hashed);//get hashed_url

		strncpy(cache_directory,hashed,3); //front 3 chars
		cache_directory[3]=0; //to not have trash value
		strncpy(cache_file,&hashed[3],sizeof(hashed)-3); // string copy etc bits

		strcpy(dirname,home);//dirname = /home/kw2018202060/cache
		strcat(dirname,cache_directory);//dirname = /home/kw2018202060/cache/bfb
		strcpy(filename,dirname);//filename = home/kw2018202060/cache/bfb
		strcat(filename,slash);//filename = home/kw2018202060/cache/bfb/
		strcat(filename,cache_file);//filename = home/kw2018202060/cache/bfb/gdfnmidfngjerklnsdfajbrejkasdfg

		hit_or_miss=0;	//If this cannot find the same name, hm=0
		if((dir=opendir(home))==NULL){return 0;} //not have directory

		while(d=readdir(dir))
		{	//read directory
			if(d->d_ino !=0)
			{ //file is
				if(strcmp(d->d_name,cache_directory)==0)
				{//same name ?
					find=1;
					break;
				}			
			}
		}

		if(find==1)
		{	//there is same name directory
			strcat(dirname,slash);
			dir=opendir(dirname);
			while(d=readdir(dir))
			{	//read directory entry
				if(d->d_ino !=0)
				{	//If there is a file
					if(strcmp(d->d_name,cache_file)==0)
					{	//same name
						hit_or_miss=1;
						break;
					}
				}
			}			
		}
		else
			hit_or_miss=0; //reset hit_or_miss state
		find=0; //find state reset
		
		char host_parsing[BUFFSIZE];
		if(strstr(buf,"Host: ") != NULL) {
			strcpy(host_parsing,strstr(buf,"Host: "));
			strtok(host_parsing,"\r\n");
			strtok(host_parsing," ");
			strcpy(host_name,strtok(NULL," "));
		}

		int preprocessing[5]={0,};// url preprocessing
		preprocessing[0]=strncmp(host_name,"firefox",7);
		preprocessing[1]=strncmp(host_name,"push",4);
		preprocessing[2]=strncmp(host_name,"safebrowsing",12);
		preprocessing[3]=strncmp(host_name,"detectportal",12);

		char *ptr=strchr(host_name,':');
		if(ptr==NULL)
			preprocessing[4]=-1;
		else
			preprocessing[4]=strncmp(ptr,":443",4);

		if(preprocessing[0]!=0&&preprocessing[1]!=0&&preprocessing[2]!=0&&preprocessing[3]!=0)
		{
			char* url_parsing=NULL;
			strcpy(tok,url);
			url_parsing=strtok(tok, "/");
			url_parsing=strtok(NULL, "/");	
			strcpy(real_url,url_parsing);

			if(check==0){
				strcpy(inputurl,url);
				strcpy(checkurl,real_url);
				strcpy(inputurlfile,filename);
				check=1;
				write_state=1;		
				process_count++;
			}
			else if(strcmp(inputurl,url)==0){
				write_state=1; //same url
			}
			else if(strstr(url,checkurl)!=NULL||strstr(url,".png")!=NULL){	
				write_state=0; //not input url
			}
			else{ //new input url
				strcpy(inputurl,url);
				strcpy(checkurl,real_url);
				strcpy(inputurlfile,filename);
				write_state=1;
				process_count++;
			}
		}
		if((pid=fork())<0){ //fork error
			close(client_fd);
			close(socket_fd);
			continue;
		}

		if(pid==0){	//child process
			if(hit_or_miss==1){ // if hit
				if(strstr(url,inputurl)!=NULL||strstr(url,".png")!=NULL){// url checking
					char buffer[BUFFSIZE]={0,};
					int fd=open(filename,O_RDONLY); //read only
					len=read(fd,buffer,BUFFSIZE); //read
					close(fd);
                    printf("*PID# %d is waiting for the semaphore. \n",getpid());
				    p(semid); //like wait function
                    printf("*PID# %d is in the critical zone. \n",getpid());

					len=write(client_fd, buffer, len);//send response

                    sleep(8);//sleep to watch different processes
					if(strstr(url,"favicon")==NULL&&strcmp(url,inputurl)==0&&check==1&&write_state==1)
                    {//favicon and other url state
                        err=pthread_create(&tid, NULL,thr_fn, NULL);	//thread creat
					    if(err!=0){//error check
						    printf("pthread_create() error.\n");	//print error message
						    return 0;
					    }
						time_t logtime; //logtime to write
						struct tm *lt; 
						time(&logtime);
						lt=localtime(&logtime); //to get time
						strftime(time_info,50,"-[%Y/%m/%d, %T]\n",lt);//setting time_info
					
						FILE* fp=fopen(global_home,"at");//open logfile.txt
						char str[100]; 
						strcpy(str,"[HIT] ");
						strncat(str,&filename[25],sizeof(filename)-25);
						strcat(str,time_info);	
						fputs(str, fp); //write hit information at logfile.txt

						str[0]='\0';
						strcpy(str,"[HIT] ");	
						strcat(str,url);	
						strcat(str,"\n");	
						fputs(str, fp); //write hit url at logfile.txt
						fclose(fp); //close logfile
						write_state=0; //log write state reset

                        					//print message when thread is terminated
					    if(pthread_join(tid,&tret)==0)
                        {	//waiting for tid thread termination
						    printf("*TID# %lu is exitis exited. \n",tid);
					    }
					}

					bzero(buffer,sizeof(buffer)); //filling zero buffer
				}
			}

			else if(hit_or_miss==0)
			{//miss 
				IPAddr=getIPAddr(host_name);//get ip address

				if((server_fd=socket(PF_INET, SOCK_STREAM,0))<0)
				{//server socket
					printf("Server : Can't open stream socket\n");
					return 0;
				}
	
				bzero((char*)&server_addr,sizeof(server_addr));
				server_addr.sin_family=AF_INET;		
				server_addr.sin_port=htons(80);
				server_addr.sin_addr.s_addr=inet_addr(IPAddr);

				if(connect(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr))<0) 
				{//server connect
                    printf("Server : Can't connect");
					return 0;
                }
				len1=write(server_fd,buf,len1);//request
                printf("*PID# %d is waiting for the semaphore. \n",getpid());
				p(semid);  //like wait function
				char response_message[BUFFSIZE]={0,};
                printf("*PID# %d is in the critical zone. \n",getpid());

				if(strstr(url,"favicon")==NULL&&strcmp(url,inputurl)==0&&check==1&&write_state==1)
				{//check favicon or not
				    err=pthread_create(&tid, NULL,thr_fn, NULL);	//thread creat
					if(err!=0)
                    {//error check
						printf("pthread_create() error.\n");	//print error message
						return 0;
					}
                	time_t logtime;
					struct tm *lt;
					time(&logtime);
					lt=localtime(&logtime);
					strftime(time_info,50,"-[%Y/%m/%d, %T]\n",lt);

					char str[100];
					strcpy(str,"[MISS] ");	
					strcat(str,url);	
					strcat(str,time_info);	

					FILE* fp_out=fopen(global_home,"at");
					fputs(str, fp_out); //write
					process_count++; //child process count
					fclose(fp_out);
					write_state=0;

                    //print message when thread is terminated
					if(pthread_join(tid,&tret)==0)
                    {	//waiting for tid thread termination
						printf("*TID# %lu is exitis exited. \n",tid);
					}
				}

				sleep(8); //sleep to watch different processes
				if((len=read(server_fd,response_message,BUFFSIZE))>0)
				{ //response
					if(strcmp(url,inputurl)==0||(strstr(url,inputurl)==NULL&&strstr(url,".png")==NULL)){
						mkdir(dirname, 0777);
						creat(filename,0777); //make cache file
						int fd=open(filename,O_WRONLY);
						write(fd,response_message,len);
						close(fd);
					}

					else if(check==1){//cache write
						int fd=open(inputurlfile,O_WRONLY);
						lseek(fd,0,SEEK_END); //pointing end
						write(fd,response_message,len);
						close(fd);
					}

					if((len=write(client_fd,response_message,len))<0){
						printf("error\n");
					}
					bzero(buf,sizeof(buf)); //filling zero buf
					bzero(response_message,sizeof(response_message)); //filling zero response message
				}

				close(server_fd); //close server
			}

            printf("*PID# %d exited the critical zone. \n",getpid());
            v(semid); // like signal function
			close(client_fd); //close client
			exit(0); //child process exit
		}		
		close(client_fd); //close client

	}
	close(socket_fd); //close proxy
	// sleep(10);
	if((semctl(semid, 0 ,IPC_RMID, arg))==-1)
    { //control semaphore
        perror("semctl failed");
        exit(1);
    }

	return 0;
}