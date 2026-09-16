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

    srand(time(NULL));
    
    char letra = 'A' + (rand() % 26);
    
    printf("Letra aleatoria: %c\n", letra);
}


int validar_palavra(const char *palavra, char letra) {
    
    if (lenght(*palavra) < 4) {
        printf("Palavra menor que 5 letras");
        return 0;
    } 
    
    toupper(*palavra);
    char primeira_letra = *palavra;
    
    if (primeira_letra != letra) {
        printf("Palavra não começa com a letra escolhida");
        return 0;
    } 
    if (isalpha(*palavra)) {
        printf("Palavra contém apenas letras");
        return 1;
    }
    printf("Palavra não contém apenas letras");
    return 0;

}


int enviar_msg(int fd, const char *tipo, const char *conteudo) {
    char buffer[BUFFER_SIZE];
    
    snprintf(buffer, sizeof(buffer), "%s|%s\n", tipo, conteudo);
    
    int resultado = send(fd, buffer, strlen(buffer), 0);
    
    if (resultado == -1) {
        perror("Erro ao enviar mensagem");
    }
   
    return resultado;
}

int receber_msg(int fd, char *tipo, char *conteudo, size_t tam){
    char buffer_recebe_msg[BUFFER_SIZE];
    char caractere = '|';
    int valor = recv(fd, buffer_recebe_msg, tam, 0);
    if(valor == 0){
        return 0;//conexão encerrada pelo outro lado 
    } 
    else if(valor < 0){
        printf("erro jogo.c receber msd");
        return -1;//deu merda
    }
    else{
        printf("deu certo, garchomp"); //deu certo
    }
    char *ocorrencia =strchr(buffer_recebe_msg, caractere);
    if(ocorrencia != NULL){
        *ocorrencia ='\0';
        char *primeira_parte = buffer_recebe_msg;
        char *resto = ocorrencia+1;
        printf("primeiro: %s\n", primeira_parte);
        printf("resto: %s\n", resto);
        return valor;
    } else{
        printf("messagem não está no formato certo >:(");
        return -1;
    }
    

}

int receber_com_timeout(int fd, char *buffer, size_t tam, int segundos){
    fd_set conjunto;
    struct timeval timeout;
    int resultado_select;

    FD_ZERO(&conjunto);
    FD_SET(fd, &conjunto);

    timeout.tv_sec = segundos;
    timeout.tv_usec = 0;

    while (resultado_select == -1) {
        resultado_select = select(fd + 1, &conjunto, NULL, NULL, &timeout);
    }

    if (resultado_select == -1) {
        printf("Erro no select");
        return -1; 
    }

    if (resultado_select == 0) {
        return -2; 
    }

    if (FD_ISSET(fd, &conjunto)) {
        size_t bytes_recebidos = recv(fd, buffer, tam, 0);
        
        if (bytes_recebidos == -1) {
            printf("Erro no recv");
            return -1;
        }
        
        return (int)bytes_recebidos; 
    }

    return -1;
}