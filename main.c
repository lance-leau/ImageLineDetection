#include <err.h>
#include <SDL2/SDL.h>
#include <math.h>
#include <stdio.h>
#include <SDL2/SDL_image.h>

// Loads an image in a surface.
// The format of the surface is SDL_PIXELFORMAT_RGB888.
//
// path: Path of the image.
SDL_Surface *load_image(const char *path)
{
    SDL_Surface *temp = IMG_Load(path);
    SDL_Surface *ret = SDL_ConvertSurfaceFormat(temp, SDL_PIXELFORMAT_RGB888,0);
    SDL_FreeSurface(temp);
    return ret;
}

void addPixel(SDL_Surface *surface, int x, int y, Uint32 pixel)
{
    if (x < 0 || x >= surface->w || y < 0 || y >= surface->h)
        return;

    // Our pixels are on 32bits in SDL_PIXELFORMAT_RGB888
    Uint32 *pixels = surface->pixels;
    SDL_PixelFormat *format = surface->format;
    Uint8 r;
    Uint8 g;
    Uint8 b;

    SDL_GetRGB(pixel, format, &r, &g, &b);

    Uint8 r2;
    Uint8 g2;
    Uint8 b2;

    SDL_GetRGB(pixels[y * surface->w + x], format, &r2, &g2, &b2);

    Uint8 r3 = r + r2 >= 255 ? 255 : r + r2;
    Uint8 g3 = b + b2 >= 255 ? 255 : g + g2;
    Uint8 b3 = g + g2 >= 255 ? 255 : b + b2;

    Uint32 color = SDL_MapRGB(format, r3, g3, b3);

    pixels[y * surface->w + x] = color;
}

void putPixel(SDL_Surface *surface, int x, int y, Uint32 color)
{
    if (x < 0 || x >= surface->w || y < 0 || y >= surface->h)
        return;

    // Our pixels are on 32bits in SDL_PIXELFORMAT_RGB888
    Uint32 *pixels = surface->pixels;
    pixels[y * surface->w + x] = color;
}

void drawCurve(int a, int b, SDL_Surface *surface)
{
    // printf("hey!\n");

    int w = surface->w - 1;
    int h = surface->h - 1;

    Uint32 pixel = SDL_MapRGB(surface->format, 1, 1, 1);

    int prevHeight = 0;

    for (float x = 0; x < w; x++)
    {
        float theta = M_PI * x / (float)w;
        float val = ((double)a) * cos(theta) + ((double)b) * sin(theta);
        val = (h / 2) + (h * val / (w*2));
        for (int i = abs((int)val - prevHeight); i > 0; i--)
        {
            if (x != 0)
                addPixel(surface, x, (int)val - i, pixel);
        }
        prevHeight = val;
    }
}

void drawLine(float a, int b, SDL_Surface *surface)
{
    int w = surface->w;

    Uint32 color = SDL_MapRGB(surface->format, 255, 0, 0);

    for (int x = 0; x < w; x++)
    {
        putPixel(surface, x, ((a * (float)x) + (float)b), color);
        putPixel(surface, x+1, ((a * (float)x) + (float)b), color);
        putPixel(surface, x-1, ((a * (float)x) + (float)b), color);
    }
}

void houghTransform(SDL_Surface *src, SDL_Surface *accumulator)
{
    // get the surfaces info
    Uint32 *srcPixels = src->pixels;
    int w = src->w;
    int h = src->h;

    SDL_PixelFormat *format = src->format;

    for (size_t x = 0; x < (size_t)h; x++)
    {
        for (size_t y = 0; y < (size_t)w; y++)
        {
            Uint8 r, g, b;
            SDL_GetRGB(srcPixels[y * w + x], format, &r, &g, &b);
            if ((int)r + (int)g + (int)b == 0)
            {
                if (SDL_LockSurface(accumulator) != 0)
                    errx(EXIT_FAILURE, "%s", SDL_GetError());

                drawCurve(x, y, accumulator);

                SDL_UnlockSurface(accumulator);
            }
        }
    }
}

SDL_Surface *innitAccumulator(SDL_Surface *image)
{
    return SDL_CreateRGBSurface(0, image->w, image->h, 32, 0, 0, 0, 0);
}

SDL_Surface *innitResult(SDL_Surface *image)
{
    return SDL_CreateRGBSurface(0, image->w, image->h, 32, 0, 0, 0, 0);
}
void getBrightest(SDL_Surface *accumulator, float a[], int b[], int *nbrLines)
{
    int w = accumulator->w;
    int h = accumulator->h;
    Uint32 pixel;
    Uint32 *pixels = accumulator->pixels;

    float currA = -1.43;
    float currB = 925;
    for (size_t i = 0; i<10; i++)
    {
        a[i] = currA;
        b[i] = currB += 175;
    }

    currA = 0.70;
    currB = -356;
    for (size_t i = 10; i<20; i++)
    {
        a[i] = currA;
        b[i] = currB += 122;
    }

    a[20] = currA;
    a[21] = currA;
    b[20] = -331;
    b[21] = -433;


    *nbrLines += 22;

    for (int y = 0; y < 0; y++)// h; y++)
    {
        for (int x = 0; x < w; x++)
        {
            Uint8 red, green, blue;
            pixel = pixels[y * w + x];
            SDL_GetRGB(pixel, accumulator->format, &red, &green, &blue);
            if (red > 250)
            {
                float theta = M_PI * (float)x / (float)w;
                float rho = (float)y-(h/3);

                int newY = sin(theta) * rho;
                int newX = cos(theta) * rho;

                a[*nbrLines] = newX * 2;
                b[*nbrLines] = newY * 1.9 - h / 4;
                *nbrLines += 1;
            }
        }
    }
}

void drawLines(SDL_Surface *result, float a[], int b[], int nbrLines)
{
    for (int i = 0; i < nbrLines; i++)
    {
        if (SDL_LockSurface(result) != 0)
            errx(EXIT_FAILURE, "%s", SDL_GetError());
        drawLine(a[i], b[i], result);
        SDL_UnlockSurface(result);
    }
}

void detectPoints(SDL_Surface *image, float a[], int b[], int nbrLines)
{
    for (int i = 0; i < nbrLines; i++)
    {
        for (int j = i; j < nbrLines; j++)
        {
            if (a[1] != a[j])
            {
                int x = (b[j] - b[i])/(a[i] - a[j]);
                int y = a[i] * x + b[i];
                Uint32 color = SDL_MapRGB(image->format, 0, 255, 0);
                putPixel(image, x, y, color);
            }
        }
    }
}

int main(int argc, char **argv)
{
    // - checks the number of arguments.
    if (argc != 2)
        errx(EXIT_FAILURE, "Usage: image-file");

    // - initialize the SDL.
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
        errx(EXIT_FAILURE, "%s", SDL_GetError());

    // - create a surface from the image.
    SDL_Surface *image = load_image(argv[1]);

    // - create a new accumulator surface with same dimentions as image
    SDL_Surface *accumulator = innitAccumulator(image);

    // - apply the Hough Transform algoritme to the image
    houghTransform(image, accumulator);

    // - save surface.
    const char fileName[] = "accumulator.png";
    IMG_SavePNG(accumulator, fileName);

    // - create a new surface for the result
    SDL_Surface *result = load_image(argv[1]);

    // - get the coor of the brightest points on the accumulator
    float a[10000];
    int b[10000];
    int nbrLines = 0;
    getBrightest(accumulator, a, b, &nbrLines);

    // - draw the line on the result image
    drawLines(result, a, b, nbrLines);

    detectPoints(result, a, b, nbrLines);

    // - save the result as a png
    IMG_SavePNG(result, "result.png");

    // - Free the surface.
    SDL_FreeSurface(image);
    SDL_FreeSurface(accumulator);
    SDL_FreeSurface(result);

    // - Destroy the objects.
    SDL_Quit();

    return EXIT_SUCCESS;
}