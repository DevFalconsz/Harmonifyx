#define CURL_STATICLIB
#define _CRT_SECURE_NO_WARNINGS
#include <SDL.h>
#include <SDL_image.h>
#include <SDL_mixer.h>
#include <SDL_ttf.h>
#include <stdio.h>
#include <stdlib.h>
#include <curl.h>
#include <string.h>
#include <vector>
#include <string>
#include <fstream>

using namespace std;

//Vari�veis globais
// Vari�vel para rastrear o estado de reprodu��o da m�sica
SDL_bool isPaused = SDL_FALSE;
TTF_Font* font;
SDL_bool isInputActive = SDL_FALSE;

// Estrutura para representar o campo de entrada de texto
struct InputText {
    char text[256]; // Texto digitado pelo usu�rio
    SDL_bool active;    // Indica se o campo est� ativo para entrada de texto
    SDL_Rect rect;      // Ret�ngulo que delimita o campo de entrada
};

// Estrutura para armazenar dados tempor�rios
struct AudioData {
    char* data;
    size_t size;
};

// Estrutura para armazenar informa��es sobre o �udio
struct AudioInfo {
    const char* url;
    Mix_Chunk* audio_chunk;
};

// Estrutura para armazenar informa��es sobre um bot�o
struct Button {
    int centerX;
    int centerY;
    int width;
    int height;
    SDL_bool isClicked;
};

// Fun��o de callback para lidar com os dados recebidos
size_t write_callback(void* data, size_t size, size_t nmemb, void* user_data) {
    struct AudioData* audio_data = (struct AudioData*)user_data;

    // Calcular o tamanho total dos dados
    size_t total_size = size * nmemb;

    // Alocar espa�o para os novos dados
    char* new_data = (char*)realloc(audio_data->data, audio_data->size + total_size + 1);
    if (new_data == NULL) {
        fprintf(stderr, "Erro ao alocar mem�ria para dados de �udio\n");
        return 0;
    }

    // Copiar os novos dados para a estrutura de dados tempor�rios
    memcpy(&(new_data[audio_data->size]), data, total_size);
    audio_data->data = new_data;
    audio_data->size += total_size;

    // Adicionar um terminador nulo para garantir que os dados sejam tratados como uma string
    audio_data->data[audio_data->size] = '\0';

    // Retornar o n�mero de bytes processados
    return total_size;
}

// Fun��o para inicializar as bibliotecas SDL, SDL_mixer e libcurl
int initializeLibraries() {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
        fprintf(stderr, "Erro ao inicializar o SDL: %s\n", SDL_GetError());
        return 0;
    }

    // Inicializar o SDL_mixer
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
        fprintf(stderr, "Erro ao inicializar o SDL_mixer: %s\n", Mix_GetError());
        SDL_Quit();
        return 0;
    }

    // Inicializar a biblioteca libcurl
    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
        fprintf(stderr, "Erro ao inicializar o libcurl\n");
        Mix_CloseAudio();
        SDL_Quit();
        return 0;
    }

    // Inicializar o SDL_ttf
    if (TTF_Init() == -1) {
        fprintf(stderr, "Erro ao inicializar o SDL_ttf: %s\n", TTF_GetError());
        Mix_CloseAudio();
        SDL_Quit();
        return 0;
    }

    // Carregar a fonte TTF
    font = TTF_OpenFont("fonte.ttf", 25); // Substitua "sua_fonte.ttf" pelo caminho da sua fonte TTF e o tamanho desejado
    if (!font) {
        fprintf(stderr, "Erro ao carregar a fonte TTF: %s\n", TTF_GetError());
        TTF_Quit();
        Mix_CloseAudio();
        SDL_Quit();
        return 0;
    }

    return 1;
}

// Fun��o para carregar uma m�sica
Mix_Chunk* loadMusic(const char* url) {
    // Inicializar estrutura para armazenar dados tempor�rios
    struct AudioData audio_data = { NULL, 0 };

    // Criar uma inst�ncia CURL
    CURL* curl = curl_easy_init();
    if (!curl) {
        fprintf(stderr, "Erro ao inicializar o libcurl\n");
        return NULL;
    }

    // Configurar a URL
    curl_easy_setopt(curl, CURLOPT_URL, url);

    // Configurar a fun��o de callback para processar os dados recebidos
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &audio_data);

    // Configurar a op��o para streaming
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    // Executar a solicita��o
    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        fprintf(stderr, "Erro ao realizar a solicita��o: %s\n", curl_easy_strerror(res));
        curl_easy_cleanup(curl);
        return NULL;
    }

    // Carregar o �udio com o SDL_mixer
    Mix_Chunk* audio_chunk = Mix_LoadWAV_RW(SDL_RWFromMem(audio_data.data, (int)audio_data.size), 1);
    if (audio_chunk == NULL) {
        fprintf(stderr, "Erro ao carregar o �udio com SDL_mixer: %s\n", Mix_GetError());
    }

    // Limpar recursos
    free(audio_data.data);
    curl_easy_cleanup(curl);

    return audio_chunk;
}

std::vector<std::string> readArchive() {
    std::vector<std::string> urls;

    std::ifstream arquivo("links.txt");
    if (arquivo.is_open()) {
        std::string line;

        // L� o arquivo linha por linha e armazena cada linha como um URL no vetor 'urls'
        while (std::getline(arquivo, line)) {
            // Remover caracteres de quebra de linha (\n, \r, ou \r\n)
            line.erase(std::remove(line.begin(), line.end(), '\n'), line.end());
            line.erase(std::remove(line.begin(), line.end(), '\r'), line.end());

            // Adicionar o URL ao vetor 'urls'
            urls.push_back(line);
        }

        arquivo.close();
    }

    return urls;
}


// Fun��o para carregar os arquivos de �udio e retornar informa��es sobre os arquivos carregados com sucesso
struct AudioInfo* loadAudioFiles(int* num_audios) {
    // Definir as URLs dos arquivos de �udio
    std::vector<std::string> urls = readArchive();
    std::vector<const char*> audio_urls;

    for (const std::string& url : urls) {
        audio_urls.push_back(url.c_str());
    }

    // Atualizar o n�mero de �udios para o tamanho do vetor audio_urls
    *num_audios = audio_urls.size();

    // Alocar mem�ria para a lista de informa��es de �udio
    struct AudioInfo* audio_info_list = (struct AudioInfo*)malloc(*num_audios * sizeof(struct AudioInfo));
    if (audio_info_list == NULL) {
        fprintf(stderr, "Erro ao alocar mem�ria para as informa��es de �udio\n");
        return NULL;
    }

    int num_loaded_audios = 0; // Contador para o n�mero de �udios carregados com sucesso

    for (int i = 0; i < *num_audios; ++i) {
        audio_info_list[i].url = audio_urls[i];
        audio_info_list[i].audio_chunk = loadMusic(audio_urls[i]);
        if (audio_info_list[i].audio_chunk == NULL) {
            // Lidar com falha no carregamento da m�sica conforme necess�rio
            fprintf(stderr, "Falha ao carregar a m�sica na URL: %s\n", audio_urls[i]);
        }
        else {
            // Incrementar o contador de �udios carregados com sucesso
            num_loaded_audios++;
        }
    }

    // Redimensionar o array de audio_info_list para conter apenas os �udios carregados com sucesso
    struct AudioInfo* loaded_audio_info_list = (struct AudioInfo*)realloc(audio_info_list, num_loaded_audios * sizeof(struct AudioInfo));
    if (loaded_audio_info_list == NULL && num_loaded_audios > 0) {
        // Se houve sucesso no carregamento de pelo menos um �udio, mas falha ao redimensionar o array,
        // liberar o array original e retornar NULL
        free(audio_info_list);
        fprintf(stderr, "Erro ao redimensionar o array de informa��es de �udio\n");
        return NULL;
    }

    // Atualizar o n�mero de �udios para refletir apenas os �udios carregados com sucesso
    *num_audios = num_loaded_audios;

    return loaded_audio_info_list;
}

// Fun��o para desenhar um bot�o com uma imagem
void drawImageButton(SDL_Renderer* renderer, SDL_Texture* texture, int x, int y, int width, int height) {
    SDL_Rect dstRect = { x, y,  width, height };
    SDL_RenderCopy(renderer, texture, NULL, &dstRect);
}

// Fun��o para alternar entre pausar e retomar a reprodu��o da m�sica
void togglePauseResume() {
    if (isPaused) {
        Mix_Resume(-1); // Retomar a reprodu��o da m�sica
        isPaused = SDL_FALSE; // Atualizar o estado para n�o pausado
    }
    else {
        Mix_Pause(-1); // Pausar a reprodu��o da m�sica
        isPaused = SDL_TRUE; // Atualizar o estado para pausado
    }
}

// Fun��o para renderizar o t�tulo com uma fonte e tamanho espec�ficos
void renderTitle(SDL_Renderer* renderer, TTF_Font* font, const char* title, int fontSize) {
    SDL_Color titleColor = { 255, 255, 255, 255 }; // Cor branca para o t�tulo
    TTF_Font* titleFont = TTF_OpenFont("fonte.ttf", fontSize); // Carregue a fonte com o tamanho desejado
    if (!titleFont) {
        fprintf(stderr, "Erro ao carregar a fonte para o t�tulo: %s\n", TTF_GetError());
        return;
    }

    SDL_Surface* titleSurface = TTF_RenderText_Blended(titleFont, title, titleColor);
    if (titleSurface == nullptr) {
        fprintf(stderr, "Erro ao renderizar o t�tulo: %s\n", TTF_GetError());
        TTF_CloseFont(titleFont); // Feche a fonte carregada
        return;
    }

    // Defina a posi��o do t�tulo no meio superior da janela
    int titleX = (800 - titleSurface->w) / 2; // Centralize horizontalmente
    int titleY = 20; // Defina uma margem de 20 pixels a partir do topo da janela

    // Crie uma textura a partir da superf�cie do t�tulo
    SDL_Texture* titleTexture = SDL_CreateTextureFromSurface(renderer, titleSurface);
    if (titleTexture == nullptr) {
        fprintf(stderr, "Erro ao criar a textura do t�tulo: %s\n", SDL_GetError());
        SDL_FreeSurface(titleSurface);
        TTF_CloseFont(titleFont); // Feche a fonte carregada
        return;
    }

    // Renderize a textura do t�tulo na tela
    SDL_Rect titleRect = { titleX, titleY, titleSurface->w, titleSurface->h };
    SDL_RenderCopy(renderer, titleTexture, nullptr, &titleRect);

    // Libere a superf�cie e a textura do t�tulo
    SDL_FreeSurface(titleSurface);
    SDL_DestroyTexture(titleTexture);
    TTF_CloseFont(titleFont); // Feche a fonte carregada
}

// Fun��o para renderizar o campo de entrada de texto na tela
void renderInputText(SDL_Renderer* renderer, struct InputText* inputText) {
    SDL_Rect inputRect = { 250, 200, 300, 30 }; // Posi��o e tamanho do campo de entrada
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); // Cor branca
    SDL_RenderDrawRect(renderer, &inputRect); // Desenha o campo de entrada
    SDL_Color textColor = { 0, 0, 0, 255 }; // Cor preta

    // Verificar se o texto n�o est� vazio antes de tentar renderiz�-lo
    if (inputText->active && inputText->text[0] != '\0') {
        TTF_SetFontStyle(font, TTF_STYLE_NORMAL);

        SDL_Surface* textSurface = TTF_RenderText_Solid(font, inputText->text, textColor);
        if (textSurface == nullptr) {
            fprintf(stderr, "Erro ao renderizar o texto: %s\n", TTF_GetError());
            return;
        }

        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, textSurface);
        if (texture == nullptr) {
            fprintf(stderr, "Erro ao criar a textura do texto: %s\n", SDL_GetError());
            SDL_FreeSurface(textSurface);
            return;
        }

        SDL_Rect textRect = { inputRect.x + 5, inputRect.y + 5, textSurface->w, textSurface->h };

        // Verificar se o texto ultrapassa os limites da caixa de texto
        if (textRect.x + textRect.w > inputRect.x + inputRect.w) {
            // Se o texto ultrapassa � direita, ajuste a largura do ret�ngulo de texto
            textRect.w = inputRect.x + inputRect.w - textRect.x;
        }

        SDL_RenderCopy(renderer, texture, nullptr, &textRect);

        SDL_FreeSurface(textSurface);
        SDL_DestroyTexture(texture);
    }
}

// Fun��o para lidar com o evento de pressionar a tecla Enter
void handleEnterKeyPress(struct InputText* inputText) {
    // Abra o arquivo para escrita em modo de anexa��o (append)
    FILE* file = fopen("links.txt", "a");
    if (file == NULL) {
        fprintf(stderr, "Erro ao abrir o arquivo para escrita\n");
        return;
    }

    // Escreva o texto no arquivo
    fprintf(file, "%s\n", inputText->text);

    // Feche o arquivo
    fclose(file);

    // Limpe o campo de entrada de texto
    strcpy(inputText->text, "");
}

// Fun��o para processar eventos de entrada de texto
void handleTextInputEvent(SDL_Event* event, struct InputText* inputText, SDL_Renderer* renderer) {
    if (event->type == SDL_TEXTINPUT && inputText->active) {
        // Concatenar o texto de entrada � string existente, se n�o estiver cheia
        if (strlen(inputText->text) + strlen(event->text.text) < sizeof(inputText->text)) {
            strcat(inputText->text, event->text.text);
            // Atualizar a tela para refletir a mudan�a no texto
            SDL_RenderPresent(renderer);
        }
    }
    else if (event->type == SDL_KEYDOWN && inputText->active) {
        // Processar a tecla de backspace para apagar o �ltimo caractere
        if (event->key.keysym.sym == SDLK_BACKSPACE && strlen(inputText->text) > 0) {
            // Apagar o �ltimo caractere
            inputText->text[strlen(inputText->text) - 1] = '\0';
            // Limpar o campo de entrada de texto
            SDL_SetRenderDrawColor(renderer, 44, 47, 51, 255); // Definir a cor de fundo
            SDL_Rect inputRect = { 250, 200, 300, 30 }; // Posi��o e tamanho do campo de entrada
            SDL_RenderFillRect(renderer, &inputRect); // Preencher o ret�ngulo com a cor de fundo
            // Renderizar o campo de entrada de texto novamente
            renderInputText(renderer, inputText);
            // Atualizar a tela
            SDL_RenderPresent(renderer);
        }
        if (event->key.keysym.sym == SDLK_v && SDL_GetModState() & KMOD_CTRL) {
            // Manipular a funcionalidade de colar (Ctrl + V)
            const char* clipboardText = SDL_GetClipboardText();
            if (clipboardText != NULL) {
                // Concatenar o texto da �rea de transfer�ncia ao texto existente
                strcat(inputText->text, clipboardText);
                SDL_free((void*)clipboardText); // Liberar mem�ria alocada pela fun��o SDL_GetClipboardText()
            }
        }
        // Processar a tecla Enter
        if (event->key.keysym.sym == SDLK_RETURN) {
            handleEnterKeyPress(inputText);
            SDL_SetRenderDrawColor(renderer, 44, 47, 51, 255); // Definir a cor de fundo
            SDL_Rect inputRect = { 250, 200, 300, 30 };
            SDL_RenderFillRect(renderer, &inputRect);
        }
    }
}

// Fun��o para liberar todos os recursos
void freeResources(SDL_Window* window, SDL_Renderer* renderer, Mix_Chunk** audio_chunks, int num_chunks) {
    // Liberar cada chunk de �udio
    for (int i = 0; i < num_chunks; ++i) {
        Mix_FreeChunk(audio_chunks[i]);
    }

    // Fechar o SDL_mixer
    Mix_CloseAudio();

    // Finalizar a biblioteca libcurl
    curl_global_cleanup();

    // Destruir o renderizador e a janela
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    // Finalizar o SDL
    SDL_Quit();
}

int main(int argc, char* argv[]) {
    if (!initializeLibraries()) { return 1; }

    SDL_Window* window = SDL_CreateWindow("Harmonifyx", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 800, 600, SDL_WINDOW_SHOWN);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    // Bot�o para iniciar a playlist
    struct Button playButton;
    playButton.centerX = 280; // Centralizado na largura da janela
    playButton.centerY = 450; // Posicionado no ter�o inferior da janela
    playButton.width = 100;   // Largura do bot�o
    playButton.height = 100;  // Altura do bot�o
    playButton.isClicked = SDL_FALSE;

    // Bot�o para pausar/resumir a playlist
    struct Button pauseButton;
    pauseButton.centerX = 420; // Centralizado na largura da janela
    pauseButton.centerY = 450; // Posicionado no ter�o inferior da janela
    pauseButton.width = 100;   // Largura do bot�o
    pauseButton.height = 100;  // Altura do bot�o
    pauseButton.isClicked = SDL_FALSE;

    // �ndice da m�sica atual
    int currentAudioIndex = 0;
    SDL_bool shouldStartLoop = SDL_FALSE;

    //while (true){
    while (currentAudioIndex < 100) {
        // Carregar superf�cies
        SDL_Surface* iconSurface = SDL_LoadBMP("icon.bmp");
        SDL_Surface* playButtonSurface = IMG_Load("playbutton.png");
        SDL_Texture* playButtonTexture = SDL_CreateTextureFromSurface(renderer, playButtonSurface);
        SDL_Surface* pauseButtonSurface = IMG_Load("pausebutton.png");
        SDL_Texture* pauseButtonTexture = SDL_CreateTextureFromSurface(renderer, pauseButtonSurface);
        if (!iconSurface) { fprintf(stderr, "Erro ao carregar o �cone: %s\n", SDL_GetError()); }
        if (!playButtonSurface) { fprintf(stderr, "Erro ao carregar a imagem do bot�o de reprodu��o: %s\n", IMG_GetError()); }
        if (!playButtonTexture) { fprintf(stderr, "Erro ao criar a textura do bot�o de reprodu��o: %s\n", SDL_GetError()); }
        if (!pauseButtonSurface) { fprintf(stderr, "Erro ao carregar a imagem do bot�o de pausar/resumir: %s\n", IMG_GetError()); }
        if (!pauseButtonTexture) { fprintf(stderr, "Erro ao criar a textura do bot�o de pausar/resumir: %s\n", SDL_GetError()); }

        // Estrutura para representar o campo de entrada de texto
        struct InputText inputText;
        strcpy(inputText.text, ""); // Inicializa o texto como vazio
        inputText.active = SDL_TRUE; // Define o campo de entrada como ativo
        inputText.rect.x = 250; // Posi��o x do campo de entrada
        inputText.rect.y = 200; // Posi��o y do campo de entrada
        inputText.rect.w = 300; // Largura do campo de entrada
        inputText.rect.h = 30; // Altura do campo de entrada

        // Configurar o �cone da janela
        SDL_SetWindowIcon(window, iconSurface);

        // Libera��o das superf�cies 
        SDL_FreeSurface(iconSurface);
        SDL_FreeSurface(playButtonSurface);
        SDL_FreeSurface(pauseButtonSurface);

        // Limpar a tela
        SDL_SetRenderDrawColor(renderer, 44, 47, 51, 255);
        SDL_RenderClear(renderer);

        // Desenhar os bot�es na tela
        drawImageButton(renderer, playButtonTexture, playButton.centerX, playButton.centerY, playButton.width, playButton.height);
        drawImageButton(renderer, pauseButtonTexture, pauseButton.centerX, pauseButton.centerY, pauseButton.width, pauseButton.height);

        renderInputText(renderer, &inputText);
        renderTitle(renderer, font, "Harmonifyx", 100);

        // Atualizar a tela
        SDL_RenderPresent(renderer);

        while (!shouldStartLoop) {
            SDL_Event event;
            while (SDL_PollEvent(&event) != 0) {
                if (event.type == SDL_QUIT) {
                    freeResources(window, renderer, NULL, 0);
                    exit(0);
                    break;
                }
                else if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
                    int mouseX, mouseY;
                    SDL_GetMouseState(&mouseX, &mouseY);

                    // Verificar se o clique ocorreu dentro dos limites do bot�o de reprodu��o
                    if (mouseX >= playButton.centerX && mouseX <= playButton.centerX + playButton.width &&
                        mouseY >= playButton.centerY && mouseY <= playButton.centerY + playButton.height) {
                        shouldStartLoop = SDL_TRUE;
                        printf("Carregando m�sicas e tocando logo ap�s!.\n");
                    }
                    if (mouseX >= inputText.rect.x && mouseX <= inputText.rect.x + inputText.rect.w &&
                        mouseY >= inputText.rect.y && mouseY <= inputText.rect.y + inputText.rect.h) {
                        inputText.active = SDL_TRUE; // Ativar o campo de entrada
                    }
                    else {
                        inputText.active = SDL_FALSE; // Desativar o campo de entrada
                    }
                }
                else {
                    // Processar eventos de entrada de texto
                    handleTextInputEvent(&event, &inputText, renderer);
                }
            }
            // Desenhar o campo de entrada de texto na tela
            renderInputText(renderer, &inputText);

            // Atualizar a tela
            SDL_RenderPresent(renderer);
        }

        // Exibir "CARREGANDO" durante o carregamento dos arquivos
        SDL_Color titleColor = { 255, 255, 255, 255 };
        SDL_Surface* loadingSurface = TTF_RenderText_Blended(font, "LOADING MUSICS...", titleColor);
        SDL_Texture* loadingTexture = SDL_CreateTextureFromSurface(renderer, loadingSurface);
        int loadingTextWidth = loadingSurface->w;
        int loadingTextHeight = loadingSurface->h;
        SDL_FreeSurface(loadingSurface);

        // Calcular a posi��o para centralizar o texto na tela
        int loadingTextX = (800 - loadingTextWidth) / 2;
        int loadingTextY = (600 - loadingTextHeight) / 2;

        // Renderizar o texto "CARREGANDO" na tela
        SDL_Rect loadingTextRect = { loadingTextX, loadingTextY, loadingTextWidth, loadingTextHeight };
        SDL_RenderCopy(renderer, loadingTexture, NULL, &loadingTextRect);
        SDL_DestroyTexture(loadingTexture);
        SDL_RenderPresent(renderer);

        // Carregar e tocar a pr�xima m�sica
        int num_audios;
        struct AudioInfo* audio_info_list = loadAudioFiles(&num_audios);

        // Remover o texto "CARREGANDO" da tela
        SDL_SetRenderDrawColor(renderer, 44, 47, 51, 255); // Definir a cor de fundo
        SDL_RenderFillRect(renderer, &loadingTextRect); // Preencher o ret�ngulo com a cor de fundo
        SDL_RenderPresent(renderer);

        // Agora o loop principal inicia somente quando o bot�o de reprodu��o foi clicado
        while (shouldStartLoop && currentAudioIndex < 100) {
            // Aguardar at� que a m�sica termine
            while (Mix_Playing(-1) != 0) {
                SDL_Event event;
                while (SDL_PollEvent(&event) != 0) {
                    if (event.type == SDL_QUIT) {
                        freeResources(window, renderer, NULL, 0);
                        exit(0);
                        break;
                    }
                    else if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
                        int mouseX, mouseY;
                        SDL_GetMouseState(&mouseX, &mouseY);

                        if (mouseX >= pauseButton.centerX && mouseX <= pauseButton.centerX + pauseButton.width &&
                            mouseY >= pauseButton.centerY && mouseY <= pauseButton.centerY + pauseButton.height) {
                            togglePauseResume();
                            printf("Clicado em PAUSAR/RESUME!.\n");
                        }
                    }
                }
                SDL_Delay(100); // Esperar um curto per�odo antes de verificar novamente
            }

            // Carregar e tocar a pr�xima m�sica
            if (currentAudioIndex < num_audios) {
                Mix_PlayChannel(-1, audio_info_list[currentAudioIndex].audio_chunk, 0);
                currentAudioIndex++;
            }
            else {
                // Se j� reproduzimos todas as m�sicas, reinicie a reprodu��o desde o in�cio
                currentAudioIndex = 0;
            }
        }
    }

    return 0;
}
