# P2P-Napster-Framework
A centralized P2P network that uses the framework that napster was built on

This is a centralized P2P Network written in c.
The framework was based on the Napster framework.

This program is only compatible with UNIX based machines, including Windows Subsytem for Linux,
due to its use of the arpa/inet standard library as well as system pathing.

gcc will need to be installed before making the files.
Once the file is downloaded, you will need to run 'make all' from a terminal to create the executable files for the Server and Client.
They can be individually made using 'make server' to compile the server code and 'make client' to compile the client code.

If the user is running multiple clients off of the same machine, they will need to put the compiled clients within different directories.
Meaning that you cannot run one client within the same directory as another due to resources within directories.
When the client files are put inside different directories, you should create a folder inside all client directories (named without spaces).

The server executable can simply be executed within its given file using ./Server [ipv4].
You will need to enter your computer's ipv4 as a default parameter due to how the networking works between clients on the same machine, and others.

The client executables can be run as ./Client [server address] 3000.
Server address will be 127.0.0.1 if connecting to a server on the same machine.
The 3000 is the port that the server uses by default.

When the client is first run it will do a first time setup, it will request the port that should be opened for peers to connect,
the location for files to be uploaded and downloaded to (this will be ./[folder name]) if you have followed the instructions for putting clients in seperate directories,
the client ID, and this is important, the ID MUST be different for every client that will be connecting to the server, and whether the client will allow peers to upload
files to it.

When everything is all setup, throw some test files (no spaces in the file name) into each peers folder for testing. 
The files can be anything from image files, to mp3, to text files.

Once all peers are connected to the server, searches for files can be made, however they must be searches for exact names with file extensions.
An example would be something like 'search: image1.jpg' or 'search: test.pdf'.
If you have chosen peers to allow uploads, an upload can be made in much the same way.
An example would look something like 'upload: image1.jpg' or 'upload: test.pdf'.
For the upload to go through, there must be a peer, other than the one doing the upload, that allows peers to upload.
If this is not the case, the program breaks.
If you add a file to the peer folder and would like to update your file list to the server you simply write 'update' to do so.

On the server end there are three notable commands:
/list lists all the files along with the peer they belong to
/clients lists all client ID's that are connected.
/quit exits the program.

If the server exits before the peers, the peers will continue to seem connected until the user tries to send a request to the server or
if the user terminates the program.
The client can be exited by typing /quit in a similar fashion to the server.

For any questions contact me at driscoll_p06145@utpb.edu
