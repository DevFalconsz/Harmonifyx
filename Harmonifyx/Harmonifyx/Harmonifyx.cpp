#define CURL_STATICLIB
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl.h>
#include <SDL.h>
#include <SDL_mixer.h>

// Fun��o para desenhar um c�rculo
void drawCircle(SDL_Renderer* renderer, int centerX, int centerY, int radius) {
    SDL_SetRenderDrawColor(renderer, 0, 0, 255, 255);

    for (int angle = 0; angle < 360; ++angle) {
        int x = centerX + (int)(radius * cos(angle * M_PI / 180.0));
        int y = centerY + (int)(radius * sin(angle * M_PI / 180.0));
        SDL_RenderDrawPoint(renderer, x, y);
    }
}

// Fun��o para desenhar um tri�ngulo (um "play" simplificado) apontando para a direita
void drawPlaySymbol(SDL_Renderer* renderer, int x, int y, int size) {
    SDL_SetRenderDrawColor(renderer, 0, 0, 255, 255);

    // Desenhar tri�ngulo (seta apontando para a direita)
    SDL_RenderDrawLine(renderer, x - size / 2, y - size / 2, x + size / 2, y);
    SDL_RenderDrawLine(renderer, x + size / 2, y, x - size / 2, y + size / 2);

    // Linhas adicionais para fechar o tri�ngulo
    SDL_RenderDrawLine(renderer, x - size / 2, y - size / 2, x - size / 2, y + size / 2);
}

// Estrutura para armazenar dados tempor�rios
struct AudioData {
    char* data;
    size_t size;
};

struct Button {
    int centerX;
    int centerY;
    int radius;
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

int main(int argc, char* argv[]) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
        fprintf(stderr, "Erro ao inicializar o SDL: %s\n", SDL_GetError());
        return 1;
    }

    // Inicializar o SDL_mixer
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
        fprintf(stderr, "Erro ao inicializar o SDL_mixer: %s\n", Mix_GetError());
        SDL_Quit();
        return 1;
    }

    // Inicializar a biblioteca libcurl
    curl_global_init(CURL_GLOBAL_DEFAULT);

    // Crie uma inst�ncia CURL
    CURL* curl = curl_easy_init();
    if (!curl) {
        fprintf(stderr, "Erro ao inicializar o libcurl\n");
        Mix_CloseAudio();
        SDL_Quit();
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("Harmonifyx", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 800, 600, SDL_WINDOW_SHOWN);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    // URLs de streaming de �udio ao vivo
    const char* urls[] = {
        "https://opengameart.org/sites/default/files/song18_0.mp3",
        "https://opengameart.org/sites/default/files/No%20More%20Magic_5.mp3",
        "https://opengameart.org/sites/default/files/the_field_of_dreams.mp3",
    };

    // Inicializar dados de �udio
    struct AudioData audio_data = { NULL, 0 };

    // Configurar a fun��o de callback para processar os dados recebidos
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &audio_data);

    // Configurar a op��o para streaming
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    struct Button button;
    button.centerX = 400; // Centralizado na largura da janela
    button.centerY = 500; // Posicionado no ter�o inferior da janela
    button.radius = 50;   // Raio do bot�o
    button.isClicked = SDL_FALSE;

    // Lista de Mix_Chunk para armazenar as m�sicas
    Mix_Chunk* audio_chunks[sizeof(urls) / sizeof(urls[0])] = { NULL };

    // Carregar todas as m�sicas antecipadamente
    for (int i = 0; i < sizeof(urls) / sizeof(urls[0]); ++i) {
        // Configurar a URL
        curl_easy_setopt(curl, CURLOPT_URL, urls[i]);

        // Executar a solicita��o
        CURLcode res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            fprintf(stderr, "Erro ao realizar a solicita��o: %s\n", curl_easy_strerror(res));
        }
        else {
            // Tocar o �udio com o SDL_mixer
            audio_chunks[i] = Mix_LoadWAV_RW(SDL_RWFromMem(audio_data.data, (int)audio_data.size), 1);
            if (audio_chunks[i] == NULL) {
                fprintf(stderr, "Erro ao carregar o �udio com SDL_mixer: %s\n", Mix_GetError());
            }
        }

        // Limpar recursos
        free(audio_data.data);
        audio_data.data = NULL;
        audio_data.size = 0;
    }

    // �ndice da m�sica atual
    int currentAudioIndex = 0;
    SDL_bool shouldStartLoop = SDL_FALSE;

    while (currentAudioIndex < sizeof(urls) / sizeof(urls[0])) {
        // Limpar a tela
        SDL_SetRenderDrawColor(renderer, 44, 47, 51, 255);
        SDL_RenderClear(renderer);

        // Carregar a superf�cie do �cone
        SDL_Surface* iconSurface = SDL_LoadBMP("icon.bmp");
        if (!iconSurface) {
            fprintf(stderr, "Erro ao carregar o �cone: %s\n", SDL_GetError());
            // Tratar o erro conforme necess�rio
        }

        // Configurar o �cone da janela
        SDL_SetWindowIcon(window, iconSurface);

        // Libera��o da superf�cie do �cone, pois j� foi configurada
        SDL_FreeSurface(iconSurface);

        // Desenhar o bot�o redondo
        drawCircle(renderer, button.centerX, button.centerY, button.radius);

        // Desenhar o s�mbolo de "play" dentro do bot�o
        drawPlaySymbol(renderer, button.centerX + 8, button.centerY, button.radius);

        // Atualizar a tela
        SDL_RenderPresent(renderer);

        // Aguardar at� que o bot�o seja clicado
        while (!shouldStartLoop) {
            SDL_Event event;
            while (SDL_PollEvent(&event) != 0) {
                if (event.type == SDL_QUIT) {
                    //shouldStartLoop = SDL_TRUE; // Liberar recursos finais
                    break;
                }
                else if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
                    int mouseX, mouseY;
                    SDL_GetMouseState(&mouseX, &mouseY);

                    // Verificar se o bot�o foi clicado
                    int distanceSquared = (mouseX - button.centerX) * (mouseX - button.centerX) +
                        (mouseY - button.centerY) * (mouseY - button.centerY);
                    if (distanceSquared <= button.radius * button.radius) {
                        shouldStartLoop = SDL_TRUE;
                        printf("Bot�o clicado! Iniciando loop.\n");
                    }
                }
            }
        }

        // Agora o loop principal inicia somente quando o bot�o foi clicado
        while (shouldStartLoop && currentAudioIndex < sizeof(urls) / sizeof(urls[0])) {
            // Aguardar at� que a m�sica termine
            while (Mix_Playing(-1) != 0) {
                SDL_Event event;
                while (SDL_PollEvent(&event) != 0) {
                    if (event.type == SDL_QUIT) {
                        shouldStartLoop = SDL_FALSE; // Para sair do programa
                        break;
                    }
                }
                SDL_Delay(100); // Esperar um curto per�odo antes de verificar novamente
            }

            // Carregar e tocar a pr�xima m�sica
            Mix_PlayChannel(-1, audio_chunks[currentAudioIndex], 0);
            currentAudioIndex++;

            // Aguardar at� que a m�sica termine antes de passar para a pr�xima
            while (Mix_Playing(-1) != 0) {
                SDL_Event event;
                while (SDL_PollEvent(&event) != 0) {
                    if (event.type == SDL_QUIT) {
                        shouldStartLoop = SDL_FALSE; // Para sair do programa
                        break;
                    }
                }
                SDL_Delay(100); // Esperar um curto per�odo antes de verificar novamente
            }
        }
    }

    // Liberar recursos finais
    for (int i = 0; i < sizeof(urls) / sizeof(urls[0]); ++i) {
        Mix_FreeChunk(audio_chunks[i]);
    }

    curl_easy_cleanup(curl);
    free(audio_data.data);

    // Finalizar a biblioteca libcurl
    curl_global_cleanup();

    // Fechar o SDL_mixer
    Mix_CloseAudio();

    // Finalizar o SDL
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
