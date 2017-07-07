#include "../include/apidisk.h"
#include "../include/bitmap2.h"
#include "../include/t2fs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//estrutura de dados do handle
typedef struct
{
	int CP; // current pointer
	int nroBlocoAtual; // bloco lógico atual
	int MFTNumber;
	int dadoModificado;
	int MFTmodificado;
	char fileName[51];
	int handlevalido;
	char buffer_blocoDados[4 * SECTOR_SIZE];
	char buffer_blocoMFT[2 * SECTOR_SIZE];
}handle;

//variaveis globais
struct t2fs_bootBlock boot;
handle listaHandle[20];
int inicializou = 0;




int inicializa()
{
	unsigned char buffer_setor[SECTOR_SIZE];
	if(read_sector(0,buffer_setor) == 0)
	{
		//lendo setor de boot
		memcpy(boot.id, buffer_setor, 4);
		memcpy(&boot.version, buffer_setor + 4,2);
		memcpy(&boot.blockSize, buffer_setor + 6,2);
		memcpy(&boot.MFTBlocksSize, buffer_setor + 8,2);
		memcpy(&boot.diskSectorSize, buffer_setor + 10,4);

		//invalidando handles
		int i;
		for(i=0;i<20;i++)
			listaHandle[i].handlevalido = -1;

		inicializou = 1;
		return 0;
	}
	else
		return -1;
}
int read_block(unsigned int block, char *buffer)
{
	if(!inicializou)
	{
		if(inicializa() != 0)
			return -1;
	}
	else printf("ja inicializou\n");

	int i;
	for(i=0;i<boot.blockSize;i++)
		if(read_sector(boot.blockSize * block + i, buffer + i) != 0)
			return -1;
	return 0;
	
}



int main()
{
	inicializa();
	char bufferBloco[boot.blockSize * SECTOR_SIZE];
	//char MFTroot[32*]

	printf("%s\n", boot.id);
	printf("%x\n", boot.version);
	printf("%x\n", boot.blockSize);
	printf("%x\n", boot.MFTBlocksSize);
	printf("%x\n", boot.diskSectorSize);

	printf("bloco 1\n\n");

	read_block(1,bufferBloco);
	
	return 0;
}