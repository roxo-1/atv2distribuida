#ifndef JOGO_H
#define JOGO_H

#include "protocolo.h"

// Estruturas
typedef struct {
    char nome[NOME_SIZE];
    int fd;
    int pontos;
    char palavra_atual[BUFFER_SIZE];
} jogador;

typedef struct {
    int max_joga = MAX_JOGADORES;
    int rodada;
    char letra_atual;
} partida;


// validações

char sortear_letra(void);

int validar_palavra(const char *palavra, char letra);

int enviar_msg(int fd, const char *tipo, const char *conteudo);

int receber_msg(int fd, char *tipo, char *conteudo, size_t tam);

int receber_com_timeout(int fd, char *buffer, size_t tam, int segundos);
