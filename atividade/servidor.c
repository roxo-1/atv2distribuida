#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include "protocolo.h"
#include "jogo.h"
#include "jogo.c"

//foi reutilizado parte do código fornecido pelo professor :/ 

#define PORTA 9090

typedef struct {
    int   fd;                /* file descriptor do socket */
    char  nome[NOME_SIZE];   /* nome do usuário */
    char  ip[INET_ADDRSTRLEN]; /* endereço IP */
    int   porta;             /* porta do cliente */
} Cliente;

/* Lista global de clientes conectados */
static Cliente clientes[MAX_JOGADORES];
static int num_clientes = 0;

void configurar_sinais(void){}
void tratar_sigint(int sig){}
int inicializar_servidor(int porta){}
void* tratar_partida(void *arg){}
int executar_rodada(int rodada_num, partida *partida){}


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

int main(int argc, char *argv[]){
    int server_fd;
    struct sockaddr_in servidor_addr;
    int opt = 1;

    /* -------------------------------------------------------
     Criar o socket do servidor
     * ------------------------------------------------------- */
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("Erro ao criar socket");
        exit(EXIT_FAILURE);
    }

    /* Permite reutilizar a porta imediatamente */
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    /* -------------------------------------------------------
     * PASSO 2: Bind + Listen
     * ------------------------------------------------------- */
    memset(&servidor_addr, 0, sizeof(servidor_addr));
    servidor_addr.sin_family = AF_INET;
    servidor_addr.sin_addr.s_addr = INADDR_ANY;
    servidor_addr.sin_port = htons(PORTA);

    if (bind(server_fd, (struct sockaddr *)&servidor_addr, sizeof(servidor_addr)) == -1) {
        printf("Erro no bind");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 10) == -1) {
        printf("Erro no listen");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("========================================\n");
    printf("   SERVIDOR DE JOGO - Porta %d\n", PORTA);
    printf("   Aguardando conexões...\n");
    printf("   Max clientes: %d\n", MAX_JOGADORES);
    printf("========================================\n\n");

    /* -------------------------------------------------------
     * Loop principal com select()
     *
     * A cada iteração:
     *   1. Montamos o fd_set com server_fd + todos os clientes
     *   2. Chamamos select() — bloqueia até alguma atividade
     *   3. Verificamos QUEM tem atividade e tratamos
     * ------------------------------------------------------- */
    while (1) {
        fd_set read_fds;  /* conjunto de fds para monitorar */
        int    max_fd;    /* maior fd (necessário para select) */

        /*
         * Reconstruir o fd_set a cada iteração.
         * select() MODIFICA o fd_set — só os fds prontos ficam marcados.
         * Por isso, precisamos montar novamente.
         */
        FD_ZERO(&read_fds);
        FD_SET(server_fd, &read_fds);
        max_fd = server_fd;

        /* Adiciona todos os clientes conectados ao conjunto */
        for (int i = 0; i < num_clientes; i++) {
            FD_SET(clientes[i].fd, &read_fds);
            if (clientes[i].fd > max_fd) {
                max_fd = clientes[i].fd;
            }
        }

        /*
         * select() bloqueia aqui até que:
         *   - Um novo cliente tente conectar (server_fd pronto)
         *   - Um cliente conectado envie dados (cliente_fd pronto)
         *   - Um cliente desconecte (cliente_fd pronto com 0 bytes)
         */
        int atividade = select(max_fd + 1, &read_fds, NULL, NULL, NULL);
        if (atividade == -1) {
            /* EINTR acontece se um sinal interromper o select — normalmente
               podemos simplesmente continuar o loop */
            if (errno == EINTR) continue;
            perror("Erro no select");
            break;
        }

        /* -------------------------------------------------------
         * CASO 1: Atividade no server_fd → nova conexão!
         * ------------------------------------------------------- */
        if (FD_ISSET(server_fd, &read_fds)) {
            struct sockaddr_in cliente_addr;
            socklen_t cliente_len = sizeof(cliente_addr);

            int novo_fd = accept(server_fd, (struct sockaddr *)&cliente_addr, &cliente_len);
            if (novo_fd == -1) {
                perror("Erro no accept");
                continue;   /* não encerra o servidor por causa de um erro */
            }

            char ip_str[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &cliente_addr.sin_addr, ip_str, sizeof(ip_str));
            int porta_cliente = ntohs(cliente_addr.sin_port);

            printf("[+] Nova conexão: %s:%d (fd=%d)\n", ip_str, porta_cliente, novo_fd);

            /*
             * O primeiro dado que o cliente envia é o nome do usuário.
             * Esperamos recebê-lo logo após a conexão.
             */
            char nome[NOME_SIZE] = {0};
            ssize_t n = recv(novo_fd, nome, NOME_SIZE - 1, 0);
            if (n <= 0) {
                printf("[-] Cliente desconectou antes de informar nome\n");
                close(novo_fd);
                continue;
            }
            nome[n] = '\0';

            /* Remove possível \n do nome */
            char *newline = strchr(nome, '\n');
            if (newline) *newline = '\0';

            /* Tenta adicionar à lista */
            if (adicionar_cliente(novo_fd, nome, ip_str, porta_cliente) == -1) {
                const char *msg_cheio = "Servidor cheio. Tente mais tarde.\n";
                send(novo_fd, msg_cheio, strlen(msg_cheio), 0);
                close(novo_fd);
                printf("[!] Servidor cheio, conexão recusada\n");
                continue;
            }

            printf("[+] \"%s\" entrou no chat (%d clientes online)\n",
                   nome, num_clientes);

            /* Notifica todos os outros */
            char aviso[BUFFER_SIZE];
            snprintf(aviso, sizeof(aviso),
                     ">>> %.*s entrou no chat (%d online) <<<\n",
                     NOME_SIZE - 1, nome, num_clientes);
            broadcast(aviso, novo_fd);

            /* Mensagem de boas-vindas para o novo cliente */
            char bemvindo[BUFFER_SIZE];
            snprintf(bemvindo, sizeof(bemvindo),
                     ">>> Bem-vindo ao chat, %.*s! (%d online) <<<\n",
                     NOME_SIZE - 1, nome, num_clientes);
            send(novo_fd, bemvindo, strlen(bemvindo), 0);
        }

        /* -------------------------------------------------------
         * CASO 2: Atividade em algum cliente → mensagem ou desconexão
         *
         * Percorremos a lista de trás para frente (i--) para
         * evitar problemas ao remover clientes durante a iteração.
         * ------------------------------------------------------- */
        for (int i = num_clientes - 1; i >= 0; i--) {
            int cli_fd = clientes[i].fd;

            if (!FD_ISSET(cli_fd, &read_fds)) {
                continue;   /* este cliente não tem atividade */
            }

            char buffer[BUFFER_SIZE] = {0};
            ssize_t bytes = recv(cli_fd, buffer, BUFFER_SIZE - 1, 0);

            if (bytes <= 0) {
                /* ----- Cliente desconectou ----- */
                char nome_saiu[NOME_SIZE];
                remover_cliente(cli_fd, nome_saiu, sizeof(nome_saiu));
                close(cli_fd);

                printf("[-] \"%s\" saiu do chat (%d online)\n",
                       nome_saiu, num_clientes);

                char aviso[BUFFER_SIZE];
                snprintf(aviso, sizeof(aviso),
                         ">>> %.*s saiu do chat (%d online) <<<\n",
                         NOME_SIZE - 1, nome_saiu, num_clientes);
                broadcast(aviso, -1);  /* -1 = envia para todos */

            } else {
                /* ----- Mensagem recebida ----- */
                buffer[bytes] = '\0';

                /* Remove \n final se existir */
                char *newline = strchr(buffer, '\n');
                if (newline) *newline = '\0';

                /* Verifica se a mensagem não está vazia após trim */
                if (strlen(buffer) == 0) continue;

                const char *nome_remetente = nome_por_fd(cli_fd);
                printf("[%s]: %s\n", nome_remetente, buffer);

                /* Formata "[Nome]: mensagem" e envia para todos os outros */
                char msg_formatada[BUFFER_SIZE];
                snprintf(msg_formatada, sizeof(msg_formatada),
                         "[%.*s]: %.*s\n",
                         NOME_SIZE - 1, nome_remetente,
                         BUFFER_SIZE - NOME_SIZE - 5, buffer);
                broadcast(msg_formatada, cli_fd);
            }
        }
    }

    /* -------------------------------------------------------
     * Limpeza final (só chega aqui se o loop for interrompido)
     * ------------------------------------------------------- */
    for (int i = 0; i < num_clientes; i++) {
        close(clientes[i].fd);
    }
    close(server_fd);
    printf("\n[SERVIDOR] Encerrado.\n");

    return 0;
}