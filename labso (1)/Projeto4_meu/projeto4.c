/*
	Projeto 4 - Programação concorrente

	Rafael Paschoal Giordano - 408298

	Thales Gonçalves Chagas - 408577

*/

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <crypt.h>
#include <unistd.h>
#include <pthread.h>

#define TAM_HASH 256
#define TAM_SENHA 4

#define N_ITENS 50

char pass[TAM_SENHA +1];

char *senha_passada_arg;

char *senha_compartilhada[N_ITENS];

int buffer[N_ITENS];

int inicio = 0, final = 0, cont = 0;

pthread_mutex_t bloqueio;
pthread_cond_t nao_vazio, nao_cheio;

void calcula_hash_senha(const char *senha, char *hash);

void incrementa_senha(char *senha);
void testa_senha(const char *hash_alvo, const char *senha);

void* consumidor(void *v);
void* produtor(void *v);

int main(int argc, char *argv[]) {
  int i;

  //char senha[TAM_SENHA + 1];
  if (argc != 2) {
    printf("Uso: %s <hash>", argv[0]);
    return 1;
  }

  for (i = 0; i < N_ITENS; i++)
    {
        senha_compartilhada[i] = NULL;
    }
  senha_passada_arg = argv[1];
  for (i = 0; i < TAM_SENHA; i++) {
    pass[i] = 'a';
  }
  pass[TAM_SENHA] = '\0';
  
  pthread_t thr_produtor;
  pthread_t thr_consumidor1,thr_consumidor2,thr_consumidor3,thr_consumidor4;

  pthread_mutex_init(&bloqueio, NULL);
  pthread_cond_init(&nao_cheio, NULL);
  pthread_cond_init(&nao_vazio, NULL);

  /* Limpa o buffer */
  for (i = 0; i < N_ITENS; i++)
    buffer[i] = 0;
  
  pthread_create(&thr_produtor, NULL, produtor, NULL);
  pthread_create(&thr_consumidor1, NULL, consumidor, NULL);
  pthread_create(&thr_consumidor2, NULL, consumidor, NULL);

  pthread_create(&thr_consumidor3, NULL, consumidor, NULL);
  pthread_create(&thr_consumidor4, NULL, consumidor, NULL);


  pthread_join(thr_produtor, NULL); 
  pthread_join(thr_consumidor1, NULL);
  pthread_join(thr_consumidor2, NULL);
  pthread_join(thr_consumidor3, NULL);
  pthread_join(thr_consumidor4, NULL);


  return 0;
}


void testa_senha(const char *hash_alvo, const char *senha) {
  char hash_calculado[TAM_HASH + 1];

  calcula_hash_senha(senha, hash_calculado);
  if (!strcmp(hash_alvo, hash_calculado)) {
    printf("Achou! %s\n", senha);
    exit(0);
  }
}

void incrementa_senha(char *senha) {
  int i;

  i = TAM_SENHA - 1;
  while (i >= 0) {
    if (senha[i] != 'z') {
      senha[i]++;
      i = -2;
    } else {
      senha[i] = 'a';
      i--;
    }
  }
  if (i == -1) {
    printf("Não achou!\n");
    exit(1);
  }
}


void calcula_hash_senha(const char *senha, char *hash) {
  struct crypt_data data;
  data.initialized = 0;
  strcpy(hash, crypt_r(senha, "aa", &data));
}

void* produtor(void *v) {
  int aux;

  while(1) {
    pthread_mutex_lock(&bloqueio);
    while (cont == N_ITENS) {
      pthread_cond_wait(&nao_cheio, &bloqueio);
    }
    senha_compartilhada[final] = pass;
    buffer[final] = final;
    incrementa_senha(pass);
    final = (final + 1) % N_ITENS;
    aux = cont;
    cont = aux + 1;
    pthread_cond_signal(&nao_vazio);
    pthread_mutex_unlock(&bloqueio);
    
  }
  printf("Produção encerrada.\n");     
  return NULL;
}

void* consumidor(void *v) {
  int aux;
  char senha[TAM_SENHA +1];

  while(1) {
    pthread_mutex_lock(&bloqueio);
    while (cont == 0) {
      pthread_cond_wait(&nao_vazio, &bloqueio);
    }
    strcpy(senha, senha_compartilhada[inicio]);
    inicio = (inicio + 1) % N_ITENS;
    aux = buffer[inicio]; /* Item é consumido */
    
    aux = cont; 
 
     
    cont = aux - 1; 

        
    pthread_cond_signal(&nao_cheio);
    pthread_mutex_unlock(&bloqueio);
    testa_senha(senha_passada_arg, senha);

    
  }
  printf("Consumo encerrado.\n");     
  return NULL;
}
