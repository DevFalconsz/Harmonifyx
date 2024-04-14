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

//Variáveis globais
// Variável para rastrear o estado de reprodução da música
SDL_bool isPaused = SDL_FALSE;
TTF_Font* font;
SDL_bool isInputActive = SDL_FALSE;

// Estrutura para representar o campo de entrada de texto
struct InputText {
    char text[256]; // Texto digitado pelo usuário
    SDL_bool active;    // Indica se o campo está ativo para entrada de texto
    SDL_Rect rect;      // Retângulo que delimita o campo de entrada
};

// Estrutura para armazenar dados temporários
struct AudioData {
    char* data;
    size_t size;
};

// Estrutura para armazenar informações sobre o áudio
struct AudioInfo {
    const char* url;
    Mix_Chunk* audio_chunk;
};

// Estrutura para armazenar informações sobre um botão
struct Button {
    int centerX;
    int centerY;
    int width;
    int height;
    SDL_bool isClicked;
};

// Função de callback para lidar com os dados recebidos
size_t write_callback(void* data, size_t size, size_t nmemb, void* user_data) {
    struct AudioData* audio_data = (struct AudioData*)user_data;

    // Calcular o tamanho total dos dados
    size_t total_size = size * nmemb;

    // Alocar espaço para os novos dados
    char* new_data = (char*)realloc(audio_data->data, audio_data->size + total_size + 1);
    if (new_data == NULL) {
        fprintf(stderr, "Erro ao alocar memória para dados de áudio\n");
        return 0;
    }

    // Copiar os novos dados para a estrutura de dados temporários
    memcpy(&(new_data[audio_data->size]), data, total_size);
    audio_data->data = new_data;
    audio_data->size += total_size;

    // Adicionar um terminador nulo para garantir que os dados sejam tratados como uma string
    audio_data->data[audio_data->size] = '\0';

    // Retornar o número de bytes processados
    return total_size;
}

// Função para inicializar as bibliotecas SDL, SDL_mixer e libcurl
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

// Função para carregar uma música
Mix_Chunk* loadMusic(const char* url) {
    // Inicializar estrutura para armazenar dados temporários
    struct AudioData audio_data = { NULL, 0 };

    // Criar uma instância CURL
    CURL* curl = curl_easy_init();
    if (!curl) {
        fprintf(stderr, "Erro ao inicializar o libcurl\n");
        return NULL;
    }

    // Configurar a URL
    curl_easy_setopt(curl, CURLOPT_URL, url);

    // Configurar a função de callback para processar os dados recebidos
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &audio_data);

    // Configurar a opção para streaming
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    // Executar a solicitação
    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        fprintf(stderr, "Erro ao realizar a solicitação: %s\n", curl_easy_strerror(res));
        curl_easy_cleanup(curl);
        return NULL;
    }

    // Carregar o áudio com o SDL_mixer
    Mix_Chunk* audio_chunk = Mix_LoadWAV_RW(SDL_RWFromMem(audio_data.data, (int)audio_data.size), 1);
    if (audio_chunk == NULL) {
        fprintf(stderr, "Erro ao carregar o áudio com SDL_mixer: %s\n", Mix_GetError());
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

        // Lê o arquivo linha por linha e armazena cada linha como um URL no vetor 'urls'
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


// Função para carregar os arquivos de áudio e retornar informações sobre os arquivos carregados com sucesso
struct AudioInfo* loadAudioFiles(int* num_audios) {
    // Definir as URLs dos arquivos de áudio
    std::vector<std::string> urls = readArchive();
    std::vector<const char*> audio_urls;

    for (const std::string& url : urls) {
        audio_urls.push_back(url.c_str());
    }

    // Atualizar o número de áudios para o tamanho do vetor audio_urls
    *num_audios = audio_urls.size();

    // Alocar memória para a lista de informações de áudio
    struct AudioInfo* audio_info_list = (struct AudioInfo*)malloc(*num_audios * sizeof(struct AudioInfo));
    if (audio_info_list == NULL) {
        fprintf(stderr, "Erro ao alocar memória para as informações de áudio\n");
        return NULL;
    }

    int num_loaded_audios = 0; // Contador para o número de áudios carregados com sucesso

    for (int i = 0; i < *num_audios; ++i) {
        audio_info_list[i].url = audio_urls[i];
        audio_info_list[i].audio_chunk = loadMusic(audio_urls[i]);
        if (audio_info_list[i].audio_chunk == NULL) {
            // Lidar com falha no carregamento da música conforme necessário
            fprintf(stderr, "Falha ao carregar a música na URL: %s\n", audio_urls[i]);
        }
        else {
            // Incrementar o contador de áudios carregados com sucesso
            num_loaded_audios++;
        }
    }

    // Redimensionar o array de audio_info_list para conter apenas os áudios carregados com sucesso
    struct AudioInfo* loaded_audio_info_list = (struct AudioInfo*)realloc(audio_info_list, num_loaded_audios * sizeof(struct AudioInfo));
    if (loaded_audio_info_list == NULL && num_loaded_audios > 0) {
        // Se houve sucesso no carregamento de pelo menos um áudio, mas falha ao redimensionar o array,
        // liberar o array original e retornar NULL
        free(audio_info_list);
        fprintf(stderr, "Erro ao redimensionar o array de informações de áudio\n");
        return NULL;
    }

    // Atualizar o número de áudios para refletir apenas os áudios carregados com sucesso
    *num_audios = num_loaded_audios;

    return loaded_audio_info_list;
}

// Função para desenhar um botão com uma imagem
void drawImageButton(SDL_Renderer* renderer, SDL_Texture* texture, int x, int y, int width, int height) {
    SDL_Rect dstRect = { x, y,  width, height };
    SDL_RenderCopy(renderer, texture, NULL, &dstRect);
}

// Função para alternar entre pausar e retomar a reprodução da música
void togglePauseResume() {
    if (isPaused) {
        Mix_Resume(-1); // Retomar a reprodução da música
        isPaused = SDL_FALSE; // Atualizar o estado para não pausado
    }
    else {
        Mix_Pause(-1); // Pausar a reprodução da música
        isPaused = SDL_TRUE; // Atualizar o estado para pausado
    }
}

// Função para renderizar o título com uma fonte e tamanho específicos
void renderTitle(SDL_Renderer* renderer, TTF_Font* font, const char* title, int fontSize) {
    SDL_Color titleColor = { 255, 255, 255, 255 }; // Cor branca para o título
    TTF_Font* titleFont = TTF_OpenFont("fonte.ttf", fontSize); // Carregue a fonte com o tamanho desejado
    if (!titleFont) {
        fprintf(stderr, "Erro ao carregar a fonte para o título: %s\n", TTF_GetError());
        return;
    }

    SDL_Surface* titleSurface = TTF_RenderText_Blended(titleFont, title, titleColor);
    if (titleSurface == nullptr) {
        fprintf(stderr, "Erro ao renderizar o título: %s\n", TTF_GetError());
        TTF_CloseFont(titleFont); // Feche a fonte carregada
        return;
    }

    // Defina a posição do título no meio superior da janela
    int titleX = (800 - titleSurface->w) / 2; // Centralize horizontalmente
    int titleY = 20; // Defina uma margem de 20 pixels a partir do topo da janela

    // Crie uma textura a partir da superfície do título
    SDL_Texture* titleTexture = SDL_CreateTextureFromSurface(renderer, titleSurface);
    if (titleTexture == nullptr) {
        fprintf(stderr, "Erro ao criar a textura do título: %s\n", SDL_GetError());
        SDL_FreeSurface(titleSurface);
        TTF_CloseFont(titleFont); // Feche a fonte carregada
        return;
    }

    // Renderize a textura do título na tela
    SDL_Rect titleRect = { titleX, titleY, titleSurface->w, titleSurface->h };
    SDL_RenderCopy(renderer, titleTexture, nullptr, &titleRect);

    // Libere a superfície e a textura do título
    SDL_FreeSurface(titleSurface);
    SDL_DestroyTexture(titleTexture);
    TTF_CloseFont(titleFont); // Feche a fonte carregada
}

// Função para renderizar o campo de entrada de texto na tela
void renderInputText(SDL_Renderer* renderer, struct InputText* inputText) {
    SDL_Rect inputRect = { 250, 200, 300, 30 }; // Posição e tamanho do campo de entrada
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); // Cor branca
    SDL_RenderDrawRect(renderer, &inputRect); // Desenha o campo de entrada
    SDL_Color textColor = { 0, 0, 0, 255 }; // Cor preta

    // Verificar se o texto não está vazio antes de tentar renderizá-lo
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
            // Se o texto ultrapassa à direita, ajuste a largura do retângulo de texto
            textRect.w = inputRect.x + inputRect.w - textRect.x;
        }

        SDL_RenderCopy(renderer, texture, nullptr, &textRect);

        SDL_FreeSurface(textSurface);
        SDL_DestroyTexture(texture);
    }
}

// Função para lidar com o evento de pressionar a tecla Enter
void handleEnterKeyPress(struct InputText* inputText) {
    // Abra o arquivo para escrita em modo de anexação (append)
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

// Função para processar eventos de entrada de texto
void handleTextInputEvent(SDL_Event* event, struct InputText* inputText, SDL_Renderer* renderer) {
    if (event->type == SDL_TEXTINPUT && inputText->active) {
        // Concatenar o texto de entrada à string existente, se não estiver cheia
        if (strlen(inputText->text) + strlen(event->text.text) < sizeof(inputText->text)) {
            strcat(inputText->text, event->text.text);
            // Atualizar a tela para refletir a mudança no texto
            SDL_RenderPresent(renderer);
        }
    }
    else if (event->type == SDL_KEYDOWN && inputText->active) {
        // Processar a tecla de backspace para apagar o último caractere
        if (event->key.keysym.sym == SDLK_BACKSPACE && strlen(inputText->text) > 0) {
            // Apagar o último caractere
            inputText->text[strlen(inputText->text) - 1] = '\0';
            // Limpar o campo de entrada de texto
            SDL_SetRenderDrawColor(renderer, 44, 47, 51, 255); // Definir a cor de fundo
            SDL_Rect inputRect = { 250, 200, 300, 30 }; // Posição e tamanho do campo de entrada
            SDL_RenderFillRect(renderer, &inputRect); // Preencher o retângulo com a cor de fundo
            // Renderizar o campo de entrada de texto novamente
            renderInputText(renderer, inputText);
            // Atualizar a tela
            SDL_RenderPresent(renderer);
        }
        if (event->key.keysym.sym == SDLK_v && SDL_GetModState() & KMOD_CTRL) {
            // Manipular a funcionalidade de colar (Ctrl + V)
            const char* clipboardText = SDL_GetClipboardText();
            if (clipboardText != NULL) {
                // Concatenar o texto da área de transferência ao texto existente
                strcat(inputText->text, clipboardText);
                SDL_free((void*)clipboardText); // Liberar memória alocada pela função SDL_GetClipboardText()
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

// Função para liberar todos os recursos
void freeResources(SDL_Window* window, SDL_Renderer* renderer, Mix_Chunk** audio_chunks, int num_chunks) {
    // Liberar cada chunk de áudio
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

    // Botão para iniciar a playlist
    struct Button playButton;
    playButton.centerX = 280; // Centralizado na largura da janela
    playButton.centerY = 450; // Posicionado no terço inferior da janela
    playButton.width = 100;   // Largura do botão
    playButton.height = 100;  // Altura do botão
    playButton.isClicked = SDL_FALSE;

    // Botão para pausar/resumir a playlist
    struct Button pauseButton;
    pauseButton.centerX = 420; // Centralizado na largura da janela
    pauseButton.centerY = 450; // Posicionado no terço inferior da janela
    pauseButton.width = 100;   // Largura do botão
    pauseButton.height = 100;  // Altura do botão
    pauseButton.isClicked = SDL_FALSE;

    // Índice da música atual
    int currentAudioIndex = 0;
    SDL_bool shouldStartLoop = SDL_FALSE;

    //while (true){
    while (currentAudioIndex < 100) {
        // Carregar superfícies
        SDL_Surface* iconSurface = SDL_LoadBMP("icon.bmp");
        SDL_Surface* playButtonSurface = IMG_Load("playbutton.png");
        SDL_Texture* playButtonTexture = SDL_CreateTextureFromSurface(renderer, playButtonSurface);
        SDL_Surface* pauseButtonSurface = IMG_Load("pausebutton.png");
        SDL_Texture* pauseButtonTexture = SDL_CreateTextureFromSurface(renderer, pauseButtonSurface);
        if (!iconSurface) { fprintf(stderr, "Erro ao carregar o ícone: %s\n", SDL_GetError()); }
        if (!playButtonSurface) { fprintf(stderr, "Erro ao carregar a imagem do botão de reprodução: %s\n", IMG_GetError()); }
        if (!playButtonTexture) { fprintf(stderr, "Erro ao criar a textura do botão de reprodução: %s\n", SDL_GetError()); }
        if (!pauseButtonSurface) { fprintf(stderr, "Erro ao carregar a imagem do botão de pausar/resumir: %s\n", IMG_GetError()); }
        if (!pauseButtonTexture) { fprintf(stderr, "Erro ao criar a textura do botão de pausar/resumir: %s\n", SDL_GetError()); }

        // Estrutura para representar o campo de entrada de texto
        struct InputText inputText;
        strcpy(inputText.text, ""); // Inicializa o texto como vazio
        inputText.active = SDL_TRUE; // Define o campo de entrada como ativo
        inputText.rect.x = 250; // Posição x do campo de entrada
        inputText.rect.y = 200; // Posição y do campo de entrada
        inputText.rect.w = 300; // Largura do campo de entrada
        inputText.rect.h = 30; // Altura do campo de entrada

        // Configurar o ícone da janela
        SDL_SetWindowIcon(window, iconSurface);

        // Liberação das superfícies 
        SDL_FreeSurface(iconSurface);
        SDL_FreeSurface(playButtonSurface);
        SDL_FreeSurface(pauseButtonSurface);

        // Limpar a tela
        SDL_SetRenderDrawColor(renderer, 44, 47, 51, 255);
        SDL_RenderClear(renderer);

        // Desenhar os botões na tela
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

                    // Verificar se o clique ocorreu dentro dos limites do botão de reprodução
                    if (mouseX >= playButton.centerX && mouseX <= playButton.centerX + playButton.width &&
                        mouseY >= playButton.centerY && mouseY <= playButton.centerY + playButton.height) {
                        shouldStartLoop = SDL_TRUE;
                        printf("Carregando músicas e tocando logo após!.\n");
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

        // Calcular a posição para centralizar o texto na tela
        int loadingTextX = (800 - loadingTextWidth) / 2;
        int loadingTextY = (600 - loadingTextHeight) / 2;

        // Renderizar o texto "CARREGANDO" na tela
        SDL_Rect loadingTextRect = { loadingTextX, loadingTextY, loadingTextWidth, loadingTextHeight };
        SDL_RenderCopy(renderer, loadingTexture, NULL, &loadingTextRect);
        SDL_DestroyTexture(loadingTexture);
        SDL_RenderPresent(renderer);

        // Carregar e tocar a próxima música
        int num_audios;
        struct AudioInfo* audio_info_list = loadAudioFiles(&num_audios);

        // Remover o texto "CARREGANDO" da tela
        SDL_SetRenderDrawColor(renderer, 44, 47, 51, 255); // Definir a cor de fundo
        SDL_RenderFillRect(renderer, &loadingTextRect); // Preencher o retângulo com a cor de fundo
        SDL_RenderPresent(renderer);

        // Agora o loop principal inicia somente quando o botão de reprodução foi clicado
        while (shouldStartLoop && currentAudioIndex < 100) {
            // Aguardar até que a música termine
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
                SDL_Delay(100); // Esperar um curto período antes de verificar novamente
            }

            // Carregar e tocar a próxima música
            if (currentAudioIndex < num_audios) {
                Mix_PlayChannel(-1, audio_info_list[currentAudioIndex].audio_chunk, 0);
                currentAudioIndex++;
            }
            else {
                // Se já reproduzimos todas as músicas, reinicie a reprodução desde o início
                currentAudioIndex = 0;
            }
        }
    }

    return 0;
}
