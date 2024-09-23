#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <sys/stat.h>

#define BUFFER_SIZE 1024
#define FILE_DIRECTORY "server_files"  // 服务器存储文件的目录

// 函数声明
void handle_client(int client_sock);
void upload_file(int client_sock, const char *filename);
void download_file(int client_sock, const char *filename);
void delete_file(int client_sock, const char *filename);
void list_files(int client_sock);

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <port>\n", argv[0]);
        return -1;
    }

    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_size = sizeof(client_addr);
    int port = atoi(argv[1]);  // 从命令行获取端口号

    mkdir(FILE_DIRECTORY, 0777);  // 创建文件存储目录

    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);  // 使用自定义端口号
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_sock, 5) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d...\n", port);

    while (1) {
        client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &addr_size);
        if (client_sock < 0) {
            perror("Accept failed");
            continue;
        }

        printf("Client connected.\n");
        handle_client(client_sock);
        close(client_sock);
        printf("Client disconnected.\n");
    }

    close(server_sock);
    return 0;
}

// 处理客户端请求
void handle_client(int client_sock) {
    char buffer[BUFFER_SIZE];

    while (1) {
        int n = recv(client_sock, buffer, BUFFER_SIZE, 0);
        if (n <= 0) break;

        buffer[n] = '\0';

        char command[BUFFER_SIZE];
        char filename[BUFFER_SIZE];

        if (sscanf(buffer, "%s %[^\n]", command, filename) < 1) continue;

        if (strcmp(command, "upload") == 0) {
            upload_file(client_sock, filename);
        } else if (strcmp(command, "download") == 0) {
            download_file(client_sock, filename);
        } else if (strcmp(command, "delete") == 0) {
            delete_file(client_sock, filename);
        } else if (strcmp(command, "list") == 0) {
            list_files(client_sock);
        }
    }
}

// 上传文件
void upload_file(int client_sock, const char *filename) {
    char filepath[BUFFER_SIZE];
    sprintf(filepath, "%s/%s", FILE_DIRECTORY, filename);

    FILE *file = fopen(filepath, "wb");
    if (file == NULL) {
        perror("File creation failed");
        return;
    }

    char buffer[BUFFER_SIZE];
    char *line;

    while (1) {
        int n = recv(client_sock, buffer, BUFFER_SIZE, 0);
        if (n <= 0) break;

        buffer[n] = '\0';

        line = strtok(buffer, "\n");
        while (line != NULL) {
            if (strcmp(line, "END_OF_FILE") == 0) break;
            fwrite(line, 1, strlen(line), file);
            fwrite("\n", 1, 1, file);
            line = strtok(NULL, "\n");
        }

        if (line != NULL && strcmp(line, "END_OF_FILE") == 0) break;
    }

    fclose(file);
    printf("File %s uploaded successfully.\n", filename);
    send(client_sock, "Upload complete", strlen("Upload complete"), 0);
}

// 下载文件
void download_file(int client_sock, const char *filename) {
    char filepath[BUFFER_SIZE];
    sprintf(filepath, "%s/%s", FILE_DIRECTORY, filename);

    FILE *file = fopen(filepath, "rb");
    if (file == NULL) {
        perror("File open failed");
        send(client_sock, "ERROR: File not found\n", 22, 0);
        return;
    }

    char buffer[BUFFER_SIZE];

    while (fgets(buffer, BUFFER_SIZE, file) != NULL) {
        send(client_sock, buffer, strlen(buffer), 0);
    }

    send(client_sock, "END_OF_FILE\n", strlen("END_OF_FILE\n"), 0);
    fclose(file);
    printf("File %s downloaded successfully.\n", filename);
}

// 删除文件
void delete_file(int client_sock, const char *filename) {
    char filepath[BUFFER_SIZE];
    sprintf(filepath, "%s/%s", FILE_DIRECTORY, filename);

    if (remove(filepath) == 0) {
        send(client_sock, "File deleted successfully\n", 26, 0);
        printf("File %s deleted successfully.\n", filename);
    } else {
        send(client_sock, "ERROR: File deletion failed\n", 28, 0);
        perror("File deletion failed");
    }
}

// 列出服务器端文件
void list_files(int client_sock) {
    DIR *d;
    struct dirent *dir;
    char buffer[BUFFER_SIZE];

    d = opendir(FILE_DIRECTORY);
    if (d) {
        while ((dir = readdir(d)) != NULL) {
            if (dir->d_type == DT_REG) {
                snprintf(buffer, BUFFER_SIZE, "%s\n", dir->d_name);
                send(client_sock, buffer, strlen(buffer), 0);
            }
        }
        closedir(d);
    }

    send(client_sock, "END_OF_FILE", strlen("END_OF_FILE"), 0);
}

