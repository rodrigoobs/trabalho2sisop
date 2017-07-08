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
	char fileName[MAX_FILE_NAME_SIZE];
	int handlevalido;
	char buffer_blocoDados[4 * SECTOR_SIZE];
	char buffer_blocoMFT[2 * SECTOR_SIZE];
}handle;

//variaveis globais
struct t2fs_bootBlock boot;
struct t2fs_4tupla MFTRootTupla[32];
handle listaHandle[20];
int recordsPorBloco;
int inicializou = 0;

//protótipos
int inicializa();
int read_block(unsigned int block, unsigned char *buffer);
int readMFT(unsigned int registro, unsigned char *buffer);
int findMFTNumber(char fileName[]);
struct t2fs_record findt2fsRecord(struct t2fs_4tupla MFTtupla[], char *name);



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

		//lendo Registro MFT do root
		if(readMFT(1, (unsigned char*)MFTRootTupla) != 0)
			return -1;

		recordsPorBloco = (boot.blockSize * SECTOR_SIZE)/sizeof(struct t2fs_record);

		return 0;
	}
	else
		return -1;
}
int read_block(unsigned int block, unsigned char *buffer)
{
	int i;
	for(i=0;i<boot.blockSize;i++)
	{
		if(read_sector((boot.blockSize * block) + i, buffer + (i*SECTOR_SIZE)) != 0)
			return -1;
	}
	return 0;
}

int readMFT(unsigned int registro, unsigned char *buffer)
{
	unsigned char readBuffer[boot.blockSize * SECTOR_SIZE];
	read_block((registro/2)+1, readBuffer);
	if(registro % 2 == 0)//par
		memcpy(buffer, readBuffer, 512);
	else//impar
		memcpy(buffer, readBuffer+512, 512);

	return 0;
}

int findMFTNumber(char fileName[])
{
	char * pch;
	struct t2fs_4tupla MFTcurrent[32];
	struct t2fs_record record;

	memcpy(MFTcurrent,MFTRootTupla,512);
	pch = strtok (fileName,"/");
	while (pch != NULL)
	{
    	//printf ("%s\n",pch);
    	record = findt2fsRecord(MFTcurrent, pch);
    	readMFT(record.MFTNumber, (unsigned char *)MFTcurrent);
    	pch = strtok (NULL, "/");
    }
	return record.MFTNumber;
}

struct t2fs_record findt2fsRecord(struct t2fs_4tupla MFTtupla[], char *name)
{
	int i = 0,j,k;
	struct t2fs_record blocoDir[recordsPorBloco];
	while(MFTtupla[i].atributeType == 1)
	{
		for(j=0; j<MFTtupla[i].numberOfContiguosBlocks; j++)
		{
			read_block(MFTtupla[i].logicalBlockNumber + j, (unsigned char *) blocoDir);
			for(k=0; k<recordsPorBloco;k++)
			{
				if(strncmp(blocoDir[k].name, name, MAX_FILE_NAME_SIZE) == 0)
					return blocoDir[k];
			}
		}
		i++;
	}
	struct t2fs_record erro;
	erro.MFTNumber = -1;
	return erro;
}



int main()
{
	inicializa();
	struct t2fs_record record[(boot.blockSize * SECTOR_SIZE) / sizeof(struct t2fs_record)];

	printf("setor de boot:\n");
	printf("%s\n", boot.id);
	printf("%x\n", boot.version);
	printf("%d\n", boot.blockSize);
	printf("%x\n", boot.MFTBlocksSize);
	printf("%x\n", boot.diskSectorSize);

	printf("\nMFT root\n\n");
	
	int i;
	for(i=0;i<32;i++){
		printf("Tupla %d:\nAtribute Type:%u\n", i,MFTRootTupla[i].atributeType);
		printf("VBN: %u\n", MFTRootTupla[i].virtualBlockNumber);
		printf("LBN: %u\n", MFTRootTupla[i].logicalBlockNumber);
		printf("MFT Number: %u\n\n", MFTRootTupla[i].numberOfContiguosBlocks);		
	}



	read_block(MFTRootTupla[0].logicalBlockNumber, (unsigned char *)record);

	printf("\nDiretorio Raiz:\n\n");
	for(i=0;i<(boot.blockSize * SECTOR_SIZE) / sizeof(struct t2fs_record);i++){
		printf("record %d:\nType val:%hhu\n",i, record[i].TypeVal);
		printf("name: %s\n", record[i].name);
		printf("blocks file size: %u\n", record[i].blocksFileSize);
		printf("bytes file size: %u\n", record[i].bytesFileSize);
		printf("MFT Number: %u\n\n", record[i].MFTNumber);		
	}

	printf("findMFTNumber\n");

	char name[] = "/file1";
	printf("%d\n", findMFTNumber(name));

	
	return 0;
}