#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "protocolo.h"
#include "jogo.h"
#include "jogo.c"

int servidor_rodando = 1;
int socket_servidor = -1;
int socket_esperando = -1;
char nome_do_esperando[NOME_SIZE];
pthread_mutex_t lock_fila = PTHREAD_MUTEX_INITIALIZER;
int id_partida_global = 1;

typedef struct {
    jogador j1;
    jogador j2;
    int id;
} PartidaInfo;

typedef struct {
    int socket_cliente;
} ConexaoInfo;

void manda(jogador *j, const char *tipo, const char *msg) {
    enviar_msg(j->fd, tipo, msg);
}

int pegar_nome(int sock, char *nome_buffer) {
    char tipo[BUFFER_SIZE];
    char conteudo[BUFFER_SIZE];

    enviar_msg(sock, PROTO_NOME, "");

    int lido = receber_msg(sock, tipo, conteudo, BUFFER_SIZE);
    if (lido <= 0) {
        return -1; // deu merda na conexão
    }

    if (strlen(conteudo) == 0) {
        strcpy(nome_buffer, "JogadorAnonimo");
    } else {
        strcpy(nome_buffer, conteudo);
    }
    return 0;
}

void *rodar_partida(void *arg) {
    PartidaInfo *dados = (PartidaInfo *)arg;
    jogador player1 = dados->j1;
    jogador player2 = dados->j2;
    int id = dados->id;
    free(dados);
    printf("[PARTIDA %d INICIADA] %s contra %s\n", id, player1.nome, player2.nome);
    char boas_vindas[BUFFER_SIZE];
    sprintf(boas_vindas, "Partida iniciada! %s vs %s. Boa sorte!", player1.nome, player2.nome);
    manda(&player1, PROTO_MSG, boas_vindas);
    manda(&player2, PROTO_MSG, boas_vindas);

    for (int r = 1; r <= TOTAL_RODADAS; r++) {
        char letra = sortear_letra();
        
        char msg_rodada[BUFFER_SIZE];
        sprintf(msg_rodada, "%d|%c|%d", r, letra, TEMPO_LIMITE);
        manda(&player1, PROTO_RODADA, msg_rodada);
        manda(&player2, PROTO_RODADA, msg_rodada);

        printf("Rodada %d rodando! Letra da vez: %c\n", r, letra);

        char palavra1[BUFFER_SIZE] = "";
        char palavra2[BUFFER_SIZE] = "";
        
        char tipo[BUFFER_SIZE];
        
        receber_msg(player1.fd, tipo, palavra1, BUFFER_SIZE);
        receber_msg(player2.fd, tipo, palavra2, BUFFER_SIZE);

        int ok1 = validar_palavra(palavra1, letra);
        int ok2 = validar_palavra(palavra2, letra);

        if (ok1 && ok2 && strcasecmp(palavra1, palavra2) == 0) {
            ok1 = 0;
            ok2 = 0;
        }

        if (ok1) player1.pontos++;
        if (ok2) player2.pontos++;

        char resp1[BUFFER_SIZE], resp2[BUFFER_SIZE];
        sprintf(resp1, "Resultado: vc mandou %s (%s). Oponente: %s", palavra1, ok1 ? "valida" : "invalida", palavra2);
        sprintf(resp2, "Resultado: vc mandou %s (%s). Oponente: %s", palavra2, ok2 ? "valida" : "invalida", palavra1);

        manda(&player1, PROTO_RESULTADO, resp1);
        manda(&player2, PROTO_RESULTADO, resp2);

        char placar1[BUFFER_SIZE], placar2[BUFFER_SIZE];
        sprintf(placar1, "%s|%d|%s|%d", player1.nome, player1.pontos, player2.nome, player2.pontos);
        sprintf(placar2, "%s|%d|%s|%d", player2.nome, player2.pontos, player1.nome, player1.pontos);
        manda(&player1, PROTO_PLACAR, placar1);
        manda(&player2, PROTO_PLACAR, placar2);
    }

    char final[BUFFER_SIZE];
    if (player1.pontos > player2.pontos) {
        sprintf(final, "Vitoria de %s! Placar: %d x %d", player1.nome, player1.pontos, player2.pontos);
    } else if (player2.pontos > player1.pontos) {
        sprintf(final, "Vitoria de %s! Placar: %d x %d", player2.nome, player2.pontos, player1.pontos);
    } else {
        sprintf(final, "Empate! Placar: %d x %d", player1.pontos, player2.pontos);
    }

    manda(&player1, PROTO_FIM, final);
    manda(&player2, PROTO_FIM, final);

    close(player1.fd);
    close(player2.fd);

    printf("[PARTIDA %d FINALIZADA]\n", id);
    return NULL;
}

void *esperar_jogador(void *arg) {
    ConexaoInfo *info = (ConexaoInfo *)arg;
    int sock = info->socket_cliente;
    free(info);

    char nome[NOME_SIZE];
    if (pegar_nome(sock, nome) != 0) {
        printf("Erro ao pegar nome do socket %d, fechando...\n", sock);
        close(sock);
        return NULL;
    }

    pthread_mutex_lock(&lock_fila);

    if (socket_esperando == -1) {
        socket_esperando = sock;
        strcpy(nome_do_esperando, nome);
        pthread_mutex_unlock(&lock_fila);

        printf("%s ta esperando outro jogador entrar...\n", nome);
        enviar_msg(sock, PROTO_AGUARDE, "Aguarde outro jogador conectar...");
    } else {
        PartidaInfo *partida = malloc(sizeof(PartidaInfo));

        partida->j1.fd = socket_esperando;
        strcpy(partida->j1.nome, nome_do_esperando);
        partida->j1.pontos = 0;

        partida->j2.fd = sock;
        strcpy(partida->j2.nome, nome);
        partida->j2.pontos = 0;

        partida->id = id_partida_global++;

        socket_esperando = -1;
        pthread_mutex_unlock(&lock_fila);

        pthread_t t_partida;
        pthread_create(&t_partida, NULL, rodar_partida, partida);
        pthread_detach(t_partida);
    }

    return NULL;
}

int main(int argc, char *argv[]) {
    int porta = 7070; // porta padrao de teste
    if (argc > 1) {
        porta = atoi(argv[1]);
    }

    srand(time(NULL));

    // 1. Cria socket
    socket_servidor = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_servidor < 0) {
        printf("Erro no socket!\n");
        return 1;
    }

    int opt = 1;
    setsockopt(socket_servidor, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // 2. Configura endereco
    struct sockaddr_in servidor;
    servidor.sin_family = AF_INET;
    servidor.sin_addr.s_addr = INADDR_ANY;
    servidor.sin_port = htons(porta);

    // 3. Bind
    if (bind(socket_servidor, (struct sockaddr *)&servidor, sizeof(servidor)) < 0) {
        printf("Erro no bind! Tente outra porta.\n");
        return 1;
    }

    // 4. Listen
    listen(socket_servidor, 5);

    printf("Servidor rodando na porta %d! Esperando conexoes...\n", porta);

    while (servidor_rodando) {
        struct sockaddr_in cliente;
        socklen_t tamanho = sizeof(cliente);

        int cliente_sock = accept(socket_servidor, (struct sockaddr *)&cliente, &tamanho);
        if (cliente_sock < 0) {
            printf("Erro no accept!\n");
            continue;
        }

        printf("Nova conexao aceita no socket %d\n", cliente_sock);

        ConexaoInfo *info = malloc(sizeof(ConexaoInfo));
        info->socket_cliente = cliente_sock;

        pthread_t t_cliente;
        pthread_create(&t_cliente, NULL, esperar_jogador, info);
        pthread_detach(t_cliente);
    }

    close(socket_servidor);
    return 0;
}
