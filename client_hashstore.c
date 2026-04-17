#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 9000
#define HASH "2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824"

typedef struct{
    int sock;
    struct sockaddr_in server_addr;
    char c;
    char header[256];
    int n;
} Net;

int manage_input(int argc, char* args[])
{
    if(argc == 1){ return -1; }

    if(!strcmp(args[1], "help") || !strcmp(args[1], "-h"))
    { 
        printf("HELP is here dont worry :D\n");
        printf("GET hash -d  --> to get file -d(is optional) to download optional\nLIST       --> to get all files on server\nUPLOAD path/to/file  --> to upload file to server\nDELETE hash --> to delete file with selected has\n");
        return 0; // end
    }
    else if(!strcmp(args[1], "GET") && argc > 1)
    {
        if(argc > 2 && !strcmp(args[3], "-d")){ return 5; } //5 = GET + download
        return 1;// 1 = GET
    }
    else if(!strcmp(args[1], "LIST"))
    {
        return 2;// 2 = LIST
    }
    else if(!strcmp(args[1], "UPLOAD") && argc > 2)
    {
        return 3; // 3 = UPLOAD
    }
    else if(!strcmp(args[1], "DELETE") && argc > 1)
    {
        return 4; // 4 = DELETE
    }
    else{
        return -1;
    }
    return -1;
}

int connect_to_server(Net *net)
{
    //#################
    //## net stuff ###
    //############
    net->n = 0;

    // vytvorenie socketu
    if ((net->sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket error");
        return 1;
    }

    memset(&net->server_addr, 0, sizeof(net->server_addr));
    net->server_addr.sin_family = AF_INET;
    net->server_addr.sin_port = htons(SERVER_PORT);

    if (inet_pton(AF_INET, SERVER_IP, &net->server_addr.sin_addr) <= 0) {
        perror("inet_pton error");
        close(net->sock);
        return 1;
    }

    // pripojenie na server
    if (connect(net->sock, (struct sockaddr*)&net->server_addr, sizeof(net->server_addr)) < 0) {
        perror("connect error");
        close(net->sock);
        return 1;
    }

    printf("Pripojené na %s:%d\n", SERVER_IP, SERVER_PORT);
    return 0;
}


int Call_Get(Net* net, char hash[], int downlaod)
{
    // =====================================================
    // --- ukazka volania GET ---
    // ======================================================
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "GET %s\n", hash);
    send(net->sock, cmd, strlen(cmd), 0);

    // precitaj hlavičku
    while(read(net->sock, &net->c, 1) == 1 && net->c != '\n') net->header[net->n++] = net->c;
    net->header[net->n] = '\0';
    printf("Hlavička servera: %s\n", net->header);

    if(strncmp(net->header, "200", 3) != 0){
        close(net->sock);
        return 1;
    }

    int length;
    char desc[128];
    char ok[8];
    sscanf(net->header, "200 %7s %d %127[^\n]", ok, &length, desc);
    FILE *fptr;
    
    if(downlaod)
    {
        char fileName[128];
        snprintf(fileName, sizeof(fileName), "DOWN_%s", desc);

        fptr = fopen(fileName, "wb");
    }

    printf("Obsah súboru:\n");
    for(int i=0; i<length; i++){
        if(read(net->sock, &net->c, 1) <= 0) break;
        if(downlaod){ fputc(net->c, fptr); }
        putchar(net->c);
    }
    putchar('\n');
    if(downlaod){ 
        fclose(fptr);
        printf("Succesfully downloaded file\n"); }
    return 0;
    // --- KONIEK UKAZKY ---
}

int Call_List(Net* net)
{
    char cmd[] = "LIST\n";
    send(net->sock, cmd, strlen(cmd), 0);
    
    net->n = 0;
    // precitaj hlavičku
    while(read(net->sock, &net->c, 1) == 1 && net->c != '\n') net->header[net->n++] = net->c;
    net->header[net->n] = '\0';
    printf("Hlavička servera: %s\n", net->header);

    if(strncmp(net->header, "200", 3) != 0){
        close(net->sock);
        return 1;
    }

    int length;
    sscanf(net->header, "200 OK %d", &length);
    printf("Server has %d items.\n", length); 
    printf("Listed files:\n");

    // 2. read lines
    for(int i = 0; i < length; i++) {
        char line[256];
        int j = 0;

        while(read(net->sock, &net->c, 1) == 1 && net->c != '\n') {
            if(j < sizeof(line) - 1) {
                line[j++] = net->c;
            }
        }
        line[j] = '\0';

        
        //get hash and desc out of it 
        char h[65];// /0 je nakonci
        char desc[192];
        if (sscanf(line, "%64s %[^\n]", h, desc) == 2) {
            printf("%s  |  %s\n", h, desc);
        }
    }
    return 0;
}

int Call_Upload(Net* net, const char* filepath, const char* description) {
    FILE* file = fopen(filepath, "rb");
    if (!file) return -1;

    fseek(file, 0, SEEK_END); // seek to end of file
    long size = ftell(file); // get current file pointer
    fseek(file, 0, SEEK_SET); // seek back to beginning of file

    unsigned char* buffer = malloc(size);
    fread(buffer, 1, size, file);
    fclose(file);

    //send command
    char cmd[512];
    int cmd_len = snprintf(cmd, sizeof(cmd), "UPLOAD %ld %s\n", size, description);
    send(net->sock, cmd, cmd_len, 0);

    //send bytes
    send(net->sock, buffer, size, 0);
    free(buffer);


    net->n = 0;
    // precitaj hlavičku
    while(read(net->sock, &net->c, 1) == 1 && net->c != '\n') net->header[net->n++] = net->c;
    net->header[net->n] = '\0';
    printf("Hlavička servera: %s\n", net->header);

    if (!strncmp(net->header, "200 STORED", 10)) {
        printf("Success: %s\n", net->header);
    } else if (!strncmp(net->header, "409 HASH_EXISTS", 15)) {
        printf("File already on server: %s\n", net->header);
    } else {
        printf("Server Error: %s\n", net->header);
    }

    return 0;
}

int Call_Delete(Net *net, char hash[])
{       
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "DELETE %s\n", hash);
    send(net->sock, cmd, strlen(cmd), 0);
    
    net->n = 0;
    // precitaj hlavičku
    while(read(net->sock, &net->c, 1) == 1 && net->c != '\n') net->header[net->n++] = net->c;
    net->header[net->n] = '\0';
    printf("Hlavička servera: %s\n", net->header);

    if(strncmp(net->header, "200", 3) != 0){
        close(net->sock);
        return 1;
    }
    printf("Hash %s succesfully deleted\n", hash); 
    return 0;
}
    

int main(int argc, char* argv[]) {

    int input_cmd = manage_input(argc, argv);
    int error;
    if(input_cmd == -1) { 
        printf("An error occured... type help to get help -> ./program_name help \n");
        return 0; 
    } // end
    else if(input_cmd == 0){ return 0; }

    Net net;
    if(connect_to_server(&net)){ return 0; }

    if(input_cmd == 1){ error = Call_Get(&net, argv[2], 0); }
    // tu implementovať protokol dalej, mozno pisat linearne alebo vytvorit procedury/funkcie v pripade c++ aj metody
    //oki
    else if(input_cmd == 2) { error = Call_List(&net); }
    
    else if(input_cmd == 3) { error = Call_Upload(&net, argv[2], argv[3]); }

    else if(input_cmd == 4) { error = Call_Delete(&net, argv[2]); }

    else if(input_cmd == 5){ error = Call_Get(&net, argv[2], 1); }

    if(error){ printf("An error occured... type help to get help -> ./program_name help \n"); }

    close(net.sock);
    printf("Spojenie ukončené\n");
    return 0;
}
