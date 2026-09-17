CC = gcc
CFLAGS = -Wall -Wextra -pedantic -std=c11
LDFLAGS = -pthread

# Binários finais
EXEC_SERVER = servidor
EXEC_CLIENT = cliente

# Objetos
OBJ_SERVER = servidor.o jogo.o
OBJ_CLIENT = cliente.o jogo.o

.PHONY: all clean

all: $(EXEC_SERVER) $(EXEC_CLIENT)

# Compilação do servidor
$(EXEC_SERVER): $(OBJ_SERVER)
	$(CC) $(OBJ_SERVER) -o $@ $(LDFLAGS)

# Compilação do cliente
$(EXEC_CLIENT): $(OBJ_CLIENT)
	$(CC) $(OBJ_CLIENT) -o $@

# Regra genérica para objetos (.c -> .o)
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Dependências de cabeçalhos
servidor.o: servidor.c protocolo.h jogo.h
cliente.o: cliente.c protocolo.h jogo.h
jogo.o: jogo.c jogo.h protocolo.h

clean:
	rm -f *.o $(EXEC_SERVER) $(EXEC_CLIENT)
