#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 12345
#define BUFFER_SIZE 1024

#include <pthread.h>

void *listen_server(void *arg) {
    int client_socket = *(int *)arg;
    char buffer[BUFFER_SIZE];
    while(1) {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
        if(bytes_received <= 0) {
            printf("Conexiunea cu serverul a fost pierdută.\n");
            close(client_socket);
            pthread_exit(NULL);
        }
        buffer[bytes_received] = '\0';
        printf("[SERVER] %s\n", buffer);

        if(strstr(buffer, "Jocul va începe") != NULL) 
        {
            printf("=== Începe QUIZ-ul ===\n");
        }
        if(strstr(buffer, "Jocul s-a terminat.") != NULL) 
        {
            printf("=== Final QUIZ ===\n");
            break;
        }
    }
    return NULL;
}

int main() {
    int client_socket;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];

    client_socket=socket(AF_INET,SOCK_STREAM,0);
    if(client_socket<0) 
    {
        perror("Eroare la crearea socketului");
        exit(1);
    }

    server_addr.sin_family=AF_INET;
    server_addr.sin_port=htons(PORT);
    server_addr.sin_addr.s_addr=inet_addr("127.0.0.1");

    if(connect(client_socket,(struct sockaddr*)&server_addr,sizeof(server_addr))<0) 
    {
        perror("Eroare la conectarea la server");
        close(client_socket);
        exit(1);
    }
    int bytes_received=recv(client_socket,buffer,BUFFER_SIZE-1,0);
    if(bytes_received>0) 
    {
        buffer[bytes_received]='\0';
        printf("%s",buffer); 
    } 
    else 
    {
        perror("Eroare la recv");
        close(client_socket);
        exit(1);
     }
buffer[strcspn(buffer,"\n")]='\0';
    fgets(buffer,BUFFER_SIZE,stdin);
    if(send(client_socket,buffer,strlen(buffer),0)<=0) 
    {
        perror("Eroare la send");
        close(client_socket);
        exit(1);
    }
    memset(buffer,0,BUFFER_SIZE);
    bytes_received=recv(client_socket,buffer,BUFFER_SIZE-1,0);
    if(bytes_received>0) 
    {
        buffer[bytes_received]='\0';
        printf("%s", buffer); 
    } 
    else 
    {
        perror("Eroare la recv");
        close(client_socket);
        exit(1);
    }

    printf("Alegeți o comandă (start, quit, reguli)\n");

    pthread_t listen_thread;
    if(pthread_create(&listen_thread,NULL,listen_server,&client_socket)!=0)
    {
        perror("Eroare la crearea thread-ului de ascultare");
        close(client_socket);
        exit(1);
    }
    pthread_detach(listen_thread);

    while(1)
    {
        
        fgets(buffer,BUFFER_SIZE,stdin);
        buffer[strcspn(buffer,"\n")]='\0';
        send(client_socket,buffer,strlen(buffer),0);
        if(strcmp(buffer,"quit")==0)
            break;

    }

    close(client_socket);
    return 0;
}
