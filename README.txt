Aaron Gordon 0884023
Assignment 2 for CIS*3210

All functionality should be present (attempted) for testing.

To run:
- make or make all(make clean is functional but isn't needed)
- ./server PORT (mine was 12027) to start the server and commands shown later to end them.
- ./client IP/HOSTNAME:PORT FILENAME where FILENAME is a mandatory filename to be saved, the IP and port are mandatory as is the colon.
- If you wish, you can run the script with bash script.sh IP/HOSTNAME FILENAME which will use the 12027 port automatically and run the client many times with the same file (file.txt by default but you can edit the script if needed). This can be repeated endlessly.
- Uploading is as simple as running the client with whatever file you wish regardless of size. The file should be queued and saved by the server.
- The manner of file saving for duplicate files is a NUM-FILENAME where the NUM is the smallest number possible without overlapping, e.g. if file.txt, 1-file.txt and 2-file.txt exist, 3-file.txt will be created. The logic behind this is that it allows any number of files of the same name to be created, simple as that.
- d for viewing data, s for soft shutdown and h for hard shutdown are all present. s prompts the user a second time as the bonus describes. If anything else is selected in the UI, nothing occurs but a reprompt.

Error handling is done throughout.