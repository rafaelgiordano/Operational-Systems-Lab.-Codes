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

int formatado = 1, cont_buffer; /*Variavel se indica se*/
char buffer_global[513];

typedef struct {

	int aberto;
	int byte_bloco;
	unsigned short bloco;
	int bytes_lidos;
} Arquivo;

Arquivo aberto_para[128];

typedef struct {
       char used;
       char name[25];
       unsigned short first_block;
       int size;
} dir_entry;

dir_entry dir[128];

/*protótipo funções auxiliares*/
int escrever_buffer_disco(int file);
int alocar_novo_bloco(int file, int *setor);
void salvar_fat();
void salvar_diretorio();

/*Inicia o disco*/
int fs_init() {

	int i, pos = 0;
	char *buffer1 = (char*) fat;
  	char *buffer_aux = (char*) dir;

	/*Lê a FAT do disco na memória*/
	for (i = 0; i < 256; i++){
		
		bl_read(i, &buffer1[pos]);
		pos += 512;
	}
	
	/*Verifica se a FAT está formatada*/
	for(i = 0; i < 33; i++){
		if(i != 32 && fat[i] != 3){
			formatado = 0;
			break;
		} else if (i == 32 && fat[i] != 4){
			formatado = 0;
			break;
		}
	}

	/*Inicializa o vetor que informa se um arquivo está aberto com todos fechados*/
	for (i = 0; i < 128; i++)
		aberto_para[i].aberto = -2;
	cont_buffer = 0;

	pos = 0;
	/*lê o bloco que contém o diretório*/
   	for (i = 256; i < 264; i++) {
    		bl_read(i, &buffer_aux[pos]);
    		pos+=512;
   	}

  	return 1;
}


/*Formata a FAT e o diretório*/
int fs_format() {
  /*--------------------------------------------------------
   Valor | Significado
  1      |  Agrupamento Livre
  2      |  Último Agrupamento de Arquivo
  3      |  Agrupamento de FAT  
  4      |  Agrupamento de Diretóio
---------------------------------------------------------*/

	int i; 

	/*inicializando a FAT*/
	for(i = 0;i < 65536; i++){
		if(i < 32)
			fat[i] = 3;
		else if (i > 32)
			fat[i] = 1;
		else
			fat[i] = 4;
	}

	/*Formata o diretório*/
	for (i = 0; i< 128; i++){
		dir[i].used = '0';
	}

	/*Salva a FAT e seta a variável que informa se a FAT está formatada ou não*/
	salvar_fat();
	formatado = 1;
	return 1;
}

/*Retorna o espaço livre no dispositivo em bytes*/
int fs_free() {

	int i, acc=0;

	/*Verifica se o disco está formatado*/
	if(!formatado){
		printf("Disco não formatado\n");
	}

	/*Verifica o tamanho livre do disco através da FAT*/
	for(i = 33;i < 65536; i++){
		if(fat[i] == 1){
			acc += 4096;
		}
	}
	return acc; /*retorna o tamanho livre*/
}

/*Lista todos os arquivos do diretório*/
int fs_list(char *buffer, int size) {

	buffer[0] = '\0'; //será o buffer que recebe os arquivos listados

	char dir_aux[128];
	int i;

	/*Verifica se o disco está formatado*/
	if(!formatado){
		printf("Disco não formatado\n");
		return 0;
	}

	/*Verifica as entradas do diretório que estão sendo usadas listando o arquivo das mesmas*/
	for (i = 0; i < 128; i++){
		if(dir[i].used == '1'){
			sprintf(dir_aux, "%s\t\t%d\n",dir[i].name, dir[i].size);
			strcat(buffer, dir_aux);
		}
	}

	return 1;
}

/*Cria um novo arquivo*/
int fs_create(char* file_name) {
  
	int posicao = -1;
	int pos_fat = -1, i;

	/*Verifica se o disco está formatado*/
	if(!formatado){
		printf("Disco não formatado\n");
		return 0;
	}


	for (i = 0; i < 128; i++) {
		/*Verifica se tem arquivo com o mesmo nome e pega a primeira posição vazia do diretorio*/
		if(dir[i].used == '1' && !strcmp(dir[i].name,file_name)){ 
			printf("Arquivo já existente\n");
			return 0;
		}else if (dir[i].used == '0' && posicao == -1) {
	  		posicao = i;
		}
	}

	/*Verfica se achou alguma posição vazia no diretório*/
	if (posicao == -1) {
		printf("\nNão há espaço no diretório\n");
		return 0;
	}

	/*Coloca o primeiro bloco livre como o primeiro bloco do arquivo criado*/
	for (i = 33; i < 65536; i++) {
		if (fat[i] == 1) {
			pos_fat = i;
			fat[i] = 2;
			break;
		}
	} 

	/*Verfica se achou algum espaço vazio no disco*/
	if (pos_fat == -1) {
		printf("Não há espaço em disco\n");
		return 0;
	} 

	/*Seta os dados do arquivo no diretório*/
	dir[posicao].first_block = pos_fat; 
	dir[posicao].used = '1';
	strcpy(dir[posicao].name, file_name);
	dir[posicao].size = 0;
	aberto_para[pos_fat].aberto = -1;

	/*retorna a posição do arquivo no diretório*/
	salvar_fat();
	return posicao;
}

/*Remove um arquivo do diretório e libera seus blocos*/
int fs_remove(char *file_name) {
	int fat_aux, i, pos; 

	/*Verifica se o disco está formatado*/
	if(!formatado){
		printf("Disco não formatado\n");
		return 0;
	}

	/*procura o arquivo com o nome informado*/
     	for (i = 0; i < 128; i++){
      		if(dir[i].used != '0' && !strcmp(dir[i].name,file_name)){ 
        		break;
      		}
    	}

	/*Verifica se o arquivo foi encontrado*/
    	if (i >= 128) {
      		printf("Arquivo inexistente\n");
      		return 0;
    	}

	/*Seta os valores do diretório na entrada em que o arquivo estava*/
      	dir[i].used = '0';
      	dir[i].size = 0;
	aberto_para[i].aberto = -2;
     	pos = i;
      	i = dir[i].first_block;
      	dir[pos].first_block = -1;
	
	/*Percorre todos os blocos do arquivo para colocá-los como livres*/
      	while(i != 2){
        	fat_aux = fat[i];
        	fat[i] = 1;
        	i = fat_aux;
      	}

	/*Salva a FAT*/
	salvar_fat();

	/*Retorna que o arquivo foi excluido com sucesso*/
	return 1;
}

/*Abre um arquivo para escrita ou para leitura*/
int fs_open(char *file_name, int mode) {

	int i;
	 
	/*Verifica se o disco está formatado*/
	if(!formatado){
		printf("Disco não formatado\n");
		return -1;
	}

	/*Percorre o diretório procurando por um arquivo que deseja-se abrir*/
	for (i = 0; i < 128; i++){
		/*Compara o nome dos arquivos do diretório para verificar se tem um igual*/
		if(dir[i].used != '0' && !strcmp(dir[i].name,file_name)){ 
			break;
		}
	}

	/*Se o contador i tiver passado a ultima entrada do diretório significa que o arquivo não existe*/
	if (i >= 128 && mode == FS_R){
		printf("Arquivo inexistente\n");
		return -1;
	/*Se o arquivo não existe e o modo de abertura for para escrita
	criamos o arquivo através da função fs_create que vai retornar
	a posição do vetor de diretórios que esse arquivo foi colocado*/  
	}else if(i >= 128 && mode == FS_W){
		i = fs_create(file_name);
		aberto_para[i].aberto = 1;
		aberto_para[i].bytes_lidos = aberto_para[i].byte_bloco = 0;
		aberto_para[i].bloco = dir[i].first_block;
	/*Caso o arquivo já exista e foi aberto para escrita nós removeremos o arquivo através
	da função fs_remove e criaremos um novo arquivo através da função
	fs_create que retornará a posição do arquivo no vetor de diretórios*/  
	}else if(i < 128 && mode == FS_W){
		fs_remove(file_name);
		i = fs_create(file_name);
		aberto_para[i].aberto = 1;
		aberto_para[i].bytes_lidos = aberto_para[i].byte_bloco = 0;
		aberto_para[i].bloco = dir[i].first_block;
	/*Abre o arquivo para escrita, caso ele exista*/
	} else if (i < 128){
		aberto_para[i].aberto = 0;
		aberto_para[i].bytes_lidos = aberto_para[i].byte_bloco = 0;
		aberto_para[i].bloco = dir[i].first_block;
	}

	return i;
}

/*Fecha o arquivo*/
int fs_close(int file)  {

	/*Verifica se o disco está formatado*/
	if(!formatado){
		printf("Disco não formatado\n");
		return -1;
	}

	/*Verifica se arquivo está aberto*/
	if (aberto_para[file].aberto == -2){
		printf("Não existe arquivo aberto com esse identificador");
		return 0;

	/*Verifica se o arquivo está aberto para escrita e, se tiver, se tem algo no buffer*/
	} else if (aberto_para[file].aberto == 1 && cont_buffer != 0){
		escrever_buffer_disco(file);
	}
	
	/*Coloca o arquivo como fechado e retorna sucesso*/
	aberto_para[file].aberto = -1;
	return 1;
}

/*Escreve size bytes do buffer no arquivo file*/
int fs_write(char *buffer, int size, int file) {

	int qntd, livre_buffer,i, j;

	/*Verificando se o disco está formatado*/
	if(!formatado){
		printf("Disco não formatado\n");
		return -1;
  	}

	/*Verifica se o arquivo está aberto e para escrita*/
	if (aberto_para[file].aberto == -2){
		printf("Arquivo inexistente\n");
		return -1;
	} else if (aberto_para[file].aberto == -1){
		printf("Arquivo não aberto\n");
		return -1;
	} else if (aberto_para[file].aberto == 0){
		printf("Arquivo não aberto para escrita\n");
		return -1;
	}

	/*recebe a quantidade a ser escrita*/
	qntd = size;
	size = 0;


	/*Escreve enquanto houver bytes a serem escritos*/
	while (qntd > 0){
		livre_buffer = 512-cont_buffer;
		/*Verifica se a quantidade cabe inteira no buffer*/
		if (livre_buffer >= qntd){
			strncat(buffer_global, buffer, qntd);
			buffer_global[cont_buffer + qntd] = '\0';
			cont_buffer += qntd;
			dir[file].size += qntd;
			size += qntd;
			qntd = 0;
		} else {
			/*Caso a quantidade não caiba inteira no buffer, escreve apenas o que cabe
			  e atualiza o buffer passado excluindo as posições já escritas*/
			strncat(buffer_global, buffer, livre_buffer);
			buffer_global[512] = '\0';
			qntd -= livre_buffer;
			j = livre_buffer;
			for (i = 0; i < qntd; i++){
				buffer[i] = buffer[j];
				j++;
			}
			buffer[i] = '\0';
			cont_buffer += livre_buffer;
			dir[file].size += livre_buffer;
			size = livre_buffer;
		}

		/*Escreve o buffer no disco caso ele esteja cheio*/
		if (cont_buffer == 512){
			if (escrever_buffer_disco(file) == 0)
				return -1;
		}
	}

	buffer[0] = '\0'; //Zera o buffer que manda o que é para escrever

	salvar_diretorio();
	return size;
}

/*Lê size bytes do arquivo file no buffer*/
int fs_read(char *buffer, int size, int file) {

	char buffer_aux[513];
	int setor, qntd, bloco, cont, pos, byte_setor, qntd_livre_setor;	

	/*Verificando se o disco está formatado*/
	if(!formatado){
		printf("Disco não formatado\n");
		return -1;
  	}

	/*Verifica se o arquivo está aberto e para leitura*/
	if (aberto_para[file].aberto == -2){
		printf("Arquivo inexistente\n");
		return -1;
	} else if (aberto_para[file].aberto == -1){
		printf("Arquivo não aberto\n");
		return -1;
	} else if (aberto_para[file].aberto == 1){
		printf("Arquivo não aberto para leitura\n");
		return -1;
	}

	/*Verifica o setor do arquivo em que se encontra o ponteiro*/
	setor = (aberto_para[file].byte_bloco/512) + (aberto_para[file].bloco * 8);
	bloco = aberto_para[file].bloco;
	byte_setor = aberto_para[file].byte_bloco % 512;

	buffer[0] = '\0'; //zera o buffer que vai conter os bytes lidos

	/*Verifica se o arquivo tem a quantidade de bytes a serem lidos, caso não tenha é lido
		a quantidade dispobível no arquivo*/
	if (size < (dir[file].size - aberto_para[file].bytes_lidos))
		qntd = size;
	else
		qntd = dir[file].size - aberto_para[file].bytes_lidos;

	size = qntd; //seta a vaiável que vai retornar a quantidade lida

	/*Faz a leitura dos bytes*/	
	while (qntd > 0){

		cont = 0;
		qntd_livre_setor = 512-byte_setor;

		/*Lê o setor inteiro*/
		bl_read(setor, buffer_aux);

		/*pega apenas os bytes do setor a partir do byte em que o ponteiro se encontra*/
		if (byte_setor > 0){
			pos = byte_setor;
			for (cont = 0; cont < (512 - byte_setor); cont++){
				buffer_aux[cont] = buffer_aux[pos];
				pos++;
			}
			buffer_aux[cont] = '\0';
		}

		/*Verifica se a quantidade a ser lida pôde ser lida toda do setor atual ou se necessita de um
		  outro setor*/
		if (qntd <= qntd_livre_setor){

			/*concatena os bytes lidos do setor no buffer*/
			strncat(buffer, buffer_aux, qntd);
			
			/*seta os valores do byte atual no setor e no bloco*/
			byte_setor += qntd;
			aberto_para[file].byte_bloco += qntd;

			/*Decrementa a quantidade a ser lida*/
			qntd -= qntd;
		} else {

			/*concatena os bytes lidos do setor no buffer*/
			strncat(buffer, buffer_aux, qntd_livre_setor);
		
			/*seta os valores do byte atual no setor e no bloco e a quantidade a ser lida*/
			byte_setor = 0;
			qntd -= qntd_livre_setor;
			aberto_para[file].byte_bloco += qntd_livre_setor;

			/*Incrementa o setor*/
			setor++;

			/*Verifica se o novo setor é de um outro bloco do arquivo*/
			if (setor % 8 == 0 && setor != dir[file].first_block * 8){
				bloco = fat[bloco];
				setor = bloco * 8;
				aberto_para[file].byte_bloco = 0;
			}
		}
	}

	/*seta o arquivo aberto com os novos parametros*/
	aberto_para[file].bytes_lidos += size;
	aberto_para[file].bloco = bloco;
	aberto_para[file].byte_bloco = aberto_para[file].bytes_lidos % CLUSTERSIZE;
	
	/*retorna a quantidade lida*/
	return size;
}


/*Implementação funções auxiliares*/
int escrever_buffer_disco(int file){

	int setor, byte_setor, i, setor_ant;
	unsigned short bloco = dir[file].first_block;
	char buffer_aux[512];

	/*Verifica o ultimo bloco do arquivo*/
	while(fat[bloco] != 2){
		bloco = fat[bloco];
	}

	/*Calcula o setor a ser escrito*/
	aberto_para[file].byte_bloco = (dir[file].size-cont_buffer) % CLUSTERSIZE;
	setor = (aberto_para[file].byte_bloco/512) + (8 * bloco);

	buffer_aux[0] = '\0'; //zera o buffer se auxilio

	/*Guarda a posição do setor atual*/
	setor_ant = setor;

	/*Aloca um novo bloco se necessário*/
	if ((setor+1) % 8 == 0) {
		aberto_para[file].bloco = bloco;
		if (!alocar_novo_bloco(file, &setor))
			return 0;
	}

	/*calcula em que byte do setor deve ser feita a escrita, lê o setor e pega as posições que estão
		antes desse byte e concatena com as posições do buffer que contém o que é pra ser escrito
		que têm espaço para serem escritas nesse setor*/
	byte_setor = aberto_para[file].byte_bloco % 512;
	bl_read(setor_ant, buffer_aux);
	buffer_aux[byte_setor] = '\0';
	strncat(buffer_aux, buffer_global, (512 - byte_setor));
	buffer_aux[512] = '\0';

	/*Escreve o buffer com o conteudo que o setor já tinha + o conteudo do buffer do arquivo que cabia nele no setor*/
	bl_write(setor_ant, buffer_aux);

	/*Verifica se o setor já está cheio e não é o ultimo do bloco*/
	if ((setor_ant + 1) % 8 != 0 && (512 - byte_setor - cont_buffer) == 0)
		setor = setor_ant+1;

	/*Verifica se o setor está cheio e é o ultimo do bloco para ir para o proximo bloco do arquivo*/
	else if ((setor_ant + 1) % 8 == 0 && byte_setor != 0){
		bloco = fat[bloco];
		setor = bloco * 8;
	}

	/*Verifica se setor ainda tem espaço para ser escrito*/
	if (setor != setor_ant && byte_setor != 0){
		int j = byte_setor;
		for (i = 0; i < 512-byte_setor; i++){
			buffer_global[i] = buffer_global[j];
			j++;
		}
		buffer_global[i] = '\0';
		bl_write(setor, buffer_global);
	}

	/*Zera o buffer global e seu contador*/
	buffer_global[0] = '\0';
	cont_buffer = 0;

	return 512;
}

/*Aloca um novo bloco do disco para o arquivo size*/
int alocar_novo_bloco(int file, int *setor){

	int i;

	/*Procura pelo primeiro bloco vazio da FAT*/
	for (i = 33; i < 65536; i++){
		/*Se o bloco estver vazio utiliza ele*/
		if (fat[i] == 1){
			fat[i] = 2; //seta o bloco como usado
			fat[aberto_para[file].bloco] = i; //arruma a posição da FAT que contém o antigo ultimo bloco do arquivo
			//aberto_para[file].bloco = i; //seta o bloco atual
			*setor = 8 * i; //verifica o primeiro setor do novo bloco

			/*Salva a FAT*/
			salvar_fat();
			return 1;
		}
	}

	/*Retorna que não foi possível alocar um bloco*/
	return 0;
}

/*Salva a FAT no disco*/
void salvar_fat(){

	int setor = 0, i, j, k = 0;
	char *buffer_fat = (char *) fat;
	char buffer[513];

	for (i = 0; i < 256; i++){
		for (j = 512*i; k < 512; k++, j++){
			buffer[k] = buffer_fat[j];
		}
		buffer[512] = '\0';
		bl_write(setor, buffer);
		setor++;
		k = 0;
	}
}

/*Salva o diretório no disco*/
void salvar_diretorio(){

	char buffer[513];
	char *buffer_aux = (char *) dir;
	int i, setor = 256, j, k = 0;

	for (i = 0; i < 8; i++){
		for (j = i*512; k < 512; k++, j++){
			buffer[k] = buffer_aux[j];
		}
		k = 0;
		buffer[512] = '\0';
		bl_write(setor, buffer);
		setor++;
	}
}
