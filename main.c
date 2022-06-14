/* nuklear - 1.32.0 - public domain */
/* ===============================================================
 *
 *               ПОДКЛЮЧЕНИЕ НЕОБХОДИМЫХ БИБЛИОТЕК
 *
 * ===============================================================*/

// Библиотеки для работы с графическим интерфейсом

#define COBJMACROS
#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <d3d11.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <time.h>

// Задание параметром приложения (констант)

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600

#define MAX_VERTEX_BUFFER 512 * 1024
#define MAX_INDEX_BUFFER 128 * 1024

// Подключение библиотеки Nuklear (через которую и осуществляется работа с окном)

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#define NK_IMPLEMENTATION
#define NK_D3D11_IMPLEMENTATION
#include "nuklear.h"
#include "nuklear_d3d11.h"

/* ===============================================================
 *
 *                 ОБЪЯВЛЕНИЕ ТИПОВ, ПЕРЕМЕННЫХ
 *
 * ===============================================================*/

// Настройки d3d11 - одной из возможных библиотек для вывода изображения в окне

static IDXGISwapChain *swap_chain;
static ID3D11Device *device;
static ID3D11DeviceContext *context;
static ID3D11RenderTargetView* rt_view;

// Создание типа записи (структуры)

typedef struct _record
{
    int PK;        // Первыичный ключ -- уникальный
    char name[30]; // Имя - строка (30 символов)
    float pay;     // Оплата за минуту
    int count;     // Количество минут
} rec;

rec table[30];          // Создание таблицы из таких записей
int rec_count;          // Количество заполненных записей в таблице
char buffer[256];       // Буфер для ввода текста в edit box
char result[30][3][30]; // Вывод результата (т.к. в результате свои поля, а в таблице свои, создаётся массив для 30 строк
                        // в котором 3 столбца по 30 символов каждый
char res_out = 0;       // переменная логического типа, выводить ли результат или нет

/* ===============================================================
 *
 *                 ОБЪЯВЛЕНИЕ ФУНКЦИЙ
 *
 * ===============================================================*/

void clear_buf(char* buf, int n) // функция, отвечающая за очистку буфера
{
    for (int i = 0; i < n; i++)
    {
        buf[i] = '\0';           // Заполнение пустыми символами
    }
}

// Функция, отвечающая зв инициализацию окна в windows

static void
set_swap_chain_size(int width, int height)
{
    ID3D11Texture2D *back_buffer;
    D3D11_RENDER_TARGET_VIEW_DESC desc;
    HRESULT hr;

    if (rt_view)
        ID3D11RenderTargetView_Release(rt_view);

    ID3D11DeviceContext_OMSetRenderTargets(context, 0, NULL, NULL);

    hr = IDXGISwapChain_ResizeBuffers(swap_chain, 0, width, height, DXGI_FORMAT_UNKNOWN, 0);
    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET || hr == DXGI_ERROR_DRIVER_INTERNAL_ERROR)
    {
        /* to recover from this, you'll need to recreate device and all the resources */
        MessageBoxW(NULL, L"DXGI device is removed or reset!", L"Error", 0);
        exit(0);
    }
    assert(SUCCEEDED(hr));

    memset(&desc, 0, sizeof(desc));
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;

    hr = IDXGISwapChain_GetBuffer(swap_chain, 0, &IID_ID3D11Texture2D, (void **)&back_buffer);
    assert(SUCCEEDED(hr));

    hr = ID3D11Device_CreateRenderTargetView(device, (ID3D11Resource *)back_buffer, &desc, &rt_view);
    assert(SUCCEEDED(hr));

    ID3D11Texture2D_Release(back_buffer);
}

// Функция обработки сообщений раз в тик времени в Windows

static LRESULT CALLBACK
WindowProc(HWND wnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    switch (msg)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    case WM_SIZE:
        if (swap_chain)
        {
            int width = LOWORD(lparam);
            int height = HIWORD(lparam);
            set_swap_chain_size(width, height);
            nk_d3d11_resize(context, width, height);
        }
        break;
    }

    if (nk_d3d11_handle_event(wnd, msg, wparam, lparam))
        return 0;

    return DefWindowProcW(wnd, msg, wparam, lparam);
}

/* ===============================================================
 *
 *                 ОСНОВНАЯ ФУНКЦИЯ
 *
 * ===============================================================*/

int main(void)
{
    struct nk_context *ctx; // Создание окна в nuklear
    struct nk_colorf bg;    // Фон в окне

    // Создание переменных, необходимых для работе в окне Windows

    WNDCLASSW wc;           
    RECT rect = { 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT };
    DWORD style = WS_OVERLAPPEDWINDOW;
    DWORD exstyle = WS_EX_APPWINDOW;
    HWND wnd;
    int running = 1;
    HRESULT hr;
    D3D_FEATURE_LEVEL feature_level;
    DXGI_SWAP_CHAIN_DESC swap_chain_desc;

    /* Инициаоизация окна типа Win32 */

    memset(&wc, 0, sizeof(wc));
    wc.style = CS_DBLCLKS;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = GetModuleHandleW(0);
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = L"NuklearWindowClass";
    RegisterClassW(&wc);

    AdjustWindowRectEx(&rect, style, FALSE, exstyle);

    wnd = CreateWindowExW(exstyle, wc.lpszClassName, L"Aboba",
        style | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT,
        rect.right - rect.left, rect.bottom - rect.top,
        NULL, NULL, wc.hInstance, NULL);

    /* Инициализация D3D11 */
    memset(&swap_chain_desc, 0, sizeof(swap_chain_desc));
    swap_chain_desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swap_chain_desc.BufferDesc.RefreshRate.Numerator = 60;
    swap_chain_desc.BufferDesc.RefreshRate.Denominator = 1;
    swap_chain_desc.SampleDesc.Count = 1;
    swap_chain_desc.SampleDesc.Quality = 0;
    swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swap_chain_desc.BufferCount = 1;
    swap_chain_desc.OutputWindow = wnd;
    swap_chain_desc.Windowed = TRUE;
    swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    swap_chain_desc.Flags = 0;
    if (FAILED(D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE,
        NULL, 0, NULL, 0, D3D11_SDK_VERSION, &swap_chain_desc,
        &swap_chain, &device, &feature_level, &context)))
    {
        /* if hardware device fails, then try WARP high-performance
           software rasterizer, this is useful for RDP sessions */
        hr = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_WARP,
            NULL, 0, NULL, 0, D3D11_SDK_VERSION, &swap_chain_desc,
            &swap_chain, &device, &feature_level, &context);
        assert(SUCCEEDED(hr));
    }
    set_swap_chain_size(WINDOW_WIDTH, WINDOW_HEIGHT);

 /* ===============================================================
 *
 *                 НАЧАЛО ОСНОВНОГО КОДА
 *
 * ===============================================================*/

    // Начальное значение в edit
    strcpy(buffer, "D:\\Database.txt");

    /* Инициализация GUI интерфейса */
    ctx = nk_d3d11_init(device, WINDOW_WIDTH, WINDOW_HEIGHT, MAX_VERTEX_BUFFER, MAX_INDEX_BUFFER);
    {struct nk_font_atlas *atlas;
    nk_d3d11_font_stash_begin(&atlas);
    nk_d3d11_font_stash_end();}

    bg.r = 0.10f, bg.g = 0.18f, bg.b = 0.24f, bg.a = 1.0f;
    while (running)
    {
        // Обработка сообщений
        MSG msg;
        nk_input_begin(ctx);
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
                running = 0;
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        nk_input_end(ctx);

        /* Начало интерфейса */

        // Если создание окна будет успешно, то продолжение кода, иначе вывод соответствующего сообщения

        if (nk_begin(ctx, "Calculator", nk_rect(50, 50, 480, 400),  // Инициализация окна ( Где показывать, Название окна, 
            NK_WINDOW_BORDER | NK_WINDOW_MOVABLE |                  // расположение окна (где находится левый верхний угол и его размеры)
            NK_WINDOW_MINIMIZABLE | NK_WINDOW_TITLE))               // доп. параметры (показ границ окна, возможность его двигать, скрывать, и показ его заголовка)
        {
            // Выделение области под объект
            nk_layout_row_dynamic(ctx, 30, 1); 
            //Инифиализация объекта
            nk_label_wrap(ctx, "Hello! This program can calculate cost of one call telephone company!", NK_TEXT_CENTERED);
            
            nk_layout_row_dynamic(ctx, 60, 1);
            nk_label(ctx, "Please, write the path to the database file below:", NK_TEXT_ALIGN_BOTTOM | NK_TEXT_ALIGN_LEFT);
            
            nk_layout_row_dynamic(ctx, 40, 1);
            // Создание элемента EditBox, в котором пользователь прописывает путь
            nk_flags event = nk_edit_string_zero_terminated(ctx,
                NK_EDIT_BOX | NK_EDIT_AUTO_SELECT, //focus will auto select all text (NK_EDIT_BOX not sure)
                buffer, sizeof(buffer), nk_filter_ascii);

            // Вывод результата
            
            for (int i = 0; i < rec_count; i++)
            {
                nk_layout_row_dynamic(ctx, 15, 2);
                nk_label(ctx, result[i][0], NK_TEXT_CENTERED);
                nk_label(ctx, result[i][1], NK_TEXT_CENTERED);
            }
            
            // Создание пустого пространства
            nk_layout_row_dynamic(ctx, 60, 1);

            // Выделение места под кнопку
            nk_layout_row_dynamic(ctx, 30, 1);
            // Создание кнопки
            if (nk_button_label(ctx, "Calculate!"))
            {

                /* ===============================================================
                *
                *                 ПРИ НАЖАТИИ КНОПКИ
                *
                * ===============================================================*/

                // Инициализация переменных
                rec_count = 0;                                                  // Обнуление числа записей в таблице
                res_out = 0;                                                    // Обнуление числа записей в выходной таблице
                char boo = 0;                                                   // Логическая переменная, отвечающая за запуск чтения файла
                if (!boo)
                {
                    char* filename = buffer;                                    // Передача путя к файлу в специальную переменную. По умолчанию: "D:\\Database.txt";
                    FILE* fp;                                                   // Переменная файла
                    if ((fp = fopen(filename, "r+")) != NULL)                   // При успешном открытии файла на чтение присваиваем переменной fp данный файл
                    {
                        enum cols {                                             // Более удобное описание столбцов (вместо цыферок - букАвки)
                            NAME,                                               //0, соответствует первому полю записи
                            PAY,                                                //1, соответствует второму полю записи
                            COUNT                                               //2, соответствует третьему полю записи
                        };
                        char c;                                                 // Читаемый символ    
                        char buf[30];                                           // Буфер для значений
                        int j = 0;                                              // переменная для буфера buf (где находится каретка)
                        int i = 0;                                              // переменная для отслеживания столбца
                        clear_buf(buf, 30);                                     // очистка буфера

                        while ((c = getc(fp)) != EOF)                           // Чтение символа пока не конец файла
                        {
                            if (c == '\t' || (c == '\n' && i == 2))             // Отслеживание перехода столбца
                            {
                                switch (i)
                                {
                                case NAME:
                                    table[rec_count].PK = rec_count;            // Записываем значение первичного ключа в нужный столбец
                                    for (int ji = 0; ji < 30; ji++)             //Цикл для очистки буфера и переноса строки из буфера в столбец
                                    {
                                        table[rec_count].name[ji] = buf[ji];    //Перенос символа
                                        buf[ji] = '\0';                         //Очистка буфера
                                    }
                                    break;
                                case PAY:
                                    table[rec_count].pay = atof(buf);           // Перенос значения в нужный столбец
                                    clear_buf(buf, 30);                         // очистка буфера
                                    break;
                                case COUNT:
                                    table[rec_count].count = atoi(buf);         // Перенос значения в нужный столбец
                                    clear_buf(buf, 30);                         // Очистка буфера
                                    rec_count++;                                // Увеличение количества ЗАПОЛНЕННЫХ записей на 1
                                    i = -1;                                     // Сброс столбца
                                    break;
                                };
                                i++;                                            // Переход в следующий столбец
                                j = 0;                                          // Сброс каретки в буфере
                                continue;
                            }
                            buf[j++] = c;                                       // Перенос читаемого символа в буфер
                        }
                        fclose(fp);                                             //Закрытие файла
                        for (int i = 0; i < rec_count; i++)                     // Пробег по всем записям таблицы
                            {
                            for (int ji = 0; ji < 30; ji++)                     // Перенос значения первого столбца (имени (строки)) в результирующую таблицу
                                result[i][0][ji] = table[i].name[ji];           
                            sprintf(result[i][1], "%.2f",                       // Подсчёт общей цены за звонок(-и) и перенос в результирующую таблицу
                                table[i].pay * table[i].count);
                            
                            }
                        res_out = 1;                                            // Меняем переключатель на "ДА" (выводить таблицу)
                    }
                    else MessageBox(NULL, L"Error while reading file!", L"Error", 0);   // Ежели чтение файла было неуспешным, то выводится соответствующее сообщение
                }
            }
    }
    nk_end(ctx);    // Завершение окна


       /* ===============================================================
        *
        *                 ОТРИСОВКА ИНТЕРФЕЙСА НА ЭКРАН
        *
        * ===============================================================*/

        /* Draw */
        ID3D11DeviceContext_ClearRenderTargetView(context, rt_view, &bg.r);
        ID3D11DeviceContext_OMSetRenderTargets(context, 1, &rt_view, NULL);
        nk_d3d11_render(context, NK_ANTI_ALIASING_ON);
        hr = IDXGISwapChain_Present(swap_chain, 1, 0);
        if (hr == DXGI_ERROR_DEVICE_RESET || hr == DXGI_ERROR_DEVICE_REMOVED) {
            /* to recover from this, you'll need to recreate device and all the resources */
            MessageBoxW(NULL, L"D3D11 device is lost or removed!", L"Error", 0);
            break;
        } else if (hr == DXGI_STATUS_OCCLUDED) {
            /* window is not visible, so vsync won't work. Let's sleep a bit to reduce CPU usage */
            Sleep(10);
        }
        assert(SUCCEEDED(hr));
    }

    /* ===============================================================
     *
     *                 Освобождеине памяти после завершения
     *
     * ===============================================================*/

    ID3D11DeviceContext_ClearState(context);
    nk_d3d11_shutdown();
    ID3D11RenderTargetView_Release(rt_view);
    ID3D11DeviceContext_Release(context);
    ID3D11Device_Release(device);
    IDXGISwapChain_Release(swap_chain);
    UnregisterClassW(wc.lpszClassName, wc.hInstance);
    return 0;
}
