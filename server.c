#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <pthread.h>
#include <netinet/in.h>
#include <sys/socket.h>

typedef struct transfer_node {
  int sock;
  int fileSize;
  int chunkSize;
  char fileName[21];
  struct transfer_node *next;
} TransferNode;

typedef struct {
  TransferNode *head;
  TransferNode *tail;
  int len;
} TransferQueue;

int online = 1;
pthread_cond_t cond;
pthread_mutex_t uiMutex;
pthread_mutex_t queueMutex;
pthread_mutex_t finishMutex;
TransferQueue *queue;

void* ui_thread(void* arg) {
  char input[64];
  int i;
  TransferNode *node;

  printf("Welcome to the UI for the server! Type h for a hard shutdown, s for a soft shutdown and d to display active transfers.\n\n");
  while(online == 1) {
    pthread_mutex_lock(&uiMutex);
    if(!fgets(input, 64, stdin)) {
      perror("fgets failed");
    }
    
    if(strcmp(input, "d\n") == 0) {
      pthread_mutex_lock(&queueMutex);
      if(queue->len != 0) {
        node = queue->head;
        while(node) {
          printf("%s of file size %d and chunk size %d\n", node->fileName, node->fileSize, node->chunkSize);
          node = node->next;
        }
      } else {
        printf("List is currently empty. Queue up files with the client to view the transfer queue.\n");
      }
      pthread_mutex_unlock(&queueMutex);
    } else if(strcmp(input, "s\n") == 0) {
      online = 0;
    } else if(strcmp(input, "h\n") == 0) {
      online = -1;
    } else {
      fprintf(stderr, "Input must be s, h or d.\n", input);
    }
    
    pthread_mutex_unlock(&uiMutex);
  }
  
  return NULL;
}

void* connection_thread(void *arg) {
  char buffer[513];
  char confirm[256];
  int rec;
  TransferNode *node = (TransferNode*)arg;
  
  strcpy(confirm, "confirm");
  
  rec = recv(node->sock, buffer, 512, 0);
  if(rec <= 0) {
    perror("1recv failed");
    return NULL;
  } else {
    buffer[strlen(buffer)] = '\0';
    node->fileSize = atoi(buffer);
    
    if(node->fileSize == 0) {
      perror("Invalid file size");
      return NULL;
    }
  }
  send(node->sock, confirm, strlen(confirm), 0);
  
  rec = recv(node->sock, buffer, 512, 0);
  if(rec <= 0) {
    perror("2recv failed");
    return NULL;
  } else {
    buffer[strlen(buffer)] = '\0';
    node->chunkSize = atoi(buffer);
    
    if(node->chunkSize == 0) {
      perror("Invalid chunk size");
      return NULL;
    }
  }
  send(node->sock, confirm, strlen(confirm), 0);
  
  rec = recv(node->sock, buffer, 20, 0);
  if(rec <= 0) {
    perror("3recv failed");
    return NULL;
  } else {
    if((int)strlen(buffer) <= 0) {
      perror("Invalid filename");
    }
    
    strcpy(node->fileName, buffer);
    buffer[strlen(node->fileName)] = '\0';
  }
  send(node->sock, confirm, strlen(confirm), 0);
  
  pthread_mutex_lock(&queueMutex);
  
  node->next = NULL;
  if(queue->len == 0) {
    queue->head = node;
    queue->tail = node;
    queue->len++;
    pthread_cond_signal(&cond);
  } else {
    queue->tail->next = node;
    queue->tail = node;
    queue->len++;
  }
  
  pthread_mutex_unlock(&queueMutex);
  
  return NULL;
}

void* worker_thread(void *arg) {
  char line[2000];
  char tempName[256];
  char confirm[256];
  int i;
  int toRead;
  FILE *file;
  TransferNode *node;
  TransferNode *toFree;
  
  strcpy(confirm, "confirm");
  pthread_mutex_init(&finishMutex, NULL);
  pthread_mutex_lock(&finishMutex);
  
  while(1) {
    pthread_cond_wait(&cond, &finishMutex);
    if(queue->len == 0) {
      break;
    }
    
    node = queue->head;
    while(node && queue->len > 0) {
      pthread_mutex_lock(&queueMutex);
      
      toRead = node->sock;
      if(access(node->fileName, F_OK) != -1) {
        strcpy(tempName, node->fileName);
        while(access(tempName, F_OK) != -1) {
          i++;
          sprintf(tempName, "%d-%s", i, node->fileName);
        }
        
        file = fopen(tempName, "w+");
      } else {
        file = fopen(node->fileName, "w+");
      }
      
      toFree = node;
      node = node->next;
      
      if(queue->len == 1) {
        queue->head = NULL;
        queue->tail = NULL;
      } else {
        queue->head = node;
      }
      
      free(toFree);
      pthread_mutex_unlock(&queueMutex);
      
      while(recv(toRead, line, 2000, 0) > 0) {
        line[strlen(line)] = '\0';
        fprintf(file, "%s", line);
        send(toRead, confirm, strlen(confirm), 0);
      }
      
      queue->len--;
      fclose(file);
      close(toRead);
    }
  }
  
  pthread_mutex_unlock(&finishMutex);
  return NULL;
}

int main(int argc, char *argv[]) {
	struct sockaddr_in dest;
	struct sockaddr_in serv;
  struct timeval tv;
	char buffer[513];
  char input[256];
	int connectSocket;
  int enable = 1;
  int i;
	int len;
	int port;
	int sock;
  fd_set fd;
  pthread_t tfrThreads[32];
  pthread_t uiThread;
  pthread_t wrkThread;
	socklen_t socksize = sizeof(struct sockaddr_in);
  TransferNode *node;

	if(argc != 2) {
		printf("Format for server creation is ./server [[PORT NUMBER]].\n");
		exit(0);
	}

	port = atoi(argv[1]);
	if(!port || port < 0) {
		printf("Port must be a number and non-negative.\n");
		exit(0);
	}

	memset(&serv, 0, sizeof(serv));
	serv.sin_family = AF_INET;
	serv.sin_addr.s_addr = htonl(INADDR_ANY);
	serv.sin_port = htons(port);

	sock = socket(AF_INET, SOCK_STREAM, 0);
  if(sock == -1) {
    perror("Unable to open socket");
    exit(1);
  }
  
  setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));
	if(bind(sock, (struct sockaddr*)&serv, sizeof(struct sockaddr)) == -1) {
		perror("Server bind was unsuccessful");
    close(sock);
		exit(1);
	}
  
  FD_ZERO(&fd);
  FD_SET(sock, &fd);

  listen(sock, 20);
  pthread_cond_init(&cond, NULL);
  pthread_mutex_init(&uiMutex, NULL);
  pthread_mutex_init(&queueMutex, NULL);
  pthread_create(&uiThread, NULL, ui_thread, NULL);
  pthread_create(&wrkThread, NULL, worker_thread, NULL);
  
  queue = (TransferQueue*)malloc(sizeof(TransferQueue));
  queue->head = queue->tail = NULL;
  queue->len = 0;
  
	do {
    FD_ZERO(&fd);
    FD_SET(sock, &fd);
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    connectSocket = select(sock + 1, &fd, NULL, NULL, &tv);

    if(connectSocket == -1) {
      fprintf(stderr, "Issue selecting, exiting program...\n");
      online = -1;
    } else if(connectSocket != 0) {
      connectSocket = accept(sock, (struct sockaddr*)&dest, &socksize);
      node = (TransferNode*)malloc(sizeof(TransferNode));
      node->sock = connectSocket;
      pthread_create(&tfrThreads[connectSocket], NULL, connection_thread, (void*)node);
    }
  } while(online == 1);
  
  if(online == 0) { 
    pthread_mutex_lock(&queueMutex);
    
    printf("\nInput y to finish all transfers, anything else to exit immediately.\n");
    fgets(input, 64, stdin);
    if(strcmp(input, "y\n") == 0) {
      pthread_mutex_unlock(&queueMutex);
      pthread_cond_signal(&cond);
      pthread_join(wrkThread, NULL);
    } else {
      pthread_mutex_unlock(&queueMutex);
    }
  }

  pthread_join(uiThread, NULL);
	close(sock);
	return 0;
}
