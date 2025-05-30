#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdint.h>
#include "package.h"

#define LW_BRIDGE_BASE   0xFF200000
#define LW_BRIDGE_SPAN   0x00005000
#define LEDR_BASE        0x00000000
#define RETURN_BASE      0x00000010

#define CABECALHO_BMP 54
#define LARGURA  320     // substitua pelo valor real da largura da imagem
#define ALTURA   240     // substitua pelo valor real da altura da imagem

uint8_t imagem_gray[ALTURA][LARGURA];

void imprimir_matriz_resultado(uint8_t vetorC[25], uint8_t matrix_size_bits) {
    printf("Matriz Resultante:\n");

    int tamanho = matrix_size_bits + 2; // 00->2x2, 01->3x3, 10->4x4, 11->5x5
    for (int linha = 0; linha < tamanho; linha++) {
        for (int coluna = 0; coluna < tamanho; coluna++) {
            printf("%4d ", (int8_t)vetorC[linha * 5 + coluna]);
        }
        printf("\n");
    }
}

uint8_t get_valor(uint8_t matriz[ALTURA][LARGURA], int i, int j) {
    if (i >= 0 && i < ALTURA && j >= 0 && j < LARGURA) {
        return matriz[i][j];
    } else {
        return 0;
    }
}

// Submatrizes 5x5 centradas em cada pixel
void matriz_5x5(volatile uint32_t *LEDR_ptr, uint8_t imagem_gray[ALTURA][LARGURA], uint8_t matrix_size_bits, uint8_t opcode) {

    int8_t matriz[5][5] = {
        { 0, 0, 0, 0, 0 },
        { 0, 0, 0, 0, 0 },
        { 0, 0, 0, 0, 0 },
        { 0, 0, 0, 0, 0 },
        { 0, 0, 0, 0, 0 }
    };

    for (int i = 0; i < ALTURA; i++) {
        for (int j = 0; j < LARGURA; j++) {
            uint8_t submatriz[5][5];
            for (int dx = -2; dx <= 2; dx++) {
                for (int dy = -2; dy <= 2; dy++) {
                    submatriz[dx + 2][dy + 2] = get_valor(imagem_gray, i + dx, j + dy);
                }
            }

            printf("Submatriz 5x5 centrada em (%d, %d):\n", i, j);
            for (int x = 0; x < 5; x++) {
                for (int y = 0; y < 5; y++) {
                    printf("%3d ", submatriz[x][y]);
                }
                printf("\n");
            }

            uint8_t vetorC[5][5];
            uint8_t data = (matrix_size_bits << 3) | (opcode & 0b111);

            enviar_dados_para_fpga(LEDR_ptr, submatriz, (uint8_t (*)[5])matriz, data);
            receber_dados_da_fpga(LEDR_ptr, vetorC);
            imprimir_matriz_resultado((uint8_t *)vetorC, matrix_size_bits);

            
            printf("\n");
        }
    }
}

void print_progress_bar(int current, int total) {
    int width = 25;
    int filled;
    int i;

    if (total == 0) total = 1;
    filled = (current * width) / total;

    printf("\r[");
    for (i = 0; i < width; i++) {
        if (i < filled)
            printf("#");
        else
            printf(" ");
    }
    printf("] %d%%", (current * 100) / total);
    fflush(stdout);
}


int main(void) {
    int fd = -1;
    void *LW_virtual = configurar_mapeamento(&fd);
    if (LW_virtual == NULL) return -1;

    volatile uint32_t *LEDR_ptr = (volatile uint32_t *)(LW_virtual + LEDR_BASE);

    *LEDR_ptr |= (1 << 29);
    *LEDR_ptr &= ~(1 << 31);

    int8_t matrizA[5][5] = {
        { -5, -8, -10, -8, -5 },
        { -4, -10, -20, -10, -4 },
        {  0,   0,   0,   0,  0 },
        {  4,  10,  20,  10,  4 },
        {  5,   8,  10,  8,  5 }
    };

    uint8_t matrizB[5][5] = {
        {25, 24, 23, 22, 21},
        {20, 19, 18, 17, 16},
        {15, 14, 13, 12, 11},
        {10, 9, 8, 7, 6},
        {5, 4, 3, 2, 1}
    };

    printf("Selecione a operação desejada:\n");
    printf("0 = Soma\n");
    printf("1 = Subtração\n");
    printf("2 = Oposta\n");
    printf("3 = Multiplicação\n");
    printf("4 = Transposição\n");
    printf("5 = Determinante\n");
    printf("6 = Multiplicação por inteiro\n");
    printf("Digite o código da operação (0-6): ");

    int op;
    scanf("%d", &op);

    if (op < 0 || op > 6) {
        printf("Operação inválida.\n");
        munmap(LW_virtual, LW_BRIDGE_SPAN);
        close(fd);
        return -1;
    }

    uint8_t matrix_size_bits = 0b11;       // 11 para 5x5
    uint8_t opcode = (uint8_t)op;
    uint8_t data = (matrix_size_bits << 3) | (opcode & 0b000);

    // Carregar imagem BMP em escala de cinza
    FILE *arquivo_bmp = fopen("Fototestez.bmp", "rb");
    if (!arquivo_bmp) {
        perror("Erro ao abrir o arquivo BMP");
        munmap(LW_virtual, LW_BRIDGE_SPAN);
        close(fd);
        return 1;
    }

    fseek(arquivo_bmp, CABECALHO_BMP, SEEK_SET);
    int padding = (4 - (LARGURA * 3) % 4) % 4;

    for (int i = ALTURA - 1; i >= 0; i--) {
        for (int j = 0; j < LARGURA; j++) {
            unsigned char b = fgetc(arquivo_bmp);
            unsigned char g = fgetc(arquivo_bmp);
            unsigned char r = fgetc(arquivo_bmp);
            uint8_t gray = (uint8_t)(0.299 * r + 0.587 * g + 0.114 * b);
            imagem_gray[i][j] = gray;
        }
        fseek(arquivo_bmp, padding, SEEK_CUR);
    }

    fclose(arquivo_bmp);

    // Salvar imagem em arquivo texto e binário
    FILE *saida_txt = fopen("imagem_gray.txt", "w");
    if (!saida_txt) {
        perror("Erro ao criar arquivo de saída texto");
        munmap(LW_virtual, LW_BRIDGE_SPAN);
        close(fd);
        return 1;
    }
    fclose(saida_txt);

    FILE *saida_bin = fopen("imagem_gray.bin", "wb");
    if (!saida_bin) {
        perror("Erro ao criar arquivo binário");
        munmap(LW_virtual, LW_BRIDGE_SPAN);
        close(fd);
        return 1;
    }
    fwrite(imagem_gray, sizeof(uint8_t), ALTURA * LARGURA, saida_bin);
    fclose(saida_bin);

    printf("Conversao para escala de cinza concluida. Arquivos gerados.\n");

    printf("Processando submatrizes 5x5...\n");
    matriz_5x5(LEDR_ptr, imagem_gray, matrix_size_bits, opcode);

    munmap(LW_virtual, LW_BRIDGE_SPAN);
    close(fd);
    return 0;
}
