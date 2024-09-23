#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <sys/stat.h>

#define BUFFER_SIZE 1024

// 函数声明
void upload_file(int sock, const char *filename);
void download_file(int sock, const char *filename);
void delete_file(int sock, const char *filename);
void list_files(int sock);
void update_local(int sock);
void update_server(int sock);
void list_local_files(char local_files[][BUFFER_SIZE], int *file_count);

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Usage: %s <server_ip> <port>\n", argv[0]);
        return -1;
    }

    int client_sock;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];
    char command[30], filename[100];
    int port = atoi(argv[2]);  // 从命令行获取端口号

    client_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (client_sock < 0) {
        perror("Socket creation failed");
        return -1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);  // 使用自定义端口号
    server_addr.sin_addr.s_addr = inet_addr(argv[1]);

    if (connect(client_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        return -1;
    }

    printf("Connected to server at %s on port %d\n", argv[1], port);

    while (1) {
        printf("Enter command (upload, download, delete, list, update_local, update_server, exit): ");
        fgets(command, sizeof(command), stdin);

        if (strncmp(command, "upload", 6) == 0) {
            char *filename_start = strchr(command, ' ');
            if (filename_start != NULL) {
                filename_start++;
                char *newline = strchr(filename_start, '\n');
                if (newline != NULL) *newline = '\0';
                upload_file(client_sock, filename_start);
            } else {
                printf("Please provide a valid filename for upload.\n");
            }
        } else if (strncmp(command, "download", 8) == 0) {
            char *filename_start = strchr(command, ' ');
            if (filename_start != NULL) {
                filename_start++;
                char *newline = strchr(filename_start, '\n');
                if (newline != NULL) *newline = '\0';
                download_file(client_sock, filename_start);
            } else {
                printf("Please provide a valid filename for download.\n");
            }
        } else if (strncmp(command, "delete", 6) == 0) {
            char *filename_start = strchr(command, ' ');
            if (filename_start != NULL) {
                filename_start++;
                char *newline = strchr(filename_start, '\n');
                if (newline != NULL) *newline = '\0';
                delete_file(client_sock, filename_start);
            } else {
                printf("Please provide a valid filename for delete.\n");
            }
        } else if (strncmp(command, "list", 4) == 0) {
            list_files(client_sock);
        } else if (strncmp(command, "update_local", 12) == 0) {
            update_local(client_sock);
        } else if (strncmp(command, "update_server", 13) == 0) {
            update_server(client_sock);
        } else if (strncmp(command, "exit", 4) == 0) {
            break;
        } else {
            printf("Invalid command!\n");
        }
    }

    close(client_sock);
    return 0;
}

// 上传文件
void upload_file(int sock, const char *filename) {
    FILE *file = fopen(filename, "rb");
    if (file == NULL) {
        printf("File %s not found!\n", filename);
        return;
    }

    char buffer[BUFFER_SIZE];
    sprintf(buffer, "upload %s", filename);
    send(sock, buffer, strlen(buffer), 0);

    while (fgets(buffer, BUFFER_SIZE, file) != NULL) {
        send(sock, buffer, strlen(buffer), 0);
    }

    send(sock, "END_OF_FILE\n", strlen("END_OF_FILE\n"), 0);
    fclose(file);
    recv(sock, buffer, BUFFER_SIZE, 0);
    printf("%s\n", buffer);
}

// 下载文件
void download_file(int sock, const char *filename) {
    char buffer[BUFFER_SIZE];
    sprintf(buffer, "download %s", filename);
    send(sock, buffer, strlen(buffer), 0);

    FILE *file = fopen(filename, "wb");
    if (file == NULL) {
        printf("Failed to create file %s\n", filename);
        return;
    }

    char *line;
    while (1) {
        int n = recv(sock, buffer, BUFFER_SIZE, 0);
        if (n <= 0) {
            printf("Error receiving data or connection closed.\n");
            break;
        }
        buffer[n] = '\0';

        line = strtok(buffer, "\n");
        while (line != NULL) {
            if (strcmp(line, "END_OF_FILE") == 0) {
                break;
            }
            fwrite(line, 1, strlen(line), file);
            fwrite("\n", 1, 1, file);
            line = strtok(NULL, "\n");
        }
        if (line != NULL && strcmp(line, "END_OF_FILE") == 0) {
            break;
        }
    }

    fclose(file);
    printf("File %s downloaded successfully.\n", filename);
}

// 删除文件
void delete_file(int sock, const char *filename) {
    char buffer[BUFFER_SIZE];
    sprintf(buffer, "delete %s", filename);
    send(sock, buffer, strlen(buffer), 0);
    int n = recv(sock, buffer, BUFFER_SIZE, 0);
    if (n > 0) {
        buffer[n] = '\0';
        printf("%s\n", buffer);
    }
}

// 列出文件
void list_files(int sock) {
    char buffer[BUFFER_SIZE];
    char *line;

    strcpy(buffer, "list");
    send(sock, buffer, strlen(buffer), 0);

    printf("Server file list:\n");
    while (1) {
        int n = recv(sock, buffer, BUFFER_SIZE, 0);
        if (n <= 0) {
            printf("Error receiving data or connection closed.\n");
            break;
        }
        buffer[n] = '\0';

        line = strtok(buffer, "\n");
        while (line != NULL) {
            if (strcmp(line, "END_OF_FILE") == 0) {
                break;
            }
            printf("%s\n", line);
            line = strtok(NULL, "\n");
        }
        if (line != NULL && strcmp(line, "END_OF_FILE") == 0) {
            break;
        }
    }
    printf("File list received.\n");
}

// 列出本地文件
void list_local_files(char local_files[][BUFFER_SIZE], int *file_count) {
    DIR *d;
    struct dirent *dir;
    char buffer[BUFFER_SIZE];

    char exe_path[BUFFER_SIZE];
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    if (len != -1) {
        exe_path[len] = '\0';
    }
    char *exe_name = strrchr(exe_path, '/');
    if (exe_name != NULL) {
        exe_name++;
    }

    d = opendir(".");
    if (d) {
        while ((dir = readdir(d)) != NULL) {
            if (dir->d_type == DT_REG && strcmp(dir->d_name, exe_name) != 0) {
                strcpy(local_files[*file_count], dir->d_name);
                (*file_count)++;
            }
        }
        closedir(d);
    }
}

// 同步服务器文件到本地
void update_local(int sock) {
    char server_files[100][BUFFER_SIZE];
    char local_files[100][BUFFER_SIZE];
    int server_file_count = 0, local_file_count = 0;
    char buffer[BUFFER_SIZE];

    strcpy(buffer, "list");
    send(sock, buffer, strlen(buffer), 0);

    while (1) {
        int n = recv(sock, buffer, BUFFER_SIZE, 0);
        if (n <= 0) {
            printf("Error receiving data or connection closed.\n");
            break;
        }
        buffer[n] = '\0';
        char *line = strtok(buffer, "\n");
        while (line != NULL) {
            if (strcmp(line, "END_OF_FILE") == 0) {
                break;
            }
            strcpy(server_files[server_file_count], line);
            server_file_count++;
            line = strtok(NULL, "\n");
        }
        if (line != NULL && strcmp(line, "END_OF_FILE") == 0) {
            break;
        }
    }

    list_local_files(local_files, &local_file_count);

    for (int i = 0; i < server_file_count; i++) {
        int found = 0;
        for (int j = 0; j < local_file_count; j++) {
            if (strcmp(server_files[i], local_files[j]) == 0) {
                found = 1;
                break;
            }
        }
        if (!found) {
            printf("Downloading %s from server...\n", server_files[i]);
            download_file(sock, server_files[i]);
        }
    }

    for (int i = 0; i < local_file_count; i++) {
        int found = 0;
        for (int j = 0; j < server_file_count; j++) {
            if (strcmp(local_files[i], server_files[j]) == 0) {
                found = 1;
                break;
            }
        }
        if (!found) {
            printf("Deleting local file: %s\n", local_files[i]);
            remove(local_files[i]);
        }
    }
}

// 同步本地文件到服务器
void update_server(int sock) {
    char server_files[100][BUFFER_SIZE];
    char local_files[100][BUFFER_SIZE];
    int server_file_count = 0, local_file_count = 0;
    char buffer[BUFFER_SIZE];

    strcpy(buffer, "list");
    send(sock, buffer, strlen(buffer), 0);

    while (1) {
        int n = recv(sock, buffer, BUFFER_SIZE, 0);
        if (n <= 0) {
            printf("Error receiving data or connection closed.\n");
            break;
        }
        buffer[n] = '\0';
        char *line = strtok(buffer, "\n");
        while (line != NULL) {
            if (strcmp(line, "END_OF_FILE") == 0) {
                break;
            }
            strcpy(server_files[server_file_count], line);
            server_file_count++;
            line = strtok(NULL, "\n");
        }
        if (line != NULL && strcmp(line, "END_OF_FILE") == 0) {
            break;
        }
    }

    list_local_files(local_files, &local_file_count);

    for (int i = 0; i < local_file_count; i++) {
        int found = 0;
        for (int j = 0; j < server_file_count; j++) {
            if (strcmp(local_files[i], server_files[j]) == 0) {
                found = 1;
                break;
            }
        }
        if (!found) {
            printf("Uploading %s to server...\n", local_files[i]);
            upload_file(sock, local_files[i]);
        }
    }

    for (int i = 0; i < server_file_count; i++) {
        int found = 0;
        for (int j = 0; j < local_file_count; j++) {
            if (strcmp(server_files[i], local_files[j]) == 0) {
                found = 1;
                break;
            }
        }
        if (!found) {
            printf("Deleting server file: %s\n", server_files[i]);
            delete_file(sock, server_files[i]);
        }
    }
}

