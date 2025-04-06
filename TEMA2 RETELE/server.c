#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>

#define PORT 12345
#define BUFFER_SIZE 1024
#define MAX_CLIENTS 1000
#define MAX_QUESTIONS 100
#define MAX_TEXT_LENGTH 256
#define MAX_OPTION_LENGTH 128
#define TIMP 10 

typedef struct {
    int id;
    char text[MAX_TEXT_LENGTH];
    char options[4][MAX_OPTION_LENGTH];
    char correct;
} Question;

typedef struct {
    char answer;
    int answered;
} Answer;

typedef struct {
    int socket;
    int id;
    int score;
    char name[50];
    Answer answer;
    int ready;
    int used;
} Client;


pthread_mutex_t clients_mutex=PTHREAD_MUTEX_INITIALIZER;
Client listc[MAX_CLIENTS];
int countc=0;
pthread_mutex_t game_mutex=PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t game_cond=PTHREAD_COND_INITIALIZER;
int game_started=0;
int current_question=0;


Question questions[MAX_QUESTIONS];
int totalq=0;

void addc(Client client)
{
    pthread_mutex_lock(&clients_mutex);
    if(countc<MAX_CLIENTS)
        listc[countc++]=client;
    pthread_mutex_unlock(&clients_mutex);
}

void removec(int id)
{
    pthread_mutex_lock(&clients_mutex);
    for (int i=0;i<countc;i++) 
    {
        if(listc[i].id==id) 
        {
            printf("Clientul %d a fost eliminat\n", id);
            close(listc[i].socket);
            listc[i].socket=-1;
            listc[i].answer.answered=0;
            listc[i].answer.answer='X';
            memset(listc[i].name,0,sizeof(listc[i].name));
            for (int j=i;j<countc-1;j++)
                listc[j] = listc[j + 1];

            listc[countc-1].socket=-1;
            listc[countc-1].id=0;
            listc[countc-1].answer.answered=0;
            listc[countc-1].answer.answer='X';
            memset(listc[countc-1].name,0,sizeof(listc[countc-1].name));
            countc--;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}


int load_questions(const char *filename, Question *questions, int *totalq) 
{
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Eroare la fopen");
        return -1;
    }

    char line[BUFFER_SIZE];
    int qindex=-1;
    int oindex=0;
    while (fgets(line,sizeof(line),file)) 
    {
        char *start=line;
        while(isspace(*start)) 
            start++;
        char *end=start+strlen(start)-1;
        while(end>start&&isspace(*end)) 
        {
            *end='\0';
            end--;
        }

        if (strncmp(start,"<question",9)==0) 
        {
            if (qindex+1>=MAX_QUESTIONS) 
            {
                printf("S-a depasit limita de intrebari\n");
                break;
            }
            qindex++;
            oindex=0; 
            questions[qindex].id=qindex+1;
        } 
        else if(strncmp(start,"<text>",6)==0) 
        {
            char *tstart=strchr(start,'>')+1;
            char *tend=strstr(tstart,"</text>");
            if(tend) 
            {
                size_t lg=tend-tstart;
                if(lg>=MAX_TEXT_LENGTH) 
                    lg=MAX_TEXT_LENGTH-1;
                strncpy(questions[qindex].text,tstart,lg);
                questions[qindex].text[lg]='\0';
            }
        } 
        else if(strncmp(start,"<option>",8)==0) 
        {
            if(oindex<4) 
            {
                char *ostart=strchr(start,'>')+1;
                char *oend=strstr(ostart,"</option>");
                if(oend) 
                {
                    size_t lg=oend-ostart;
                    if(lg>=MAX_OPTION_LENGTH) 
                        lg=MAX_OPTION_LENGTH-1;
                    strncpy(questions[qindex].options[oindex],ostart,lg);
                    questions[qindex].options[oindex][lg] = '\0';
                    oindex++;
                }
            }
        } 
        else if(strncmp(start,"<correct>",9)==0) 
        {
            char *cstart=strchr(start,'>')+1;
            char *cend=strstr(cstart, "</correct>");
            if(cend)
                questions[qindex].correct=toupper(cstart[0]);
        }
    }

    fclose(file);
    *totalq=qindex+1;
    return 0;
}


int check_answer(Question q, char client_answer) {
    if (toupper(client_answer) == q.correct) {
        return 1;
    } else {
        return 0;
    }
}


#define WINNERS_BUFFER_SIZE 4096
#define FINAL_MSG_BUFFER_SIZE 4096

void *game_loop_thread(void *arg)
{
    pthread_mutex_lock(&game_mutex);
    while(!game_started)
    {
        pthread_cond_wait(&game_cond,&game_mutex);
    }
    pthread_mutex_unlock(&game_mutex);

    for(int q=0;q<totalq;q++)
    {
        current_question=q;
        for(int i=0;i<countc;i++)
        {
            if(listc[i].socket==-1)
                continue;
            char msg[BUFFER_SIZE];
            snprintf(msg,sizeof(msg),"Întrebare %d: %s\n %s\n %s\n %s\n %s\nRăspunsul tău (A,B,C,D,50/50): ", questions[current_question].id,questions[current_question].text,questions[current_question].options[0],questions[current_question].options[1],questions[current_question].options[2],questions[current_question].options[3]);
            send(listc[i].socket,msg,strlen(msg),0);

            pthread_mutex_lock(&clients_mutex);
            listc[i].answer.answered=0;
            listc[i].answer.answer='X';
            pthread_mutex_unlock(&clients_mutex);

            int waited=0;
            while(waited<TIMP)
            {
                sleep(1);
                waited++;

                pthread_mutex_lock(&clients_mutex);
                if(listc[i].answer.answered)
                {
                    pthread_mutex_unlock(&clients_mutex);
                    break;
                }
                pthread_mutex_unlock(&clients_mutex);
            }
            pthread_mutex_lock(&clients_mutex);
            if(listc[i].answer.answered)
            {
                if(check_answer(questions[q],listc[i].answer.answer))
                {
                    listc[i].score++;
                    send(listc[i].socket,"Corect!\n\n",10,0);
                }
                else
                {
                    char correct_msg[BUFFER_SIZE];
                    snprintf(correct_msg,sizeof(correct_msg),"Greșit. Răspunsul corect e %c.\n\n",questions[q].correct);
                    send(listc[i].socket,correct_msg,strlen(correct_msg),0);
                }
            }else{
                send(listc[i].socket,"Nu ai dat niciun raspuns\n\n",26,0);
            }
            pthread_mutex_unlock(&clients_mutex);
            sleep(1);
        }
    }

    pthread_mutex_lock(&clients_mutex);
    int max_score=-1;
    for(int i=0;i<countc;i++)
    {
        if(listc[i].score>max_score)
            max_score=listc[i].score;
    }

    char w[BUFFER_SIZE]="Câștigător: ";
    for(int i=0;i<countc;i++)
    {
        if(listc[i].score==max_score)
        {
            strncat(w,listc[i].name,sizeof(w)-strlen(w)-1);
            strncat(w," ",sizeof(w)-strlen(w)-1);
        }
    }

    typedef struct{
        char name[50];
        int score;
    }PlayerScore;

    PlayerScore ps[MAX_CLIENTS];
    for(int i=0;i<countc;i++)
    {
        strncpy(ps[i].name,listc[i].name,sizeof(ps[i].name)-1);
        ps[i].score=listc[i].score;
    }

    for(int i=0;i<countc-1;i++)
    {
        for(int j=i+1;j<countc;j++)
        {
            if(ps[i].score<ps[j].score)
            {
                PlayerScore temp=ps[i];
                ps[i]=ps[j];
                ps[j]=temp;
            }
        }
    }
    char clas[BUFFER_SIZE]="Clasament:\n";
    for(int i=0;i<countc;i++)
    {
        char entry[100];
        snprintf(entry,sizeof(entry)*2,"%d. %s - %d puncte\n",i+1,ps[i].name,ps[i].score);
        strncat(clas,entry,sizeof(clas)-strlen(clas)-1);
    }
    char final_msg[BUFFER_SIZE];
    snprintf(final_msg,sizeof(final_msg)*3,"Jocul s-a terminat.\n %scu %d puncte\n\n %s\n",w,max_score,clas);

    for(int i=0;i<countc;i++)
    {
        if(listc[i].socket!=-1)
        {
            send(listc[i].socket,final_msg,strlen(final_msg),0);
            //send(listc[i].socket,"Thanks for playing\n",23,0);
            close(listc[i].socket);
            listc[i].socket=-1;
        }
    }
    pthread_mutex_unlock(&clients_mutex);

    return NULL;
}


void *handle_client(void *arg)
{
    Client *client=(Client*)arg;
    char buffer[BUFFER_SIZE];
    char mesaj[BUFFER_SIZE];

    printf("Client cu id %d conectat\n",client->id);
    strcpy(buffer,"Introdu numele:\n");
    send(client->socket,buffer,strlen(buffer),0);


    memset(buffer,0,BUFFER_SIZE);
    int bytes_received=recv(client->socket,buffer,BUFFER_SIZE-1,0);
    if(bytes_received<=0)
    {
        printf("Clientul %d s-a deconectat înainte de a introduce numele.\n",client->id);
        close(client->socket);
        removec(client->id);
        pthread_exit(NULL);
    }
    buffer[strcspn(buffer,"\n")]='\0';
    strncpy(client->name,buffer,sizeof(client->name)-1);
    client->name[sizeof(client->name)-1]='\0'; 
    printf("Clientul %d a introdus numele: %s\n",client->id,client->name);

    strcpy(mesaj,"Ai intrat în joc!\n");
    send(client->socket,mesaj,strlen(mesaj),0);
    while(1)
    {
        memset(buffer,0,BUFFER_SIZE);
        int bytes_recieved=recv(client->socket,buffer,BUFFER_SIZE-1,0);
        if(bytes_recieved<=0)
        {
            printf("Client cu id %d deconectat\n",client->id);
            close(client->socket);
            removec(client->id);
            pthread_exit(NULL);
        }
        buffer[strcspn(buffer,"\n")]='\0';
        printf("Am primit comanda '%s' de la clientul %d\n",buffer,client->id);

        if(strcmp(buffer,"start")==0&&client->id==1)
        {
            strcpy(mesaj, "Jocul va începe\n");
            send(client->socket, mesaj,strlen(mesaj),0);
            pthread_mutex_lock(&game_mutex);
            client->ready=1;
            game_started=1;
            pthread_cond_signal(&game_cond);
            pthread_mutex_unlock(&game_mutex);
        }

        else if(strcmp(buffer,"start")==0&&client->id!=1)
        {
            strcpy(mesaj, "Doar primul jucator poate da startul\n");
            send(client->socket,mesaj,strlen(mesaj),0);
        }
        else if(strcmp(buffer,"quit")==0)
        {
            strcpy(mesaj, "Am primit comanda quit\n");
            send(client->socket, mesaj, strlen(mesaj), 0);
            printf("Clientul %d a trimis comanda quit\n", client->id);
            close(client->socket);
            removec(client->id);
            pthread_exit(NULL);
        }
        else if(strcmp(buffer,"reguli")==0)
        {
            strcpy(mesaj, "Reguli:\n"
            "1.După ce toți jucătorii și-au introdus numele, primul jucător alege opțiunea start\n"
            "2.Fiecare jucător va primi pe rând câte o întrebare. Timpul de răspuns la fiecare întrebare este de 10 secunde\n"
            "3.Fiecare întrebare valorează 1 punct\n"
            "4.Opțiunea 50/50 elimină 2 variante ale unei întrbări. Un jucător poate folosi o singură dată opțiunea\n"
            "5.Jucătorul cu cele mai multe puncte câștigă. Clasamentul și câștigătorul vor fi anunțate\n\n"
            "Alege o comandă (start,quit)");
            send(client->socket,mesaj,strlen(mesaj),0);
            continue;
        }
        else if(strcmp(buffer,"50/50")==0)
        {
            if (client->used==1) 
            {
                strcpy(mesaj,"Ai folosit deja 50/50!\n");
                send(client->socket,mesaj,strlen(mesaj),0);
                continue;
            }
            client->used = 1;
            pthread_mutex_lock(&clients_mutex);
            char correct_option=questions[current_question].correct;

            char all_options[]={'A','B','C','D'};
            int wrong_options[2];
            int wrong_count=0;

            for (int i=0;i<4;i++) 
            {
                if (all_options[i]!=correct_option) 
                    wrong_options[wrong_count++]=i;
            }

            srand(time(NULL));
            int temp=wrong_options[0];
            wrong_options[0]=wrong_options[rand() % 2];
            wrong_options[1]=temp;

            char keep[2];
            keep[0]=correct_option;
            keep[1]=all_options[wrong_options[0]];

            char qnou[BUFFER_SIZE];
            snprintf(qnou,sizeof(qnou), 
                "50/50 Folosit\nÎntrebare %d: %s\n%s\n%s\nRăspunsul tău (A, B, C, D): ",
                questions[current_question].id, 
                questions[current_question].text,
                questions[current_question].options[keep[0]-'A'],
                questions[current_question].options[keep[1]-'A']);

            send(client->socket,qnou,strlen(qnou), 0);

            pthread_mutex_unlock(&clients_mutex);
        }


        else
        {
            if(strlen(buffer)>=1)
            {
                char answer=toupper(buffer[0]);
                if(answer>='A'&&answer<='D') {
                    pthread_mutex_lock(&clients_mutex);
                    client->answer.answer=answer;
                    client->answer.answered=1;
                    pthread_mutex_unlock(&clients_mutex);
                    strcpy(mesaj, "Răspuns primit\n");
                }
                else 
                    strcpy(mesaj,"Răspuns invalid alege doar dintre A, B, C sau D.\n");
            }
            else
                strcpy(mesaj,"Comanda invalida\n");
            send(client->socket,mesaj,strlen(mesaj),0);
        }
    }
    close(client->socket);

    pthread_exit(NULL);
}

int main()
{
    int server_socket;
    struct sockaddr_in server, from;

    for(int i=0;i<MAX_CLIENTS;i++) 
    {
        listc[i].score=0;
        listc[i].socket=-1;
        memset(listc[i].name,0,sizeof(listc[i].name));
        listc[i].answer.answered=0;
        listc[i].answer.answer='X';
    }

    if(load_questions("questions.xml", questions,&totalq)!=0) 
    {
        printf("Eroare la load questions\n");
        exit(1);
    }
    printf("S-au incarcat %d intrebari\n",totalq);

    server_socket=socket(AF_INET,SOCK_STREAM,0);
    if(server_socket==-1)
    {
        perror("Eroare la socket");
        exit(1);
    }

    server.sin_family=AF_INET;
    server.sin_port=htons(PORT);
    server.sin_addr.s_addr=INADDR_ANY;
    int opt=1;
    if (setsockopt(server_socket,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt))<0) 
    {
        perror("Eroare la setsockopt");
        close(server_socket);
        exit(1);
    }

    if(bind(server_socket,(struct sockaddr *)&server,sizeof(server))<0)
    {
        perror("Eroare la bind");
        close(server_socket);
        exit(1);
    }
    if(listen(server_socket,MAX_CLIENTS)<0)
    {
        perror("Eroare la listen");
        close(server_socket);
        exit(1);
    }

    printf("Serverul rulează pe portul %d...\n", PORT);

    pthread_t game_thread;
    if(pthread_create(&game_thread,NULL,game_loop_thread,NULL)!= 0) 
    {
        perror("Eroare la crearea game thread");
        close(server_socket);
        exit(1);
    }

    while(1)
    {
        Client nou;
        socklen_t addr_size=sizeof(from);
        nou.socket=accept(server_socket,(struct sockaddr *)&from,&addr_size);
        if(nou.socket<0)
        {
            perror("Eroare la accept");
            continue;
        }

        pthread_mutex_lock(&clients_mutex);
        nou.id=countc+1;
        nou.score=0;
        strcpy(nou.name,"");
        nou.answer.answered = 0;
        nou.answer.answer='X';
        nou.ready=0;
        listc[countc]=nou;
        Client *pclient = &listc[countc];

        countc++;
        pthread_mutex_unlock(&clients_mutex);
        pthread_t thread;
        
        if(pthread_create(&thread,NULL,handle_client, (void *)pclient)!=0)
        {
            perror("Eroare la crearea thread client");
            close(nou.socket);
            removec(nou.id);
            continue;
        }
        pthread_detach(thread);
    }
    close(server_socket);

    return 0;
}
