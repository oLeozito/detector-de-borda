#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define LARGURA 320
#define ALTURA 240
#define CABECALHO_BMP 54

uint8_t imagem_gray[ALTURA][LARGURA];

int main() {
    FILE *arquivo_bmp = fopen("/home/aluno/Documentos/Leonardo/detector-de-borda-main/c-imagem/Fototestez.bmp", "rb");
    if (!arquivo_bmp) {
        perror("Erro ao abrir o arquivo BMP");
        return 1;
    }

    // Ignora cabeçalho do BMP (assumido como 54 bytes para BMP 24 bits)
    fseek(arquivo_bmp, CABECALHO_BMP, SEEK_SET);

    // Cada linha do BMP é alinhada em múltiplos de 4 bytes
    int padding = (4 - (LARGURA * 3) % 4) % 4;

    for (int i = ALTURA - 1; i >= 0; i--) {  // BMP é salvo de baixo pra cima
        for (int j = 0; j < LARGURA; j++) {
            unsigned char b = fgetc(arquivo_bmp);
            unsigned char g = fgetc(arquivo_bmp);
            unsigned char r = fgetc(arquivo_bmp);

            uint8_t gray = (uint8_t)(0.299 * r + 0.587 * g + 0.114 * b);
            imagem_gray[i][j] = gray;
        }
        // Pula o padding no final de cada linha
        fseek(arquivo_bmp, padding, SEEK_CUR);
    }

    fclose(arquivo_bmp);

    // Salva como texto para depuração
    FILE *saida_txt = fopen("imagem_gray.txt", "w");
    if (!saida_txt) {
        perror("Erro ao criar arquivo de saída texto");
        return 1;
    }

    for (int i = 0; i < ALTURA; i++) {
        for (int j = 0; j < LARGURA; j++) {
            fprintf(saida_txt, "%3d ", imagem_gray[i][j]);
        }
        fprintf(saida_txt, "\n");
    }

    fclose(saida_txt);

    // Salva como arquivo binário
    FILE *saida_bin = fopen("imagem_gray.bin", "wb");
    if (!saida_bin) {
        perror("Erro ao criar arquivo binário");
        return 1;
    }

    fwrite(imagem_gray, sizeof(uint8_t), ALTURA * LARGURA, saida_bin);
    fclose(saida_bin);

    printf("Conversão para escala de cinza concluída. Arquivos gerados.\n");
    return 0;
}
