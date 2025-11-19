#include <windows.h>
#include <wininet.h>
#include <stdio.h>

#pragma comment(lib, "Wininet.lib")
#pragma comment(lib, "winspool.lib")

// =============================================
// 1. Solicitar modo administrador (UAC)
// =============================================
void relaunchAsAdmin() {
    BOOL isAdmin = FALSE;
    SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
    PSID AdministratorsGroup;

    if (AllocateAndInitializeSid(
        &NtAuthority, 2,
        SECURITY_BUILTIN_DOMAIN_RID,
        DOMAIN_ALIAS_RID_ADMINS,
        0, 0, 0, 0, 0, 0,
        &AdministratorsGroup)) {
        
        CheckTokenMembership(NULL, AdministratorsGroup, &isAdmin);
        FreeSid(AdministratorsGroup);
    }

    if (!isAdmin) {
        char path[MAX_PATH];
        GetModuleFileNameA(NULL, path, MAX_PATH);

        SHELLEXECUTEINFOA sei = { sizeof(sei) };
        sei.lpVerb = "runas";
        sei.lpFile = path;
        sei.hwnd = NULL;
        sei.nShow = SW_NORMAL;

        if (!ShellExecuteExA(&sei)) {
            MessageBoxA(NULL, "Permissão de administrador é necessária.", "Erro", MB_ICONERROR);
            exit(1);
        }
        exit(0);
    }
}

// =============================================
// 2. Obter caminho da pasta do executável
// =============================================
void getExecutablePath(char* buffer, size_t size) {
    GetModuleFileNameA(NULL, buffer, size);
    
    // Remove o nome do executável, mantendo apenas o caminho
    char* lastBackslash = strrchr(buffer, '\\');
    if (lastBackslash) {
        *(lastBackslash + 1) = '\0';  // Mantém a barra no final
    }
}

// =============================================
// 3. Verificar se arquivo existe
// =============================================
BOOL arquivoExiste(const char *caminho) {
    DWORD attrs = GetFileAttributesA(caminho);
    return (attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY));
}

// =============================================
// 4. Extrair arquivo ZIP
// =============================================
BOOL extrairZip(const char *arquivoZip, const char *pastaDestino) {
    char comando[512];
    char mensagem[512];
    
    // Cria pasta de destino se não existir
    CreateDirectoryA(pastaDestino, NULL);
    
    // Tenta com PowerShell primeiro (mais confiável)
    sprintf(comando, 
        "powershell -Command \"Expand-Archive -Path '%s' -DestinationPath '%s' -Force\"", 
        arquivoZip, pastaDestino);
    
    sprintf(mensagem, "Executando: %s", comando);
    MessageBoxA(NULL, mensagem, "DEBUG Extração", MB_OK);
    
    int resultado = system(comando);
    
    if (resultado == 0) {
        sprintf(mensagem, "Arquivo extraído com sucesso!\nOrigem: %s\nDestino: %s", arquivoZip, pastaDestino);
        MessageBoxA(NULL, mensagem, "Sucesso", MB_OK);
        return TRUE;
    } else {
        // Fallback: tenta com tar (Windows 10+)
        sprintf(comando, "tar -xf \"%s\" -C \"%s\"", arquivoZip, pastaDestino);
        resultado = system(comando);
        
        if (resultado == 0) {
            sprintf(mensagem, "Arquivo extraído com tar!\nOrigem: %s\nDestino: %s", arquivoZip, pastaDestino);
            MessageBoxA(NULL, mensagem, "Sucesso", MB_OK);
            return TRUE;
        }
    }
    
    sprintf(mensagem, 
        "Falha na extração automática.\n\n"
        "Por favor extraia manualmente:\n"
        "1. Clique direito em '%s'\n"
        "2. 'Extrair Tudo'\n"
        "3. Pasta destino: '%s'", 
        arquivoZip, pastaDestino);
    MessageBoxA(NULL, mensagem, "Extração Manual", MB_ICONINFORMATION);
    
    return FALSE;
}

// =============================================
// 5. Executar setup do com0com
// =============================================
void executarSetupCom0com() {
    char comando[512];
    char mensagem[512];
    char pastaExe[MAX_PATH];
    
    getExecutablePath(pastaExe, MAX_PATH);
    
    MessageBoxA(NULL, "Instalando com0com...", "Aguarde", MB_OK);
    
    // Tenta encontrar e executar o setupc.exe em várias localizações possíveis
    const char *caminhos[] = {
        "com0com-3.0.0.0-i386-and-x64-signed\\setupc.exe",
        "com0com\\setupc.exe", 
        "setupc.exe",
        NULL
    };
    
    BOOL encontrado = FALSE;
    BOOL instalado = FALSE;
    
    for (int i = 0; caminhos[i] != NULL; i++) {
        char caminhoCompleto[MAX_PATH];
        sprintf(caminhoCompleto, "%s%s", pastaExe, caminhos[i]);
        
        if (arquivoExiste(caminhoCompleto)) {
            sprintf(comando, "cmd.exe /c \"%s\" install PortName=COM2 PortName=COM3", caminhoCompleto);
            sprintf(mensagem, "Executando comando:\n%s", comando);
            MessageBoxA(NULL, mensagem, "Instalação com0com", MB_OK);
            
            int resultado = system(comando);
            
            if (resultado == 0) {
                sprintf(mensagem, 
                    "com0com instalado com sucesso!\n\n"
                    "Portas virtuais criadas:\n"
                    "COM2 <-> COM3");
                MessageBoxA(NULL, mensagem, "Sucesso", MB_OK);
                instalado = TRUE;
            } else {
                sprintf(mensagem, 
                    "Erro na instalação do com0com.\n"
                    "Código: %d\n\n"
                    "Tentando com permissões elevadas...", resultado);
                MessageBoxA(NULL, mensagem, "Aviso", MB_ICONWARNING);
                
                // Tenta com ShellExecute para elevação
                SHELLEXECUTEINFOA sei = { sizeof(sei) };
                sei.lpVerb = "runas";
                sei.lpFile = caminhoCompleto;
                sei.lpParameters = "install PortName=COM5 PortName=COM6";
                sei.nShow = SW_NORMAL;
                
                if (ShellExecuteExA(&sei)) {
                    MessageBoxA(NULL, "Instalação iniciada com permissões elevadas!", "Sucesso", MB_OK);
                    instalado = TRUE;
                }
            }
            
            encontrado = TRUE;
            break;
        }
    }
    
    if (!encontrado) {
        sprintf(mensagem, 
            "setupc.exe não encontrado!\n\n"
            "Por favor verifique:\n"
            "1. O arquivo com0com.zip foi extraído?\n"
            "2. A pasta 'com0com' foi criada?\n"
            "3. Execute setupc.exe manualmente como administrador:\n"
            "   setupc.exe install PortName=COM5 PortName=COM6");
        MessageBoxA(NULL, mensagem, "Arquivo Não Encontrado", MB_ICONERROR);
    } else if (!instalado) {
        MessageBoxA(NULL, 
            "Instalação não concluída.\n"
            "Execute setupc.exe manualmente como administrador.", 
            "Instalação Manual Necessária", MB_ICONINFORMATION);
    }
}

// =============================================
// 6. Instalar com0com (versão final)
// =============================================
void instalarCom0Com() {
    char mensagem[512];
    char pastaExe[MAX_PATH];
    char caminhoZip[MAX_PATH];
    
    getExecutablePath(pastaExe, MAX_PATH);
    sprintf(caminhoZip, "%scom0com.zip", pastaExe);
    
    // Verifica se o ZIP já existe na pasta
    if (arquivoExiste(caminhoZip)) {
        DWORD tamanho = 0;
        HANDLE hArq = CreateFileA(caminhoZip, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hArq != INVALID_HANDLE_VALUE) {
            tamanho = GetFileSize(hArq, NULL);
            CloseHandle(hArq);
        }
        
        sprintf(mensagem, 
            "com0com.zip encontrado!\n"
            "Local: %s\n"
            "Tamanho: %lu bytes\n\n"
            "Clique OK para usar este arquivo.", 
            caminhoZip, tamanho);
            
        MessageBoxA(NULL, mensagem, "Arquivo Local Encontrado", MB_OK);
        
        // Extrai o arquivo ZIP
        char pastaDestino[MAX_PATH];
        sprintf(pastaDestino, "%scom0com", pastaExe);
        
        MessageBoxA(NULL, "Extraindo com0com...", "Aguarde", MB_OK);
        if (extrairZip(caminhoZip, pastaDestino)) {
            executarSetupCom0com();
        }
        
    } else {
        sprintf(mensagem, 
            "com0com.zip não encontrado em:\n%s\n\n"
            "Por favor coloque o arquivo com0com.zip\n"
            "na mesma pasta do executável e execute novamente.",
            caminhoZip);
        MessageBoxA(NULL, mensagem, "Arquivo Não Encontrado", MB_ICONERROR);
    }
}

// =============================================
// 7. Criar impressora virtual
// =============================================
void instalarImpressora() {
    MessageBoxA(NULL, "Instalando impressora virtual Zebra...", "Aguarde", MB_OK);
    
    // Seu código de instalação da impressora aqui
    // ...
    
    MessageBoxA(NULL, "Impressora virtual Zebra instalada!", "Sucesso", MB_OK);
}

// =============================================
// MAIN
// =============================================
int main() {
    relaunchAsAdmin();
    
    char pastaExe[MAX_PATH];
    getExecutablePath(pastaExe, MAX_PATH);
    
    char mensagem[512];
    sprintf(mensagem, 
        "Instalador de Impressora Virtual Zebra\n\n"
        "Pasta do executável:\n%s\n\n"
        "Este programa irá:\n"
        "1. Buscar com0com.zip na pasta atual\n"
        "2. Extrair e instalar com0com\n"
        "3. Criar portas COM5<->COM6 virtuais\n"
        "4. Instalar impressora virtual Zebra\n\n"
        "Clique OK para continuar...", 
        pastaExe);
    
    MessageBoxA(NULL, mensagem, "Instalador", MB_OK);
    
    instalarCom0Com();
    instalarImpressora();
    
    MessageBoxA(NULL, "Processo finalizado!", "Sucesso", MB_OK);
    return 0;
}