server: ./Server/Server.c
	gcc -pthread -o ./Server/Server ./Server/Server.c -I ./Server/include

client: ./Client/Client.c
	gcc -pthread -o ./Client/Client ./Client/Client.c -I ./Client/include

all: ./Server/Server.c ./Client/Client.c
	gcc -pthread -o ./Server/Server ./Server/Server.c -I ./Server/include
	gcc -pthread -o ./Client/Client ./Client/Client.c -I ./Client/include
