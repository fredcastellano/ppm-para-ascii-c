#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Largura maxima da arte ASCII, em colunas/caracteres */
#define OUT_WIDTH_MAX 200

/* Corrige a proporcao da imagem: um caractere no terminal e mais alto do
   que largo, entao precisamos de menos linhas do que colunas equivalentes */
#define CHAR_ASPECT 0.43

/* Paleta de caracteres do mais escuro (@) ao mais claro (espaco) */
static const char ASCII_CHARS[] = "@%#*+=-:. ";
#define NUM_CHARS (sizeof(ASCII_CHARS) - 1)

/* Le o cabecalho do PPM pulando espacos, tabs, quebras de linha e
   comentarios (que comecam com '#'), e retorna o proximo caractere util */
static int proximo_token(FILE *f) {
    int c = fgetc(f);
    for (;;) {
        while (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            c = fgetc(f);
        }
        if (c == '#') {
            while (c != '\n' && c != EOF) {
                c = fgetc(f);
            }
            continue;
        }
        break;
    }
    return c;
}

/* Le um numero inteiro do cabecalho do PPM (largura, altura ou maxval) */
static int ler_inteiro(FILE *f) {
    int c = proximo_token(f);
    int valor = 0;
    while (c >= '0' && c <= '9') {
        valor = valor * 10 + (c - '0');
        c = fgetc(f);
    }
    ungetc(c, f);
    return valor;
}

/* Espera o usuario apertar ENTER antes de encerrar, para a janela do
   terminal nao fechar sozinha (com sucesso ou erro) */
static void pausar_e_sair(int codigo) {
    int c;

    /* Descarta o que sobrou na linha digitada antes (ex.: o '\n' deixado
       pelo scanf), para que o getchar abaixo realmente espere o ENTER.
       fflush(stdin) nao serve aqui: o padrao C nao define seu comportamento. */
    while ((c = getchar()) != '\n' && c != EOF) {
    }

    printf("\nPressione ENTER para sair...");
    getchar();
    exit(codigo);
}

int main(void) {
    /* --- Leitura do nome do arquivo e abertura em modo binario --- */
    char nome_arquivo[260];
    printf("Digite o nome do arquivo de imagem PPM: ");
    if (scanf("%259s", nome_arquivo) != 1) {
        printf("Nome de arquivo invalido.\n");
        pausar_e_sair(1);
    }

    FILE *entrada = fopen(nome_arquivo, "rb");
    if (entrada == NULL) {
        printf("Erro: nao foi possivel abrir o arquivo '%s'.\n", nome_arquivo);
        pausar_e_sair(1);
    }

    /* --- Validacao da assinatura do formato (PPM binario = "P6") --- */
    char magico[3] = {0};
    magico[0] = (char) fgetc(entrada);
    magico[1] = (char) fgetc(entrada);

    if (magico[0] != 'P' || magico[1] != '6') {
        printf("Erro: '%s' nao e um arquivo PPM binario (P6) valido.\n", nome_arquivo);
        fclose(entrada);
        pausar_e_sair(1);
    }

    /* --- Leitura do restante do cabecalho: largura, altura e maxval --- */
    int largura = ler_inteiro(entrada);
    int altura = ler_inteiro(entrada);
    int valor_max = ler_inteiro(entrada);
    fgetc(entrada); /* consome o unico espaco em branco apos o maxval */

    if (largura <= 0 || altura <= 0 || valor_max <= 0) {
        printf("Erro: cabecalho do arquivo PPM invalido.\n");
        fclose(entrada);
        pausar_e_sair(1);
    }

    /* --- Carregamento dos pixels (cada pixel tem 3 bytes: R, G, B) --- */
    size_t total_bytes = (size_t) largura * altura * 3;
    unsigned char *pixels = malloc(total_bytes);
    if (pixels == NULL) {
        printf("Erro: memoria insuficiente para carregar a imagem.\n");
        fclose(entrada);
        pausar_e_sair(1);
    }

    if (fread(pixels, 1, total_bytes, entrada) != total_bytes) {
        printf("Erro: dados de pixel incompletos no arquivo.\n");
        free(pixels);
        fclose(entrada);
        pausar_e_sair(1);
    }
    fclose(entrada);

    /* --- Calculo do tamanho da arte ASCII, respeitando o limite de
       colunas e corrigindo a proporcao com CHAR_ASPECT --- */
    int saida_largura = (largura < OUT_WIDTH_MAX) ? largura : OUT_WIDTH_MAX;
    int saida_altura = (int) (((double) altura / largura) * saida_largura * CHAR_ASPECT);
    if (saida_altura < 1) {
        saida_altura = 1;
    }

    /* Matriz de caracteres que vai guardar o resultado, uma linha por vez */
    char *arte[saida_altura > 0 ? saida_altura : 1];
    for (int linha = 0; linha < saida_altura; linha++) {
        arte[linha] = malloc(saida_largura + 1);
        if (arte[linha] == NULL) {
            printf("Erro: memoria insuficiente para a arte ASCII.\n");
            for (int k = 0; k < linha; k++) {
                free(arte[k]);
            }
            free(pixels);
            pausar_e_sair(1);
        }
    }

    /* --- Algoritmo de conversao: para cada posicao da arte ASCII, calcula
       o bloco de pixels correspondente na imagem original, tira a media de
       R, G e B desse bloco e converte a luminancia em um caractere --- */
    for (int linha = 0; linha < saida_altura; linha++) {
        /* Faixa de linhas de pixels (y0 a y1) que correspondem a esta linha da arte */
        int y0 = (int) ((double) linha * altura / saida_altura);
        int y1 = (int) ((double) (linha + 1) * altura / saida_altura);
        if (y1 <= y0) {
            y1 = y0 + 1;
        }
        if (y1 > altura) {
            y1 = altura;
        }

        for (int coluna = 0; coluna < saida_largura; coluna++) {
            /* Faixa de colunas de pixels (x0 a x1) que correspondem a esta coluna da arte */
            int x0 = (int) ((double) coluna * largura / saida_largura);
            int x1 = (int) ((double) (coluna + 1) * largura / saida_largura);
            if (x1 <= x0) {
                x1 = x0 + 1;
            }
            if (x1 > largura) {
                x1 = largura;
            }

            /* Soma R, G e B de todos os pixels do bloco para depois tirar a media */
            long soma_r = 0, soma_g = 0, soma_b = 0;
            long quantidade = 0;
            for (int y = y0; y < y1; y++) {
                for (int x = x0; x < x1; x++) {
                    size_t indice = ((size_t) y * largura + x) * 3;
                    soma_r += pixels[indice];
                    soma_g += pixels[indice + 1];
                    soma_b += pixels[indice + 2];
                    quantidade++;
                }
            }

            double r_medio = (double) soma_r / quantidade;
            double g_medio = (double) soma_g / quantidade;
            double b_medio = (double) soma_b / quantidade;

            /* Luminancia: formula padrao que da mais peso ao verde, porque
               o olho humano e mais sensivel a essa cor */
            double luminancia = 0.299 * r_medio + 0.587 * g_medio + 0.114 * b_medio;

            /* Converte a luminancia (0 a valor_max) em um indice da paleta
               de caracteres (pixels claros -> espaco, escuros -> '@') */
            int indice_char = (int) ((luminancia / valor_max) * (NUM_CHARS - 1));
            if (indice_char < 0) {
                indice_char = 0;
            }
            if (indice_char >= (int) NUM_CHARS) {
                indice_char = (int) NUM_CHARS - 1;
            }

            arte[linha][coluna] = ASCII_CHARS[indice_char];
        }
        arte[linha][saida_largura] = '\0';
    }

    free(pixels); /* os pixels originais ja nao sao mais necessarios */

    /* --- Exibicao da arte ASCII na tela --- */
    printf("\n");
    for (int linha = 0; linha < saida_altura; linha++) {
        printf("%s\n", arte[linha]);
    }

    /* --- Geracao do nome do arquivo de saida: mesma base, extensao .txt --- */
    char nome_saida[270];
    strncpy(nome_saida, nome_arquivo, sizeof(nome_saida) - 1);
    nome_saida[sizeof(nome_saida) - 1] = '\0';
    char *ponto = strrchr(nome_saida, '.');
    if (ponto != NULL) {
        strcpy(ponto, ".txt");
    } else {
        strcat(nome_saida, ".txt");
    }

    /* --- Gravacao da arte ASCII no arquivo de saida --- */
    FILE *saida = fopen(nome_saida, "w");
    if (saida == NULL) {
        printf("Erro: nao foi possivel criar o arquivo de saida '%s'.\n", nome_saida);
        for (int linha = 0; linha < saida_altura; linha++) {
            free(arte[linha]);
        }
        pausar_e_sair(1);
    }

    for (int linha = 0; linha < saida_altura; linha++) {
        fprintf(saida, "%s\n", arte[linha]);
        free(arte[linha]);
    }
    fclose(saida);

    printf("\nArquivo ASCII art salvo em: %s\n", nome_saida);

    pausar_e_sair(0);
    return 0;
}
