// Server
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include "tp_socket.c"

void error(const char *msg){
	perror(msg);
	exit(1);
}

void create_packet(int seqnum, char checksum, char* dados, char* packet, int total_lido){
	unsigned char seqnum_bytes[4];
	seqnum_bytes[3] = (seqnum >> 24) & 0xFF;
	seqnum_bytes[2] = (seqnum >> 16) & 0xFF;
	seqnum_bytes[1] = (seqnum >> 8) & 0xFF;
	seqnum_bytes[0] = seqnum & 0xFF;
	memmove(packet, seqnum_bytes, 4);

	packet[4] = checksum;

	memmove(packet+5, dados, total_lido);
}

void create_seqnum_pkg(int seqnum, char *seqnum_pkg){
	unsigned char seqnum_bytes[4];
	seqnum_bytes[3] = (seqnum >> 24) & 0xFF;
	seqnum_bytes[2] = (seqnum >> 16) & 0xFF;
	seqnum_bytes[1] = (seqnum >> 8) & 0xFF;
	seqnum_bytes[0] = seqnum & 0xFF;

	memmove(seqnum_pkg, seqnum_bytes, 4);
}

int extract_packet(char* packet, char checksum, char* dados, int count ){
	unsigned char seqnum_bytes[4];
	memmove(seqnum_bytes, packet, 4);

	//checksum = &(packet[4]);

	memmove(dados, packet+5, count-5+1);//strlen(packet)-4);
	printf("Dados em extract %s \n", dados);

	return *(int*)seqnum_bytes;

}

int extract_seqnum(char* packet){
	unsigned char seqnum_bytes[4];
	memmove(seqnum_bytes, packet, 4);

	return *(int*)seqnum_bytes;
}

char checksum(char* s, int total_lido ){
	char sum = 1;
	int i = 0;

	while (i < total_lido)
	{	
		sum += *s;
		s++;
		i++;
	}
	return sum;
}

char extract_checksum(char* packet){
	return packet[4];
}

int main(int argc, char * argv[]){
	// PROCESSANDO ARGUMENTOS DA LINHA DE COMANDO
	if(argc < 3){	
		fprintf(stderr , "Parametros faltando \n");
		printf("Formato: [porta_do_servidor], [tamanho_buffer] \n");
		exit(1);
	}
	int portno = atoi(argv[1]);
	int tam_buffer = atoi(argv[2]);

	printf("Porta do servidor: %d\n", portno);
	printf("Tamanho do buffer: %d\n", tam_buffer);

	//INICIALIZA VARIAVEIS
	so_addr cliente;  // FROM
	char nome_do_arquivo[256] = { 0 };
	char pacote_com_nome[256] = { 0 };
	char ack_recebido[2] = { 0 };
	int count;
	char sum = '\0';
	char sum_recebido = '\0';
	int seqnum = 0;
	int seqnum_recebido;
	int N = 1;
	int base = 1;
	int next_seqnum = 1;
	char seqnum_pkg[5] = { 0 };
	int tam_cabecalho = 5;
	char *buffer = calloc(tam_buffer, sizeof (*buffer));

	// INICIALIZANDO TP SOCKET
	tp_init();

	// CRIANDO SOCKET UDP
	int udp_socket;
	udp_socket = tp_socket(portno);
	if (udp_socket == -1){
		error("Falha ao criar o socket \n");
	}
	else if (udp_socket == -2){
		error("Falha ao estabelecer endereco (tp_build_addr) \n");
	}
	else if (udp_socket == -3){
		error("Falha de bind \n");
	}

	// INICIALIZA TEMPORIZACAO
	struct timeval tv;
	tv.tv_sec = 1;
	tv.tv_usec = 0;
	if(setsockopt(udp_socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv))<0){
		perror("Error setsockopt \n");
	}

	// REBEBE UM BUFFER COM NOME DO ARQUIVO 
	do {
		count = tp_recvfrom(udp_socket, pacote_com_nome, sizeof(nome_do_arquivo), &cliente);
		if (count > 0){
			printf("Recebidos %d bytesssssss \n", count);
			//extrai a substring com o nome do arquivo, sem o ACK
			printf("Pacote com nome %s \n", pacote_com_nome);
			sum_recebido = extract_checksum(pacote_com_nome);
			seqnum_recebido = extract_packet(pacote_com_nome, sum_recebido, nome_do_arquivo, count);
			printf("Nome recebido: %s \n", nome_do_arquivo);
			printf("seqnum_recebido : %d \n", seqnum_recebido);
			printf("sum recebido %c \n", sum_recebido);
			sum = checksum(nome_do_arquivo, strlen(nome_do_arquivo));
			printf("Chacksum recebido %c, checkum calculado %c, iguais %d \n", sum_recebido, sum, sum != sum_recebido);
		}

	}while((count == -1) || (sum != sum_recebido));

	// AGUARDANDO ACK DO NOME DO ARQUIVO
	//create_seqnum_pkg(seqnum, seqnum_pkg);
	//do {
	//	//tp_sendto(udp_socket, seqnum_pkg, sizeof(seqnum_pkg), &cliente); // seqnum = 0
	//	count = tp_recvfrom(udp_socket, buffer, sizeof(buffer), &cliente);  // Esperando  seqnum = 1
	//	seqnum_recebido = extract_seqnum(buffer);

	//}while ((count == -1) || (seqnum_recebido != 1) ); 
	//count = -1;
	// Exibe mensagem
	//printf("Cliente confirmou inicio da conexao! Comecaremos a enviar dados\n");

	// ABRE O ARQUIVO
	FILE *arq;
	arq = fopen(nome_do_arquivo, "r");
	if(arq == NULL){
		printf("Erro na abertura do arquivo.\n");
		exit(1);
	}
	int total_lido;
	int tam_dados = tam_buffer-tam_cabecalho;
	char *dados = calloc(tam_dados, sizeof (*dados));

	//ROTINA GoBakN - LOOP PRINCIPAL
	do {
		total_lido = fread(dados, 1, tam_dados, arq);
		sum = checksum(dados, total_lido);

		do {
			create_packet(base, sum, dados, buffer, total_lido);
			tp_sendto(udp_socket, buffer, total_lido + tam_cabecalho, &cliente); 
			printf("Enviado seqnum %d \n", base);
			count = tp_recvfrom(udp_socket, seqnum_pkg, sizeof(seqnum_pkg), &cliente); 
			if (count > 0){
				seqnum_recebido = extract_seqnum(seqnum_pkg);
				printf("Recebido seqnum %d \n", seqnum_recebido);
				sum_recebido = extract_checksum(seqnum_pkg);
				sum = checksum(seqnum_pkg, 4);
				printf("checksum_recebido:%c\nchecksum calculado:%c\n%d\n", sum_recebido, sum, sum_recebido==sum);
				if(sum == sum_recebido){
					printf("seq recebido:%d",seqnum_recebido);
					base = seqnum_recebido + 1;
					printf("Base, %d next_seqnum %d \n", base, next_seqnum);
				}
			}
		} while(base == next_seqnum);
		next_seqnum ++;
		memset(buffer, 0, tam_buffer);
		memset(dados, 0, tam_dados);
		memset(seqnum_pkg, 0, 4);
	}while(total_lido != 0);

	//FECHA O ARQUIVO
	fclose(arq);

	// LIBERA OS PONTEIROS ALOCADOS
	free(buffer);
	free(dados);

	return 0;
}
