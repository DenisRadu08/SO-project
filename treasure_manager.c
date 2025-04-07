#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>
#include <time.h>


typedef struct{

    int id;
    char name[32];
    float latitude,longitude;
    char clue[1028];
    int value;
}Treasure;

void read_txt(char *buffer,size_t len)
{
    char c;
    int i=0;
    while(i<len-1 && read(0,&c,1)>0 && c!='\n')
    {
        buffer[i++]=c;
    }
    buffer[i]='\0';
}

void write_txt(const char *msg)
{
    write(1,msg,strlen(msg));
}

void add(const char *hunt_id)
{
    char dir[256];
    char file[512];
    char text[1024];
    snprintf(dir,sizeof(dir),"./%s",hunt_id);
    if(mkdir(dir,0777)==-1 && errno!=EEXIST)
    {
        write_txt("Directory create error");
        exit(-1);
    }

    snprintf(file,sizeof(file),"%s/treasure.txt",dir);
    
    int fd=open(file,O_WRONLY|O_CREAT|O_APPEND,0777);
    if(fd==-1)
    {
        write_txt("Error when opening file");
        exit(-1);
    }

    Treasure tr;
    write_txt("ID: ");
    read_txt(text,sizeof(text));
    tr.id=atoi(text);

    write_txt("Treasure name: ");
    read_txt(tr.name,sizeof(tr.name));

    write_txt("Latitude: ");
    read_txt(text,sizeof(text));
    tr.latitude=atof(text);

    write_txt("Longitude: ");
    read_txt(text,sizeof(text));
    tr.longitude=atof(text);

    write_txt("Clue: ");
    read_txt(tr.clue,sizeof(tr.clue));

    write_txt("Value: ");
    read_txt(text,sizeof(text));
    tr.value=atoi(text);

    char buffer[4096];
    int len=snprintf(buffer,sizeof(buffer),"%d,%s,%.2f,%.2f,%s,%d,\n",tr.id,tr.name,tr.latitude,tr.longitude,tr.clue,tr.value);

    write(fd,buffer,len);
    close(fd);
    write_txt("Treasure was added successfully.\n");
}

void list(const char *hunt_id)
{
    char file[256];
    snprintf(file,sizeof(file),"./%s/treasure.txt",hunt_id);

    struct stat st;
    if(stat(file,&st)==-1)
    {
        write_txt("ID is wrong");
        exit(-1);
    }

    write_txt("Name: ");
    write_txt(hunt_id);
    write_txt("\n");

    char buffer[512];
    snprintf(buffer,sizeof(buffer),"File size: %ld bytes\n",st.st_size);
    write_txt(buffer);

    char time_buffer[128];
    struct tm *time=localtime(&st.st_mtime);
    strftime(time_buffer,sizeof(time_buffer),"Last time modified: %Y-%m-%d %H:%M:%S\n",time);
    write_txt(time_buffer);
    write_txt("\n");

    int fd=open(file,O_RDONLY);
    if(fd==-1)
    {
        write_txt("Error when opening file");
        exit(-1);
    }

    write_txt("Treasure list: ");
    char text[4096];
    int index=0;
    char c;
    while(read(fd,&c,1)>0)
    {
        if(c=='\n' || index>=sizeof(text)-1)
        {
            text[index]='\0';
            if(strlen(text)>0)
            {
                int id,value;
                char name[32],clue[1028];
                float latitude,longitude;
                char *token=strtok(text,",");
                id=atoi(token);

                token=strtok(NULL,",");
                strcpy(name,token);

                token=strtok(NULL,",");
                latitude=atof(token);

                token=strtok(NULL,",");
                longitude=atof(token);

                token=strtok(NULL,",");
                strcpy(clue,token);

                token=strtok(NULL,",");
                value=atoi(token);

                char msg[2048];
                snprintf(msg,sizeof(msg),"\nID: %d\nName: %s\nLatitude: %.2f\nLongitude: %.2f\nClue: %s\nValue: %d\n",id,name,latitude,longitude,clue,value);
                write_txt(msg);
            }
            index=0;
        }
        else
        {
            text[index++]=c;
        }
    }
    close(fd);

}

void view(const char *hunt_id, const char *tr_id)
{
    char file[512];
    snprintf(file,sizeof(file),"./%s/treasure.txt",hunt_id);

    int fd=open(file,O_RDONLY);
    if(fd==-1)
    {
        write_txt("Error when opening file");
        exit(-1);
    }
    int index=0,ok=0;
    char c;
    char text[4096];

    while(read(fd,&c,1)>0)
    {
        if(c=='\n' || index>=sizeof(text)-1)
        {
            text[index]='\0';
            if(strlen(text)>0)
            {
                int id,value;
                char name[32],clue[1028];
                float latitude,longitude;
                char *token=strtok(text,",");
                id=atoi(token);

                token=strtok(NULL,",");
                strcpy(name,token);

                token=strtok(NULL,",");
                latitude=atof(token);

                token=strtok(NULL,",");
                longitude=atof(token);

                token=strtok(NULL,",");
                strcpy(clue,token);

                token=strtok(NULL,",");
                value=atoi(token);

                if(atoi(tr_id)==id)
                {
                    char msg[2048];
                    snprintf(msg,sizeof(msg),"ID: %d\nName: %s\nLatitude: %.2f\nLongitude: %.2f\nClue: %s\nValue: %d\n",id,name,latitude,longitude,clue,value);
                    write_txt(msg);
                    ok=1;
                    break;
                }
            }
            index=0;
        }
        else
        {
            text[index++]=c;
        }
    }

    if(ok==0)
    {
        write_txt("Non-existent ID\n");
    }
    close(fd);

}

int main(int argc,char **argv)
{
    if(argc<3)
    {
        write_txt("Not enough arguments");
        return -1;
    }

    if(strcmp(argv[1],"add")==0)
    {
        add(argv[2]);
    }
    else if(strcmp(argv[1],"list")==0)
    {
        list(argv[2]);
    }
    else if(strcmp(argv[1],"view")==0)
    {
        view(argv[2],argv[3]);
    }
    else
    {
        write_txt("Wrong command");
    }
    return 0;
}