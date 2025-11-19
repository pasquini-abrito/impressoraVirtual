#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <winspool.h>

#define MAX_PRINTERS 20
#define MAX_IMAGES 100
#define MAX_FILENAME 256
#define BUFFER_SIZE 4096

typedef struct {
    char name[256];
    char port[256];
    BOOL is_default;
} Printer;

typedef struct {
    char filename[MAX_FILENAME];
    char full_path[MAX_PATH];
} ImageFile;

Printer printers[MAX_PRINTERS];
int printer_count = 0;

ImageFile images[MAX_IMAGES];
int image_count = 0;

// Função para detectar impressoras instaladas no Windows
void detect_printers() {
    printf("Detectando impressoras...\n");
    
    DWORD needed = 0, returned = 0;
    
    // Primeira chamada para obter o tamanho necessário
    EnumPrinters(PRINTER_ENUM_LOCAL | PRINTER_ENUM_CONNECTIONS, 
                 NULL, 
                 2, 
                 NULL, 
                 0, 
                 &needed, 
                 &returned);
    
    if (needed == 0) {
        printf("Nenhuma impressora encontrada ou erro na detecção.\n");
        return;
    }
    
    BYTE* printer_info = (BYTE*)malloc(needed);
    if (!printer_info) {
        printf("Erro ao alocar memória.\n");
        return;
    }
    
    if (EnumPrinters(PRINTER_ENUM_LOCAL | PRINTER_ENUM_CONNECTIONS, 
                     NULL, 
                     2, 
                     printer_info, 
                     needed, 
                     &needed, 
                     &returned)) {
        
        PRINTER_INFO_2* printer = (PRINTER_INFO_2*)printer_info;
        
        for (DWORD i = 0; i < returned && printer_count < MAX_PRINTERS; i++) {
            strncpy(printers[printer_count].name, printer->pPrinterName, 255);
            strncpy(printers[printer_count].port, printer->pPortName, 255);
            printers[printer_count].is_default = printer->Attributes & PRINTER_ATTRIBUTE_DEFAULT;
            
            printer_count++;
            printer++;
        }
    }
    
    free(printer_info);
}

// Função para listar todas as imagens GRF na pasta atual
void list_grf_files() {
    printf("Procurando arquivos GRF na pasta atual...\n");
    
    WIN32_FIND_DATA find_file_data;
    HANDLE hFind = FindFirstFile("*.grf", &find_file_data);
    
    if (hFind == INVALID_HANDLE_VALUE) {
        printf("Nenhum arquivo GRF encontrado.\n");
        return;
    }
    
    do {
        if (!(find_file_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            strncpy(images[image_count].filename, find_file_data.cFileName, MAX_FILENAME - 1);
            
            // Obter caminho completo
            GetCurrentDirectory(MAX_PATH, images[image_count].full_path);
            strcat(images[image_count].full_path, "\\");
            strcat(images[image_count].full_path, find_file_data.cFileName);
            
            printf("Encontrado: %s\n", images[image_count].filename);
            image_count++;
            
            if (image_count >= MAX_IMAGES) {
                printf("Limite máximo de imagens atingido.\n");
                break;
            }
        }
    } while (FindNextFile(hFind, &find_file_data) != 0);
    
    FindClose(hFind);
}

// Função para enviar dados para a impressora
BOOL send_to_printer(const char* printer_name, const char* data, DWORD data_size) {
    HANDLE hPrinter;
    DOC_INFO_1 doc_info;
    DWORD bytes_written;
    
    // Abrir a impressora
    if (!OpenPrinter((LPSTR)printer_name, &hPrinter, NULL)) {
        printf("Erro ao abrir impressora: %s\n", printer_name);
        return FALSE;
    }
    
    // Configurar documento
    doc_info.pDocName = "Imagem GRF";
    doc_info.pOutputFile = NULL;
    doc_info.pDatatype = "RAW";
    
    // Iniciar documento
    if (StartDocPrinter(hPrinter, 1, (LPBYTE)&doc_info) == 0) {
        printf("Erro ao iniciar documento na impressora.\n");
        ClosePrinter(hPrinter);
        return FALSE;
    }
    
    // Iniciar página
    if (!StartPagePrinter(hPrinter)) {
        printf("Erro ao iniciar página na impressora.\n");
        EndDocPrinter(hPrinter);
        ClosePrinter(hPrinter);
        return FALSE;
    }
    
    // Enviar dados
    if (!WritePrinter(hPrinter, (LPVOID)data, data_size, &bytes_written)) {
        printf("Erro ao enviar dados para impressora.\n");
        EndPagePrinter(hPrinter);
        EndDocPrinter(hPrinter);
        ClosePrinter(hPrinter);
        return FALSE;
    }
    
    // Finalizar página e documento
    EndPagePrinter(hPrinter);
    EndDocPrinter(hPrinter);
    ClosePrinter(hPrinter);
    
    return TRUE;
}

// Função para carregar e enviar imagem GRF
BOOL send_grf_image(const char* printer_name, const char* image_path) {
    HANDLE hFile;
    DWORD bytes_read, file_size;
    BYTE* buffer;
    
    // Abrir arquivo
    hFile = CreateFile(image_path, GENERIC_READ, FILE_SHARE_READ, NULL, 
                      OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    
    if (hFile == INVALID_HANDLE_VALUE) {
        printf("Erro ao abrir arquivo: %s\n", image_path);
        return FALSE;
    }
    
    // Obter tamanho do arquivo
    file_size = GetFileSize(hFile, NULL);
    if (file_size == INVALID_FILE_SIZE) {
        printf("Erro ao obter tamanho do arquivo.\n");
        CloseHandle(hFile);
        return FALSE;
    }
    
    // Alocar buffer
    buffer = (BYTE*)malloc(file_size);
    if (!buffer) {
        printf("Erro ao alocar memória.\n");
        CloseHandle(hFile);
        return FALSE;
    }
    
    // Ler arquivo
    if (!ReadFile(hFile, buffer, file_size, &bytes_read, NULL)) {
        printf("Erro ao ler arquivo.\n");
        free(buffer);
        CloseHandle(hFile);
        return FALSE;
    }
    
    CloseHandle(hFile);
    
    if (bytes_read != file_size) {
        printf("Erro: tamanho lido diferente do esperado.\n");
        free(buffer);
        return FALSE;
    }
    
    printf("Enviando %s (%lu bytes) para %s...\n", 
           strrchr(image_path, '\\') + 1, file_size, printer_name);
    
    // Enviar para impressora
    BOOL success = send_to_printer(printer_name, (char*)buffer, file_size);
    
    free(buffer);
    return success;
}

// Função para mostrar menu de impressoras
int select_printer() {
    printf("\n=== IMPRESSORAS DETECTADAS ===\n");
    
    for (int i = 0; i < printer_count; i++) {
        printf("%d. %s", i + 1, printers[i].name);
        if (printers[i].is_default) {
            printf(" [PADRÃO]");
        }
        printf(" (%s)\n", printers[i].port);
    }
    
    printf("%d. Usar impressora padrão\n", printer_count + 1);
    printf("%d. Sair\n", printer_count + 2);
    
    int choice;
    printf("\nSelecione uma impressora: ");
    scanf("%d", &choice);
    
    if (choice > 0 && choice <= printer_count) {
        return choice - 1;
    } else if (choice == printer_count + 1) {
        // Encontrar impressora padrão
        for (int i = 0; i < printer_count; i++) {
            if (printers[i].is_default) {
                printf("Usando impressora padrão: %s\n", printers[i].name);
                return i;
            }
        }
        printf("Nenhuma impressora padrão encontrada.\n");
        return -1;
    } else if (choice == printer_count + 2) {
        return -2;
    } else {
        printf("Seleção inválida!\n");
        return -3;
    }
}

// Função para enviar todas as imagens para uma impressora
void send_all_images_to_printer(int printer_index) {
    if (printer_index < 0 || printer_index >= printer_count) {
        printf("Índice de impressora inválido.\n");
        return;
    }
    
    printf("\n=== ENVIANDO IMAGENS PARA %s ===\n", printers[printer_index].name);
    
    int success_count = 0;
    
    for (int i = 0; i < image_count; i++) {
        if (send_grf_image(printers[printer_index].name, images[i].full_path)) {
            printf("✓ %s enviado com sucesso!\n", images[i].filename);
            success_count++;
            
            // Pequena pausa entre imagens
            Sleep(1000);
        } else {
            printf("✗ Erro ao enviar %s\n", images[i].filename);
        }
    }
    
    printf("\nResumo: %d de %d imagens enviadas com sucesso.\n", 
           success_count, image_count);
}

int main() {
    printf("=== DRIVER DE IMPRESSORA TÉRMICA - WINDOWS ===\n");
    
    // Detectar impressoras
    detect_printers();
    
    if (printer_count == 0) {
        printf("Nenhuma impressora foi detectada.\n");
        printf("Verifique se as impressoras estão instaladas e conectadas.\n");
        system("pause");
        return 1;
    }
    
    // Listar arquivos GRF
    list_grf_files();
    
    if (image_count == 0) {
        printf("\nNenhum arquivo GRF encontrado na pasta atual!\n");
        printf("Certifique-se de que os arquivos .grf estão na mesma pasta do executável.\n");
        system("pause");
        return 1;
    }
    
    printf("\nEncontradas %d imagens GRF.\n", image_count);
    
    int running = 1;
    while (running) {
        // Selecionar impressora
        int printer_index;
        do {
            printer_index = select_printer();
        } while (printer_index == -3); // Repetir se seleção inválida
        
        if (printer_index == -2) {
            break; // Sair
        } else if (printer_index == -1) {
            continue; // Voltar ao menu
        }
        
        // Enviar todas as imagens para a impressora selecionada
        send_all_images_to_printer(printer_index);
        
        // Perguntar se deseja continuar
        printf("\nDeseja enviar para outra impressora? (s/n): ");
        char continue_choice;
        scanf(" %c", &continue_choice);
        
        if (continue_choice != 's' && continue_choice != 'S') {
            running = 0;
        }
    }
    
    printf("\nPrograma encerrado.\n");
    system("pause");
    return 0;
}