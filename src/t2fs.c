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
	int handlevalido; // -1->invalido
	char buffer_blocoDados[4 * SECTOR_SIZE];
	struct t2fs_4tupla buffer_blocoMFT[32];
}handle;

struct hDir
{
	handle h;
	struct hDir *next;
};
typedef struct hDir handleDir;

//variaveis globais
struct t2fs_bootBlock boot;
struct t2fs_4tupla MFTRootTupla[32];
handle listaHandle[20];
handleDir *diretorio;
int recordsPorBloco;
int inicializou = 0;
int numArquivos;
int numDiretorio;

//protótipos
int inicializa();
int read_block(unsigned int block, unsigned char *buffer);
int readMFT(unsigned int registro, unsigned char *buffer);
int writeMFT(unsigned int registro, unsigned char *buffer);
int findMFTNumber(char fileName[]);
struct t2fs_record findt2fsRecord(struct t2fs_4tupla MFTtupla[], char *name);
int allocatorBlock(int flag1, int blockNumber);
int allocMFT(int reg);



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
		numArquivos = 0;

		//lendo Registro MFT do root
		if(readMFT(1, (unsigned char*)MFTRootTupla) != 0)
			return -1;

		//ponteiro para os handles de diretorios
		diretorio = NULL;
		numDiretorio = 0;


		recordsPorBloco = (boot.blockSize * SECTOR_SIZE)/sizeof(struct t2fs_record);

		inicializou = 1;

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

int write_block(unsigned int block, unsigned char *buffer)
{
	int i;
	for(i=0;i<boot.blockSize;i++)
	{
		if(write_sector((boot.blockSize * block) + i, buffer + (i*SECTOR_SIZE)) != 0)
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

int writeMFT(unsigned int registro, unsigned char *buffer)
{
	unsigned char readBuffer[boot.blockSize * SECTOR_SIZE];
	read_block((registro/2)+1, readBuffer);
	if(registro % 2 == 0)//par
		memcpy(readBuffer, buffer, 512);
	else//impar
		memcpy(readBuffer+512, buffer, 512);
	write_block((registro/2)+1, readBuffer);

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
	erro.TypeVal = 0;
	return erro;
}

 int allocatorBlock(int flag1, int blockNumber)
 {
 	if(flag1 == 0)//alocar novo bloco, de preferencia proximo do blockNumber
 	{
 		if(getBitmap2(blockNumber + 1) == 0)
 		{
 			if(setBitmap2(blockNumber + 1, 1) == 0)
 				return blockNumber + 1;
 			else
 				return -1;
  		}
 		else
 		{
 			int newBlock = 0;
 			while(newBlock <= boot.MFTBlocksSize)
 			{
 				newBlock = searchBitmap2(0);
 			}
 			return newBlock;
 		}
 	}
 	else//desalocar blockNumber
 	{
 		if(setBitmap2(blockNumber, 0) == 0)
 		{
 			return 0;
 		}
 		return -1;
 	}
 }

int allocMFT(int reg)
{
	struct t2fs_4tupla mft[64];
	if(reg == 0)//alocar novo registro
	{
		int registro = 4;
		int block = 3;
		while(block <= boot.MFTBlocksSize)
		{
			read_block(block, (unsigned char *)mft);
			if(mft[0].atributeType == -1)
				return registro;
			else if(mft[32].atributeType == -1)
			{
				return registro + 1;
			}
			block++;
			registro += 2;
		}
		return -1;
	}
	else if(reg > 3)//desalocar rigistro indicado por reg
	{
		int regParImpar=0;
		read_block((reg/2)+1,(unsigned char *)mft);
		if(reg % 2 == 0)
			regParImpar = 0;
		else
			regParImpar = 32;

		mft[regParImpar].atributeType = -1;
		mft[regParImpar].virtualBlockNumber = -1;
		mft[regParImpar].logicalBlockNumber = -1;
		mft[regParImpar].numberOfContiguosBlocks = -1;

		write_block((reg/2)+1,(unsigned char *)mft);
		return 0;
	}
	return -1;
}

int identify2 (char *name, int size)
{
	if(!inicializou)
	{
		if(inicializa() != 0)
			return -1;
	}

	char nomes[] = "Lucas Bauer, Geronimo Veit e Rodrigo Oliveira";

	if(size > 46)
		strncpy(name, nomes, 46);
	else
		return -1;
	return 0;
}

int swap_block(int handle)
{
	int i=0,j, blocoLogico=-1,indice=0,novoBloco=-1;
	if(listaHandle[handle].dadoModificado == 1)
	{
		write_block(listaHandle[handle].nroBlocoAtual, (unsigned char*)listaHandle[handle].buffer_blocoDados);
	}
	while(listaHandle[handle].buffer_blocoMFT[i].atributeType == 1)
	{
		for(j=0; j<listaHandle[handle].buffer_blocoMFT[i].numberOfContiguosBlocks; j++)
		{
			if(listaHandle[handle].nroBlocoAtual == i + j)
			{
				indice = i;
				if ((j + 1)<listaHandle[handle].buffer_blocoMFT[i].numberOfContiguosBlocks)
				{
					blocoLogico = listaHandle[handle].buffer_blocoMFT[i].logicalBlockNumber + j+1;
				}
				else
				{
					novoBloco = allocatorBlock(0,listaHandle[handle].buffer_blocoMFT[i].logicalBlockNumber + j);
					if(novoBloco == listaHandle[handle].buffer_blocoMFT[i].logicalBlockNumber + j + 1)
					{
						blocoLogico = novoBloco;
						listaHandle[handle].buffer_blocoMFT[i].numberOfContiguosBlocks++;
					}
					else
						allocatorBlock(1,novoBloco);

				}
				break;
			} 
		}
		i++;
	}
	if(blocoLogico == -1)
	{
		if(indice + 1 < 32)
		{
			if(listaHandle[handle].buffer_blocoMFT[indice+1].atributeType == 1)
			{
				blocoLogico = listaHandle[handle].buffer_blocoMFT[indice+1].logicalBlockNumber;
			}
			else if(listaHandle[handle].buffer_blocoMFT[indice+1].atributeType == 0)
			{
				listaHandle[handle].buffer_blocoMFT[indice+1].atributeType = 1;
				listaHandle[handle].buffer_blocoMFT[indice+1].virtualBlockNumber = listaHandle[handle].nroBlocoAtual + 1;
				listaHandle[handle].buffer_blocoMFT[indice+1].logicalBlockNumber = allocatorBlock(0,0);
				listaHandle[handle].buffer_blocoMFT[indice+1].numberOfContiguosBlocks = 1;
				listaHandle[handle].MFTmodificado = 1;
				blocoLogico = listaHandle[handle].buffer_blocoMFT[indice+1].logicalBlockNumber;
			}
		}
		else return -1;
	}

	read_block(blocoLogico, (unsigned char*)listaHandle[handle].buffer_blocoDados);
	listaHandle[handle].CP = 0;
	return 0;
}

FILE2 create2 (char *filename)
{
	if(!inicializou)
	{
		if(inicializa() != 0)
			return -1;
	}

	int cont = 0,i;
	char filepath[strlen(filename)], nameaux[strlen(filename)], name[MAX_FILE_NAME_SIZE];
	strcpy(nameaux, filename);

	//separando path do nome
	filepath[0] = '\0';
	name[0] = '\0';
	char * pch;
	pch = strtok (filename,"/");
	while (pch != NULL)
	{
		cont++;
    	pch = strtok (NULL, "/");
    }

    pch = strtok (nameaux,"/");
    for(i=0; i<cont; i++)
    {
    	if(i<cont-1)
    	{
    		strcat(filepath, "/");
    		strcat(filepath, pch);
    	}
    	else
    		strcat(name,pch);
    	pch = strtok (NULL, "/");
    }

    //teste se nome é valido
    if(strlen(name) > MAX_FILE_NAME_SIZE)
    	return -1;


    int MFTdir;

    if(strcmp(filepath,"") == 0)
    {
   		strcpy(filepath,"/");
   		MFTdir = 1;
   	}
    else
    	MFTdir = findMFTNumber(filepath);


    struct t2fs_4tupla Tupla[32];
    struct t2fs_record dirRecord[recordsPorBloco];


    readMFT(MFTdir, (unsigned char *)Tupla);
    

    //teste se já existe arquivo com mesmo nome
    dirRecord[0] = findt2fsRecord(Tupla, name);
    if(dirRecord[0].TypeVal == 1 || dirRecord[0].TypeVal ==2)
    	return -1;


    //verifica se tem espaço livre no vetor de handle
    if(numArquivos >= 20)
    	return -1;


    //alocando record na pasta diretório
    i = 0;
    int j,k,achou = 0, indice,bloco;
    while(Tupla[i].atributeType == 1)
	{
		for(j=0; j<Tupla[i].numberOfContiguosBlocks; j++)
		{
			read_block(Tupla[i].logicalBlockNumber + j, (unsigned char *) dirRecord);
			for(k=0; k<recordsPorBloco;k++)
			{
				if(dirRecord[k].TypeVal != 1 && dirRecord[k].TypeVal != 2)
				{
					achou = 1;
					indice = k;
					bloco = Tupla[i].logicalBlockNumber + j;
					break;
				}
			}
		}
		i++;
	}


	if(achou)
	{
		dirRecord[indice].TypeVal = 1;
		strncpy(dirRecord[k].name, name, MAX_FILE_NAME_SIZE);
		dirRecord[indice].blocksFileSize = 1;
		dirRecord[inicializou].bytesFileSize = 0;
		dirRecord[indice].MFTNumber = allocMFT(0);
	}
	else
		return -1;


	write_block(bloco, (unsigned char *)dirRecord);

	//escrever novo registro MFT
	readMFT(dirRecord[indice].MFTNumber, (unsigned char *)Tupla);
	Tupla[0].atributeType = 1;
	Tupla[0].virtualBlockNumber = 0;
	Tupla[0].logicalBlockNumber = allocatorBlock(0,0);
	Tupla[0].numberOfContiguosBlocks = 1;
	Tupla[1].atributeType = 0;
	Tupla[1].virtualBlockNumber = 0;
	Tupla[1].logicalBlockNumber = 0;
	Tupla[1].numberOfContiguosBlocks = 0;
	writeMFT(dirRecord[indice].MFTNumber, (unsigned char *)Tupla);


	//criar handle arquivo
	int handle = -1;
	for(i=0; i<20; i++)
		if(listaHandle[i].handlevalido == -1)
		{
			handle = i;
			break;
		}

	if(handle == -1)
		return -1;

	listaHandle[handle].CP = 0;
	listaHandle[handle].nroBlocoAtual = 0;
	listaHandle[handle].MFTNumber = dirRecord[indice].MFTNumber;
	listaHandle[handle].dadoModificado = 0;
	listaHandle[handle].MFTmodificado = 1;
	strncpy(listaHandle[handle].fileName,name,MAX_FILE_NAME_SIZE);
	listaHandle[handle].handlevalido = 1;
	memcpy(listaHandle[handle].buffer_blocoMFT, Tupla, 512);
	read_block(listaHandle[handle].buffer_blocoMFT[0].logicalBlockNumber, (unsigned char *)listaHandle[handle].buffer_blocoDados);

	numArquivos++;

    return handle;
}

int delete2 (char *filename)
{
	if(!inicializou)
	{
		if(inicializa() != 0)
			return -1;
	}

	//teste para ver se arquivo existe
	if(findMFTNumber(filename) == -1)
		return -1;

	int cont = 0,i;
	char filepath[strlen(filename)], nameaux[strlen(filename)], name[MAX_FILE_NAME_SIZE];
	strcpy(nameaux, filename);

	//separando path do nome
	filepath[0] = '\0';
	name[0] = '\0';
	char * pch;
	pch = strtok (filename,"/");
	while (pch != NULL)
	{
		cont++;
    	pch = strtok (NULL, "/");
    }

    pch = strtok (nameaux,"/");
    for(i=0; i<cont; i++)
    {
    	if(i<cont-1)
    	{
    		strcat(filepath, "/");
    		strcat(filepath, pch);
    	}
    	else
    		strcat(name,pch);
    	pch = strtok (NULL, "/");
    }


    int MFTdir, MFTarq;
    MFTarq = findMFTNumber(filename);
    if(strcmp(filepath,"") == 0)
    {
   		strcpy(filepath,"/");
   		MFTdir = 1;
   	}
    else
    	MFTdir = findMFTNumber(filepath);

    struct t2fs_4tupla Tupla[32];
    struct t2fs_record dirRecord[recordsPorBloco];

    //atualizando record do arquivo
    readMFT(MFTdir, (unsigned char *)Tupla);
    i = 0;
    int j,k,achou = 0, indice,bloco;
    while(Tupla[i].atributeType == 1)
	{
		for(j=0; j<Tupla[i].numberOfContiguosBlocks; j++)
		{
			read_block(Tupla[i].logicalBlockNumber + j, (unsigned char *) dirRecord);
			for(k=0; k<recordsPorBloco;k++)
			{
				if(strcmp(dirRecord[k].name, name) ==0)
				{
					achou = 1;
					indice = k;
					bloco = Tupla[i].logicalBlockNumber + j;
					break;
				}
			}
		}
		i++;
	}
	if(achou)
	{
		dirRecord[indice].TypeVal = 0;
		dirRecord[indice].name[0] = '\0';
		dirRecord[indice].blocksFileSize = 0;
		dirRecord[indice].bytesFileSize = 0;
		dirRecord[indice].MFTNumber = 0;
		write_block(bloco, (unsigned char*)dirRecord);
	}
	else
		return -1;


	//excluindo blocos de dados
	readMFT(MFTarq, (unsigned char *) Tupla);
	while(Tupla[i].atributeType == 1)
	{
		for(j=0; j<Tupla[i].numberOfContiguosBlocks; j++)
		{
			setBitmap2(Tupla[i].logicalBlockNumber + j, 0);
		}
		i++;
	}

	//liberando registro MFT
	if(allocMFT(MFTarq) != 0)
		return -1;

	return 0;
}

FILE2 open2 (char *filename)
{
	if(!inicializou)
	{
		if(inicializa() != 0)
			return -1;
	}

    int MFT_Number, i=0;
    struct t2fs_4tupla buffer_MFT[32];
    MFT_Number = findMFTNumber(filename);
 
    if (MFT_Number>0){
        readMFT(MFT_Number, (unsigned char *)buffer_MFT);
        while(listaHandle[i].handlevalido != -1 && i<20)
            i++;
        if(i==20){
            printf("Nao ha espaco suficiente\n");
            return -1;
        }
        else{
            listaHandle[i].CP = 0;
            listaHandle[i].MFTNumber = MFT_Number;
            listaHandle[i].nroBlocoAtual = 0;
            listaHandle[i].dadoModificado = 0;
            listaHandle[i].MFTmodificado = 0;
            strncpy(listaHandle[i].fileName, filename, MAX_FILE_NAME_SIZE);
            listaHandle[i].handlevalido = 1;
            read_block(buffer_MFT[0].logicalBlockNumber, (unsigned char*)listaHandle[i].buffer_blocoDados);
            memcpy(listaHandle[i].buffer_blocoMFT, buffer_MFT, 512);
        }
        
    }
    else{
        printf("Arquivo nao existe\n");
        return -1;
    }
    return i;
}

int close2 (FILE2 handle)
{
	if(!inicializou)
	{
		if(inicializa() != 0)
			return -1;
	}

	int i=0,j, blocoLogico;
	while(listaHandle[handle].buffer_blocoMFT[i].atributeType == 1)
	{
		for(j=0; j<listaHandle[handle].buffer_blocoMFT[i].numberOfContiguosBlocks; j++){
			if(listaHandle[handle].nroBlocoAtual == i + j){
				blocoLogico = listaHandle[handle].buffer_blocoMFT[i].logicalBlockNumber + j;
				break;
			}
		}
		i++;
	}


    if (listaHandle[handle].dadoModificado == 1){
        if(write_block(blocoLogico, (unsigned char *)listaHandle[handle].buffer_blocoDados)<0){
            printf("Erro ao descarregar arquivo\n");
            return -1;
        printf("aqui no write\n");
        }
    }
    
    if (listaHandle[handle].MFTmodificado == 1){
        if(write_block(listaHandle[handle].MFTNumber , (unsigned char *) listaHandle[handle].buffer_blocoMFT)<0){
            printf("Erro ao descarregar arquivo\n");
            return -1;
        }
    }

    //desalocar handle
    listaHandle[handle].handlevalido = -1;     
    return 0;
}

int read2 (FILE2 handle, char *buffer, int size)
{
	if(!inicializou)
	{
		if(inicializa() != 0)
			return -1;
	}

	//testa se o handle está aberto
	if(listaHandle[handle].handlevalido != 1)
		return -1;

	//testar se ainda tem bytes a serem lidos

	int i;
    for(i=0; i<size; i++)
    {
    	if(listaHandle[handle].CP > (boot.blockSize*SECTOR_SIZE))
    	{
    		swap_block(handle);
    	}
    	else
    	{
    		buffer[i] = listaHandle[handle].buffer_blocoDados[listaHandle[handle].CP];
    		listaHandle[handle].CP++;
    	}
    }
	return i;
}

int write2 (FILE2 handle, char *buffer, int size)
{
	if(!inicializou)
	{
		if(inicializa() != 0)
			return -1;
	}
	//testa se o handle é valido
	if(listaHandle[handle].handlevalido != 1)
		return -1;

	int i;
    for(i=0; i<size; i++)
    {
    	if(listaHandle[handle].CP > (boot.blockSize*SECTOR_SIZE))
    	{
    		swap_block(handle);
    	}
    	else
    	{
    		listaHandle[handle].buffer_blocoDados[listaHandle[handle].CP] = buffer[i];
    		listaHandle[handle].CP++;
    	}
    }
    listaHandle[handle].dadoModificado = 1;
	return i;
}

int mkdir2 (char *pathname)
{
	if(!inicializou)
	{
		if(inicializa() != 0)
			return -1;
	}

	int cont = 0,i;
	char filepath[strlen(pathname)], nameaux[strlen(pathname)], name[MAX_FILE_NAME_SIZE];
	strcpy(nameaux, pathname);

	//separando path do nome
	filepath[0] = '\0';
	name[0] = '\0';
	char * pch;
	pch = strtok (pathname,"/");
	while (pch != NULL)
	{
		cont++;
    	pch = strtok (NULL, "/");
    }

    pch = strtok (nameaux,"/");
    for(i=0; i<cont; i++)
    {
    	if(i<cont-1)
    	{
    		strcat(filepath, "/");
    		strcat(filepath, pch);
    	}
    	else
    		strcat(name,pch);
    	pch = strtok (NULL, "/");
    }

    //teste se nome é valido
    if(strlen(name) > MAX_FILE_NAME_SIZE)
    	return -1;


    int MFTdir;

    if(strcmp(filepath,"") == 0)
    {
   		strcpy(filepath,"/");
   		MFTdir = 1;
   	}
    else
    	MFTdir = findMFTNumber(filepath);

    

    struct t2fs_4tupla Tupla[32];
    struct t2fs_record dirRecord[recordsPorBloco];


    readMFT(MFTdir, (unsigned char *)Tupla);
    

    //teste se já existe diretorio com mesmo nome
    dirRecord[0] = findt2fsRecord(Tupla, name);
    if(dirRecord[0].TypeVal == 1 || dirRecord[0].TypeVal ==2)
    	return -1;


    //alocando record na pasta diretório
    i = 0;
    int j,k,achou = 0, indice,bloco;
    while(Tupla[i].atributeType == 1)
	{
		for(j=0; j<Tupla[i].numberOfContiguosBlocks; j++)
		{
			read_block(Tupla[i].logicalBlockNumber + j, (unsigned char *) dirRecord);
			for(k=0; k<recordsPorBloco;k++)
			{
				if(dirRecord[k].TypeVal != 1 && dirRecord[k].TypeVal != 2)
				{
					achou = 1;
					indice = k;
					bloco = Tupla[i].logicalBlockNumber + j;
					break;
				}
			}
		}
		i++;
	}


	if(achou)
	{
		dirRecord[indice].TypeVal = 2;
		strncpy(dirRecord[k].name, name, MAX_FILE_NAME_SIZE);
		dirRecord[indice].blocksFileSize = 1;
		dirRecord[inicializou].bytesFileSize = 0;
		dirRecord[indice].MFTNumber = allocMFT(0);
	}
	else
		return -1;


	write_block(bloco, (unsigned char *)dirRecord);

	//escrever novo registro MFT
	readMFT(dirRecord[indice].MFTNumber, (unsigned char *)Tupla);
	Tupla[0].atributeType = 1;
	Tupla[0].virtualBlockNumber = 0;
	Tupla[0].logicalBlockNumber = allocatorBlock(0,0);
	Tupla[0].numberOfContiguosBlocks = 1;
	Tupla[1].atributeType = 0;
	Tupla[1].virtualBlockNumber = 0;
	Tupla[1].logicalBlockNumber = 0;
	Tupla[1].numberOfContiguosBlocks = 0;
	writeMFT(dirRecord[indice].MFTNumber, (unsigned char *)Tupla);


	//criar handle arquivo

	handleDir *novo = malloc(sizeof(handleDir));
	novo->h.CP = 0;
	novo->h.nroBlocoAtual = 0;
	novo->h.MFTNumber = dirRecord[indice].MFTNumber;
	novo->h.dadoModificado = 0;
	novo->h.MFTmodificado = 1;
	strncpy(novo->h.fileName,name,MAX_FILE_NAME_SIZE);
	novo->h.handlevalido = 1;
	memcpy(novo->h.buffer_blocoMFT, Tupla, 512);
	read_block(novo->h.buffer_blocoMFT[0].logicalBlockNumber, (unsigned char *)novo->h.buffer_blocoDados);
	novo->next = NULL;


	handleDir *ptaux;
	ptaux = diretorio;
	if(ptaux == NULL)
		ptaux = novo;
	else
	{	while (ptaux->next != NULL)
		{
			ptaux = ptaux->next;
		}
			ptaux->next = novo;
	}

	printf("direpath%s\n", filepath);
    printf("dirname%s\n", name);
    printf("MFTdir%d\n", MFTdir);
    printf("mftnewdir%d\n", diretorio->h.MFTNumber);

    exit(9);


	

	numDiretorio++;

    return 0;
}


int main()
{
	char name[] = "/dir1";
	printf("mkdir%d\n", mkdir2(name));



	/*char buff[1025] = "teste file 3 !!";
	char buff2[100];
	buff2[99] = '\0';

	int handle = open2(name);
	printf("handle%d\n", handle);
	printf("write %d\n", write2(handle, buff, 16));
	printf("close %d\n", close2(handle));
	handle = open2(name);
	printf("handle%d\n", handle);
	printf("read %d\n", read2(handle, buff2, 16)); 
	printf("%s\n", buff2);*/


	
	/*printf("%d\n", create2(name3));
	printf("%d\n", create2(name4));*/
/*
	int indice = create2(name);
	printf("handle%d\n", indice);

	printf("Teste handle\n");
	printf("cp%d\n", listaHandle[indice].CP);
	printf("MFTNumber%d\n", listaHandle[indice].MFTNumber);
	printf("nroBlocoAtual%d\n", listaHandle[indice].nroBlocoAtual);
	printf("dadoModificado%d\n", listaHandle[indice].dadoModificado);
	printf("MFTmodificado%d\n", listaHandle[indice].MFTmodificado);
	printf("filename%s\n", listaHandle[indice].fileName);
	printf("handlevalido%d\n", listaHandle[indice].handlevalido);

	close2(indice);

	printf("Teste handle\n");
	printf("cp%d\n", listaHandle[indice].CP);
	printf("MFTNumber%d\n", listaHandle[indice].MFTNumber);
	printf("nroBlocoAtual%d\n", listaHandle[indice].nroBlocoAtual);
	printf("dadoModificado%d\n", listaHandle[indice].dadoModificado);
	printf("MFTmodificado%d\n", listaHandle[indice].MFTmodificado);
	printf("filename%s\n", listaHandle[indice].fileName);
	printf("handlevalido%d\n", listaHandle[indice].handlevalido);

	exit(1);


	




	
	char name[] = "/file3",name2[] = "/file4";
	printf("%d\n", create2(name));
	printf("%d\n", create2(name2));

	

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





	struct t2fs_record record[recordsPorBloco];
	read_block(MFTRootTupla[0].logicalBlockNumber, (unsigned char *)record);
	int i;
	printf("\nDiretorio Raiz:\n\n");
	for(i=0;i<recordsPorBloco;i++){
		printf("record %d:\nType val:%hhu\n",i, record[i].TypeVal);
		printf("name: %s\n", record[i].name);
		printf("blocks file size: %u\n", record[i].blocksFileSize);
		printf("bytes file size: %u\n", record[i].bytesFileSize);
		printf("MFT Number: %u\n\n", record[i].MFTNumber);		
	}

	printf("%d\n", delete2(name3));
	printf("%d\n", delete2(name4));


	read_block(MFTRootTupla[0].logicalBlockNumber, (unsigned char *)record);

	printf("\nDiretorio Raiz:\n\n");
	for(i=0;i<recordsPorBloco;i++){
		printf("record %d:\nType val:%hhu\n",i, record[i].TypeVal);
		printf("name: %s\n", record[i].name);
		printf("blocks file size: %u\n", record[i].blocksFileSize);
		printf("bytes file size: %u\n", record[i].bytesFileSize);
		printf("MFT Number: %u\n\n", record[i].MFTNumber);		
	}*/

	return 0;
}
