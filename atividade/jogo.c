#include "protocolo.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <errno.h>

char sortear_letra(void) {
    char letra = 'A' + (rand() % 26);
    printf("Letra sorteada: %c\n", letra);
    return letra;
}

int validar_palavra(const char *palavra, char letra) {
    if (palavra == NULL) return 0;

    size_t tam = strlen(palavra);
    if (tam < 4) {
        printf("Palavra com menos de 4 letras\n");
        return 0;
    }

    // Compara a primeira letra ignorando maiúscula/minúscula
    if (toupper((unsigned char)palavra[0]) != toupper((unsigned char)letra)) {
        printf("Palavra não começa com a letra escolhida\n");
        return 0;
    }

    // Valida se todos os caracteres são letras
    for (size_t i = 0; i < tam; i++) {
        if (!isalpha((unsigned char)palavra[i])) {
            printf("Palavra contém caracteres não alfabéticos\n");
            return 0;
        }
    }

    return 1;
}

int enviar_msg(int fd, const char *tipo, const char *conteudo) {
    char buffer[BUFFER_SIZE];
    
    snprintf(buffer, sizeof(buffer), "%s|%s\n", tipo, conteudo);
    
    ssize_t resultado = send(fd, buffer, strlen(buffer), 0);
    if (resultado == -1) {
        perror("Erro ao enviar mensagem");
    }
    
    return (int)resultado;
}

int receber_msg(int fd, char *tipo, char *conteudo, size_t tam) {
    char buffer_recebe_msg[BUFFER_SIZE];
    
    ssize_t valor = recv(fd, buffer_recebe_msg, sizeof(buffer_recebe_msg) - 1, 0);
    if (valor == 0) {
        return 0; // Conexão encerrada
    } 
    if (valor < 0) {
        perror("Erro no recv em receber_msg");
        return -1;
    }

    buffer_recebe_msg[valor] = '\0';

    // Remove eventual '\n' ou '\r' no final
    buffer_recebe_msg[strcspn(buffer_recebe_msg, "\r\n")] = '\0';

    char *ocorrencia = strchr(buffer_recebe_msg, '|');
    if (ocorrencia != NULL) {
        *ocorrencia = '\0';
        char *primeira_parte = buffer_recebe_msg;
        char *resto = ocorrencia + 1;

        if (tipo) {
            strncpy(tipo, primeira_parte, tam - 1);
            tipo[tam - 1] = '\0';
        }
        if (conteudo) {
            strncpy(conteudo, resto, tam - 1);
            conteudo[tam - 1] = '\0';
        }

        return (int)valor;
    }

    printf("Mensagem fora do formato TIPO|CONTEUDO\n");
    return -1;
}

int receber_com_timeout(int fd, char *buffer, size_t tam, int segundos) {
    fd_set conjunto;
    struct timeval timeout;
    int resultado_select;

    FD_ZERO(&conjunto);
    FD_SET(fd, &conjunto);

    timeout.tv_sec = segundos;
    timeout.tv_usec = 0;

    do {
        resultado_select = select(fd + 1, &conjunto, NULL, NULL, &timeout);
    } while (resultado_select == -1 && errno == EINTR); // Repete apenas se for interrompido por sinal

    if (resultado_select == -1) {
        perror("Erro no select");
        return -1; 
    }

    if (resultado_select == 0) {
        return -2; // Timeout estourado
    }

    if (FD_ISSET(fd, &conjunto)) {
        ssize_t bytes_recebidos = recv(fd, buffer, tam - 1, 0);
        
        if (bytes_recebidos <= 0) {
            return (int)bytes_recebidos; // 0 = desconectou, -1 = erro
        }
        
        buffer[bytes_recebidos] = '\0';
        buffer[strcspn(buffer, "\r\n")] = '\0';
        
        return (int)bytes_recebidos; 
    }

    return -1;
}