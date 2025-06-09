/*
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#define WIDTH 320
#define HEIGHT 240

// Função para salvar imagem em escala de cinza no formato PGM (P2 ou P5)
int salvar_imagem_pgm(const char* nome_arquivo, uint8_t imagem[HEIGHT][WIDTH]) {
    FILE* fp = fopen(nome_arquivo, "wb");
    if (!fp) {
        perror("Erro ao abrir arquivo para escrita");
        return -1;
    }

    // Cabeçalho do PGM em modo binário (P5)
    fprintf(fp, "P5\n%d %d\n255\n", WIDTH, HEIGHT);

    // Escrita binária dos pixels
    for (int i = 0; i < HEIGHT; i++) {
        fwrite(imagem[i], sizeof(uint8_t), WIDTH, fp);
    }

    fclose(fp);
    return 0;
}
    */