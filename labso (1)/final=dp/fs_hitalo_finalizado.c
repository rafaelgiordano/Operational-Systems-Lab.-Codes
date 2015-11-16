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

typedef struct {
    int aberto; //-2 inexistente, -1 fechado, 0 aberto leitura, 1 aberto escrita
    int byte_bloco;
    unsigned short bloco;
    int bytes_lidos;
} ARQUIVO;

ARQUIVO arquivo[128];

dir_entry dir[128];
int formatado, cont_buffer = 0;
char buffer_global[513];

void fat_write();
int descarregaBuffer(int file);
int alocaBloco(int file, int *setor);
void diretorio_write();

int fs_init() {
    //printf("Função não implementada: fs_init\n");

    int i;
    char *bufferFAT = (char*)fat, *bufferDIR = (char*)dir;

    //le a fat do disco para a memoria, passando como parametro a posicao de cada setor e juntamente o buffer
    //que incrementa posições de 512 em 512, que é o tamanho do setor
    //LEITURA DO DISCO SEMPRE DE SETOR EM SETOR
    for (i = 0; i < 256; i++){
        bl_read(i, &bufferFAT[i * 512]);
    }


    //lida a fat para a memoria, devemos nos assegurar que ela esta formatada da maneira correta
    //para isso, de acordo com o modelo apresentado pelo professor, precisamos garantir que as 32 primeiras posições da fat
    //sao relativas ao agrupamento da fat, que corresponde ao numero 3
    if(fat[32] == 4){
        for(i = 0; i <= 32; i++){
            if(i != 32 && fat[i] != 3){
                formatado = 0; //se em alguma das primeiras 32 posições nao possuirem valor 3(agrupamento da fat), o disco nao esta formatado
                break;
            }
        }
    }else{
        formatado = 0; //se caso a 33a posição nao for do diretorio formatado = 0
    }

    //le o diretorio
    for (i = 256; i < 264; i++) {
        bl_read(i, &bufferDIR[i * 512]);
    }

    for (i = 0; i < 128; i++){
        arquivo[i].aberto = -2;
    }

    return 1;
}

int fs_format() {
    //printf("Função não implementada: fs_format\n");
    int i;
 
    for(i = 0;i < 65536; i++){
        if(i < 32){
            fat[i] = 3;
        }else{
            if(i > 32){
                fat[i] = 1;
            }else{
                fat[i] = 4;
            }
        }
    }

    for (i = 0; i< 128; i++){
        dir[i].used = '0';
    }

    fat_write();
    formatado = 1;
    return 1;
}

int fs_free() {
    //printf("Função não implementada: fs_free\n");
    
    int i, livre = 0;

    /*Verifica se o disco está formatado*/
    if(formatado == 0){
        printf("Disco não formatado\n");
    }

    /*Verifica o tamanho livre do disco através da FAT*/
    for(i = 33;i < 65536; i++){
        if(fat[i] == 1){
            livre += 4096; //adiciona-se o tamanho do bloco, caso a posição da fat esteja marcada como livre
        }
    }
    return livre;
}

int fs_list(char *buffer, int size) {
    //printf("Função não implementada: fs_list\n");
    strcpy(buffer,"");
    char aux[128];
    int i;

    if(formatado == 0){
        printf("Disco não formatado\n");
        return 0;
    }

    for (i = 0; i < 128; i++){
        if(dir[i].used == '1'){
            sprintf(aux, "%s\t\t%d\n",dir[i].name, dir[i].size);
            strcat(buffer, aux);
        }
    }
    return 1;
}

int fs_create(char* file_name) {
    //printf("Função não implementada: fs_create\n");
    int i, pos = -1, pos_fat = -1;

    if(formatado == 0){
        printf("Disco não formatado\n");
        return 0;
    }


    //verifica o nome do arquivo, para que nao exista arquivos com nomes duplicados
    //posicao guarda a primeira posicao livre encontrada no diretorio
    for(i = 0; i < 128; i++) {
        if(dir[i].used == '1' && strcmp(dir[i].name,file_name) == 0){ 
            printf("Arquivo ja existente\n");
            return 0;
        }else{ 
            if(dir[i].used == '0' && pos == -1){
                pos = i;
            }
        }
    }

    //se a posicao e invalida, nao há espaco no diretorio
    if(pos == -1) {
        printf("\nNão há espaço no diretório\n");
        return 0;
    }

    //procura a primeira posiçao livre na fat para alocar o arquivo
    for(i = 33; i < 65536; i++){
        if (fat[i] == 1) {
            pos_fat = i;
            fat[i] = 2;
            break;
        }
    }

    //se a posicao for invalida nao há espaço livre
    if(pos_fat == -1) {
        printf("Fat cheia\n");
        return 0;
    }

    dir[pos].first_block = pos_fat; 
    dir[pos].used = '1';
    strcpy(dir[pos].name, file_name);
    dir[pos].size = 0;
    arquivo[pos_fat].aberto = -1; //cria o arquivo e seta ele como fechado

    fat_write();

    return pos;
}

int fs_remove(char *file_name) {
    //printf("Função não implementada: fs_remove\n");
    int flag = 0, i, fat_aux, pos;

    if(formatado == 0){
        printf("Disco não formatado\n");
        return 0;
    }

    for (i = 0; i < 128; i++){
        if(dir[i].used != '0' && strcmp(dir[i].name,file_name) == 0){ 
            flag = 1;
            break;
        }
    }

    if (flag == 0) {
        printf("Arquivo inexistente\n");
        return 0;
    }
    arquivo[i].aberto = -2; //com o arquivo removido deve setar -2 para inexistente
    dir[i].used = '0';
    dir[i].size = 0;
    pos = i;
    i = dir[i].first_block;
    dir[pos].first_block = -1;
    
    while(i != 2){
        fat_aux = fat[i];
        fat[i] = 1;
        i = fat_aux;
    }

    fat_write();
    return 1;
}

int fs_open(char *file_name, int mode) {
    //printf("Função não implementada: fs_open\n");
    int flag = 0, i;

    if(formatado == 0){
        printf("Disco não formatado\n");
        return -1;
    }

    for (i = 0; i < 128; i++){
        if(dir[i].used != '0' && strcmp(dir[i].name,file_name) == 0){
            flag = 1; 
            break;
        }
    }

    if ((flag = 0) && (mode == FS_R)){
        printf("%s: Arquivo inexistente\n", file_name);
        return -1;
    }else{
        //-2 inexistente, -1 fechado, 0 aberto leitura, 1 aberto escrita
        if((flag = 0) && (mode == FS_W)){
            i = fs_create(file_name);
            arquivo[i].aberto = 1;
            arquivo[i].bytes_lidos = 0; 
            arquivo[i].byte_bloco = 0;
            arquivo[i].bloco = dir[i].first_block;  
        }else{
            if((flag = 1) && (mode == FS_R)){
                arquivo[i].aberto = 0;
                arquivo[i].bytes_lidos = 0;
                arquivo[i].byte_bloco = 0;
                arquivo[i].bloco = dir[i].first_block;
            }else{
                if((flag = 1) && (mode == FS_W)){
                    fs_remove(file_name);
                    i = fs_create(file_name);
                    arquivo[i].aberto = 1;
                    arquivo[i].bytes_lidos = 0;
                    arquivo[i].byte_bloco = 0;
                    arquivo[i].bloco = dir[i].first_block;
                }
            }
        }
    }

    return i;
}

int fs_close(int file) {
    //printf("Função não implementada: fs_close\n");

    if(formatado == 0){
        printf("Disco não formatado\n");
        return -1;
    }

    if (arquivo[file].aberto == -2){
        printf("Arquivo inexistente");
        return 0;
    }else{
        //se o arquivo estiver aberto para escrita, é preciso decarregar o buffer com as alterações
        //cont_buffer controla quantas entradas ocorreram no buffer
        if(arquivo[file].aberto == 1 && cont_buffer != 0){
            descarregaBuffer(file);
        }
    }
    arquivo[file].aberto = -1;
    return 1;
}

int fs_write(char *buffer, int size, int file) {
    //printf("Função não implementada: fs_write\n");

    int qntd, buffer_livre, i, j;

    if(formatado == 0){
        printf("Disco não formatado\n");
        return -1;
    }

    //verifica em que modo o arquivo foi aberto
    if (arquivo[file].aberto == -2){
        printf("Arquivo inexistente\n");
        return -1;
    }else{ 
        if (arquivo[file].aberto == -1){
            printf("Arquivo não aberto\n");
            return -1;
        }else{
            if (arquivo[file].aberto == 0){
                printf("Arquivo aberto apenas para leitura\n");
                return -1;
            }
        }
    }
    qntd = size;
    size = 0;

    while (qntd > 0){
        buffer_livre = 512 - cont_buffer;
        //verificção para ver se a quantidade a ser escrita cabe inteira no buffer
        if (buffer_livre >= qntd){
            strncat(buffer_global, buffer, qntd);
            buffer_global[cont_buffer + qntd] = '\0';
            cont_buffer += qntd;
            dir[file].size += qntd;
            size += qntd;
            qntd = 0;
        }else{
            //se a quantidade nao couber no buffer, entao se escreve o que é possivel e atualiza o buffer para que fique
            //com o que nao foi escrito, e na proxima iteração entao ele é escrito 
            strncat(buffer_global, buffer, buffer_livre);
            buffer_global[512] = '\0';
            qntd -= buffer_livre;
            j = buffer_livre;
            for (i = 0; i < qntd; i++){
                buffer[i] = buffer[j];
                j++;
            }
            buffer[i] = '\0';
            cont_buffer += buffer_livre;
            dir[file].size += buffer_livre;
            size = buffer_livre;
        }

        //assim que o buffer encher, descarrega ele pro disco
        if (cont_buffer == 512){
            if (descarregaBuffer(file) == 0)
                return -1;
        }
    }
    //terminado entao, zera o buffer
    strcpy(buffer,"");

    diretorio_write();
    return size;
}

int fs_read(char *buffer, int size, int file) {
    //printf("Função não implementada: fs_read\n");
    char buffer_aux[513];
    int setor, qntd, bloco, cont, posicao, byte_setor, qntd_setorLivre;

    if(formatado == 0){
        printf("Disco não formatado\n");
        return -1;
    }

    //verifica em que modo o arquivo foi aberto
    if(arquivo[file].aberto == -2){
        printf("Arquivo inexistente\n");
        return -1;
    }else{ 
        if (arquivo[file].aberto == -1){
            printf("Arquivo não aberto\n");
            return -1;
        }else{
            if (arquivo[file].aberto == 1){
                printf("Arquivo não aberto para leitura\n");
                return -1;
            }
        }
    }
    strcpy(buffer,"");

    setor = (arquivo[file].byte_bloco/512) + (arquivo[file].bloco * 8);
    bloco = arquivo[file].bloco;
    byte_setor = arquivo[file].byte_bloco % 512;

    if (size < (dir[file].size - arquivo[file].bytes_lidos)){
        qntd = size;
    }else{
        qntd = dir[file].size - arquivo[file].bytes_lidos;
        size = dir[file].size - arquivo[file].bytes_lidos;
    }

    while (qntd > 0){

        cont = 0;
        qntd_setorLivre = 512 - byte_setor;

        bl_read(setor, buffer_aux);

        //pega os bytes relativos byte que o setor se encontra
        if (byte_setor > 0){
            posicao = byte_setor;
            for (cont = 0; cont < (512 - byte_setor); cont++){
                buffer_aux[cont] = buffer_aux[posicao];
                posicao++;
            }
            buffer_aux[cont] = '\0';
        }

        //verifica se a quantidade que será lida vai ser lida apenas do setor atual, ou se sera necessario utilizar
        //um outro setor para completar a leitura
        if(qntd <= qntd_setorLivre){
            strncat(buffer, buffer_aux, qntd);
            byte_setor += qntd;
            arquivo[file].byte_bloco += qntd;
            qntd -= qntd;
        }else{
            strncat(buffer, buffer_aux, qntd_setorLivre);
            byte_setor = 0;
            qntd -= qntd_setorLivre;
            arquivo[file].byte_bloco += qntd_setorLivre;
            setor++;

            //verifica se o novo setor é de um outro bloco do arquivo
            if (setor % 8 == 0 && setor != dir[file].first_block * 8){
                bloco = fat[bloco];
                setor = bloco * 8;
                arquivo[file].byte_bloco = 0;
            }
        }
    }

    arquivo[file].bytes_lidos += size;
    arquivo[file].bloco = bloco;
    arquivo[file].byte_bloco = arquivo[file].bytes_lidos % CLUSTERSIZE;
    
    return size;
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

int descarregaBuffer(int file){
    int setor_atual, setor_anterior, byte_setor, i, bloco = dir[file].first_block, j;
    char buffer_write[512];

    strcpy(buffer_write,"");
    //encontra-se o ultimo bloco
    while(fat[bloco] != 2){
        bloco = fat[bloco];
    }

    arquivo[file].byte_bloco = (dir[file].size - cont_buffer) % CLUSTERSIZE;
    setor_atual = (arquivo[file].byte_bloco/512) + (8 * bloco);
    setor_anterior = setor_atual;

    //verifica-se a necessidade da alocação de um novo bloco, para a escrita do buffer
    if ((setor_atual + 1) % 8 == 0) {
        arquivo[file].bloco = bloco;
        if (alocaBloco(file, &setor_atual) == 0){
            return 0; //se nao houver espaço para a alocação
        }
    }

    bl_read(setor_anterior, buffer_write);
    byte_setor = arquivo[file].byte_bloco % 512;   
    buffer_write[byte_setor] = '\0';
    buffer_write[512] = '\0';
    strncat(buffer_write, buffer_global, (512 - byte_setor));
    bl_write(setor_anterior, buffer_write);

    if((setor_anterior + 1) % 8 != 0 && (512 - byte_setor - cont_buffer) == 0){
        setor_atual = setor_anterior + 1;
    }else{ 
        if((setor_anterior + 1) % 8 == 0 && byte_setor != 0){
            bloco = fat[bloco];
            setor_atual = bloco * 8;
        }
    }

    if (setor_atual != setor_anterior && byte_setor != 0){
        j = byte_setor;
        for (i = 0; i < 512 - byte_setor; i++){
            buffer_global[i] = buffer_global[j];
            j++;
        }
        buffer_global[i] = '\0';
        bl_write(setor_atual, buffer_global);
    }

    /*Zera o buffer global e seu contador*/
    strcpy(buffer_global,"");
    cont_buffer = 0;
    return 512;

}

int alocaBloco(int file, int *setor){
    int i;

    /*Procura pelo primeiro bloco vazio da FAT*/
    for (i = 33; i < 65536; i++){
        /*Se o bloco estver vazio utiliza ele*/
        if (fat[i] == 1){
            fat[i] = 2; //seta o bloco como usado
            fat[arquivo[file].bloco] = i; //arruma a posição da FAT que contém o antigo ultimo bloco do arquivo
            //aberto_para[file].bloco = i; //seta o bloco atual
            *setor = 8 * i; //verifica o primeiro setor do novo bloco

            /*Salva a FAT*/
            fat_write();
            return 1;
        }
    }

    /*Retorna que não foi possível alocar um bloco*/
    return 0;
}

void diretorio_write(){

    char buffer[513];
    char *buffer_dir = (char *) dir;
    int i, setor = 256, j, k = 0;

    for (i = 0; i < 8; i++){
        for (j = i*512; k < 512; k++, j++){
            buffer[k] = buffer_dir[j];
        }
        k = 0;
        buffer[512] = '\0';
        bl_write(setor, buffer);
        setor++;
    }
}