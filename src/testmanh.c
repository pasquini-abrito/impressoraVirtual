#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>
#include <stdlib.h>

#define BUF 4096

FILE *glog = NULL;
char* impressora_selecionada = NULL;

// Variáveis para medir tempo de execução
clock_t tempo_inicio;
clock_t tempo_recebimento_zpl;
clock_t tempo_traducao;
clock_t tempo_impressao;

// ------------------------------------------------------------------
// LOG
// ------------------------------------------------------------------
void log_open()
{
    glog = fopen("tradutor.log", "a");
    if (!glog) {
        MessageBoxA(NULL, "Falha ao abrir tradutor.log", "Erro", MB_ICONERROR);
        exit(1);
    }
}

void log_write(const char *msg)
{
    if (!glog) return;

    time_t t = time(NULL);
    struct tm lt;
    localtime_s(&lt, &t);

    fprintf(glog, "[%04d-%02d-%02d %02d:%02d:%02d] %s\n",
            lt.tm_year+1900, lt.tm_mon+1, lt.tm_mday,
            lt.tm_hour, lt.tm_min, lt.tm_sec,
            msg);
    fflush(glog);
}

void log_printf(const char *fmt, ...)
{
    if (!glog) return;

    time_t t = time(NULL);
    struct tm lt;
    localtime_s(&lt, &t);

    fprintf(glog, "[%04d-%02d-%02d %02d:%02d:%02d] ",
            lt.tm_year+1900, lt.tm_mon+1, lt.tm_mday,
            lt.tm_hour, lt.tm_min, lt.tm_sec);

    va_list ap;
    va_start(ap, fmt);
    vfprintf(glog, fmt, ap);
    va_end(ap);

    fprintf(glog, "\n");
    fflush(glog);
}

// -----------------------------------------------------------
// Funções para medir tempo
// -----------------------------------------------------------
void iniciar_tempo()
{
    tempo_inicio = clock();
    log_write("=== INICIO DA EXECUCAO ===");
    log_printf("Tempo inicial: %lu ms", tempo_inicio);
}

void log_tempo_recebimento_zpl()
{
    tempo_recebimento_zpl = clock();
    double tempo_decorrido = ((double)(tempo_recebimento_zpl - tempo_inicio)) / CLOCKS_PER_SEC * 1000;
    log_printf("ZPL recebido apos: %.2f ms", tempo_decorrido);
}

void log_tempo_traducao()
{
    tempo_traducao = clock();
    double tempo_decorrido = ((double)(tempo_traducao - tempo_recebimento_zpl)) / CLOCKS_PER_SEC * 1000;
    log_printf("Traducao concluida em: %.2f ms", tempo_decorrido);
}

void log_tempo_impressao()
{
    tempo_impressao = clock();
    
    double tempo_total = ((double)(tempo_impressao - tempo_inicio)) / CLOCKS_PER_SEC * 1000;
    double tempo_traducao_only = ((double)(tempo_traducao - tempo_recebimento_zpl)) / CLOCKS_PER_SEC * 1000;
    double tempo_impressao_only = ((double)(tempo_impressao - tempo_traducao)) / CLOCKS_PER_SEC * 1000;
    
    log_printf("=== TEMPOS DE EXECUCAO ===");
    log_printf("Tempo total: %.2f ms", tempo_total);
    log_printf("Tempo traducao: %.2f ms", tempo_traducao_only);
    log_printf("Tempo impressao: %.2f ms", tempo_impressao_only);
    log_printf("Tempo desde inicio: %.2f ms", tempo_total);
    log_printf("=== FIM DA EXECUCAO ===");
}

void log_tempo_apenas_arquivo()
{
    clock_t tempo_final = clock();
    double tempo_total = ((double)(tempo_final - tempo_inicio)) / CLOCKS_PER_SEC * 1000;
    double tempo_traducao_only = ((double)(tempo_traducao - tempo_recebimento_zpl)) / CLOCKS_PER_SEC * 1000;
    
    log_printf("=== TEMPOS DE EXECUCAO (APENAS ARQUIVO) ===");
    log_printf("Tempo total: %.2f ms", tempo_total);
    log_printf("Tempo traducao: %.2f ms", tempo_traducao_only);
    log_printf("Tempo desde inicio: %.2f ms", tempo_total);
    log_printf("=== FIM DA EXECUCAO ===");
}

// -----------------------------------------------------------
// Função auxiliar
// -----------------------------------------------------------
int get_num(const char *src, const char *cmd)
{
    const char *p = strstr(src, cmd);
    if (!p) return -1;
    p += strlen(cmd);
    return atoi(p);
}

// -----------------------------------------------------------
// TRADUTOR ZPL2 → BPLB AVANÇADO
// -----------------------------------------------------------

// Função para extrair string de comando ZPL
void get_zpl_string(const char *zpl, const char *command, char *output, int max_len) {
    output[0] = '\0';
    char *p = strstr(zpl, command);
    if (!p) return;
    
    p += strlen(command);
    int i = 0;
    while (i < max_len - 1 && p[i] && p[i] != '^' && p[i] != '~' && p[i] != '\r' && p[i] != '\n') {
        output[i] = p[i];
        i++;
    }
    output[i] = '\0';
}

// Processa configurações globais da etiqueta
void process_global_settings(const char *zpl, FILE *out) {
    fprintf(out, "N\n"); // limpa buffer
    
    // MD - densidade de impressão
    int md = get_num(zpl, "^MD");
    if (md >= 0) {
        fprintf(out, "D%d\n", md);
        log_printf("MD (Densidade): %d", md);
    }
    
    // PW - largura da etiqueta
    int pw = get_num(zpl, "^PW");
    if (pw >= 0) {
        fprintf(out, "q%d\n", pw);
        log_printf("PW (Largura): %d", pw);
    }
    
    // LH - posição inicial
    int lh_x = 0, lh_y = 0;
    char *lh_pos = strstr(zpl, "^LH");
    if (lh_pos && sscanf(lh_pos, "^LH%d,%d", &lh_x, &lh_y) == 2) {
        fprintf(out, "// LH: x=%d, y=%d\n", lh_x, lh_y);
        log_printf("LH (Home): %d,%d", lh_x, lh_y);
    }
    
    // MM - modo de impressão
    char mm_mode = 'T';
    char *mm_pos = strstr(zpl, "^MM");
    if (mm_pos) {
        mm_mode = mm_pos[3];
        fprintf(out, "// MM (Modo): %c\n", mm_mode);
    }
    
    fprintf(out, "S3\n"); // velocidade padrão
}

// Processa campos de texto
void process_text_field(const char *field_start, FILE *out) {
    int x = 0, y = 0;
    char font_type = '0', orientation = 'N';
    int height = 10, width = 10;
    char text[1024] = {0};
    
    // Extrai FO
    char *fo_pos = strstr(field_start, "^FO");
    if (fo_pos) {
        sscanf(fo_pos, "^FO%d,%d", &x, &y);
    }
    
    // Extrai A (fonte)
    char *a_pos = strstr(field_start, "^A");
    if (a_pos) {
        if (sscanf(a_pos, "^A%c%c,%d,%d", &font_type, &orientation, &height, &width) < 2) {
            sscanf(a_pos, "^A%c", &font_type);
        }
    }
    
    // Extrai FD (texto)
    char *fd_pos = strstr(field_start, "^FD");
    if (fd_pos) {
        fd_pos += 3;
        int i = 0;
        while (fd_pos[i] && fd_pos[i] != '^' && i < 1023) {
            // Para no próximo comando ou FS
            if (fd_pos[i] == '^' && fd_pos[i+1] == 'F' && fd_pos[i+2] == 'S') break;
            if (fd_pos[i] == '^' && fd_pos[i+1] == 'F' && fd_pos[i+2] == 'D') break;
            text[i] = fd_pos[i];
            i++;
        }
        text[i] = '\0';
    }
    
    if (strlen(text) > 0) {
        // Mapeamento de fontes ZPL para BPLB
        char bplb_font = '3';
        switch(font_type) {
            case '0': bplb_font = '1'; break; // pequena
            case '1': bplb_font = '2'; break; // média
            case '2': bplb_font = '3'; break; // grande
            case '3': bplb_font = '4'; break; // extra grande
            default: bplb_font = '3'; break;
        }
        
        // Mapeamento de orientação
        int rotation = 0;
        switch(orientation) {
            case 'N': rotation = 0; break;
            case 'R': rotation = 90; break;
            case 'I': rotation = 180; break;
            case 'B': rotation = 270; break;
            default: rotation = 0; break;
        }
        
        fprintf(out, "A%d,%d,%d,%c,1,1,N,\"%s\"\n", x, y, rotation, bplb_font, text);
        log_printf("Texto: [%d,%d] fonte=%c '%s'", x, y, bplb_font, text);
    }
}

// Processa código de barras
void process_barcode_field(const char *field_start, FILE *out) {
    int x = 0, y = 0;
    char barcode_type[10] = {0};
    int height = 10;
    char data[256] = {0};
    
    // Extrai FO
    char *fo_pos = strstr(field_start, "^FO");
    if (fo_pos) {
        sscanf(fo_pos, "^FO%d,%d", &x, &y);
    }
    
    // Detecta tipo de código de barras
    if (strstr(field_start, "^BC")) {
        strcpy(barcode_type, "BC");
        // Extrai altura do BC
        char *bc_pos = strstr(field_start, "^BC");
        if (bc_pos) {
            char orientation;
            if (sscanf(bc_pos, "^BC%c,%d", &orientation, &height) == 2) {
                // altura extraída
            } else {
                height = 10; // padrão
            }
        }
    }
    else if (strstr(field_start, "^B2")) {
        strcpy(barcode_type, "B2");
        height = get_num(field_start, "^B2");
        if (height <= 0) height = 10;
    }
    else if (strstr(field_start, "^BE")) {
        strcpy(barcode_type, "BE");
        height = get_num(field_start, "^BE");
        if (height <= 0) height = 10;
    }
    else if (strstr(field_start, "^BEN") || strstr(field_start, "^BCN")) {
        strcpy(barcode_type, "BCN");
        height = 15; // padrão para code 128
    }
    
    // Extrai dados
    char *fd_pos = strstr(field_start, "^FD");
    if (fd_pos) {
        fd_pos += 3;
        int i = 0;
        while (fd_pos[i] && fd_pos[i] != '^' && i < 255) {
            if (fd_pos[i] == '^' && fd_pos[i+1] == 'F' && fd_pos[i+2] == 'S') break;
            data[i] = fd_pos[i];
            i++;
        }
        data[i] = '\0';
    }
    
    if (strlen(data) > 0) {
        // Mapeamento de tipos de código de barras
        int bplb_type = 1; // Code 128 padrão
        
        if (strcmp(barcode_type, "B2") == 0) {
            bplb_type = 3; // Interleaved 2 of 5
        } else if (strcmp(barcode_type, "BE") == 0) {
            bplb_type = 8; // EAN-13
        }
        
        fprintf(out, "B%d,%d,0,%d,2,4,%d,B,\"%s\"\n", x, y, bplb_type, height, data);
        log_printf("Código Barras [%s]: [%d,%d] h=%d '%s'", barcode_type, x, y, height, data);
    }
}

// Processa elementos gráficos
void process_graphic_field(const char *field_start, FILE *out) {
    int x = 0, y = 0;
    
    // Extrai FO
    char *fo_pos = strstr(field_start, "^FO");
    if (fo_pos) {
        sscanf(fo_pos, "^FO%d,%d", &x, &y);
    }
    
    // GB - Caixa/Retângulo
    if (strstr(field_start, "^GB")) {
        int width = 100, height = 50, thickness = 2;
        char *gb_pos = strstr(field_start, "^GB");
        if (gb_pos) {
            sscanf(gb_pos, "^GB%d,%d,%d", &width, &height, &thickness);
        }
        fprintf(out, "X%d,%d,%d,%d,%d\n", x, y, width, height, thickness);
        log_printf("Caixa: [%d,%d] %dx%d esp=%d", x, y, width, height, thickness);
    }
    
    // GC - Círculo
    else if (strstr(field_start, "^GC")) {
        int diameter = 50, thickness = 2;
        char *gc_pos = strstr(field_start, "^GC");
        if (gc_pos) {
            sscanf(gc_pos, "^GC%d,%d", &diameter, &thickness);
        }
        fprintf(out, "// Círculo: [%d,%d] diam=%d esp=%d\n", x, y, diameter, thickness);
        log_printf("Círculo: [%d,%d] diam=%d", x, y, diameter);
    }
}

// Processa imagens
void process_image_field(const char *field_start, FILE *out) {
    int x = 0, y = 0;
    char image_name[128] = {0};
    
    // Extrai FO
    char *fo_pos = strstr(field_start, "^FO");
    if (fo_pos) {
        sscanf(fo_pos, "^FO%d,%d", &x, &y);
    }
    
    // XG - Recuperar imagem
    if (strstr(field_start, "^XG")) {
        char *xg_pos = strstr(field_start, "^XG");
        if (xg_pos) {
            // Extrai nome do arquivo (até próxima vírgula ou espaço)
            int i = 3; // Pula "^XG"
            while (xg_pos[i] && xg_pos[i] != ',' && xg_pos[i] != '^' && xg_pos[i] != ' ' && i < 127) {
                image_name[i-3] = xg_pos[i];
                i++;
            }
            image_name[i-3] = '\0';
        }
        
        if (strlen(image_name) > 0) {
            fprintf(out, "GG%d,%d,\"%s\"\n", x, y, image_name);
            log_printf("Imagem: [%d,%d] '%s'", x, y, image_name);
        }
    }
}

// Função principal de tradução AVANÇADA
void traduz_zpl_para_bplb_avancado(char *zpl, FILE *out) {
    log_write("Iniciando tradução ZPL2 -> BPLB avançada");
    
    // Processa configurações globais
    process_global_settings(zpl, out);
    
    // Divide o ZPL em campos (separados por ^FS)
    char *field_start = zpl;
    char *field_end;
    int field_count = 0;
    
    while ((field_end = strstr(field_start, "^FS")) != NULL) {
        // Calcula tamanho do campo atual
        int field_len = field_end - field_start;
        if (field_len <= 0) {
            field_start = field_end + 3;
            continue;
        }
        
        // Copia campo para buffer temporário
        char field_buffer[1024] = {0};
        strncpy(field_buffer, field_start, field_len);
        field_buffer[field_len] = '\0';
        
        // Classifica e processa o campo
        if (strstr(field_buffer, "^FO") && strstr(field_buffer, "^FD")) {
            if (strstr(field_buffer, "^BC") || strstr(field_buffer, "^B2") || 
                strstr(field_buffer, "^BE") || strstr(field_buffer, "^BEN")) {
                process_barcode_field(field_buffer, out);
            }
            else if (strstr(field_buffer, "^GB") || strstr(field_buffer, "^GC")) {
                process_graphic_field(field_buffer, out);
            }
            else if (strstr(field_buffer, "^XG")) {
                process_image_field(field_buffer, out);
            }
            else if (strstr(field_buffer, "^A") || strstr(field_buffer, "^FD")) {
                process_text_field(field_buffer, out);
            }
        }
        
        field_count++;
        field_start = field_end + 3; // Pula ^FS
    }
    
    // Processa quantidade de cópias (PQ)
    int pq = get_num(zpl, "^PQ");
    if (pq < 1) pq = 1;
    
    fprintf(out, "P%d\n", pq);
    log_printf("Quantidade: %d cópias", pq);
    
    log_printf("Tradução concluída. %d campos processados.", field_count);
}

// -----------------------------------------------------------
// Tradutor ZPL → BPLB Básico (mantido para compatibilidade)
// -----------------------------------------------------------
void traduz_zpl_para_bplb(char *zpl, FILE *out)
{
    // Usa a versão avançada por padrão
    traduz_zpl_para_bplb_avancado(zpl, out);
}

// -----------------------------------------------------------
// Listar todas as impressoras disponíveis
// -----------------------------------------------------------
char** listar_impressoras(int* count)
{
    char** impressoras = NULL;
    *count = 0;
    
    DWORD needed = 0, returned = 0;
    
    // Primeiro obtemos o tamanho necessário
    EnumPrinters(PRINTER_ENUM_LOCAL | PRINTER_ENUM_CONNECTIONS, NULL, 2, 
                 NULL, 0, &needed, &returned);
    
    if (needed > 0 && returned > 0) {
        BYTE* printer_info = (BYTE*)malloc(needed);
        
        if (EnumPrinters(PRINTER_ENUM_LOCAL | PRINTER_ENUM_CONNECTIONS, NULL, 2, 
                        printer_info, needed, &needed, &returned)) {
            
            PRINTER_INFO_2* printers = (PRINTER_INFO_2*)printer_info;
            
            // Alocar array para os nomes
            impressoras = (char**)malloc(returned * sizeof(char*));
            
            for (DWORD i = 0; i < returned; i++) {
                if (printers[i].pPrinterName) {
                    impressoras[i] = _strdup(printers[i].pPrinterName);
                    (*count)++;
                }
            }
        }
        free(printer_info);
    }
    
    return impressoras;
}

// -----------------------------------------------------------
// Mostrar menu de seleção de impressora
// -----------------------------------------------------------
char* selecionar_impressora_menu()
{
    int count = 0;
    char** impressoras = listar_impressoras(&count);
    
    if (count == 0) {
        printf("Nenhuma impressora encontrada!\n");
        log_write("Nenhuma impressora encontrada no sistema");
        return NULL;
    }
    
    printf("\n=== IMPRESSORAS DISPONIVEIS ===\n");
    for (int i = 0; i < count; i++) {
        printf("[%d] - %s\n", i + 1, impressoras[i]);
    }
    printf("[0] - Nenhuma (apenas salvar arquivo)\n");
    printf("\nSelecione a impressora: ");
    
    int escolha;
    if (scanf("%d", &escolha) != 1) {
        // Limpar buffer de entrada em caso de erro
        int c;
        while ((c = getchar()) != '\n' && c != EOF);
        printf("Entrada invalida!\n");
        
        // Liberar memória
        for (int i = 0; i < count; i++) free(impressoras[i]);
        free(impressoras);
        return NULL;
    }
    
    char* selecionada = NULL;
    
    if (escolha == 0) {
        printf("Modo apenas arquivo selecionado.\n");
        log_write("Usuario selecionou: Nenhuma impressora (apenas arquivo)");
    }
    else if (escolha >= 1 && escolha <= count) {
        selecionada = _strdup(impressoras[escolha - 1]);
        printf("Impressora selecionada: %s\n", selecionada);
        log_printf("Usuario selecionou impressora: %s", selecionada);
    } else {
        printf("Opcao invalida!\n");
        log_printf("Usuario selecionou opcao invalida: %d", escolha);
    }
    
    // Liberar memória
    for (int i = 0; i < count; i++) free(impressoras[i]);
    free(impressoras);
    
    return selecionada;
}

// -----------------------------------------------------------
// Menu principal de configuração
// -----------------------------------------------------------
void menu_configuracao()
{
    int opcao;
    
    do {
        printf("\n=== CONFIGURACAO DA IMPRESSORA ===\n");
        if (impressora_selecionada) {
            printf("Impressora atual: %s\n", impressora_selecionada);
        } else {
            printf("Impressora atual: NENHUMA (apenas salvar arquivo)\n");
        }
        printf("\n[1] - Selecionar/alterar impressora\n");
        printf("[2] - Remover impressora selecionada\n");
        printf("[3] - Iniciar monitoramento COM4\n");
        printf("[4] - Sair\n");
        printf("\nEscolha: ");
        
        if (scanf("%d", &opcao) != 1) {
            // Limpar buffer de entrada em caso de erro
            int c;
            while ((c = getchar()) != '\n' && c != EOF);
            printf("Entrada invalida!\n");
            continue;
        }
        
        switch (opcao) {
            case 1: {
                if (impressora_selecionada) {
                    free(impressora_selecionada);
                    impressora_selecionada = NULL;
                }
                char* nova = selecionar_impressora_menu();
                if (nova) {
                    impressora_selecionada = nova;
                }
                break;
            }
            case 2:
                if (impressora_selecionada) {
                    printf("Impressora '%s' removida.\n", impressora_selecionada);
                    log_printf("Usuario removeu impressora: %s", impressora_selecionada);
                    free(impressora_selecionada);
                    impressora_selecionada = NULL;
                } else {
                    printf("Nenhuma impressora selecionada.\n");
                }
                break;
            case 3:
                printf("Iniciando monitoramento...\n");
                return; // Sai do menu e inicia o monitoramento
            case 4:
                log_write("Programa finalizado pelo usuario");
                exit(0);
            default:
                printf("Opcao invalida!\n");
        }
    } while (1);
}

// -----------------------------------------------------------
// Enviar arquivo para impressora
// -----------------------------------------------------------
BOOL enviar_para_impressora(const char* nome_arquivo, const char* nome_impressora)
{
    log_printf("Tentando enviar %s para impressora %s", nome_arquivo, nome_impressora);
    
    // Ler conteúdo do arquivo
    FILE* arquivo = fopen(nome_arquivo, "rb");
    if (!arquivo) {
        log_printf("Erro ao abrir arquivo %s", nome_arquivo);
        return FALSE;
    }
    
    fseek(arquivo, 0, SEEK_END);
    long tamanho = ftell(arquivo);
    fseek(arquivo, 0, SEEK_SET);
    
    char* dados = (char*)malloc(tamanho + 1);
    if (!dados) {
        fclose(arquivo);
        return FALSE;
    }
    
    fread(dados, 1, tamanho, arquivo);
    dados[tamanho] = '\0';
    fclose(arquivo);
    
    // Enviar para impressora
    HANDLE hPrinter;
    DOC_INFO_1 docInfo;
    DWORD bytesWritten;
    
    // Abrir impressora
    if (!OpenPrinterA((LPSTR)nome_impressora, &hPrinter, NULL)) {
        log_printf("Erro ao abrir impressora. Código: %lu", GetLastError());
        free(dados);
        return FALSE;
    }
    
    // Configurar documento
    docInfo.pDocName = "Etiqueta Traduzida";
    docInfo.pOutputFile = NULL;
    docInfo.pDatatype = "RAW";
    
    // Iniciar documento
    if (!StartDocPrinter(hPrinter, 1, (LPBYTE)&docInfo)) {
        log_printf("Erro ao iniciar documento na impressora");
        ClosePrinter(hPrinter);
        free(dados);
        return FALSE;
    }
    
    // Iniciar página
    if (!StartPagePrinter(hPrinter)) {
        log_printf("Erro ao iniciar página na impressora");
        EndDocPrinter(hPrinter);
        ClosePrinter(hPrinter);
        free(dados);
        return FALSE;
    }
    
    // Escrever dados
    if (!WritePrinter(hPrinter, dados, tamanho, &bytesWritten)) {
        log_printf("Erro ao escrever na impressora");
        EndPagePrinter(hPrinter);
        EndDocPrinter(hPrinter);
        ClosePrinter(hPrinter);
        free(dados);
        return FALSE;
    }
    
    // Finalizar
    EndPagePrinter(hPrinter);
    EndDocPrinter(hPrinter);
    ClosePrinter(hPrinter);
    free(dados);
    
    log_printf("Arquivo enviado com sucesso para a impressora. Bytes escritos: %lu", bytesWritten);
    return TRUE;
}

// -----------------------------------------------------------
// MAIN — Lê COM4 (lado livre do com0com)
// -----------------------------------------------------------
int main()
{
    log_open();
    log_write("Tradutor ZPL2->BPLB Avançado iniciado.");
    
    // Iniciar contagem de tempo
    iniciar_tempo();
    
    printf("=== TRADUTOR ZPL para BPLB (AVANÇADO) ===\n");
    printf("Suporte a: Textos, Code128, Interleaved 2of5, EAN, Caixas, Imagens\n\n");
    
    // Mostrar menu de configuração inicial
    menu_configuracao();

    log_write("Abrindo porta COM4...");

    HANDLE h = CreateFileA(
        "\\\\.\\COM4",
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        0,
        NULL
    );

    if (h == INVALID_HANDLE_VALUE) {
        log_printf("Falha ao abrir COM4. Erro: %lu", GetLastError());
        printf("Erro abrindo COM4.\n");
        return 1;
    }

    log_write("COM4 aberta com sucesso.");

    // Configurar DCB
    DCB dcb = {0};
    dcb.DCBlength = sizeof(DCB);

    GetCommState(h, &dcb);
    dcb.BaudRate = CBR_115200;
    dcb.ByteSize = 8;
    dcb.StopBits = ONESTOPBIT;
    dcb.Parity   = NOPARITY;
    SetCommState(h, &dcb);

    // Timeouts
    COMMTIMEOUTS t = {0};
    t.ReadIntervalTimeout = 50;
    t.ReadTotalTimeoutConstant = 50;
    t.ReadTotalTimeoutMultiplier = 10;
    SetCommTimeouts(h, &t);

    printf("\nAguardando ZPL na COM4...\n");
    printf("Impressora selecionada: %s\n", impressora_selecionada ? impressora_selecionada : "NENHUMA (apenas arquivo)");
    printf("Pressione Ctrl+C para parar\n\n");
    
    log_write("Iniciando monitoramento da COM4");
    if (impressora_selecionada) {
        log_printf("Impressora configurada: %s", impressora_selecionada);
    } else {
        log_write("Nenhuma impressora configurada - modo apenas arquivo");
    }

    char buffer[BUF]={0};
    char zpl[BUF*4]={0};
    int pos = 0;

    DWORD lidos;

    while (1)
    {
        if (ReadFile(h, buffer, BUF-1, &lidos, NULL) && lidos > 0)
        {
            buffer[lidos] = 0;

            log_printf("Recebidos %lu bytes: '%s'", lidos, buffer);

            memcpy(zpl + pos, buffer, lidos);
            pos += lidos;

            if (strstr(zpl, "^XZ"))
            {
                // Marcar tempo de recebimento do ZPL completo
                log_tempo_recebimento_zpl();
                
                log_write("ZPL completo detectado (^XZ). Iniciando tradução...");

                FILE *out = fopen("saida_bplb.txt", "w");
                traduz_zpl_para_bplb_avancado(zpl, out);
                fclose(out);

                // Marcar tempo de conclusão da tradução
                log_tempo_traducao();

                printf("ZPL traduzido → saida_bplb.txt\n");
                log_write("Arquivo saida_bplb.txt gerado com sucesso.");

                // Enviar para impressora se configurada
                if (impressora_selecionada) {
                    if (enviar_para_impressora("saida_bplb.txt", impressora_selecionada)) {
                        log_write("Arquivo enviado com sucesso para a impressora");
                        printf("Arquivo enviado para: %s\n", impressora_selecionada);
                        
                        // Marcar tempo de conclusão da impressão
                        log_tempo_impressao();
                    } else {
                        log_write("Falha ao enviar para impressora");
                        printf("Erro ao enviar para impressora!\n");
                    }
                } else {
                    printf("Arquivo salvo (nenhuma impressora selecionada)\n");
                    log_tempo_apenas_arquivo();
                }

                memset(zpl, 0, sizeof(zpl));
                pos = 0;
            }
        }
        Sleep(10);
    }

    if (impressora_selecionada) free(impressora_selecionada);
    CloseHandle(h);
    return 0;
}







// gcc -o tradutor_zpl.exe main.c bplb.c -lwinspool -luser32 -lkernel32 -O2 -Wall