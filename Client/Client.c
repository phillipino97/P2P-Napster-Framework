/*
Author: Phillip Driscoll
COSC-4475

Client for the centralized P2P network created by this file and the server file.
Can only be run on unix based machines due to use of arpa/inet.h instead of
winsock32 as well as file system pathing. Works using Windows Subsystem for Linux
*/
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <dirent.h>
#include <vec.c>
//Max byte size for the server, peer, and reading file packets
#define MAX_SIZE_SERVER 1024
#define MAX_SIZE_PEER 200
#define MAX_FILE_LINE_SIZE 64
//structure containing the location and file name of a file within the struct
//vector named 'file_vec'
struct files {
  int num;
  char* file_name;
};
//method for scanning the folder given within resources.txt
//for files that are able to be downloaded and uploaded
int scanForFiles(void);
//method for sending a file over a socket
void sendFile(FILE *, int);
//method for retrieving a file sent over a socket
void getFile(int);
//method used as a thread when peers connect to the peer
void *peerThread(void *);
//method used as a thread for connecting between peers and the server
void *serverThread(void *);
/*
============================== INTEGERS ===============================
Sockets:
sock_desc_server, sock_desc_peer, client_peer_sock, peer_sock, server_sock
Variables:
connected: can be 1 or 0, determines whether the peer SHOULD be Connected
peer_port: contains the port for peers to connect to this peer on
c: used for getting the size of the sockaddr_in struct and passing it
server_port: contains the port that the peer is CURRENTLY connecting to
central_server_port: contains the port of the central server
id: used for storing the unique ID of the peer
accept_files: can be 1 or 0, used to determine whether the peer is willing to
accept uploaded files from other peers
update_files: can be 1 or 0, used to determine whether files have been updated
file_connect_to_id: contains the file id used in the files struct to figure out
which file to transfer to the peer (upload)
file_connect_from_id: contains the file id used in the files struct to figure out
which file to transfer from the peer (download)
upload: can be 1 or 0, used to determine whether the peer is trying to upload a file
file_size: contains the file size of the file being uploaded or downloaded
truetwo_electric_truealoo: constant 1, used as a place holder for true
(the name true was already used in the vector library)
*/
int sock_desc_server, peer_port, sock_desc_peer, c, client_peer_sock, *peer_sock;
int connected, server_port, central_server_port, *server_sock, id, accept_files;
int update_files = 0, file_connect_to_id = -1, file_connect_from_id = 0;
int upload = 0;
long file_size;
const int truetwo_electric_truealoo = 1;
/*
================================ CHARS ==================================
folder_name: Contains the path of the folder containing files to send and receive
central_server_address: contains the address of the central server
server_address: contains the address of the CURRENTLY connected peer or server
send_buff: buffer containing characters to be sent over a socket
read_buff: buffer containing the characters received over a socket
file_connect_from_name: name of the file being retrieved from a peer (download)
file_connect_to_name: name of the file being sent to a peer (upload)
*/
char folder_name[25], central_server_address[20], server_address[20];
char send_buff[MAX_SIZE_SERVER], read_buff[MAX_SIZE_SERVER];
char file_connect_from_name[25];
char file_connect_to_name[25];
//File for the settings of the peer
FILE *resources;
//generates a pointer to the files structure
struct files* file_vec;
//use the passing parameters to get [server_address] and [server_port]
int main(int argc, char** argv) {

  resources = fopen("./resources.txt", "ab+");
  if(!resources) {

    perror("fopen");

  }
  //if there is no data in resources.txt it runs first time setup
  //to populate the file and retrieve parameters
  char temporary[10];
  if(resources != NULL) {

    fseek(resources, 0, SEEK_END);
    int size = ftell(resources);
    if (0 == size) {
      //sets peer port
      fclose(resources);
      resources = fopen("resources.txt", "w");
      printf("First time setup!\nEnter the port you would like peers to connect on: ");
      scanf("%d", &peer_port);
      //sets the folder name containing transfer files
      printf("Enter the location that holds files to share: ");
      scanf("%s", folder_name);
      //sets the peer ID
      printf("Enter a unique integer ID: ");
      scanf("%d", &id);
      //sets willingness to accept uploaded files
      printf("Are you willing to accept uploads? (1:yes 0:no): ");
      scanf("%d", &accept_files);
      //puts that info into the file
      fprintf(resources, "Port: %d\nPath: %s\nID %d\nUpload: %d", peer_port, folder_name, id, accept_files);
      fclose(resources);

    } else {

      fclose(resources);
      resources = fopen("resources.txt", "r");
      //retrieves the values set in the file and allocates them to their
      //variables
      fscanf(resources, "%s%d", temporary, &peer_port);
      fscanf(resources, "%s%s", temporary, folder_name);
      fscanf(resources, "%s%d", temporary, &id);
      fscanf(resources, "%s%d", temporary, &accept_files);

      fclose(resources);

    }

  }

  //sets the server address and port to be the central server address and port
  //central server address and port are given as default parameters
  memcpy(central_server_address, argv[1], sizeof(central_server_address));
  central_server_port = (int) strtol(argv[2], (char **) NULL, 10);
  memcpy(server_address, central_server_address, sizeof(server_address));
  server_port = central_server_port;
  //creates the server thread and peer thread
  pthread_t server_thread;
  pthread_t peer_thread;

  if(pthread_create(&server_thread, NULL, serverThread, NULL) < 0) {

    perror("Could not create server send thread");
    return -1;

  }

  if(pthread_create(&peer_thread, NULL, peerThread, NULL) < 0) {

    perror("could not create peer read thread");
    return -1;

  }
  //creates the vector containing file information
  file_vec = vector_create();
  //updates the file list
  update_files = scanForFiles();
  //sets connected to true, every time the peer needs to switch between
  //server and peer it will need to be set to 0 to reconnect
  connected = 1;
  while(connected >= 0) {

    if(connected == 0) {

      if(pthread_create(&server_thread, NULL, serverThread, NULL) < 0) {

        perror("Could not create server send thread");
        return -1;

      }

      update_files = scanForFiles();
      connected = 1;

    }
  }

  return 0;

}

//method for scanning the folder for file names and gives them an id
int scanForFiles(void) {

  int file_num = 0;
  //creates the structure for opening directories, and has it scan
  struct dirent *de;

  DIR *dr = opendir(folder_name);
  if (dr == NULL) {

    puts("Could not open current directory" );

  }

  while ((de = readdir(dr)) != NULL) {

    file_num += 1;

  }

  closedir(dr);
  //if the sizes of the currently scanned and previously scanned
  //are not the same, it erases the current vector, and adds the new files
  if(vector_size(file_vec) != file_num) {

    vector_erase(file_vec, 0, vector_size(file_vec));

    dr = opendir(folder_name);
    int counter = 0;
    while ((de = readdir(dr)) != NULL) {
      //temporary struct for adding to the vector
      struct files temp_file;
      temp_file.num = counter;
      temp_file.file_name = de->d_name;
      vector_add(&file_vec, temp_file);

      counter += 1;

    }

    closedir(dr);
    //successfull completion
    return 1;

  }
  //unsuccessfull completion
  return 0;

}

//thread for connecting to the server and peers
void *serverThread(void *unused) {
  //structure containing the info for the socket to connect on
  struct sockaddr_in serv_addr;
  //boolean value to determine whether the peer connected correctly
  int joined_correctly = 1;
  //character pointer used for splitting responses from the server
  char *token_comp;
  //tries to open the socket
  if((sock_desc_server = socket(AF_INET, SOCK_STREAM, 0)) < 0) {

    puts("Failed creating socket\n");

  }
  //zeros out the serv_addr struct (for when it needs to reconnect)
  bzero((char *) &serv_addr, sizeof(serv_addr));
  //sets the values for the struct including the server address and port
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = inet_addr(server_address);
  serv_addr.sin_port = htons(server_port);
  //attempts a connect to the server/peer
  if(connect(sock_desc_server, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
    //if it is already trying to connect to the server it just exits
    if(strcmp(server_address, central_server_address) == 0) {

      puts("Failed to connect to server\n");
      exit(0);
      //if it is trying to connect  to a peer, it attempts to reconnect
      //to the server instead
    } else {

      puts("Failed to connect to peer\nReconnecting to server");
      memcpy(server_address, central_server_address, sizeof(server_address));
      server_port = central_server_port;
      joined_correctly = 0;

    }

  }

  if(joined_correctly) {

    printf("Connected successfully\n");
    //these values are different than -1 and 0 respectively when
    //the peer is attempting to connect to another peer
    if(file_connect_to_id == -1 && upload == 0) {
      //creates a character array and sets it to be a message to the server
      //containing the port for peers to connect on, the peer ID, and whether
      //the peer is willing to accept files
      char server_port_str[10];
      sprintf(server_port_str, "%d+%d+%d", peer_port, id, accept_files);
      send(sock_desc_server, server_port_str, strlen(server_port_str), 0);
      //sleep to allow the server to be ready for the update queue
      sleep(1);
      sprintf(send_buff, "update+%d$", id);
      //zeroes the file vector then scans for files
      vector_erase(file_vec, 0, vector_size(file_vec));
      int a = scanForFiles();
      //creates the character array that will contain the info about files
      //then loops through to grab file name and id and add it to the message
      //to the server
      char name_id[75];
      for(int i = 0; i < vector_size(file_vec); i++) {

        if(i != vector_size(file_vec)-1) {

          sprintf(name_id, "%s@%d$", file_vec[i].file_name, file_vec[i].num);
          strcat(send_buff, name_id);

        } else {

          sprintf(name_id, "%s@%d", file_vec[i].file_name, file_vec[i].num);
          strcat(send_buff, name_id);

        }

      }
      //sends the update message
      send(sock_desc_server, send_buff, MAX_SIZE_SERVER, 0);
      //retrieves the positive indicator that the server successfully updated
      recv(sock_desc_server, read_buff, MAX_SIZE_SERVER, 0);
      bzero(send_buff, MAX_SIZE_SERVER);
      bzero(read_buff, MAX_SIZE_SERVER);
      //runs while the input from the command line is not null
      while((fgets(send_buff, MAX_SIZE_SERVER, stdin) != NULL)) {
        //if /quit is sent it stops the peer
        if(strcmp(send_buff, "/quit") == 10) {

          connected = -1;
          //if update is sent, it sends the queue to update the files from the
          //peer on the server in the same way as described above
        } else if(strcmp(send_buff, "update") == 10) {

          bzero(send_buff, MAX_SIZE_SERVER);
          sprintf(send_buff, "update+%d$", id);

          int a = scanForFiles();

          char name_id[75];
          for(int i = 0; i < vector_size(file_vec); i++) {

            if(i != vector_size(file_vec)-1) {

              sprintf(name_id, "%s@%d$", file_vec[i].file_name, file_vec[i].num);
              strcat(send_buff, name_id);

            } else {

              sprintf(name_id, "%s@%d", file_vec[i].file_name, file_vec[i].num);
              strcat(send_buff, name_id);

            }

          }

          send(sock_desc_server, send_buff, MAX_SIZE_SERVER, 0);

          recv(sock_desc_server, read_buff, MAX_SIZE_SERVER, 0);
          puts(read_buff);
          //if the search indicator is sent (format must be 'search: filename')
          //it sends the server the search queue plus the name of the file to
          //search for
        } else if(strcmp((token_comp = strtok(send_buff, ": ")), "search") == 0) {

          token_comp = strtok(NULL, ": ");
          token_comp[strlen(token_comp)-1]='\0';
          sprintf(send_buff, "%s%s", "search+", token_comp);

          send(sock_desc_server, send_buff, MAX_SIZE_SERVER, 0);
          //retrieves the list of files, their addresses, ports, etc.
          recv(sock_desc_server, read_buff, MAX_SIZE_SERVER, 0);

          puts("Peer_ID\t\tFile_ID\t\tFile_Name\t\tAddress\t\tPort");
          //creates another token for splitting strings, a char pointer for
          //making a vector then splits the strings at $ and adds them
          //sample input from the server could look like this:
          //id+name+address+port$
          char *token_split;
          char** info_vec;
          info_vec = vector_create();
          token_split = strtok(read_buff, "$");
          vector_add(&info_vec, token_split);

          while((token_split = strtok(NULL, "$")) != NULL) {

            vector_add(&info_vec, token_split);

          }
          //values for the file id, port, address, and the name
          int file_id[vector_size(info_vec)];
          int file_port[vector_size(info_vec)];
          char* file_address[vector_size(info_vec)];
          char* file_name[vector_size(info_vec)];
          //loops through and uses the tokens to split into the respective parts,
          //then adds those values to the respective arrays above this comment
          for(int i = 0; i < vector_size(info_vec)-1; i++) {

            char temp_info[100];
            char temp_info2[50];
            token_split = strtok(info_vec[i], "+");
            file_id[i] = atoi(token_split);
            token_split = strtok(NULL, "+");
            file_name[i] = token_split;
            sprintf(temp_info, "%d\t\t%d\t\t%s\t\t", i, file_id[i], file_name[i]);

            token_split = strtok(NULL, "+");
            file_address[i] = token_split;
            token_split = strtok(NULL, "+");
            file_port[i] = atoi(token_split);
            sprintf(temp_info2, "%s\t%d", file_address[i], file_port[i]);

            strcat(temp_info, temp_info2);
            //prints the resulting string
            puts(temp_info);

            bzero(temp_info, 100);
            bzero(temp_info2, 50);

          }
          //prompts the user to choose which file from which location they would
          //like to download
          puts("Enter Peer_ID (-1 for no select): ");
          int selected;
          char in[10];
          fgets(in, 10, stdin);
          selected = atoi(in);

          if(selected != -1 && (selected < vector_size(info_vec)-1)) {
            //retrieves the selected value then sets the server address and port
            //accordingly, then saves the file id and the file name
            server_port = file_port[selected];
            memcpy(server_address, file_address[selected], 20);
            file_connect_to_id = file_id[selected];
            memcpy(file_connect_from_name, file_name[selected], 50);

            bzero(in, 10);
            bzero(send_buff, MAX_SIZE_SERVER);
            bzero(read_buff, MAX_SIZE_SERVER);

            //leaves the loop to reconnect to a peer
            break;

          }
          //if the user input is 'upload: filename' then it will grab the file name
          //using a token then make sure the file is in the folder
        } else if(strcmp(token_comp, "upload") == 0) {

          int file_found = 1;
          token_comp = strtok(NULL, ": ");
          for(int i = 0; i < vector_size(file_vec); i++) {

            if(strcmp(file_vec[i].file_name, token_comp) == -10) {
              strcpy(file_connect_to_name, file_vec[i].file_name);
              break;
            }
            if(i == (vector_size(file_vec) - 1)) {
              puts("File not found!");
              file_found = 0;
            }

          }
          //if the file was in the library, it sends the server a request to
          //connect to a peer that is willing to accept files
          if(file_found) {

            sprintf(send_buff, "upload+%d", id);
            send(sock_desc_server, send_buff, MAX_SIZE_SERVER, 0);

            recv(sock_desc_server, read_buff, MAX_SIZE_SERVER, 0);
            //receives the address and port of the peer to connect to and upload
            token_comp = strtok(read_buff, "+");
            memcpy(server_address, token_comp, 20);
            token_comp = strtok(NULL, "+");
            server_port = atoi(token_comp);

            bzero(send_buff, MAX_SIZE_SERVER);
            bzero(read_buff, MAX_SIZE_SERVER);

            upload = 1;

            break;

          }


        }
        //to clean buffer-->IMP otherwise previous word characters also came
        bzero(send_buff, MAX_SIZE_SERVER);
        bzero(read_buff, MAX_SIZE_SERVER);

      }
      //if there is a file id that is wanting to be downloaded
    } else if(file_connect_to_id != -1) {
      //sends the search indicator to the peer with the id of the file it wants
      //to download
      sprintf(send_buff, "search+%d", file_connect_to_id);
      send(sock_desc_server, send_buff, MAX_SIZE_SERVER, 0);
      //retrieves the file size of the file to be downloaded
      recv(sock_desc_server, read_buff, MAX_SIZE_SERVER, 0);
      file_size = atoi(read_buff);
      //gets the file
      getFile(sock_desc_server);
      //indicates a successfull transfer
      puts("File successfully transferred!\nConnecting back to server.");
      //resets the server address and port to the central ones and
      //sets the file id to -1 and file size to 0
      memcpy(server_address, central_server_address, sizeof(central_server_address));
      server_port = central_server_port;

      file_connect_to_id = -1;

      bzero(send_buff, MAX_SIZE_SERVER);
      bzero(read_buff, MAX_SIZE_SERVER);
      file_size = 0;
      //if there is an upload queued
    } else if(upload == 1) {
      //sends the upload indicator to the peer with the name of the file that
      //is being uploaded
      sprintf(send_buff, "upload+%s", file_connect_to_name);
      send(sock_desc_server, send_buff, MAX_SIZE_PEER, 0);
      bzero(send_buff, MAX_SIZE_SERVER);
      //sets the file path and opens the file for sending
      char path_to_file[50];
      sprintf(path_to_file, "%s/%s", folder_name, file_connect_to_name);
      FILE *send_file = fopen(path_to_file, "r");
      //grabs the file size and sends it to the peer
      fseek(send_file, 0, SEEK_END);
      file_size = ftell(send_file);
      sprintf(send_buff, "%ld", file_size);
      send(sock_desc_server, send_buff, MAX_SIZE_PEER, 0);
      fseek(send_file, 0, SEEK_SET);
      //sends the file
      sleep(1);
      sendFile(send_file, sock_desc_server);

      fclose(send_file);
      //indicates that the upload was successfull
      puts("File sent to peer!\nReconnecting to server!");
      //resets the server address and port to the central ones and
      //sets the upload to 0 and file size to 0
      memcpy(server_address, central_server_address, sizeof(central_server_address));
      server_port = central_server_port;

      upload = 0;
      file_size = 0;

      bzero(file_connect_to_name, 25);
      bzero(send_buff, MAX_SIZE_SERVER);
      bzero(read_buff, MAX_SIZE_SERVER);

    }

  }

  close(sock_desc_server);

  //sets the connection to zero indicating a reconnect
  connected = 0;
}

//method for use as a thread used for peers to connect to this peer
void *peerThread(void *unused) {
  //creates the server and peer structures, the needed integers, and the
  //buffers for data sent between the peers
  struct sockaddr_in server, peer;
  int n, sock, len;
  char send_peer_message[MAX_SIZE_PEER], peer_message[MAX_SIZE_PEER], data;
  //creates the socket
  sock_desc_peer = socket(AF_INET, SOCK_STREAM, 0);
  if (sock_desc_peer == -1) {

    puts("Could not create socket - Peer Read");
    exit(0);

  }

  puts("New socket created - Peer Read");
  //sets the socket to reuseable
  if (setsockopt(sock_desc_peer, SOL_SOCKET, SO_REUSEADDR, &truetwo_electric_truealoo, sizeof(int)) == -1) {

    perror("Setsockopt Error - Peer Read");
    exit(0);

  }
  //sets the socket info, including the port for peers to connect on
  server.sin_family = AF_INET;
  server.sin_addr.s_addr = INADDR_ANY;
  server.sin_port = htons(peer_port);
  //Bind
  if(bind(sock_desc_peer, (struct sockaddr *) &server, sizeof(server)) < 0) {
    //print the error message
    perror("Binding Error - Peer Read");
    exit(0);

  } else {

    puts("Binding Complete - Peer Read");

    while(true) {
      //Listens for traffic coming in
      listen(sock_desc_peer, 3);
      //Accept and incoming connection
      puts("Waiting for incoming peers...");
      //accepts the peer
      c = sizeof(struct sockaddr_in);
      client_peer_sock = accept(sock_desc_peer, (struct sockaddr*) &peer, (socklen_t*) &c);
      //allocates memory to the socket
      peer_sock = malloc(1);
      *peer_sock = client_peer_sock;
      sock = *(int*) (void*) peer_sock;
      //receives the indicator message from the peer (upload or download)
      recv(sock, peer_message, MAX_SIZE_PEER, 0);
      char* token = strtok(peer_message, "+");
      //if the peer wants to download
      if(strcmp(token, "search") == 0) {
        //splits the message to get the file id for sending
        token = strtok(NULL, "+");
        file_connect_from_id = atoi(token);
        //sets the file path
        char path_to_file[50];
        sprintf(path_to_file, "%s/%s", folder_name, file_vec[file_connect_from_id].file_name);
        FILE *send_file = fopen(path_to_file, "r");
        //gets the file size and sends it to peer
        fseek(send_file, 0, SEEK_END);
        file_size = ftell(send_file);
        sprintf(send_peer_message, "%ld", file_size);
        send(sock, send_peer_message, MAX_SIZE_SERVER, 0);
        fseek(send_file, 0, SEEK_SET);
        //sends the file
        sendFile(send_file, sock);
        //closes everything out and resets variables
        fclose(send_file);
        file_connect_from_id = 0;
        file_size = 0;
        bzero(send_peer_message, MAX_SIZE_PEER);

      } else {
        //if the message was upload, it grabs the file name then gets the file
        //size
        token = strtok(NULL, "+");
        memcpy(file_connect_from_name, token, 50);

        recv(sock, peer_message, MAX_SIZE_PEER, 0);
        file_size = atoi(peer_message);
        //gets the file
        getFile(sock);
        //resets variables
        file_size = 0;
        bzero(file_connect_from_name, 25);

      }
      //to clean buffer-->IMP otherwise previous word characters also came
      bzero(peer_message, MAX_SIZE_PEER);
      //closes the sockets and frees allocated memory
      close(sock);
      close(client_peer_sock);
      free(peer_sock);
      puts("Peer disconected");

      if (client_peer_sock < 0) {

        perror("Accept failed - Peer Read");
        break;

      }

    }

  }

}

//method for sending a file over a socket
void sendFile(FILE *file, int socket) {
  //creates the buffer using the max file packet size
  char data[MAX_FILE_LINE_SIZE];
  int x = 0;
  bzero(data, MAX_FILE_LINE_SIZE);
  //reads the file
  int nb = fread(data, 1, sizeof(data), file);
  //checks if the file still has data
  while(!feof(file)) {
    //writes the packet to the socket then reads more
    x = x + 1;
    write(socket, data, nb);
    nb = fread(data, 1, sizeof(data), file);

  }
  //prints the number of bytes sent to the peer
  char end_msg[20];
  sprintf(end_msg, "%ld bytes sent", (long) (x*MAX_FILE_LINE_SIZE));
  puts(end_msg);
  //sleep to allow peer to complete the download
  sleep(1);

}

//method for retrieving a file over a socket
void getFile(int socket) {
  //creates the file using the given file name
  FILE *file;
  char filename[50];
  int x = 0;
  sprintf(filename, "%s/%s", folder_name, file_connect_from_name);
  //creates the buffer using max file packet size
  char buffer[MAX_FILE_LINE_SIZE];
  bzero(buffer, MAX_FILE_LINE_SIZE);

  file = fopen(filename, "w");
  //reads the data from the socket
  int nb = read(socket, buffer, MAX_FILE_LINE_SIZE);
  //checks size of nb, and whether data retrieved is adequate
  while(nb >= 0 && (x*MAX_FILE_LINE_SIZE) < file_size) {
    //writes to the new file, then reads more data from the socket
    fwrite(buffer, 1, nb, file);
    nb = read(socket, buffer, MAX_FILE_LINE_SIZE);
    x = x + 1;

  }
  //prints the number of bytes retrieved from the peers
  char end_msg[20];
  sprintf(end_msg, "%ld bytes retrieved", (long) (x*MAX_FILE_LINE_SIZE));
  puts(end_msg);
  fclose(file);

}
