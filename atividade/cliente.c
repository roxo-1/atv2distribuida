#include "protocolo.h"
#include "jogo.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h> 

#define PORTA         9090
#define SERVIDOR_IP   "127.0.0.1"

#define MAX_CAMPOS 8

typedef struct {
    int    esperando_nome;
    int    esperando_palavra;
    time_t fim_da_rodada;
    int    jogando;
} estado_cliente;

static volatile sig_atomic_t encerrar = 0;

static void tratar_sigint(int sinal)
{
    (void)sinal;
    encerrar = 1;
}

static void remover_nova_linha(char *s)
{
    char *nl = strchr(s, '\n');
    if (nl) *nl = '\0';
    nl = strchr(s, '\r');
    if (nl) *nl = '\0';
}

static int dividir_campos(char *linha, char *campos[], int max)
{
    int n = 0;
    char *p = linha;

    campos[n++] = p;
    while (n < max && (p = strchr(p, '|')) != NULL) {
        *p = '\0';
        p++;
        campos[n++] = p;
    }
    return n;
}

static void processar_mensagem(char *linha, estado_cliente *est)
{
    char *campos[MAX_CAMPOS];
    char  moldura[BUFFER_SIZE];
    int   n = dividir_campos(linha, campos, MAX_CAMPOS);

    if (n < 1 || campos[0][0] == '\0') return;

    const char *tipo     = campos[0];
    const char *conteudo = (n > 1) ? campos[1] : "";

    if (strcmp(tipo, PROTO_NOME) == 0) {
        est->esperando_nome = 1;
        printf("\n  Digite seu nome: ");
        fflush(stdout);

    } else if (strcmp(tipo, PROTO_MSG) == 0) {
        printf("\n   %s\n", conteudo);

    } else if (strcmp(tipo, PROTO_AGUARDE) == 0) {
        printf("\n   %s\n", conteudo);

    } else if (strcmp(tipo, PROTO_RODADA) == 0) {
        int  num   = (n > 1) ? atoi(campos[1]) : 0;
        char letra = (n > 2) ? campos[2][0]    : '?';
        int  tempo = (n > 3) ? atoi(campos[3]) : TEMPO_LIMITE;

        printf("\n  ╔══════════════════════════════════╗\n");

        snprintf(moldura, sizeof(moldura), "        RODADA %d de %d", num, TOTAL_RODADAS);
        printf("  ║%-34s║\n", moldura);

        snprintf(moldura, sizeof(moldura), "  Letra: [%c]   Tempo: %d seg", letra, tempo);
        printf("  ║%-34s║\n", moldura);

        snprintf(moldura, sizeof(moldura), "  Mínimo: %d caracteres", MIN_CARACTERES);
        printf("  ║%-35s║\n", moldura);

        printf("  ╚══════════════════════════════════╝\n");
        printf("  Sua palavra: ");
        fflush(stdout);

        est->esperando_palavra = 1;
        est->fim_da_rodada     = time(NULL) + tempo;

    } else if (strcmp(tipo, PROTO_RESULTADO) == 0) {
        est->esperando_palavra = 0;
        printf("   %s\n", conteudo);

    } else if (strcmp(tipo, PROTO_PLACAR) == 0) {
        if (n >= 5) {
            snprintf(moldura, sizeof(moldura), "  PLACAR: %s %s  x  %s %s",
                     campos[1], campos[2], campos[4], campos[3]);
            printf("  ┌───────────────────────────────────┐\n");
            printf("  │%-35s│\n", moldura);
            printf("  └───────────────────────────────────┘\n");
        }

    } else if (strcmp(tipo, PROTO_FIM) == 0) {
        printf("\n  ╔══════════════════════════════════╗\n");
        printf("  ║           FIM DE JOGO            ║\n");
        printf("  ╚══════════════════════════════════╝\n");
        printf("   %s\n", conteudo);
        est->esperando_palavra = 0;
        est->jogando           = 0;

    } else {
        printf("\n   [?] %s|%s\n", tipo, conteudo);
    }
}

int main(int argc, char *argv[])
{
    int  sock_fd;
    struct sockaddr_in servidor_addr;

    const char *ip    = (argc > 1) ? argv[1] : SERVIDOR_IP;
    int         porta = (argc > 2) ? atoi(argv[2]) : PORTA;

    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, tratar_sigint);

    printf("╔══════════════════════════════════════╗\n");
    printf("║     BATALHA DE PALAVRAS — Cliente    ║\n");
    printf("╚══════════════════════════════════════╝\n");

    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd == -1) {
        perror("Erro ao criar socket");
        exit(EXIT_FAILURE);
    }

    memset(&servidor_addr, 0, sizeof(servidor_addr));
    servidor_addr.sin_family = AF_INET;
    servidor_addr.sin_port   = htons(porta);

    if (inet_pton(AF_INET, ip, &servidor_addr.sin_addr) <= 0) {
        perror("Endereço inválido");
        close(sock_fd);
        exit(EXIT_FAILURE);
    }

    printf("  Conectando a %s:%d...\n", ip, porta);

    if (connect(sock_fd, (struct sockaddr *)&servidor_addr, sizeof(servidor_addr)) == -1) {
        perror("Erro ao conectar (o servidor está rodando?)");
        close(sock_fd);
        exit(EXIT_FAILURE);
    }
    printf("  Conectado!\n");

    char   acumulador[BUFFER_SIZE * 2];
    size_t acum_len = 0;

    estado_cliente est = { 0, 0, 0, 1 };

    while (est.jogando && !encerrar) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(STDIN_FILENO, &read_fds);
        FD_SET(sock_fd, &read_fds);

        int max_fd = (sock_fd > STDIN_FILENO) ? sock_fd : STDIN_FILENO;

        struct timeval  tv;
        struct timeval *prazo = NULL;

        if (est.esperando_palavra) {
            long restante = (long)(est.fim_da_rodada - time(NULL));
            if (restante < 0) restante = 0;
            tv.tv_sec  = restante;
            tv.tv_usec = 0;
            prazo = &tv;
        }

        int atividade = select(max_fd + 1, &read_fds, NULL, NULL, prazo);

        if (atividade == -1) {
            if (errno == EINTR) {
                if (encerrar) break;
                continue;
            }
            perror("Erro no select");
            break;
        }

        if (atividade == 0) {
            printf("\n  Tempo esgotado! Nenhuma palavra enviada.\n");
            enviar_msg(sock_fd, PROTO_TIMEOUT, "");
            est.esperando_palavra = 0;
            continue;
        }

        if (FD_ISSET(sock_fd, &read_fds)) {
            ssize_t bytes = recv(sock_fd,
                                 acumulador + acum_len,
                                 sizeof(acumulador) - acum_len - 1,
                                 0);

            if (bytes == 0) {
                printf("\n Servidor encerrou a conexão.\n");
                break;
            }
            if (bytes < 0) {
                if (errno == EINTR) {
                    if (encerrar) break;
                    continue;
                }
                perror("Erro ao receber");
                break;
            }

            acum_len += (size_t)bytes;
            acumulador[acum_len] = '\0';

            char *inicio = acumulador;
            char *quebra;

            while ((quebra = strchr(inicio, '\n')) != NULL) {
                *quebra = '\0';
                processar_mensagem(inicio, &est);
                inicio = quebra + 1;
            }

            acum_len = strlen(inicio);
            memmove(acumulador, inicio, acum_len + 1);
        }

        if (est.jogando && FD_ISSET(STDIN_FILENO, &read_fds)) {
            char entrada[BUFFER_SIZE];

            if (fgets(entrada, sizeof(entrada), stdin) == NULL) {
                printf("\n  Saindo...\n");
                break;
            }
            remover_nova_linha(entrada);

            if (est.esperando_nome) {
                if (strlen(entrada) == 0) {
                    printf("  Nome não pode ser vazio. Digite seu nome: ");
                    fflush(stdout);
                    continue;
                }
                entrada[NOME_SIZE - 1] = '\0';
                enviar_msg(sock_fd, PROTO_NOME, entrada);
                est.esperando_nome = 0;
                printf("  Bem-vindo, %s!\n", entrada);

            } else if (est.esperando_palavra) {
                if (strlen(entrada) == 0) {
                    printf("  Sua palavra: ");
                    fflush(stdout);
                    continue;
                }
                enviar_msg(sock_fd, PROTO_PALAVRA, entrada);
                est.esperando_palavra = 0;
                printf("  Enviado: \"%s\" — aguardando o  resultado...\n", entrada);

            } else {
                printf("  (aguarde a próxima rodada)\n");
            }
        }
    }

    if (encerrar) {
        printf("\n  Interrompido pelo usuário (Ctrl+C).\n");
    }

    close(sock_fd);
    printf("\n  Conexão encerrada. Tchauu!\n");

    return 0;
}
