#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

#define MAX_BUF 512
#define DATA 1024
#define FTP 21
#define START "ftp://"

typedef struct {
  char user[MAX_BUF];
  char pass[MAX_BUF];
  char host[MAX_BUF];
  char path[MAX_BUF];
  char file[MAX_BUF];
} session;

int splitstring(char* s, session* ses) {
  char temp[MAX_BUF];
  char* pos;
  int i, p, j=0;


  if(memcmp(s,START,strlen(START))) {
    printf("ERROR: Doesn't start with ftp://\n");
    return -1;
  }

  i = strlen(START);
  pos = strchr(s,'@');

  if(pos != NULL) {
    //copiar user
    while(s[i]!=':') {
      temp[j++] = s[i];
      i++;

      if(i==MAX_BUF-1)
        return -1;
    }
    temp[j] = '\0';
    if(temp[0] != '\0')
      memcpy(ses->user,temp,strlen(temp)+1);
    else
      strcpy(ses->user, "anonymous");

    //copiar pass
    j = 0; i++;
    while(s[i]!='@') {
      temp[j++] = s[i];
      i++;

      if(i==MAX_BUF-1)
        return -1;
    }
    temp[j] = '\0';
    if(temp[0] != '\0')
      memcpy(ses->pass,temp,strlen(temp)+1);
    else
      strcpy(ses->pass, "anonymous");
      i++;
  } else {
    strcpy(ses->user, "anonymous");
    strcpy(ses->pass, "anonymous");
  }

  //copiar host
  j = 0;
  while(s[i]!='/') {
    temp[j++] = s[i];
    i++;

    if(i==MAX_BUF-1)
      return -1;
  }
  temp[j] = '\0';
  memcpy(ses->host,temp,strlen(temp)+1);

  for(p=strlen(s);s[p]!='/';p--);

  i++;

  for(j=0;i<p+1;i++) {
    temp[j++] = s[i];

    if(i==MAX_BUF-1)
      return -1;
  }
  temp[j] = '\0';
  memcpy(ses->path,temp,strlen(temp)+1);
  //copiar filename
  j = 0; i=p+1;
  while(s[i]!='\0' && s[i]!='\n') {
    temp[j++] = s[i];
    i++;

    if(i==MAX_BUF-1)
      return -1;
  }
  temp[j] = '\0';
  memcpy(ses->file,temp,strlen(temp)+1);

  printf("USER: %s\nPASS: %s\nHOST: %s\nPATH: %s\nFILE: %s\n",ses->user,ses->pass,ses->host,ses->path,ses->file);


  return 0;
}

int opencon(struct hostent* h, int port) {

	int	sockfd;
	struct	sockaddr_in server_addr;

	bzero((char*)&server_addr,sizeof(server_addr));
	server_addr.sin_family = AF_INET;
              // if(h->h_length != 32) {
              //   printf("Error: h_length is not 32\n");
              //   return -1;
              // }
	//server_addr.sin_addr.s_addr = (unsigned long) h->h_addr;	/*32 bit Internet address network byte ordered*/
  memcpy(&server_addr.sin_addr.s_addr, h->h_addr, h->h_length);
	server_addr.sin_port = htons(port);		/*server TCP port must be network byte ordered */
  memset(&server_addr.sin_zero, 0, 8);

  /*open an TCP socket*/
  if ((sockfd = socket(AF_INET,SOCK_STREAM,0)) < 0) {
    perror("socket()");
    exit(0);
  }
  // char test[INET_ADDRSTRLEN];
  // inet_ntop(AF_INET,&(server_addr.sin_addr.s_addr),test,INET_ADDRSTRLEN);
  // printf("addr: %s\n",test);
  /*connect to the server*/
  if(connect(sockfd,(struct sockaddr *)&server_addr,sizeof(server_addr)) < 0) {
    perror("connect()");
  }

  return sockfd;

}

int sendcommand(int sock, char* c, char* reply) {
  memset(reply,0,MAX_BUF);
  if(c != NULL) {
    if(send(sock,c,strlen(c),0)<0) {
      printf("Error: send() failed while attempting to send %s\n",c);
      return -1;
    }
    printf("\n> %s",c);
  }


  int done = 0, flag = 0, i = 0;
  char r;

  while(!done) {
    recv(sock,&r,1,0);

    reply[i] = r;

      if(i==4)
        if((reply[0] >= '0' && reply[0] <= '9') && (reply[1] >= '0' && reply[1] <= '9') && (reply[2] >= '0' && reply[2] <= '9') && reply[3] == ' ')
          flag = 1;

      if(reply[i] == '\n' && reply[i-1] == '\r') {
        if(flag)
          done=1;
        else {
          i = -1;
          memset(reply,0,MAX_BUF);
        }
      } else if (i > MAX_BUF) {
        printf("Error: recv() read too many bytes\n");
        return -1;
      }
    i++;
  }

  printf("< %s\n",reply);

  return 0;
}

int c_login(int sock, session ses) {
  char msg[MAX_BUF], reply[MAX_BUF];
  int rcode;

  if(sendcommand(sock, NULL, reply) < 0) {
    return -1;
  }

  memset(msg, 0, MAX_BUF);
  strcpy(msg, "USER ");
  strcat(msg, ses.user);
  strcat(msg, "\r\n");

  if(sendcommand(sock, msg, reply)<0)
    return -1;


  memset(msg, 0, MAX_BUF);
  strcpy(msg, "PASS ");
  strcat(msg, ses.pass);
  strcat(msg, "\r\n");

  if(sendcommand(sock, msg, reply)<0)
    return -1;
    sscanf(reply,"%d",&rcode);

    if(rcode == 230)
      return 0;
    else {
      printf("\nError: Couldn't login. Check your credentials and\nwhether the server is anonymous only.\n");
      return -1;
    }


  return 0;
}

int c_pasv(int sock, char* ip, int* port) {
  char msg[MAX_BUF], reply[MAX_BUF];
  int i=0, j=0, rbytes[6];

  memset(msg, 0, MAX_BUF);
  strcpy(msg, "PASV\r\n");
  if(sendcommand(sock, msg, reply)<0)
    return -1;

  do {
    i++;
  } while(reply[i]!='(');

  sscanf(reply,"%d Entering Passive Mode (%d,%d,%d,%d,%d,%d)",&rbytes[0],&rbytes[0],&rbytes[1],&rbytes[2],&rbytes[3],&rbytes[4],&rbytes[5]);

  memset(ip,0,sizeof(ip));

  sprintf(ip,"%d.%d.%d.%d",rbytes[0],rbytes[1],rbytes[2],rbytes[3]);

  *port = rbytes[4]*256+rbytes[5];

  //printf("calculado: %s:%d",ip,*port);

  return 0;
}

int c_retr(int sock, session ses) {
  int i, rcode;
  char msg[MAX_BUF], reply[MAX_BUF];

  memset(msg, 0, MAX_BUF);
  strcpy(msg, "TYPE I\r\n");

  if(sendcommand(sock, msg, reply)<0)
    return -1;

  memset(msg, 0, MAX_BUF);
  strcpy(msg, "RETR ");
  strcat(msg, ses.path);
  strcat(msg, ses.file);
  strcat(msg, "\r\n");

  if(sendcommand(sock, msg, reply)<0)
    return -1;

  for(i=0;i<MAX_BUF && reply[i]!='(';i++);

  sscanf(reply,"%d",&rcode);

  if(rcode == 150)
    return 0;
  else {
    printf("\nError: File not found.\n");
    return -1;
  }
}

int c_quit(int sock) {
  char msg[MAX_BUF], reply[MAX_BUF];

  memset(msg, 0, MAX_BUF);
  strcpy(msg, "QUIT\r\n");

  if(sendcommand(sock, msg, reply)<0)
    return -1;

  return 0;
}

int receivefile(int sock, char* name) {
  FILE *f;

  int r, i;
  char data[DATA];

  f = fopen(name,"w");


  do {
    r = recv(sock, data, DATA, 0);
    for(i=0;i<r;i++)
      if(fprintf(f,"%c",data[i]) < 0)
        printf("Error in fprintf() in receivefile\n");
  } while(r>0);

  fclose(f);
  return 0;

}

int main(int argc, char** argv) {

  int control_socket, data_socket, dport;
  char ip[MAX_BUF];

  if(argc != 2) {
    printf("Usage: %s ftp://<user>:<pass>@<host>/<path>/<file>\n",argv[0]);
  }

  session ses;

  if(splitstring(argv[1],&ses) < 0) {
    printf("Error: Argument not in correct format.\n");
    return -1;
  }

  struct hostent* h;

  h = gethostbyname(ses.host);

  control_socket = opencon(h,FTP);


  if(c_login(control_socket, ses) < 0)
    return -1;

  if(c_pasv(control_socket,ip,&dport) < 0)
    return -1;

  data_socket = opencon(h,dport);

  if(c_retr(control_socket, ses) < 0)
    return -1;

  if(receivefile(data_socket, ses.file) < 0)
    return -1;

  if(c_quit(control_socket) < 0)
    return -1;

  return 0;

}
