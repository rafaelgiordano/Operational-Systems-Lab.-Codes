/*
 * RSFS - Really Simple File System
 *
 * Copyright © 2010 Gustavo Maciel Dias Vieira
 * Copyright © 2010 Rodrigo Rocco Barbieri
 *
 * This file is part of RSFS.
 *
 * RSFS is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
/*
  ENTREGA PARCIAL : SISTEMA DE ARQUIVOS

  BRUNA POMPEU    - 408506
  HITALO SIQUEIRA - 408476
  RAFAEL GIORDANO - 408298
  THALES CHAGAS   - 408557

*/
#include <stdio.h>
#include <string.h>

#include "disk.h"
#include "fs.h"

#define CLUSTERSIZE 4096

unsigned short fat[65536];

typedef struct {
       char used;
       char name[25];
       unsigned short first_block;
       int size;
} dir_entry;

dir_entry dir[128];

void fat_write();

int fs_init() {
  //printf("Função não implementada: fs_init\n");

  int i;
  //variavel auxiliar teste de leitura fat
  char *bufferFat = ( char *) fat;

  for (i = 0; i < 256; i++){
    bl_read (i, &bufferFat[512 * i]);

  }
  //variavel auxiliar teste de leitura dir
  char *bufferDir = ( char *) dir;

  for(i =0 ; i< 8; i++){
    bl_read(i + 256, &bufferDir[ i * 512]);
  }

  if(fat[32] == 4){
    for(i = 0; i< 32; i++){
      if(fat[i] != 3){
        return 1; //disco nao formatado
      }
    }
  }else return 1;// disco nao formatado
  return 1;
}

int fs_format() {
  //printf("Função não implementada: fs_format\n");

  int i;
  // inicia fat
  fat[32] = 4;
  for ( i = 0 ; i < 32 ; i++){
    fat[i] = 3;
  }
  for (i = 33; i < 65536; i++){
    fat[i]= 1;
  }
  // marca dir como nao usado
  for( i = 0 ; i < 128; i++){
    dir[i].used = 'F'; // 'F' para false
  }
  fat_write();
  return 0;
}

int fs_free() {
  //printf("Função não implementada: fs_free\n");
  int i;
  int tam_bloco;
  int pos_livres =0;
  tam_bloco = bl_size() / 8;
  // verifica posicoes livres na fat
  for(i = 33; i < tam_bloco; i++){
    if(fat[i] ==1)
    pos_livres++;
  }

  return pos_livres * 4096;//retorna posicoes livres * o tamanho
}

int fs_list(char *buffer, int size) {
  //printf("Função não implementada: fs_list\n");

  int i;
  char dir_entry[200];
  strcpy(buffer, "");//limpa buffer
  for(i =0; i< 128; i++){
    if(dir[i].used == 'T'){
      sprintf(dir_entry, "%s\t\t%d\n", dir[i].name, dir[i].size);
      strcat(buffer, dir_entry);
    }
  }
  strcat(buffer, "\0");
  return 1;
}

int fs_create(char* file_name) {
  //printf("Função não implementada: fs_create\n");

  int i;
  //procura por arquivo existente
  for ( i = 0; i < 128; i++){
    if(!strcmp(file_name, dir[i].name) && dir[i].used == 'T'){
      printf("Arquivo ja existe.\n");
      return 0;
    }
  }
  i =0;
  while( i < 128 && dir[i].used != 'F'){
    i++;
  }
  if(i == 128){
    printf("Disco cheio\n");
    return 0;
  }
  int marca_dir = i;


  i = 33;
  while( (i < bl_size()/8) && fat[i]== 1){
    i++;
  }
  if(i == (bl_size()/8)){
    printf("Fat cheio\n");
    return 0;
  }
  int marca_fat = i;

  //insere valores na primeira posicao vazia encontrada
  dir[marca_dir].used = 'T';
  dir[marca_dir].first_block = marca_fat;
  strcpy(dir[marca_dir].name, file_name);
  dir[marca_dir].size = 0;

  //marca fat como usado
  fat[marca_fat] = 2;
  fat_write();
  //retorno arquivo criado com sucesso
  return 1;
}

int fs_remove(char *file_name) {
  //printf("Função não implementada: fs_remove\n");

  int i;

  i =0;
  while(i < 128 && !(!strcmp(file_name, dir[i].name) && dir[i].used == 'T')){
    i++;
  }
  if ( i == 128){
    printf("Arquivo nao encontrado\n");
    return 0;
  }
  int marca_dir =i;

  dir[marca_dir].used = 'F';
  dir[marca_dir].size = 0;


  i = dir[marca_dir].first_block;
  dir[marca_dir].first_block = -1;

  //loop para correr blocos do arquivo
  while(i > 2){
    int aux = dir[i].first_block;
    fat[i] = 1;
    i = aux;
  }

  fat_write();
  //retorno arquivo removido com sucesso
  return 1;
}

/* FASE PARCIAL ATÉ AQUI */

int fs_open(char *file_name, int mode) {
  printf("Função não implementada: fs_open\n");
  return -1;
}

int fs_close(int file)  {
  printf("Função não implementada: fs_close\n");
  return 0;
}

int fs_write(char *buffer, int size, int file) {
  printf("Função não implementada: fs_write\n");
  return -1;
}

int fs_read(char *buffer, int size, int file) {
  printf("Função não implementada: fs_read\n");
  return -1;
}

void fat_write(){
  char * buffer = (char *) fat;
  int i;

  for(i = 0 ; i < 256; i++){
    if(!bl_write(i, &buffer[512 * i])){
      printf("erro ao escrever fat\n");
      break;
    }
  }
}