// Client 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include "tp_socket.c"

void error(const char *msg){
	perror(msg);
	exit(1);
}

void create_packet(int seqnum, char checksum, char* dados, char* packet){
	unsigned char seqnum_bytes[4];
	seqnum_bytes[3] = (seqnum >> 24) & 0xFF;
	seqnum_bytes[2] = (seqnum >> 16) & 0xFF;
	seqnum_bytes[1] = (seqnum >> 8) & 0xFF;
	seqnum_bytes[0] = seqnum & 0xFF;
	memmove(packet, seqnum_bytes, 4);

	packet[4] = checksum;

	//strcat(packet, dados);
	memmove(packet+5, dados, 9);
	printf("Building packet %s \n", packet);

	char new_nome[10];
	memmove(new_nome, packet+5, 10);
	printf("Checksum %c \n", packet[4]);
	printf("Nome enviado: %s \n", new_nome);

}

void create_seqnum_pkg(int seqnum, char *seqnum_pkg){
	unsigned char seqnum_bytes[4];
	seqnum_bytes[3] = (seqnum >> 24) & 0xFF;
	seqnum_bytes[2] = (seqnum >> 16) & 0xFF;
	seqnum_bytes[1] = (seqnum >> 8) & 0xFF;
	seqnum_bytes[0] = seqnum & 0xFF;

	memmove(seqnum_pkg, seqnum_bytes, 4);
}

void extract_packet(char* packet, int seqnum_recebido, char* dados, int total_recebido ){
	//strncpy(ack, packet, 1);  // Esteja ciente que strncpy nao finaliza com '/0'
	//seqnum_recebido =  packet[0];
	//ack[1] = '\0';
	memmove(dados, packet+5, total_recebido-5);
}

int extract_seqnum(char* packet){
	unsigned char seqnum_bytes[4];
	memmove(seqnum_bytes, packet, 4);

	return *(int*)seqnum_bytes;
}

void toggle_ack(char *ack){
	if (strcmp(ack, "0") == 0){
		strcpy(ack, "1");
	}
	else if (strcmp(ack, "1") == 0){
		strcpy(ack, "0");
	}
	else{
		error("Valor de ACK desconhecido");
	}
}

int bytes_to_write(int total_recebido, int tam_cabecalho){
	int write_n;
	if (total_recebido-tam_cabecalho > 0){
		write_n = total_recebido-tam_cabecalho;
	}
	else{
		write_n = 0;
	}
	return write_n;
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

int main(int argc, char **argv){
	// PROCESSANDO ARGUMENTOS DA LINHA DE COMANDO
	if(argc < 5){	
		fprintf(stderr , "Parametros faltando \n");
		printf("Formato: [host_do_servidor], [porta_do_servidor], [nome_do_arquivo], [tamanho_buffer] \n");
		exit(1);
	}

	size_t host_len = strlen(argv[1]) + 1;  
	//converte o numero de porta de string para inteiro
	int porta_do_servidor;
	porta_do_servidor = atoi(argv[2]);
	size_t filename_len = strlen(argv[3]) + 1;
	char *nome_do_servidor = calloc(host_len, sizeof (*nome_do_servidor));
	char *nome_do_arquivo = calloc(filename_len, sizeof (*nome_do_arquivo));
	int tam_buffer = atoi(argv[4]);
	//verifica se o espaço de memória foi alocado com sucesso
	if (!nome_do_servidor) {   
		fprintf (stderr, "Erro de memoria ao alocar 'nome_do_servidor'\n");
		return 1;
    	}
	//salva o nome do servidor (na forma de endereço IP) recbido pela linha de comando na variável nome_do_servidor
	strncpy(nome_do_servidor, argv[1], host_len-1);
	if (!nome_do_arquivo) { 
		fprintf (stderr, "Erro de memoria ao alocar 'nome_do_arquivo'\n");
		return 1;
    	}
	//salva o nome do arquivo recebido pela linha de comando na variável nome_do_arquivo
    strncpy(nome_do_arquivo, argv[3], filename_len);

	printf("Nome do servidor: %s,\n", nome_do_servidor);
	printf("Porta do servidor: %d\n", porta_do_servidor);
	printf("Nome do arquivo: %s, %zu, %zu\n", nome_do_arquivo, sizeof(nome_do_arquivo), strlen(nome_do_arquivo));
	printf("Tamanho do buffer: %d\n", tam_buffer);

	// Inicializa Variaveis
	int seqnum = 0;
	int tam_cabecalho = 5;
	int tam_dados = tam_buffer-tam_cabecalho;
	char *dados = calloc(tam_dados, sizeof (*dados));
	int total_gravado; //recebe o numero em bytes do que foi gravado no arquivo em cada iteracao
	int tam_arquivo = 0; 
	char ack_esperado[] = "0";
	char ack_recebido[2] = { 0 };
	int max_timeouts = 10;
	int timeouts = 0;
	int write_n;
	int N = 3;
	int seqnum_recebido = 1;
	int expected_seqnum = 2;
	char seqnum_pkg[4] = { 0 };

	// INICIALIZANDO TP SOCKET
	tp_init();

	// CRIA UM SOCKET UDP
	int udp_socket;
	unsigned short porta_cliente = 9000;  // numro arbitrario para porta do cliente
	udp_socket = tp_socket(porta_cliente);
	if (udp_socket == -1){
		error("Falha ao criar o socket\n");
	}
	else if (udp_socket == -2){
		error("Falha ao estabelecer endereco (tp_build_addr)\n");
	}
	else if (udp_socket == -3){
		error("Falha de bind\n");
	}

	//ESTABELECENDO ENDERECO DE ENVIO
	so_addr server;
	if (tp_build_addr(&server,nome_do_servidor,porta_do_servidor)< 0){
		error("Falha ao estabelecer endereco do servidor\n");
	}

	// SETA NOME DO ARQUIVO
	char *nome_do_arquivo_pkg = calloc(filename_len+2, sizeof (*nome_do_arquivo_pkg));
	char ack[] = "0";
	char sum = '\0';
	char sum_recebido = '\0';
	sum = checksum(nome_do_arquivo, strlen(nome_do_arquivo));

	create_packet(seqnum, sum, nome_do_arquivo, nome_do_arquivo_pkg);
	printf("Pacote com nome do arquivo %s \n", nome_do_arquivo_pkg);
	
	// REALIZA TEMPORIZACAO ESPARA 1S
	struct timeval tv;
	tv.tv_sec = 1;
	tv.tv_usec = 0;
	if(setsockopt(udp_socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv))<0){
		perror("Error setsockopt\n");}

	struct timeval t1, t2;
	gettimeofday(&t1, 0);
	// ENVIA NOME DO ARQUIVO
	int total_recebido;
	int sent;
	char *buffer = calloc(tam_buffer, sizeof (*buffer));
	do {
		//printf("Envia nome_do_arquivo ......\n");
		printf("Checksum %c, in package %c \n", sum, nome_do_arquivo_pkg[4]);
		printf("Nome do arquivo, strlen, %s, %zu \n", nome_do_arquivo, strlen(nome_do_arquivo));
		tp_sendto(udp_socket, nome_do_arquivo_pkg, strlen(nome_do_arquivo)+tam_cabecalho+1, &server);
		total_recebido = tp_recvfrom(udp_socket, buffer, tam_buffer, &server);  // Esperando seqnum = 0
		seqnum_recebido = extract_seqnum(buffer);
		printf("seqnum_recebido %d \n", seqnum_recebido);
	}while ((total_recebido == -1) || seqnum_recebido != 2);
	printf("OK, server recebeu o nome do arquivo !\n");

	// CONFIRMA INICIO DA CONEXÃO
	seqnum = 1;
    create_seqnum_pkg(seqnum, seqnum_pkg);
	tp_sendto(udp_socket, seqnum_pkg, sizeof(seqnum_pkg), &server); // Manda seqnum = 1

	//ABRE UM ARQUIVO PARA SALVAR OS DADOS RECEBIDOS
	//printf("Nome do arquivo: %s \n", nome_do_arquivo);
	FILE *arq;
	arq = fopen(nome_do_arquivo, "w+");
	if (arq == NULL){
		printf("Problemas na criacao do arquivo");
		exit(1);	
	}
	fflush(arq);

	// RECEBE OS DADOS POR GoBackN
	do {
		do {

			total_recebido = tp_recvfrom(udp_socket, buffer, tam_buffer, &server); // Esperando seq = 2
			if (total_recebido > 0){
				//printf("Antes de seqnum_recebido \n");
				seqnum_recebido = extract_seqnum(buffer);
				printf("Antes de  sum_recebido\n");
				if (total_recebido > 4){
					sum_recebido = extract_checksum(buffer);
					//printf("Antes de extract_packet\n");
					extract_packet(buffer, seqnum_recebido, dados, total_recebido);
					//printf("Antes de  sum\n");
					sum = checksum(dados, total_recebido-tam_cabecalho);
					printf("Buffer recebido: %s \n", buffer);
					printf("seqnum_recebido %d \n", seqnum_recebido);
					printf("seqnum_esperado %d \n", expected_seqnum);
					if (sum != sum_recebido){
						printf("Received a modified package \n");
					}
				}
				if ((seqnum_recebido == expected_seqnum) && (sum == sum_recebido)){
					create_seqnum_pkg(seqnum_recebido, seqnum_pkg); // Cria seq
					tp_sendto(udp_socket, seqnum_pkg, sizeof(seqnum_pkg), &server); // Manda seq = 2
					timeouts = 0;
				}
				else{
					create_seqnum_pkg(seqnum_recebido, seqnum_pkg); // Cria seq
					tp_sendto(udp_socket, seqnum_pkg, sizeof(seqnum_pkg), &server); // Manda seq = 2
					timeouts++;
				}
			}	
			else{
				timeouts++;
			}
			//create_seqnum_pkg(seqnum_recebido, seqnum_pkg); // Cria seq
			//tp_sendto(udp_socket, seqnum_pkg, sizeof(seqnum_pkg), &server); // Manda seq = 2

		}while(((total_recebido == -1) || (seqnum_recebido != expected_seqnum) || (sum != sum_recebido)) && timeouts<=max_timeouts);
		expected_seqnum += 1;

		fflush(arq);
		write_n = bytes_to_write(total_recebido, tam_cabecalho);
		total_gravado = fwrite(dados, 1, write_n, arq);
		tam_arquivo += total_gravado;
		if ((total_gravado != total_recebido-tam_cabecalho) && (total_recebido-tam_cabecalho > 0)){
			printf("Erro na escrita do arquivo.\n");
			exit(1);
		}
		memset(dados, 0, tam_dados);
		memset(seqnum_pkg, 0, 4);

	}while((total_recebido != tam_cabecalho) && timeouts<=max_timeouts);

	printf("Tivemos %d timeouts!!\n", timeouts);
	//FECHA O ARQUIVO
	fclose(arq);
	gettimeofday(&t2, 0);	
	float tempo = (t2.tv_sec - t1.tv_sec) * 1000; //s para ms
	tempo += (t2.tv_usec - t1.tv_usec) / 1000; 
	float taxa = tam_arquivo/tempo;

	// LIBERA OS PONTEIROS ALOCADOS
	printf("Buffer = %i byte(s), %fkbps (%i bytes em %f ms) \n", tam_buffer, taxa, tam_arquivo, tempo);
	free(nome_do_arquivo_pkg);
	free(nome_do_arquivo);
	free(nome_do_servidor);
	free(buffer);
	free(dados);

	return 0;
}
