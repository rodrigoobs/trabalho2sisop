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
int read_block(unsigned int block, unsigned char *buffer)
{
	/*if(!inicializou)
	{
		if(inicializa() != 0)
			return -1;
	}
	else printf("ja inicializou\n");*/

	int i;
	for(i=0;i<boot.blockSize;i++)
	{
		if(read_sector((boot.blockSize * block) + i, buffer + (i*SECTOR_SIZE)) != 0)
			return -1;
	}
	return 0;
}

int readMFT(int registro, unsigned char *buffer)
{
	unsigned char readBuffer[boot.blockSize * SECTOR_SIZE];
	read_block((registro/2)+1, readBuffer);
	if(registro % 2 == 0)//par
		memcpy(buffer, readBuffer, 512);
	else//impar
		memcpy(buffer, readBuffer+512, 512);

	return 0;
}



int main()
{
	inicializa();
	struct t2fs_4tupla MFTrootTupla[32];
	struct t2fs_record record[(boot.blockSize * SECTOR_SIZE) / sizeof(struct t2fs_record)];

	printf("setor de boot:\n");
	printf("%s\n", boot.id);
	printf("%x\n", boot.version);
	printf("%d\n", boot.blockSize);
	printf("%x\n", boot.MFTBlocksSize);
	printf("%x\n", boot.diskSectorSize);

	printf("\nMFT root\n\n");

	readMFT(1,MFTrootTupla);

	int i;
	for(i=0;i<32;i++){
		printf("Tupla %d:\n%u\n", i,MFTrootTupla[i].atributeType);
		printf("VBN: %u\n", MFTrootTupla[i].virtualBlockNumber);
		printf("LBN: %u\n", MFTrootTupla[i].logicalBlockNumber);
		printf("MFT Number: %u\n\n", MFTrootTupla[i].numberOfContiguosBlocks);		
	}

	read_block(MFTrootTupla[0].logicalBlockNumber, record);

	printf("\nDiretorio Raiz:\n\n");
	for(i=0;i<(boot.blockSize * SECTOR_SIZE) / sizeof(struct t2fs_record);i++){
		printf("record %d:\n%hhu\n",i, record[i].TypeVal);
		printf("name: %s\n", record[i].name);
		printf("blocks file size: %u\n", record[i].blocksFileSize);
		printf("bytes file size: %u\n", record[i].bytesFileSize);
		printf("MFT Number: %u\n\n", record[i].MFTNumber);		
	}


	
	return 0;
}