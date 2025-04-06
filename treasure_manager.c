#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <sys/stat.h>

int n=0;

typedef struct{

    int id;
    char name[32];
    float latitude,longitude;
    char clue[1028];
    int value;
}Treasure;

void create_read(Treasure treasure[])
{
    char buffer[4096]={0};

    int fd=open("treasure.txt", O_RDONLY|O_CREAT, 0777);
    if(fd<0)
    {
        perror("eroare la deschidere fisier");
        return;
    }

    int biti=read(fd,buffer,sizeof(buffer)-1);
    buffer[biti]='\0';

    char *line=strtok(buffer,"\n");

    while(line!=NULL && n<101)
    {
        int cnt=0;
        char *token=strtok(line, ",");
        while(token!=NULL)
        {
            switch(cnt)
            {
                case 0: 
                    treasure[n].id=atoi(token);
                    break;

                case 1:
                    strcpy(treasure[n].name,token);
                    break;

                case 2:
                    treasure[n].latitude=atof(token);
                    break;

                case 3:
                    treasure[n].longitude=atof(token);
                    break;

                case 4:
                    strcpy(treasure[n].clue,token);
                    break;

                case 5:
                    treasure[n].value=atoi(token);
                    break;
            }
            
            token=strtok(NULL,",");
            cnt++;
        }

        n++;
        line=strtok(NULL,"\n");

    }

    close(fd);

}

int main()
{
    Treasure treasure[101];
    create_read(treasure);
    for(int i=0;i<n;i++)
    {
        printf("%d - %s - %f - %f - %s - %d\n",treasure[i].id,treasure[i].name,treasure[i].latitude,treasure[i].longitude,treasure[i].clue,treasure[i].value);
    }
    printf("%d\n",n);
}