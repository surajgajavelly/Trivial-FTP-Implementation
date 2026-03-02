CC := gcc
CFLAGS := -Wall -Wextra -g

# Directories
CLIENT_DIR := client
SERVER_DIR := server

# Targets
all: client_exe server_exe

client_exe:
	$(CC) $(CLIENT_DIR)/*.c -o client_app $(CFLAGS)

server_exe:
	$(CC) $(SERVER_DIR)/*.c -o server_app $(CFLAGS)

clean:
	rm -f client_app server_app
	@echo "Cleaned executables."