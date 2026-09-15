#include "protocolo.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>


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
    } else (isalpha(palavra)) {
        printf("Palavra contém apenas letras");
        return 1;
    }
    printf("Palavra não contém apenas letras");
    return 0;

}


int enviar_msg(int fd, const char *tipo, const char *conteudo) {
    char buffer[TAM_BUFFER];
    
    snprintf(buffer, sizeof(buffer), "%s|%s\n", tipo, conteudo);
    
    int resultado = send(fd, buffer, strlen(buffer), 0);
    
    if (resultado == -1) {
        perror("Erro ao enviar mensagem");
    }
   
    return resultado;
}

int receber_msg(int fd, char *tipo, char *conteudo, size_t tam);

int receber_com_timeout(int fd, char *buffer, size_t tam, int segundos);