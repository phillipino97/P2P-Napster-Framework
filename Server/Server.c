/*
Author: Phillip Driscoll
COSC-4475

Server for the centralized P2P network created by this file and the client files.
Can only be run on unix based machines due to use of arpa/inet.h instead of
winsock32 as well as file system pathing. Works using Windows Subsystem for Linux
*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <vec.c>
//maximum size of data from the peer
#define MAX_SIZE_CLIENT 1024
//the thread functions
void *clientThread(void *);
void *readLineThread(void *);
//search function for searching for files within peers and update function
//for updating file lists within peers
char* Search(char *);
char* Update(char *);
char* Upload(int);
//handles commands given from the client
char* handleCommandsClient(char *);
//insertion sort for sorting client ID's after clients join
void insertionSort(void);
//binary search for searching for client ID's
int binarySearchID(int, int, int);
//structure containing client info, and the vector for the client files
struct client {
  int ID;
  char* address;
  int port;
  int accept_files;
  struct files* client_files;
};
//structure containing file info
struct files {
  int ID;
  char* file_name;
};
//command used for getting server commands, server address for ipv4
char command[50];
char server_address[20];
//constant integer used to indicate true
const int truetwo_electric_truealoo_two = 1;
//creates the client structure pointer for creating a vector later
struct client* client_vec;
//integer pointer for a vector containing the ID's of peers willing to accept
//files
int* upload_vec;
//default parameter is the ipv4 address of the server
int main(int argc, char **argv) {
  //sets the server_address to the ipv4 address of the machine given as a
  //parameter
  //this is due to needing to use this network between peers on the computer
  //running the server, and peers running on different machines as it would store
  //local peers as 127.0.0.1 and that address is no good to peers on different machines
  memcpy(server_address, argv[1], sizeof(server_address));
  //creates the thread for server commands
  pthread_t read_line;
  if(pthread_create(&read_line, NULL, readLineThread, NULL) < 0) {

    perror("Could not create read line thread");
    return 1;

  }
  //sets client_vec and upload_vec to be vectors
  client_vec = vector_create();
  upload_vec = vector_create();
  //socket info as well as structures for address and port info
  int socket_desc, client_sock, c, *new_sock;
  struct sockaddr_in server, client;

  //Create socket
  socket_desc = socket(AF_INET, SOCK_STREAM, 0);
  if (socket_desc == -1) {

    printf("Could not create socket");

  }

  puts("Socket created");

  //Prepare the server sockaddr_in structure with default port (3000)
  server.sin_family = AF_INET;
  server.sin_addr.s_addr = INADDR_ANY;
  server.sin_port = htons(3000);

  //Bind
  if(bind(socket_desc, (struct sockaddr *) &server, sizeof(server)) < 0) {
    //print the error message
    perror("Bind failed. Error");
    return 1;

  }

  puts("Bind done");

  //Listen for peers
  listen(socket_desc, 3);

  //Accept and incoming connection
  puts("Waiting for incoming connections...");

  c = sizeof(struct sockaddr_in);
  while((client_sock = accept(socket_desc, (struct sockaddr*) &client,
  (socklen_t*) &c))) {
    puts("Connection accepted:");
    //generates a new thread using the new connection
    pthread_t client_thread;
    new_sock = malloc(1);
    *new_sock = client_sock;
    //grabs the client ip
    char clientip[20];
    sprintf(clientip, "%s", inet_ntoa(client.sin_addr));
    //temporary structure for adding to the client_vec
    struct client curr;
    //assigns the address to a point in memory, without allocating the memory
    //the address will be the same as the last client to connect
    curr.address = (char *) malloc(sizeof(char*)*(sizeof(clientip)));
    //sees if the connection is local, if it is, it assigns the peer
    //the ipv4 address
    if(strcmp(clientip, "127.0.0.1") == 0) {
      strcpy(curr.address, server_address);
    } else {
      strcpy(curr.address, clientip);
    }
    vector_add(&client_vec, curr);
    //creates the thread, passing the socket as a parameter
    if(pthread_create(&client_thread, NULL, clientThread, (void*) new_sock) < 0) {

      perror("Could not create read thread");
      return 1;

    }

  }

  if (client_sock < 0) {

    perror("Accept failed");
    return 1;

 }

 free(new_sock);
 return 0;

}

//used for debugging in the server, /list lists the files, /clients lists the
//client ID's
void *readLineThread(void *unused) {

  char command_in[50];
  while(truetwo_electric_truealoo_two) {

    fgets(command_in, 50, stdin);

    if(strcmp(command_in, "/list") == 10) {

      for(int i = 0; i < vector_size(client_vec); i++) {

        for(int j = 0; j < vector_size(client_vec[i].client_files); j++) {

          printf("Client ID: %d File ID: %d File Name: %s\n", client_vec[i].ID, client_vec[i].client_files[j].ID, client_vec[i].client_files[j].file_name);

        }

      }

    } else if(strcmp(command_in, "/clients") == 10) {

      for(int i = 0; i < vector_size(client_vec); i++) {
        printf("%d ", client_vec[i].ID);
      }
      printf("\n");

    } else if(strcmp(command_in, "/quit") == 10) {
      exit(0);
    }

  }

}

//method for use in a thread
void *clientThread(void *socket_desc) {
  //creates the socket, neccesary info like the id and location of the peer
  //in the client_vec vector and the send and read buffs
  int sock = *(int*) socket_desc;
  int id = vector_size(client_vec)-1;
  int location;
  char *send_buff, read_buff[MAX_SIZE_CLIENT], data;
  //first message contains info like the peer port, ID, and it creates
  //the client_files vector within the vector and whether the peer is willing
  //to accept files
  recv(sock, read_buff, MAX_SIZE_CLIENT, 0);
  char *token = strtok(read_buff, "+");
  client_vec[id].port = atoi(token);
  token = strtok(NULL, "+");
  client_vec[id].ID = atoi(token);
  client_vec[id].client_files = vector_create();
  id = atoi(token);
  token = strtok(NULL, "+");
  client_vec[id].accept_files = atoi(token);
  if(client_vec[id].accept_files) {
    vector_add(&upload_vec, id);
  }
  //sorts the client_vec vector  so the ID's are in order for searching
  insertionSort();
  //grabs the location of the peer in client_vec through a binary search
  char port_str[20];
  location = binarySearchID(0, vector_size(client_vec), id);
  sprintf(port_str, "Peer port: %d", client_vec[location].port);
  puts(port_str);

  puts("Handler assigned");
  //while the server is still getting at least one byte of data
  while(recv(sock, &data, 1, MSG_PEEK) == 1) {
    //layout of interactions with the peer is get a message, handle the message,
    //then send a response
    recv(sock, read_buff, MAX_SIZE_CLIENT, 0);
    send_buff = handleCommandsClient(read_buff);
    send(sock, send_buff, MAX_SIZE_CLIENT, 0);

    bzero(read_buff, MAX_SIZE_CLIENT);

  }

  close(sock);
  //when the client disconnects it does a search for where it was in client_vec
  //then proceeds to remove all data from its client_files vector and removes
  //the entry from client_vec
  printf("Client %d Disconnected", id);
  char temp[20];
  location = binarySearchID(0, vector_size(client_vec), id);
  if(location != -1) {

    vector_erase(client_vec[location].client_files, 0, vector_size(client_vec[location].client_files));
    free(client_vec[location].address);
    vector_remove(client_vec, location);
    for(int i = 0; i < vector_size(upload_vec); i++) {

      if(upload_vec[i] == id) {

        vector_remove(upload_vec, i);
        break;

      }

    }

  }

  puts(temp);

}

//the layout of all messages from the peer come with the first term seperated by
//a +, 'command+message'
char* handleCommandsClient(char* command) {
  //checks whether the message was one for searching for a file, updating the
  //files that the peer has, or uploading a file
  char *token = strtok(command, "+");
  if(strcmp("search", token) == 0) {

    token = strtok(NULL, "+");
    return Search(token);

  } else if(strcmp("update", token) == 0) {

    token = strtok(NULL, "+");
    return Update(token);

  } else if(strcmp("upload", token) == 0) {

    token = strtok(NULL, "+");
    return Upload(atoi(token));

  }

}

//method for searching through the files that the peers have
//and returning the info for that file if it is available
char* Search(char* command) {
  char locations[MAX_SIZE_CLIENT];
  bzero(locations, MAX_SIZE_CLIENT);
  char *end;
  free(end);

  for(int i = 0; i < vector_size(client_vec); i++) {

    for(int j = 0; j < vector_size(client_vec[i].client_files); j++) {

      if(strcmp(command, client_vec[i].client_files[j].file_name) == 0) {

        sprintf(locations, "%s%d+%s+%s+%d$\n", locations,
        client_vec[i].client_files[j].ID, client_vec[i].client_files[j].file_name,
        client_vec[i].address, client_vec[i].port);

      }

    }

  }

  end = (char *) malloc(sizeof(char*)*(sizeof(locations)));
  strcpy(end, locations);
  //returns the data of file data, addresses, ports, ID's, etc
  return end;

}

//method for updating files in the client_files vector within the client_vec
char* Update(char* id_files) {

  //sets token to be the ID of the peer, creates a vector for file names
  char *end = (char *) malloc(sizeof(char*)*(sizeof("Update Completed")));
  strcpy(end, "Update Completed");
  char *token = strtok(id_files, "$");
  char** names;
  names = vector_create();
  //searches for the ID given from token
  int id = binarySearchID(0, vector_size(client_vec), atoi(token));
  //frees previous data the is held within the client_files vector
  for(int i = 0; i < vector_size(client_vec[id].client_files); i++) {
    free(client_vec[id].client_files[i].file_name);
  }
  //erases the vector entirely
  vector_erase(client_vec[id].client_files, 0, vector_size(client_vec[id].client_files));
  //grabs the file names and adds them to the names vector
  while((token = strtok(NULL, "$")) != NULL) {

    vector_add(&names, token);

  }
  //temp char pointer for the file names
  char* file_names;
  //iterates through the whole names vector
  for(int i = 0; i < vector_size(names); i++) {
    //token for splitting the contents of the names vector
    char *token2 = strtok(names[i], "@");
    file_names = (char *) malloc(sizeof(char*)*(sizeof(token2)));
    //sets the file name
    strcpy(file_names, token2);
    //creates a temporary files structure that can be given file names
    //and ID's and pass them to the client_files vector within client_vec
    struct files temp_file;
    temp_file.file_name = file_names;
    token2 = strtok(NULL, "@");
    temp_file.ID = atoi(token2);
    vector_add(&client_vec[id].client_files, temp_file);

  }
  //returns confirmation of the update taking place
  return end;

}

//method for retrieving a peer address+port for the peer to connect to
//and upload the file
char* Upload(int curr_id) {
  //variables needed, end_up is allocated and freed as a hack to free the data
  //allocated later on
  time_t t;
  int selected;
  char* end_up = (char *) malloc(1);
  free(end_up);
  char temp[20];
  bzero(temp, 20);
  srand((unsigned) time(&t));
  //grabs a peer at random from the size of upload_vec and passes the id to the
  //binary search to get the location of the peer within client_vec
  selected = rand()%vector_size(upload_vec);
  int id = binarySearchID(0, vector_size(client_vec), upload_vec[selected]);
  //checks whether the selected peer is the peer making the request
  //if it is it runs until it isn't
  while(curr_id == client_vec[id].ID) {
    selected = rand()%vector_size(upload_vec);
    id = binarySearchID(0, vector_size(client_vec), upload_vec[selected]);
  }
  //sets the return variable to be address+port
  sprintf(temp, "%s+%d", client_vec[id].address, client_vec[id].port);
  end_up = (char *) malloc(sizeof(char*)*(sizeof(temp)));
  strcpy(end_up, temp);

  return end_up;

}

//method for insertion sorting the client_vec vector
void insertionSort(void) {

  int temp, j;
  struct client temp_vec;

  for(int i = 1; i < vector_size(client_vec); i++){

      temp = client_vec[i].ID;
      temp_vec = client_vec[i];
      j = i - 1;

      while((temp < client_vec[j].ID) && (j >= 0)){

         client_vec[j + 1] = client_vec[j];
         j = j - 1;

      }

      client_vec[j + 1] = temp_vec;

   }

}

//method for binary searching the client_vec vector based on client_vec.ID
int binarySearchID(int start, int end, int id) {

    if (end >= start) {

        int mid = start + (end - start)/2;

        if (client_vec[mid].ID == id) {

          return mid;

        }

        if (client_vec[mid].ID > id) {

          return binarySearchID(start, mid - 1, id);

        }

        return binarySearchID(mid + 1, end, id);

    }

    return -1;

}
